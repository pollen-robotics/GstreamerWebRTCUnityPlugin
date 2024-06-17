#include "GstAVPipeline.h"
#include "Unity/IUnityGraphics.h"
#include "Unity/PlatformBase.h"
// #include <assert.h>
#include <memory>

static std::unique_ptr<GstAVPipeline> gstAVPipeline = nullptr;

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
    gstAVPipeline->ReleaseTexture(texPtr);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API DestroyPipeline() { gstAVPipeline->DestroyPipeline(); }

// --------------------------------------------------------------------------
// UnitySetInterfaces

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces* unityInterfaces)
{
    gst_init(nullptr, nullptr);
#if UNITY_WIN
    gstAVPipeline = std::make_unique<GstAVPipeline>(unityInterfaces);
#elif UNITY_LINUX
    // todo
#endif
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginUnload()
{
    gstAVPipeline.reset();
    gst_deinit();
}

// --------------------------------------------------------------------------
// OnRenderEvent
// This will be called for GL.IssuePluginEvent script calls; eventID will
// be the integer passed to IssuePluginEvent. In this example, we just ignore
// that value.

static void UNITY_INTERFACE_API OnRenderEvent(int eventID)
{
    if (eventID == 1)
    {
        gstAVPipeline->Draw(true);
        gstAVPipeline->Draw(false);
    }
}

// --------------------------------------------------------------------------
// GetRenderEventFunc, an example function we export which is used to get a
// rendering event callback function.

extern "C" UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetRenderEventFunc() { return OnRenderEvent; }
