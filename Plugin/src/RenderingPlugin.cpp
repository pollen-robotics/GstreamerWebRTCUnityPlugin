// Example low level rendering Unity plugin

#include "PlatformBase.h"
#include "RenderAPI.h"

#include <assert.h>

#include "GstAVPipeline.h"
#include "GstDataPipeline.h"

static std::unique_ptr<GstAVPipeline> gstAVPipeline = nullptr;
static std::unique_ptr<GstDataPipeline> gstDataPipeline = nullptr;
static IUnityInterfaces* s_UnityInterfaces = NULL;
static IUnityGraphics* s_Graphics = NULL;

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API CreateDevice() { gstAVPipeline->CreateDevice(); }

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API CreatePipeline(const char* uri, const char* remote_peer_id)
{
    gstAVPipeline->CreatePipeline(uri, remote_peer_id);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API CreateTexture(unsigned int width, unsigned int height, bool left)
{
    gstAVPipeline->CreateTexture(width, height, left);
}

extern "C" UNITY_INTERFACE_EXPORT void* UNITY_INTERFACE_API GetTexturePtr(bool left)
{
    return gstAVPipeline->GetTexturePtr(left);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API ReleaseTexture(void* texPtr)
{
    gstAVPipeline->ReleaseTexture((ID3D11Texture2D*)texPtr);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API DestroyPipeline() { gstAVPipeline->DestroyPipeline(); }

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API DestroyDataPipeline() { gstDataPipeline->DestroyPipeline(); }

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API CreateDataPipeline()
{
    gstDataPipeline->CreatePipeline();
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetSDPOffer(const char* sdp_offer)
{
    gstDataPipeline->SetOffer(sdp_offer);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetICECandidate(const char* candidate, int mline_index)
{
    gstDataPipeline->SetICECandidate(candidate, mline_index);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SendBytesChannelService(const unsigned char * data, size_t size)
{
    gstDataPipeline->send_byte_array_channel_service(data, size);
}

/*
extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetTexture(void*
textureHandle, int w, int h)
{
        gstAVPipeline->SetTextureFromUnity(textureHandle, w, h);
}*/

// --------------------------------------------------------------------------
// UnitySetInterfaces

static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType);


extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces* unityInterfaces)
{
    s_UnityInterfaces = unityInterfaces;
    s_Graphics = s_UnityInterfaces->Get<IUnityGraphics>();
    s_Graphics->RegisterDeviceEventCallback(OnGraphicsDeviceEvent);

#if SUPPORT_VULKAN
    if (s_Graphics->GetRenderer() == kUnityGfxRendererNull)
    {
        extern void RenderAPI_Vulkan_OnPluginLoad(IUnityInterfaces*);
        RenderAPI_Vulkan_OnPluginLoad(unityInterfaces);
    }
#endif // SUPPORT_VULKAN

    // Run OnGraphicsDeviceEvent(initialize) manually on plugin load
    OnGraphicsDeviceEvent(kUnityGfxDeviceEventInitialize);

    SetEnvironmentVariable("GST_DEBUG_FILE", "Logs\\gstreamer.log");
    // SetEnvironmentVariable("GST_DEBUG", "h264decoder:6");
    // SetEnvironmentVariable("GST_TRACERS", "buffer - lateness(file =
    // \"buffer_lateness.log\")");
    gst_debug_set_default_threshold(GST_LEVEL_DEBUG);
    gst_init(nullptr, nullptr);
    gstAVPipeline = std::make_unique<GstAVPipeline>(s_UnityInterfaces);
    gstDataPipeline = std::make_unique<GstDataPipeline>();
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginUnload()
{
    s_Graphics->UnregisterDeviceEventCallback(OnGraphicsDeviceEvent);
    gstAVPipeline.reset();
    gstDataPipeline.reset();
    gst_deinit();
}

#if UNITY_WEBGL
typedef void(UNITY_INTERFACE_API* PluginLoadFunc)(IUnityInterfaces* unityInterfaces);
typedef void(UNITY_INTERFACE_API* PluginUnloadFunc)();

extern "C" void UnityRegisterRenderingPlugin(PluginLoadFunc loadPlugin, PluginUnloadFunc unloadPlugin);

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API RegisterPlugin()
{
    UnityRegisterRenderingPlugin(UnityPluginLoad, UnityPluginUnload);
}
#endif

// --------------------------------------------------------------------------
// GraphicsDeviceEvent

static RenderAPI* s_CurrentAPI = NULL;
static UnityGfxRenderer s_DeviceType = kUnityGfxRendererNull;

static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType)
{
    // Create graphics API implementation upon initialization
    if (eventType == kUnityGfxDeviceEventInitialize)
    {
        assert(s_CurrentAPI == NULL);
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
        s_CurrentAPI = NULL;
        s_DeviceType = kUnityGfxRendererNull;
    }
}

// --------------------------------------------------------------------------
// OnRenderEvent
// This will be called for GL.IssuePluginEvent script calls; eventID will
// be the integer passed to IssuePluginEvent. In this example, we just ignore
// that value.

static void UNITY_INTERFACE_API OnRenderEvent(int eventID)
{
    // Unknown / unsupported graphics device type? Do nothing
    if (s_CurrentAPI == NULL)
        return;

    if (eventID == 1)
    {
        gstAVPipeline->Draw(true);
        gstAVPipeline->Draw(false);
        /*ID3D11Texture2D* textures[2];
        textures[0] = gstAVPipeline->GetTexturePtr(true);
        textures[1] = gstAVPipeline->GetTexturePtr(false);
        s_CurrentAPI->Render((void**)textures);*/
    }
}

// --------------------------------------------------------------------------
// GetRenderEventFunc, an example function we export which is used to get a
// rendering event callback function.

extern "C" UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetRenderEventFunc() { return OnRenderEvent; }
