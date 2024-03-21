#include "GstAVPipeline.h"
#include "DebugLog.h"

#include <d3d11sdklayers.h>
#include <dxgi1_2.h>
#include <dxgiformat.h>
#include <d3d11_1.h>

#include "Unity/IUnityGraphicsD3D11.h"
using namespace Microsoft::WRL;



// Call only on plugin thread
// Creates the underlying D3D11 texture using the provided unity device.
// This texture can then be turned into a proper Unity texture on the
// managed side using Texture2D.CreateExternalTexture()
bool GstAVPipeline::CreateTexture(unsigned int width, unsigned int height, bool left)
{
	auto device = _s_UnityInterfaces->Get<IUnityGraphicsD3D11>()->GetDevice();
	HRESULT hr = S_OK;

	gst_video_info_set_format(&_render_info, GST_VIDEO_FORMAT_RGBA, width, height);

	std::unique_ptr<AppData> data = std::make_unique<AppData>();
	data->avpipeline = this;


	// Create a texture 2D that can be shared
	D3D11_TEXTURE2D_DESC desc = {};
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
	desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX | D3D11_RESOURCE_MISC_SHARED_NTHANDLE;
	//desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;


	hr = device->CreateTexture2D(&desc, nullptr, &data->texture);
	g_assert(SUCCEEDED(hr));

	hr = data->texture.As(&data->keyed_mutex);
	g_assert(SUCCEEDED(hr));

	hr = data->keyed_mutex->AcquireSync(0, INFINITE);
	g_assert(SUCCEEDED(hr));

	ComPtr<IDXGIResource1> dxgi_resource;
	hr = data->texture.As(&dxgi_resource);
	g_assert(SUCCEEDED(hr));

	HANDLE shared_handle = nullptr;
	hr = dxgi_resource->CreateSharedHandle(nullptr,
		DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, nullptr,
		&shared_handle);
	g_assert(SUCCEEDED(hr));

	auto gst_device = gst_d3d11_device_get_device_handle(_device);
	ComPtr<ID3D11Device1> device1;
	hr = gst_device->QueryInterface(IID_PPV_ARGS(&device1));
	g_assert(SUCCEEDED(hr));
	

	/* Open shared texture at GStreamer device side */
	ComPtr<ID3D11Texture2D> gst_texture;
	hr = device1->OpenSharedResource1(shared_handle,
		IID_PPV_ARGS(&gst_texture));
	g_assert(SUCCEEDED(hr));
	/* Can close NT handle now */
	CloseHandle(shared_handle);

	/* Wrap shared texture with GstD3D11Memory in order to convert texture
 * using converter API */
	GstMemory* mem = gst_d3d11_allocator_alloc_wrapped(nullptr, _device,
		gst_texture.Get(),
		/* CPU accessible (staging texture) memory size is unknown.
		* Pass zero here, then GStreamer will calculate it */
		0, nullptr, nullptr);
	g_assert(mem);

	data->shared_buffer = gst_buffer_new();
	gst_buffer_append_memory(data->shared_buffer, mem);


	if (left)
		_leftData = std::move(data);
	else
		_rightData = std::move(data);

	return true;
}

