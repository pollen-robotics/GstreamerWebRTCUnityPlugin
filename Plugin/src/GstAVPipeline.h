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
	GstBus* _bus = nullptr;
	unsigned int _busWatchId = 0;
	IUnityInterfaces* _s_UnityInterfaces = nullptr;
	GThread* pipelineLoopThread;	
	GstD3D11Device* device_ = nullptr;
	GstVideoInfo render_info_;	
	struct AppData
	{
		GstAVPipeline* avpipeline = nullptr;
		bool left;
		GstCaps* last_caps_ = nullptr;
		GstD3D11Converter* conv_ = nullptr;
		GstSample* last_sample_ = nullptr;
		Microsoft::WRL::ComPtr < ID3D11Texture2D> texture = nullptr;
		Microsoft::WRL::ComPtr<IDXGIKeyedMutex> keyed_mutex_ = nullptr;
		GstBuffer* shared_buffer_ = nullptr;
		std::mutex lock_;
	};

	std::unique_ptr<AppData> _leftData = nullptr;
	std::unique_ptr<AppData> _rightData = nullptr;

public:
	GstAVPipeline(IUnityInterfaces* s_UnityInterfaces);
	~GstAVPipeline();

	ID3D11Texture2D* GetTexturePtr(bool left = true);
	GstElement* GetPipeline();
	void draw(bool left);

	void CreatePipeline(const char* uri, const char* remote_peer_id);
	void DestroyPipeline();

	bool CreateTexture(unsigned int width, unsigned int height, bool left = true);
	void ReleaseTexture(ID3D11Texture2D* texture);

	static void on_pad_added(GstElement* src, GstPad* new_pad, gpointer data);

	static GstFlowReturn GstAVPipeline::on_new_sample(GstAppSink* appsink, gpointer user_data);

	GMainLoop* mainLoop;
};