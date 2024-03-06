#include "GstAVPipeline.h"
#include "DebugLog.h"

#include <d3d11sdklayers.h>
#include <dxgi1_2.h>
#include <dxgiformat.h>
#include <d3d11_1.h>

#include "Unity/IUnityGraphicsD3D11.h"



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

	// Create a texture 2D that can be shared
	D3D11_TEXTURE2D_DESC desc = {};
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

	// Create the texture using the device and description
	if(left)
		hr = device->CreateTexture2D(&desc, nullptr, &_texture_left);
	else
		hr = device->CreateTexture2D(&desc, nullptr, &_texture_right);
	if (FAILED(hr)) {
		Debug::Log("Could not create texture.");
		return false;
	}

	// Cast the pointer into IDXGIRes. that can be shared.
	IDXGIResource* pDXGITexture = nullptr;
	if (left)
		hr = _texture_left->QueryInterface(__uuidof(IDXGIResource), (void**)&pDXGITexture);
	else
		hr = _texture_right->QueryInterface(__uuidof(IDXGIResource), (void**)&pDXGITexture);

	if (FAILED(hr)) {
		Debug::Log("Couldnt DXGI res");
		return false;
	}

	// Create a shared handle (so the GStreamer thread can write to this)
	HANDLE sharedHandle = nullptr;
	hr = pDXGITexture->GetSharedHandle(&sharedHandle);
	if (FAILED(hr)) {
		Debug::Log("Couldnt create shared handle");
		pDXGITexture->Release();
		return false;
	}
	if (left)
	{
		_sharedHandle_left = sharedHandle;
		Debug::Log("Created left texture for pipeline:");
	}
	else
	{
		_sharedHandle_right = sharedHandle;
		Debug::Log("Created right texture for pipeline:");
	}


	return true;
}



// Callback from GStreamer when it's ready to draw on our shared ID3D11Texture.
GstFlowReturn GstAVPipeline::OnBeginDraw(GstElement* videosink, gpointer data)
{
	//GstAVPipeline* pipeline = static_cast<GstAVPipeline*>(data);
	//HANDLE sharedHandle = pipeline->GetSharedHandle();
	HANDLE sharedHandle = static_cast<HANDLE>(data);

	if (sharedHandle == nullptr) {
		return GST_FLOW_OK;
	}

	// GStreamer draws on our texture (assumed same size)
	g_signal_emit_by_name(videosink, "draw", (gpointer)sharedHandle, D3D11_RESOURCE_MISC_SHARED, 0, 0, nullptr);

	return GST_FLOW_OK;
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
		GstElement* d3d11convert = gst_element_factory_make("d3d11convert", NULL);
		GstElement* d3d11videosink = gst_element_factory_make("d3d11videosink", NULL);

		if (!avpipeline || !src || !rtph264depay || !h264parse || !d3d11h264dec || !d3d11convert  || !d3d11videosink) {
			Debug::Log("Failed to create all elements");
		}

		g_object_set(d3d11videosink, "draw-on-shared-texture", true, NULL);

		gst_bin_add_many(GST_BIN(avpipeline->GetPipeline()), rtph264depay, h264parse, d3d11h264dec, d3d11convert, d3d11videosink, NULL);

		
		if (g_str_has_prefix(pad_name, "video_0"))
		{
			Debug::Log("Connecting left video pad " + std::string(pad_name));
			g_signal_connect(d3d11videosink, "begin_draw", G_CALLBACK(OnBeginDraw), avpipeline->GetSharedHandle(true));
		}
		else
		{
			Debug::Log("Connecting right video pad " + std::string(pad_name));
			g_signal_connect(d3d11videosink, "begin_draw", G_CALLBACK(OnBeginDraw), avpipeline->GetSharedHandle(false));
			
		}

		if (!gst_element_link_many(rtph264depay, h264parse, d3d11h264dec, d3d11convert, d3d11videosink, NULL)) {
			Debug::Log("Elements could not be linked. Exiting.");
		}

		GstPad* sinkpad = gst_element_get_static_pad(rtph264depay, "sink");
		gst_pad_link(new_pad, sinkpad);
		gst_object_unref(sinkpad);

		gst_element_sync_state_with_parent(rtph264depay);
		gst_element_sync_state_with_parent(h264parse);
		gst_element_sync_state_with_parent(d3d11h264dec);
		gst_element_sync_state_with_parent(d3d11convert);
		gst_element_sync_state_with_parent(d3d11videosink);
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
	}
	g_free(pad_name);
}

// Call only on plugin thread
void GstAVPipeline::ReleaseTexture(ID3D11Texture2D* texture) {
	if (texture != nullptr)
	{
		texture->Release();
		texture = nullptr;
	}
}

/*void GstAVPipeline::SetTextureFromUnity(void* texture, unsigned int width, unsigned int height)
{
	Debug::Log("Set texture for pipeline");
	_texture = (ID3D11Texture2D *) texture;
	textureWidth = width;
	textureHeight = height;
}*/



GstAVPipeline::GstAVPipeline(IUnityInterfaces* s_UnityInterfaces) : _s_UnityInterfaces(s_UnityInterfaces)
{
}

GstAVPipeline::~GstAVPipeline()
{
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

	//GstElement* videotestsrc = gst_element_factory_make("videotestsrc", NULL);
	GstElement* webrtcsrc = gst_element_factory_make("webrtcsrc", NULL);
	//d3d11videosink = gst_element_factory_make("d3d11videosink", NULL);
	if (!_pipeline || !webrtcsrc /* || !d3d11videosink*/) {
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

	//g_object_set(G_OBJECT(d3d11videosink), "draw-on-shared-texture", true, NULL);
	/*g_signal_connect(G_OBJECT(d3d11videosink), "begin_draw", G_CALLBACK(OnBeginDraw), this);*/

	gst_bin_add_many(GST_BIN(_pipeline), webrtcsrc/*, d3d11videosink*/, NULL);
	/*if (!gst_element_link_many(webrtcsrc, d3d11videosink, NULL)) {
		Debug::Log("Elements could not be linked.", Level::Error);
		gst_object_unref(pipeline);
		pipeline = nullptr;
		return;
	}*/

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

HANDLE GstAVPipeline::GetSharedHandle(bool left) {
	if (left)
		return _sharedHandle_left;
	else
		return _sharedHandle_right;
}


ID3D11Texture2D* GstAVPipeline::GetTexturePtr(bool left) {
	if (left)
		return _texture_left;
	else
		return _texture_right;
}

GstElement* GstAVPipeline::GetPipeline()
{
	return _pipeline;
}