GstFlowReturn GstAVPipeline::on_new_sample(GstAppSink* appsink, gpointer user_data)
{
	AppData* data = static_cast<AppData*>(user_data);
	GstSample* sample = gst_app_sink_pull_sample(appsink);

	if (!sample)
		return GST_FLOW_ERROR;

	GstCaps* caps = gst_sample_get_caps(sample);
	if (!caps) {
		gst_sample_unref(sample);
		Debug::Log("Sample without caps", Level::Error);
		return GST_FLOW_ERROR;
	}

	std::lock_guard<std::mutex> lk(data->lock);
	//data->avpipeline->Enter();
	/* Caps updated, recreate converter */
	if (data->last_caps && !gst_caps_is_equal(data->last_caps, caps))
		gst_clear_object(&data->conv);

	if (!data->conv) {
		Debug::Log("Create new converter");
		GstVideoInfo in_info;
		gst_video_info_from_caps(&in_info, caps);

		/* In case of shared texture, video processor might not behave as expected.
		 * Use only pixel shader */
		auto config = gst_structure_new("converter-config",
			GST_D3D11_CONVERTER_OPT_BACKEND, GST_TYPE_D3D11_CONVERTER_BACKEND,
			GST_D3D11_CONVERTER_BACKEND_SHADER, nullptr);

		data->conv = gst_d3d11_converter_new(data->avpipeline->_device, &in_info,
			&data->avpipeline->_render_info, config);
	}

	gst_caps_replace(&data->last_caps, caps);
	gst_clear_sample(&data->last_sample);
	data->last_sample = sample;

	return GST_FLOW_OK;
}

void GstAVPipeline::Draw(bool left)
{
	//Debug::Log("drawing");
	AppData* data;
	if (left)
		data = _leftData.get();
	else
		data = _rightData.get();

	if (data == nullptr)
	{
		Debug::Log("data is null", Level::Warning);
		return;
	}

	GstSample* sample = nullptr;

	/* Steal sample pointer */
	std::lock_guard <std::mutex> lk(data->lock);
	/* If there's no updated sample, don't need to render again */
	if (!data->last_sample)
		return;

	sample = data->last_sample;
	data->last_sample = nullptr;

	auto buf = gst_sample_get_buffer(sample);
	if (!buf) {
		Debug::Log("Sample without buffer", Level::Error);
		gst_sample_unref(sample);
		return;
	}

	data->keyed_mutex->ReleaseSync(0);
	/* Converter will take gst_d3d11_device_lock() and acquire sync */
	gst_d3d11_converter_convert_buffer(data->conv, buf, data->shared_buffer);
	data->keyed_mutex->AcquireSync(0, INFINITE);
	gst_sample_unref(sample);
}

GstElement* GstAVPipeline::add_rtph264depay(GstElement* pipeline)
{
	GstElement* rtph264depay = gst_element_factory_make("rtph264depay", nullptr);
	if (!rtph264depay) {
		Debug::Log("Failed to create rtph264depay", Level::Error);
		return nullptr;
	}
	gst_bin_add(GST_BIN(pipeline), rtph264depay);
	return rtph264depay;
}

GstElement* GstAVPipeline::add_h264parse(GstElement* pipeline)
{
	GstElement* h264parse = gst_element_factory_make("h264parse", nullptr);
	if (!h264parse) {
		Debug::Log("Failed to create h264parse", Level::Error);
		return nullptr;
	}
	gst_bin_add(GST_BIN(pipeline), h264parse);
	return h264parse;
}

GstElement* GstAVPipeline::add_d3d11h264dec(GstElement* pipeline)
{
	GstElement* d3d11h264dec = gst_element_factory_make("d3d11h264dec", nullptr);
	if (!d3d11h264dec) {
		Debug::Log("Failed to create d3d11h264dec", Level::Error);
		return nullptr;
	}
	gst_bin_add(GST_BIN(pipeline), d3d11h264dec);
	return d3d11h264dec;
}

GstElement* GstAVPipeline::add_d3d11convert(GstElement* pipeline)
{
	GstElement* d3d11convert = gst_element_factory_make("d3d11convert", nullptr);
	if (!d3d11convert) {
		Debug::Log("Failed to create d3d11convert", Level::Error);
		return nullptr;
	}
	gst_bin_add(GST_BIN(pipeline), d3d11convert);
	return d3d11convert;
}

GstElement* GstAVPipeline::add_appsink(GstElement* pipeline)
{
	GstElement* appsink = gst_element_factory_make("appsink", nullptr);
	if (!appsink) {
		Debug::Log("Failed to create appsink", Level::Error);
		return nullptr;
	}

	GstCaps* caps = gst_caps_from_string("video/x-raw(memory:D3D11Memory),format=RGBA");
	g_object_set(appsink, "caps", caps, "drop", true, "max-buffers", 3, nullptr);
	gst_caps_unref(caps);

	gst_bin_add(GST_BIN(pipeline), appsink);
	return appsink;
}

