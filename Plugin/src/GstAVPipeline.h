#pragma once
#include <gst/gst.h>
#include <d3d11.h>
#include "Unity/IUnityInterface.h"
#include <mutex>
#include <gst/d3d11/gstd3d11.h>
#include <wrl.h>
#include <gst/app/app.h>

class GstAVPipeline {

private:
	GstElement* _pipeline = nullptr;
	GstD3D11Device* _device = nullptr;

	IUnityInterfaces* _s_UnityInterfaces = nullptr;
	GstVideoInfo _render_info;	
	int nb_run = 0;
	struct AppData
	{
		GstAVPipeline* avpipeline = nullptr;
		GstCaps* last_caps = nullptr;
		std::mutex lock;
		GstSample* last_sample = nullptr;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> texture = nullptr;
		Microsoft::WRL::ComPtr<IDXGIKeyedMutex> keyed_mutex = nullptr;
		GstBuffer* shared_buffer = nullptr;
		GstD3D11Converter* conv = nullptr;
	};

	std::unique_ptr<AppData> _leftData = nullptr;
	std::unique_ptr<AppData> _rightData = nullptr;

public:
	GstAVPipeline(IUnityInterfaces* s_UnityInterfaces);
	~GstAVPipeline();

	ID3D11Texture2D* GetTexturePtr(bool left = true);
	void Draw(bool left);
	void EndDraw(bool left);

	void CreatePipeline(const char* uri, const char* remote_peer_id);
	void DestroyPipeline();

	bool CreateTexture(unsigned int width, unsigned int height, bool left = true);
	void ReleaseTexture(ID3D11Texture2D* texture);

	static void on_pad_added(GstElement* src, GstPad* new_pad, gpointer data);

	static GstFlowReturn GstAVPipeline::on_new_sample(GstAppSink* appsink, gpointer user_data);

	static bool find_decoder(gint64 luid, std::string& feature_name);
};