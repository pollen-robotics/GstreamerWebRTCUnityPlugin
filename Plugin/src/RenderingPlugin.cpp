// Example low level rendering Unity plugin


#include <assert.h>
#include <d3d11.h>

#include "Unity/IUnityGraphics.h"
#include "Unity/IUnityGraphicsD3D11.h"


#include "GstAVPipeline.h"
#include "DebugLog.h"
#include "Unity/IUnityRenderingExtensions.h"


static std::unique_ptr<GstAVPipeline> gstAVPipeline = nullptr;

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API CreatePipeline(const char* uri, const char* remote_peer_id)
{
	gstAVPipeline->CreatePipeline(uri, remote_peer_id);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API CreateTexture(unsigned int width, unsigned int height, bool left)
{
	gstAVPipeline->CreateTexture(width, height, left);
}

extern "C" UNITY_INTERFACE_EXPORT void * UNITY_INTERFACE_API GetTexturePtr(bool left)
{
	return gstAVPipeline->GetTexturePtr(left);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API ReleaseTexture(void* texPtr)
{
	gstAVPipeline->ReleaseTexture((ID3D11Texture2D*)texPtr);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API DestroyPipeline()
{
	gstAVPipeline->DestroyPipeline();
}

// --------------------------------------------------------------------------
// UnitySetInterfaces

static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType);

static IUnityInterfaces* s_UnityInterfaces = nullptr;
static IUnityGraphics* s_Graphics = nullptr;

extern "C" void	UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces* unityInterfaces)
{
	s_UnityInterfaces = unityInterfaces;
	s_Graphics = s_UnityInterfaces->Get<IUnityGraphics>();
	s_Graphics->RegisterDeviceEventCallback(OnGraphicsDeviceEvent);
	
	// Run OnGraphicsDeviceEvent(initialize) manually on plugin load
	OnGraphicsDeviceEvent(kUnityGfxDeviceEventInitialize);

	SetEnvironmentVariable("GST_DEBUG_FILE", "Logs\\gstreamer.log");
	gst_debug_set_default_threshold(GST_LEVEL_INFO);
	gst_init(nullptr, nullptr);
	gstAVPipeline = std::make_unique<GstAVPipeline>(s_UnityInterfaces);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginUnload()
{
	gstAVPipeline.reset(nullptr);
	gst_deinit();
	s_Graphics->UnregisterDeviceEventCallback(OnGraphicsDeviceEvent);
}


// --------------------------------------------------------------------------
// GraphicsDeviceEvent


//static RenderAPI* s_CurrentAPI = nullptr;
//static UnityGfxRenderer s_DeviceType = kUnityGfxRendererNull;


static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType)
{
	// Create graphics API implementation upon initialization
	/*if (eventType == kUnityGfxDeviceEventInitialize)
	{
		assert(s_CurrentAPI == nullptr);
		s_DeviceType = s_Graphics->GetRenderer();
		s_CurrentAPI = CreateRenderAPI(s_DeviceType);
	}

	// Let the implementation process the device related events
	if (s_CurrentAPI)
	{
		s_CurrentAPI->ProcessDeviceEvent(eventType, s_UnityInterfaces);
	}

	// Cleanup graphics API implementation upon shutdown
	if (eventType == kUnityGfxDeviceEventShutdown)
	{
		delete s_CurrentAPI;
		s_CurrentAPI = nullptr;
		s_DeviceType = kUnityGfxRendererNull;
	}*/
}



// --------------------------------------------------------------------------
// OnRenderEvent
// This will be called for GL.IssuePluginEvent script calls; eventID will
// be the integer passed to IssuePluginEvent. In this example, we just ignore
// that value.


static void UNITY_INTERFACE_API OnRenderEvent(int eventID)
{
	// Unknown / unsupported graphics device type? Do nothing
	//if (s_CurrentAPI == nullptr)
	//	return;

	if (eventID == 1)
	{
		// true is left texture, false is right
		//gstAVPipeline->Draw(true);
		//gstAVPipeline->Draw(false);
	}

}

// --------------------------------------------------------------------------
// GetRenderEventFunc, an example function we export which is used to get a rendering event callback function.

extern "C" UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetRenderEventFunc()
{
	return OnRenderEvent;
}

// Callback for texture update events
void TextureUpdateCallback(int eventID, void* data)
{
	if (eventID == kUnityRenderingExtEventUpdateTextureBeginV2)
	{
		UnityRenderingExtTextureUpdateParamsV2* params = (UnityRenderingExtTextureUpdateParamsV2 *) data;
		unsigned int frame = params->userData;
		if(frame == 0)
			gstAVPipeline->Draw(true);
		else
			gstAVPipeline->Draw(false);
	}
	/*else if (eventID == kUnityRenderingExtEventUpdateTextureEndV2)
	{
		// UpdateTextureEnd: Free up the temporary memory.
		UnityRenderingExtTextureUpdateParamsV2* params = (UnityRenderingExtTextureUpdateParamsV2*)data;
		//free(params->texData);
		unsigned int frame = params->userData;
		if (frame == 0)
			gstAVPipeline->EndDraw(true);
		else
			gstAVPipeline->EndDraw(false);
	}*/
}

extern "C" UnityRenderingEventAndData UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetTextureUpdateCallback()
{
	return TextureUpdateCallback;
}

/*
extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API
UnityRenderingExtEvent(UnityRenderingExtEventType event, void* data)
{
	switch (event)
	{
	case kUnityRenderingExtEventBeforeDrawCall:
		// do some stuff
		break;
	case kUnityRenderingExtEventAfterDrawCall:
		// undo some stuff
		break;
	case kUnityRenderingExtEventUpdateTextureBeginV2:
		gstAVPipeline->Draw(true);
		gstAVPipeline->Draw(false);
		break;
	}
}*/