GstElement* GstAVPipeline::add_rtpopusdepay(GstElement* pipeline)
{
	GstElement* rtpopusdepay = gst_element_factory_make("rtpopusdepay", nullptr);
	if (!rtpopusdepay) {
		Debug::Log("Failed to create rtpopusdepay", Level::Error);
		return nullptr;
	}

	gst_bin_add(GST_BIN(pipeline), rtpopusdepay);
	return rtpopusdepay;
}

GstElement* GstAVPipeline::add_queue(GstElement* pipeline)
{
	GstElement* queue = gst_element_factory_make("queue", nullptr);
	if (!queue) {
		Debug::Log("Failed to create queue", Level::Error);
		return nullptr;
	}

	gst_bin_add(GST_BIN(pipeline), queue);
	return queue;
}


GstElement* GstAVPipeline::add_opusdec(GstElement* pipeline)
{
	GstElement* opusdec = gst_element_factory_make("opusdec", nullptr);
	if (!opusdec) {
		Debug::Log("Failed to create opusdec", Level::Error);
		return nullptr;
	}

	gst_bin_add(GST_BIN(pipeline), opusdec);
	return opusdec;
}

GstElement* GstAVPipeline::add_audioconvert(GstElement* pipeline)
{
	GstElement* audioconvert = gst_element_factory_make("audioconvert", nullptr);
	if (!audioconvert) {
		Debug::Log("Failed to create audioconvert", Level::Error);
		return nullptr;
	}

	gst_bin_add(GST_BIN(pipeline), audioconvert);
	return audioconvert;
}

GstElement* GstAVPipeline::add_audioresample(GstElement* pipeline)
{
	GstElement* audioresample = gst_element_factory_make("audioresample", nullptr);
	if (!audioresample) {
		Debug::Log("Failed to create audioresample", Level::Error);
		return nullptr;
	}

	gst_bin_add(GST_BIN(pipeline), audioresample);
	return audioresample;
}

GstElement* GstAVPipeline::add_wasapi2sink(GstElement* pipeline)
{
	GstElement* wasapi2sink = gst_element_factory_make("wasapi2sink", nullptr);
	if (!wasapi2sink) {
		Debug::Log("Failed to create wasapi2sink", Level::Error);
		return nullptr;
	}
	g_object_set(wasapi2sink, "low-latency", true, "provide-clock", false, nullptr);

	gst_bin_add(GST_BIN(pipeline), wasapi2sink);
	return wasapi2sink;
}

