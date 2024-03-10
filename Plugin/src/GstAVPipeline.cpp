#include "GstAVPipeline.h"
#include "DebugLog.h"

#include <d3d11sdklayers.h>
#include <dxgi1_2.h>
#include <dxgiformat.h>
#include <d3d11_1.h>


#include "Unity/IUnityGraphicsD3D11.h"
using namespace Microsoft::WRL;


static gboolean bus_call(GstBus* bus, GstMessage* msg, gpointer data)
{
	switch (GST_MESSAGE_TYPE(msg)) {

	case GST_MESSAGE_EOS:
		Debug::Log("End of stream");
		return false;

	case GST_MESSAGE_ERROR: {
		gchar* debug;
		GError* error;

		gst_message_parse_error(msg, &error, &debug);
		g_free(debug);

		Debug::Log(error->message, Level::Error);
		g_error_free(error);
		return false;
	}
	default:
		//Debug::Log("Received message of type:");
		Debug::Log(GST_MESSAGE_TYPE_NAME(msg));
		break;
	}

	return true;
}

gpointer GstMainLoopFunction(gpointer data)
{
	Debug::Log("Entering main loop");
	GstAVPipeline* pipeline = static_cast<GstAVPipeline*>(data);
	pipeline->mainLoop = g_main_loop_new(nullptr, FALSE);
	g_main_loop_run(pipeline->mainLoop);
	Debug::Log("Quitting main loop");

	return NULL;
}

// Call only on plugin thread
// Creates the underlying D3D11 texture using the provided unity device.
// This texture can then be turned into a proper Unity texture on the
// managed side using Texture2D.CreateExternalTexture()
bool GstAVPipeline::CreateTexture(unsigned int width, unsigned int height, bool left)
{
	auto device = _s_UnityInterfaces->Get<IUnityGraphicsD3D11>()->GetDevice();
	HRESULT hr = S_OK;

	gst_video_info_set_format(&render_info_, GST_VIDEO_FORMAT_RGBA, width, height);

	std::unique_ptr<AppData> data = std::make_unique<AppData>();
	data->avpipeline = this;
	data->left = left;

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

	hr = device->CreateTexture2D(&desc, nullptr, &data->texture);
	g_assert(SUCCEEDED(hr));

	hr = data->texture.As(&data->keyed_mutex_);
	g_assert(SUCCEEDED(hr));

	hr = data->keyed_mutex_->AcquireSync(0, INFINITE);
	g_assert(SUCCEEDED(hr));


	ComPtr<IDXGIResource1> dxgi_resource;
	hr = data->texture.As(&dxgi_resource);
	g_assert(SUCCEEDED(hr));

	HANDLE shared_handle = nullptr;
	hr = dxgi_resource->CreateSharedHandle(nullptr,
		DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, nullptr,
		&shared_handle);
	g_assert(SUCCEEDED(hr));

	auto gst_device = gst_d3d11_device_get_device_handle(device_);
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
	GstMemory* mem = gst_d3d11_allocator_alloc_wrapped(nullptr, device_,
		gst_texture.Get(),
		/* CPU accessible (staging texture) memory size is unknown.
		* Pass zero here, then GStreamer will calculate it */
		0, nullptr, nullptr);
	g_assert(mem);

	data->shared_buffer_ = gst_buffer_new();
	gst_buffer_append_memory(data->shared_buffer_, mem);

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

	std::lock_guard<std::mutex> lk(data->lock_);
	/* Caps updated, recreate converter */
	if (data->last_caps_ && !gst_caps_is_equal(data->last_caps_, caps))
		gst_clear_object(&data->conv_);

	if (!data->conv_) {
		GstVideoInfo in_info;
		gst_video_info_from_caps(&in_info, caps);

		/* In case of shared texture, video processor might not behave as expected.
		 * Use only pixel shader */
		auto config = gst_structure_new("converter-config",
			GST_D3D11_CONVERTER_OPT_BACKEND, GST_TYPE_D3D11_CONVERTER_BACKEND,
			GST_D3D11_CONVERTER_BACKEND_SHADER, nullptr);

		data->conv_ = gst_d3d11_converter_new(data->avpipeline->device_, &in_info,
			&data->avpipeline->render_info_, config);
	}

	gst_caps_replace(&data->last_caps_, caps);
	gst_clear_sample(&data->last_sample_);
	data->last_sample_ = sample;

	return GST_FLOW_OK;
}

