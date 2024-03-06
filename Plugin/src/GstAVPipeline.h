#pragma once
#include <gst/gst.h>
#include <d3d11.h>
#include "Unity/IUnityInterface.h"
#include <mutex>



class GstAVPipeline {

private:
	GstElement* _pipeline = nullptr;
	GstBus* _bus = nullptr;
	unsigned int _busWatchId = 0;
	ID3D11Texture2D* _texture_left = nullptr;
	ID3D11Texture2D* _texture_right = nullptr;
	unsigned int _textureWidth = 0;
	unsigned int _textureHeight = 0;
	HANDLE _sharedHandle_left = nullptr;
	HANDLE _sharedHandle_right = nullptr;
	IUnityInterfaces* _s_UnityInterfaces = nullptr;
	GThread* pipelineLoopThread;	

public:
	GstAVPipeline(IUnityInterfaces* s_UnityInterfaces);
	~GstAVPipeline();

	HANDLE GetSharedHandle(bool left = true);
	ID3D11Texture2D* GetTexturePtr(bool left = true);
	GstElement* GetPipeline();

	void CreatePipeline(const char* uri, const char* remote_peer_id);
	void DestroyPipeline();

	bool CreateTexture(unsigned int width, unsigned int height, bool left = true);	
	void ReleaseTexture(ID3D11Texture2D* texture);

	static void on_pad_added(GstElement* src, GstPad* new_pad, gpointer data);
	static GstFlowReturn OnBeginDraw(GstElement* videosink, gpointer data);

	GMainLoop* mainLoop;
};