void GstAVPipeline::on_pad_added(GstElement* src, GstPad* new_pad, gpointer data) {
	GstAVPipeline* avpipeline = static_cast<GstAVPipeline*>(data);

	gchar* pad_name = gst_pad_get_name(new_pad);
	Debug::Log("Adding pad ");
	if (g_str_has_prefix(pad_name, "video")) {
		Debug::Log("Adding video pad " + std::string(pad_name));
		GstElement* rtph264depay = add_rtph264depay(avpipeline->_pipeline);
		GstElement* h264parse = add_h264parse(avpipeline->_pipeline);
		GstElement* d3d11h264dec = add_d3d11h264dec(avpipeline->_pipeline);
		GstElement* d3d11convert = add_d3d11convert(avpipeline->_pipeline);
		GstElement* appsink = add_appsink(avpipeline->_pipeline);

		GstAppSinkCallbacks callbacks = {nullptr};
		callbacks.new_sample = on_new_sample;
		
		if (g_str_has_prefix(pad_name, "video_0"))
		{
			Debug::Log("Connecting left video pad " + std::string(pad_name));
			gst_app_sink_set_callbacks(GST_APP_SINK(appsink), &callbacks, avpipeline->_leftData.get(), nullptr);
		}
		else
		{
			Debug::Log("Connecting right video pad " + std::string(pad_name));
			gst_app_sink_set_callbacks(GST_APP_SINK(appsink), &callbacks, avpipeline->_rightData.get(), nullptr);			
		}

		if (!gst_element_link_many(rtph264depay, h264parse, d3d11h264dec, d3d11convert, appsink, nullptr)) {
			Debug::Log("Elements could not be linked.");
		}

		GstPad* sinkpad = gst_element_get_static_pad(rtph264depay, "sink");
		if (gst_pad_link(new_pad, sinkpad) != GST_PAD_LINK_OK) {
			Debug::Log("Could not link dynamic video pad to rtph264depay", Level::Error);
		}
		gst_object_unref(sinkpad);

		gst_element_sync_state_with_parent(rtph264depay);
		gst_element_sync_state_with_parent(h264parse);
		gst_element_sync_state_with_parent(d3d11h264dec);
		gst_element_sync_state_with_parent(d3d11convert);
		gst_element_sync_state_with_parent(appsink);

		gst_object_unref(rtph264depay);
		gst_object_unref(h264parse);
		gst_object_unref(d3d11h264dec);
		gst_object_unref(d3d11convert);
		gst_object_unref(appsink);
	}
	else if (g_str_has_prefix(pad_name, "audio")) {
		Debug::Log("Adding audio pad " + std::string(pad_name));
		GstElement* rtpopusdepay = add_rtpopusdepay(avpipeline->_pipeline);
		GstElement* queue = add_queue(avpipeline->_pipeline);
		GstElement* opusdec = add_opusdec(avpipeline->_pipeline);
		GstElement* audioconvert = add_audioconvert(avpipeline->_pipeline);
		GstElement* audioresample = add_audioresample(avpipeline->_pipeline);
		GstElement* wasapi2sink = add_wasapi2sink(avpipeline->_pipeline);

		if (!gst_element_link_many(rtpopusdepay, opusdec, audioconvert, audioresample, queue, wasapi2sink, nullptr)) {
			Debug::Log("Audio elements could not be linked.", Level::Error);
		}

		GstPad* sinkpad = gst_element_get_static_pad(rtpopusdepay, "sink");
		if (gst_pad_link(new_pad, sinkpad) != GST_PAD_LINK_OK) {
			Debug::Log("Could not link dynamic audio pad to rtpopusdepay", Level::Error);
		}
		gst_object_unref(sinkpad);

		gst_element_sync_state_with_parent(rtpopusdepay);
		gst_element_sync_state_with_parent(opusdec);
		gst_element_sync_state_with_parent(audioconvert);
		gst_element_sync_state_with_parent(audioresample);
		gst_element_sync_state_with_parent(queue);
		gst_element_sync_state_with_parent(wasapi2sink);

		gst_object_unref(rtpopusdepay);
		gst_object_unref(queue);
		gst_object_unref(opusdec);
		gst_object_unref(audioconvert);
		gst_object_unref(audioresample);
		gst_object_unref(wasapi2sink);
	}
	g_free(pad_name);
	//gst_bin_recalculate_latency(GST_BIN(avpipeline->_pipeline));
}

void GstAVPipeline::webrtcbin_ready(GstElement* self, gchararray peer_id, GstElement* webrtcbin, gpointer udata)
{
	Debug::Log("Configure webrtcbin", Level::Info);
	g_object_set(webrtcbin, "latency", 50, nullptr);
}

void GstAVPipeline::ReleaseTexture(ID3D11Texture2D* texture) {
	if (texture != nullptr)
	{
		texture->Release();
		texture = nullptr;
	}
}