void GstAVPipeline::draw(bool left)
{
	Debug::Log("drawing");
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
	{
		std::lock_guard <std::mutex> lk(data->lock_);
		/* If there's no updated sample, don't need to render again */
		if (!data->last_sample_)
			return;

		sample = data->last_sample_;
		data->last_sample_ = nullptr;
	}

	auto buf = gst_sample_get_buffer(sample);
	if (!buf) {
		Debug::Log("Sample without buffer", Level::Error);
		gst_sample_unref(sample);
		return;
	}

	data->keyed_mutex_->ReleaseSync(0);
	/* Converter will take gst_d3d11_device_lock() and acquire sync */
	gst_d3d11_converter_convert_buffer(data->conv_, buf, data->shared_buffer_);

	data->keyed_mutex_->AcquireSync(0, INFINITE);
}

void GstAVPipeline::on_pad_added(GstElement* src, GstPad* new_pad, gpointer data) {
	GstAVPipeline* avpipeline = static_cast<GstAVPipeline*>(data);
	gchar* pad_name = gst_pad_get_name(new_pad);
	Debug::Log("Adding pad ");
	if (g_str_has_prefix(pad_name, "video")) {
		Debug::Log("Adding video pad " + std::string(pad_name));
		GstElement* rtph264depay = gst_element_factory_make("rtph264depay", NULL);
		GstElement* h264parse = gst_element_factory_make("h264parse", NULL);
		GstElement* d3d11h264dec = gst_element_factory_make("d3d11h264dec", NULL);
		GstElement* d3d11upload = gst_element_factory_make("d3d11upload", NULL);
		GstElement* appsink = gst_element_factory_make("appsink", NULL);

		if (!avpipeline || !src || !rtph264depay || !h264parse || !d3d11h264dec || !d3d11upload  || !appsink) {
			Debug::Log("Failed to create all elements");
		}

		//g_object_set(d3d11videosink, "draw-on-shared-texture", true, NULL);
		GstCaps* caps = gst_caps_from_string("video/x-raw(memory:D3D11Memory)");
		g_object_set(appsink, "caps", caps, nullptr);
		gst_caps_unref(caps);

		gst_bin_add_many(GST_BIN(avpipeline->GetPipeline()), rtph264depay, h264parse, d3d11h264dec, d3d11upload, appsink, NULL);

		GstAppSinkCallbacks callbacks = { nullptr };
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

		if (!gst_element_link_many(rtph264depay, h264parse, d3d11h264dec, d3d11upload, appsink, NULL)) {
			Debug::Log("Elements could not be linked.");
		}

		GstPad* sinkpad = gst_element_get_static_pad(rtph264depay, "sink");
		gst_pad_link(new_pad, sinkpad);
		gst_object_unref(sinkpad);

		gst_element_sync_state_with_parent(rtph264depay);
		gst_element_sync_state_with_parent(h264parse);
		gst_element_sync_state_with_parent(d3d11h264dec);
		gst_element_sync_state_with_parent(d3d11upload);
		gst_element_sync_state_with_parent(appsink);

		gst_object_unref(rtph264depay);
		gst_object_unref(h264parse);
		gst_object_unref(d3d11h264dec);
		gst_object_unref(d3d11upload);
		gst_object_unref(appsink);
	}
	else if (g_str_has_prefix(pad_name, "audio")) {
		Debug::Log("Adding audio pad " + std::string(pad_name));
		GstElement* rtpopusdepay = gst_element_factory_make("rtpopusdepay", NULL);
		GstElement* opusdec = gst_element_factory_make("opusdec", NULL);
		GstElement* audioconvert = gst_element_factory_make("audioconvert", NULL);
		GstElement* audioresample = gst_element_factory_make("audioresample", NULL);
		GstElement* autoaudiosink = gst_element_factory_make("autoaudiosink", NULL);

		if (!rtpopusdepay || !opusdec || !audioconvert || !audioresample || !autoaudiosink) {
			Debug::Log("Failed to create audio elements");
		}
		else {
			gst_bin_add_many(GST_BIN(avpipeline->GetPipeline()), rtpopusdepay, opusdec, audioconvert, audioresample, autoaudiosink, NULL);
			if (!gst_element_link_many(rtpopusdepay, opusdec, audioconvert, audioresample, autoaudiosink, NULL)) {
				Debug::Log("Audio elements could not be linked.", Level::Error);
			}
			else {
				GstPad* sinkpad = gst_element_get_static_pad(rtpopusdepay, "sink");
				if (gst_pad_link(new_pad, sinkpad) != GST_PAD_LINK_OK) {
					Debug::Log("Could not link dynamic audio pad to rtpopusdepay", Level::Error);
				}
				gst_object_unref(sinkpad);

				gst_element_sync_state_with_parent(rtpopusdepay);
				gst_element_sync_state_with_parent(opusdec);
				gst_element_sync_state_with_parent(audioconvert);
				gst_element_sync_state_with_parent(audioresample);
				gst_element_sync_state_with_parent(autoaudiosink);
			}
		}
		gst_object_unref(rtpopusdepay);
		gst_object_unref(opusdec);
		gst_object_unref(audioconvert);
		gst_object_unref(audioresample);
		gst_object_unref(autoaudiosink);
	}
	g_free(pad_name);
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
	device_ = gst_d3d11_device_new_for_adapter_luid(luid,
		D3D11_CREATE_DEVICE_BGRA_SUPPORT);
	g_assert(device_);
}