GstAVPipeline::GstAVPipeline(IUnityInterfaces* s_UnityInterfaces) : _s_UnityInterfaces(s_UnityInterfaces)
{
	preloaded_plugins.push_back(gst_plugin_load_by_name("rswebrtc"));
	if (!preloaded_plugins.back()) {
		Debug::Log("Failed to load 'rswebrtc' plugin", Level::Error);
	}

	preloaded_plugins.push_back(gst_plugin_load_by_name("webrtc"));
	if (!preloaded_plugins.back()) {
		Debug::Log("Failed to load 'webrtc' plugin", Level::Error);
	}
	preloaded_plugins.push_back(gst_plugin_load_by_name("d3d11"));
	if (!preloaded_plugins.back()) {
		Debug::Log("Failed to load 'd3d11' plugin", Level::Error);
	}
	preloaded_plugins.push_back(gst_plugin_load_by_name("rtpmanager"));
	if (!preloaded_plugins.back()) {
		Debug::Log("Failed to load 'rtpmanager' plugin", Level::Error);
	}
	preloaded_plugins.push_back(gst_plugin_load_by_name("opus"));
	if (!preloaded_plugins.back()) {
		Debug::Log("Failed to load 'opus' plugin", Level::Error);
	}
	preloaded_plugins.push_back(gst_plugin_load_by_name("wasapi2"));
	if (!preloaded_plugins.back()) {
		Debug::Log("Failed to load 'wasapi2' plugin", Level::Error);
	}
	preloaded_plugins.push_back(gst_plugin_load_by_name("dtls"));
	if (!preloaded_plugins.back()) {
		Debug::Log("Failed to load 'dtls' plugin", Level::Error);
	}
	preloaded_plugins.push_back(gst_plugin_load_by_name("srtp"));
	if (!preloaded_plugins.back()) {
		Debug::Log("Failed to load 'srtp' plugin", Level::Error);
	}

	main_context_ = g_main_context_new();
	main_loop_ = g_main_loop_new(main_context_, FALSE);
}

GstAVPipeline::~GstAVPipeline()
{
	gst_clear_object(&_device);
	gst_object_unref(_device);
	g_main_context_unref(main_context_);
	g_main_loop_unref(main_loop_);
	for (auto& plugin : preloaded_plugins) {
		gst_object_unref(plugin);
	}
}


void GstAVPipeline::CreatePipeline(const char* uri, const char* remote_peer_id)
{
	Debug::Log("GstAVPipeline create pipeline", Level::Info);
	Debug::Log(uri, Level::Info);
	Debug::Log(remote_peer_id, Level::Info);

	_pipeline = gst_pipeline_new("Plugin AV Pipeline");

	GstElement* webrtcsrc = gst_element_factory_make("webrtcsrc", nullptr);
	GstElement* autoaudiosrc = gst_element_factory_make("wasapi2src", nullptr);
	GstElement* audioconvert = gst_element_factory_make("audioconvert", nullptr);
	GstElement* queue = gst_element_factory_make("queue", nullptr);
	GstElement* opusenc = gst_element_factory_make("opusenc", nullptr);
	GstElement* audio_caps_capsfilter = gst_element_factory_make("capsfilter", nullptr);
	GstElement* webrtcsink = gst_element_factory_make("webrtcsink", nullptr);
	if (!_pipeline || !webrtcsrc || !autoaudiosrc || !audioconvert || !queue || !opusenc || !audio_caps_capsfilter || !webrtcsink) {
		Debug::Log("Failed to create pipe elements", Level::Error);
		gst_object_unref(_pipeline);
		_pipeline = nullptr;
		return;
	}

	GObject* signaller;
	g_object_get(webrtcsrc, "signaller", &signaller, nullptr);
	if (signaller) {
		g_object_set(signaller, "producer-peer-id", remote_peer_id, "uri", uri, nullptr);
		g_signal_connect(G_OBJECT(signaller), "webrtcbin-ready", G_CALLBACK(webrtcbin_ready), this);
		g_object_unref(signaller); // Unref signaller when done
	}
	else {
		Debug::Log("Failed to get signaller property from webrtcsrc.", Level::Error);
	}

	g_signal_connect(G_OBJECT(webrtcsrc), "pad-added", G_CALLBACK(on_pad_added), this);


	g_object_get(webrtcsink, "signaller", &signaller, nullptr);
	if (signaller) {
		g_object_set(signaller, "uri", uri, nullptr);
		g_object_unref(signaller); // Unref signaller when done
	}
	else {
		Debug::Log("Failed to get signaller property from webrtcsink.", Level::Error);
	}

	GstStructure* meta_structure = gst_structure_new_empty("meta");
	gst_structure_set(meta_structure, "name", G_TYPE_STRING, "UnityClient", nullptr);
	GValue meta_value = G_VALUE_INIT;
	g_value_init(&meta_value, GST_TYPE_STRUCTURE);
	gst_value_set_structure(&meta_value, meta_structure);
	g_object_set_property(G_OBJECT(webrtcsink), "meta", &meta_value);
	gst_structure_free(meta_structure);
	g_value_unset(&meta_value);

	//g_object_set(opusenc, "audio-type", "restricted-lowdelay",/* "frame-size", 10,*/ nullptr);
	g_object_set(autoaudiosrc, "low-latency", true, "provide-clock", false, nullptr);
	GstCaps* audio_caps = gst_caps_from_string("audio/x-opus");
	gst_caps_set_simple(audio_caps, "channels", G_TYPE_INT, 1, "rate", G_TYPE_INT, 48000, nullptr);

	g_object_set(audio_caps_capsfilter, "caps", audio_caps, nullptr);

	gst_bin_add_many(GST_BIN(_pipeline), webrtcsrc, autoaudiosrc, audioconvert, queue, opusenc, audio_caps_capsfilter, webrtcsink, nullptr);

	if (!gst_element_link_many(autoaudiosrc, audioconvert, queue, opusenc, audio_caps_capsfilter, webrtcsink, nullptr)) {
		Debug::Log("Audio sending elements could not be linked.", Level::Error);
	}



	//gst_pipeline_set_latency((GstPipeline*)_pipeline, 20000000);

	/*auto state = gst_element_set_state(_pipeline, GstState::GST_STATE_PLAYING);
	if (state == GstStateChangeReturn::GST_STATE_CHANGE_FAILURE) {
		Debug::Log("Cannot set pipeline to playing state", Level::Error);
		gst_object_unref(_pipeline);
		_pipeline = nullptr;
		return;
	}*/


	thread_ = g_thread_new("bus thread", main_loop_func, this);
	if (!thread_) {
		Debug::Log("Failed to create GLib main thread", Level::Error);
	}

	/*gst_caps_unref(audio_caps);
	gst_object_unref(webrtcsrc);
	gst_object_unref(webrtcsink);
	gst_object_unref(audio_caps_capsfilter);
	gst_object_unref(queue);
	gst_object_unref(audioconvert);
	gst_object_unref(autoaudiosrc);
	gst_object_unref(opusenc);*/
}

void GstAVPipeline::CreateDevice()
{
	if (_device == nullptr)
	{
		/* Find adapter LUID of render device, then create our device with the same
	* adapter */
		ComPtr<IDXGIDevice> dxgi_device;
		auto hr = _s_UnityInterfaces->Get<IUnityGraphicsD3D11>()->GetDevice()->QueryInterface(IID_PPV_ARGS(&dxgi_device));
		g_assert(SUCCEEDED(hr));

		ComPtr<IDXGIAdapter> adapter;
		hr = dxgi_device->GetAdapter(&adapter);
		g_assert(SUCCEEDED(hr));

		DXGI_ADAPTER_DESC adapter_desc;
		hr = adapter->GetDesc(&adapter_desc);
		g_assert(SUCCEEDED(hr));

		auto luid = gst_d3d11_luid_to_int64(&adapter_desc.AdapterLuid);

		/* This device will be used by our pipeline */
		_device = gst_d3d11_device_new_for_adapter_luid(luid,
			D3D11_CREATE_DEVICE_BGRA_SUPPORT);
		g_assert(_device);

		/*if (!find_decoder(luid, decoder_factory)) {
			gst_println("GPU does not support H.264 decoding");
			decoder_factory = "avdec_h264";
		}*/

		//ComPtr <ID3D10Multithread> multi_thread;
		/*hr = _s_UnityInterfaces->Get<IUnityGraphicsD3D11>()->GetDevice()->QueryInterface(IID_PPV_ARGS(&multi_thread));
		g_assert(SUCCEEDED(hr));

		multi_thread->SetMultithreadProtected(TRUE);
		multi_thread->Release();*/
	}
	else
	{
		Debug::Log("Device already created", Level::Warning);
	}
}