GstAVPipeline::~GstAVPipeline()
{
	if (_leftData != nullptr)
	{
		gst_clear_sample(&_leftData->last_sample_);
		gst_clear_buffer(&_leftData->shared_buffer_);
		_leftData.reset(nullptr);
	}
	if (_rightData != nullptr)
	{
		gst_clear_sample(&_rightData->last_sample_);
		gst_clear_buffer(&_rightData->shared_buffer_);
		_rightData.reset(nullptr);
	}
	gst_clear_object(&_pipeline);
	gst_clear_object(&device_);
}

void GstAVPipeline::CreatePipeline(const char* uri, const char* remote_peer_id)
{
	Debug::Log("GstAVPipeline create pipeline", Level::Info);
	Debug::Log(uri, Level::Info);
	Debug::Log(remote_peer_id, Level::Info);


	_pipeline = gst_pipeline_new("Plugin AV Pipeline");

	_bus = gst_pipeline_get_bus(GST_PIPELINE(_pipeline));
	_busWatchId = gst_bus_add_watch(_bus, bus_call, nullptr);
	gst_object_unref(_bus);

	GstElement* webrtcsrc = gst_element_factory_make("webrtcsrc", NULL);
	if (!_pipeline || !webrtcsrc) {
		Debug::Log("Failed to create all elements", Level::Error);
		gst_object_unref(_pipeline);
		_pipeline = nullptr;
		return;
	}


	GObject* signaller;
	g_object_get(webrtcsrc, "signaller", &signaller, NULL);
	if (signaller) {
		g_object_set(signaller,
			"producer-peer-id", remote_peer_id,
			"uri", uri);
		g_object_unref(signaller); // Unref signaller when done
	}
	else {
		Debug::Log("Failed to get signaller property from webrtcsrc.", Level::Error);
	}

	g_signal_connect(G_OBJECT(webrtcsrc), "pad-added", G_CALLBACK(on_pad_added), this);


	gst_bin_add_many(GST_BIN(_pipeline), webrtcsrc, NULL);


	auto state = gst_element_set_state(_pipeline, GstState::GST_STATE_PLAYING);
	if (state == GstStateChangeReturn::GST_STATE_CHANGE_FAILURE) {
		gst_object_unref(_pipeline);
		_pipeline = nullptr;
		return;
	}

	// Create the loop thread?
	/*pipelineLoopThread = g_thread_new("GstUnityBridge Main Thread", GstMainLoopFunction, this);
	if (!pipelineLoopThread) {
		Debug::Log("Failed to create GLib main thread: ");
		return;
	}*/
}

void GstAVPipeline::DestroyPipeline()
{
	/*if (mainLoop != nullptr) {
		g_main_loop_quit(mainLoop);
		mainLoop = nullptr;
	}
	else {
		return;
	}*/

	if (_pipeline != nullptr) {
		Debug::Log("GstAVPipeline pipeline released", Level::Info);
		gst_element_set_state(_pipeline, GstState::GST_STATE_NULL);
		gst_object_unref(_pipeline);
		_pipeline = nullptr;
	}
	else
	{
		Debug::Log("GstAVPipeline pipeline already released", Level::Warning);
	}
}


ID3D11Texture2D* GstAVPipeline::GetTexturePtr(bool left) {
	if (left)
		return _leftData->texture.Get();
	else
		return _rightData->texture.Get();
}

GstElement* GstAVPipeline::GetPipeline()
{
	return _pipeline;
}