void GstAVPipeline::DestroyPipeline()
{
	if (main_loop_ != nullptr)
	{
		g_main_loop_quit(main_loop_);
		//g_main_loop_unref(main_loop_);
		//main_loop_ = nullptr;
	}

	if (thread_ != nullptr) {
		g_thread_join(thread_);
		thread_ = nullptr;
	}

	/*if (_pipeline != nullptr) {
		Debug::Log("GstAVPipeline pipeline released", Level::Info);
		gst_element_set_state(_pipeline, GstState::GST_STATE_NULL);
		gst_object_unref(_pipeline);
		_pipeline = nullptr;
	}
	else
	{
		Debug::Log("GstAVPipeline pipeline already released", Level::Warning);
	}*/

	if (_leftData != nullptr)
	{
		gst_clear_sample(&_leftData->last_sample);
		gst_clear_caps(&_leftData->last_caps);
		gst_clear_buffer(&_leftData->shared_buffer);
		gst_clear_object(&_leftData->conv);
		_leftData.reset(nullptr);
	}
	if (_rightData != nullptr)
	{
		gst_clear_sample(&_rightData->last_sample);
		gst_clear_caps(&_rightData->last_caps);
		gst_clear_buffer(&_rightData->shared_buffer);
		gst_clear_object(&_rightData->conv);
		_rightData.reset(nullptr);
	}


	//g_main_context_unref(main_context_);
}


ID3D11Texture2D* GstAVPipeline::GetTexturePtr(bool left) {
	if (left)
		return _leftData->texture.Get();
	else
		return _rightData->texture.Get();
}


bool GstAVPipeline::find_decoder(gint64 luid, std::string& feature_name)
{
	GList* features;
	GList* iter;

	/* Load features of d3d11 plugin */
	features = gst_registry_get_feature_list_by_plugin(gst_registry_get(),
		"d3d11");

	if (!features)
		return false;

	for (iter = features; iter; iter = g_list_next(iter)) {
		GstPluginFeature* f = GST_PLUGIN_FEATURE(iter->data);
		GstElementFactory* factory;
		const gchar* name;
		GstElement* element;
		gint64 adapter_luid;

		if (!GST_IS_ELEMENT_FACTORY(f))
			continue;

		factory = GST_ELEMENT_FACTORY(f);
		if (!gst_element_factory_list_is_type(factory,
			GST_ELEMENT_FACTORY_TYPE_DECODER))
			continue;

		name = gst_plugin_feature_get_name(f);
		if (!g_strrstr(name, "h264"))
			continue;

		element = gst_element_factory_create(factory, nullptr);
		/* unexpected */
		if (!element)
			continue;

		/* query adapter-luid associated with this decoder */
		g_object_get(element, "adapter-luid", &adapter_luid, nullptr);
		gst_object_unref(element);

		/* element object can be directly used in pipeline, but this example
		 * demonstrates a way of plugin enumeration */
		if (adapter_luid == luid) {
			feature_name = name;
			break;
		}
	}

	gst_plugin_feature_list_free(features);

	if (feature_name.empty())
		return false;

	return true;
}

gpointer GstAVPipeline::main_loop_func(gpointer data)
{
	Debug::Log("Entering main loop");
	GstAVPipeline* self = static_cast<GstAVPipeline*>(data);
	g_main_context_push_thread_default(self->main_context_);
	
	GstBus* bus = gst_element_get_bus(self->_pipeline);
	//gst_bus_add_watch(bus, busHandler, self);
	//gst_bus_set_sync_handler(bus, busSyncHandler, self, nullptr);
	//gst_bus_add_signal_watch(bus);
	g_signal_connect(bus, "message", G_CALLBACK(on_bus_message), self);

	auto state = gst_element_set_state(self->_pipeline, GstState::GST_STATE_PLAYING);
	if (state == GstStateChangeReturn::GST_STATE_CHANGE_FAILURE) {
		Debug::Log("Cannot set pipeline to playing state", Level::Error);
		gst_object_unref(self->_pipeline);
		self->_pipeline = nullptr;		
		return nullptr;
	}

	//self->main_loop_ = g_main_loop_new(nullptr, FALSE);
	g_main_loop_run(self->main_loop_);

	gst_element_set_state(self->_pipeline, GST_STATE_NULL);

	//gst_bus_set_sync_handler(bus, nullptr, nullptr, nullptr);
	//gst_bus_remove_watch(bus);
	gst_object_unref(bus);
	g_main_context_pop_thread_default(self->main_context_);
	Debug::Log("Quitting main loop");

	return nullptr;
}

void GstAVPipeline::on_bus_message(GstBus* bus, GstMessage* msg, gpointer user_data) {
	auto self = (GstAVPipeline*)user_data;
	switch (GST_MESSAGE_TYPE(msg)) {
	case GST_MESSAGE_LATENCY:
		Debug::Log("Redistribute latency...");
		gst_bin_recalculate_latency(GST_BIN(self));
		break;
	default:
		break;
	}
}

gboolean GstAVPipeline::busHandler(GstBus* bus, GstMessage* msg, gpointer data)
{
	auto self = (GstAVPipeline*)data;

	switch (GST_MESSAGE_TYPE(msg)) {
	case GST_MESSAGE_ERROR:
	{
		GError* err;
		gchar* dbg;

		gst_message_parse_error(msg, &err, &dbg);
		//gst_printerrln("ERROR %s", err->message);
		Debug::Log(err->message, Level::Error);
		if (dbg != nullptr)
			Debug::Log(dbg);
			//gst_printerrln("ERROR debug information: %s", dbg);
		g_clear_error(&err);
		g_free(dbg);
		g_main_loop_quit(self->main_loop_);
		break;
	}
	case GST_MESSAGE_EOS:
		Debug::Log("Got EOS");
		g_main_loop_quit(self->main_loop_);
		break;
	case GST_MESSAGE_LATENCY:
	{
		Debug::Log("Redistribute latency...");
		GstElement* pip = self->_pipeline;
		gst_bin_recalculate_latency(GST_BIN(pip));
		break;
	}
	default:
		Debug::Log(GST_MESSAGE_TYPE_NAME(msg));
		break;
	}

	return G_SOURCE_CONTINUE;
}

GstBusSyncReply GstAVPipeline::busSyncHandler(GstBus* bus, GstMessage* msg, gpointer user_data)
{
	auto self = (GstAVPipeline*)user_data;

	switch (GST_MESSAGE_TYPE(msg)) {
	case GST_MESSAGE_NEED_CONTEXT:
	{
		const gchar* ctx_type;
		if (!gst_message_parse_context_type(msg, &ctx_type))
			break;

		/* non-d3d11 context message is not interested */
		if (g_strcmp0(ctx_type, GST_D3D11_DEVICE_HANDLE_CONTEXT_TYPE) != 0)
			break;

		/* Pass our device to the message source element.
		 * Otherwise pipeline will create another device */
		auto context = gst_d3d11_context_new(self->_device);
		gst_element_set_context(GST_ELEMENT(msg->src), context);
		gst_context_unref(context);
		break;
	}
	default:
		break;
	}
	return GST_BUS_PASS;
}