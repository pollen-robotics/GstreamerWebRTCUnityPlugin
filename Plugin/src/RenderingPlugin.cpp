/* Copyright(c) Pollen Robotics, all rights reserved.
 This source code is licensed under the license found in the
 LICENSE file in the root directory of this source tree. */

#include "DebugLog.h"
#include "GstDataPipeline.h"
#include "GstMicPipeline.h"
#include "Unity/IUnityGraphics.h"
#include "Unity/PlatformBase.h"
// #include <assert.h>

#if UNITY_WIN
#include "GstAVPipelineD3D11.h"

static std::unique_ptr<GstAVPipelineD3D11> gstAVPipeline = nullptr;

#elif UNITY_ANDROID
#include "GstAVPipelineOpenGLES.h"

static std::unique_ptr<GstAVPipelineOpenGLES> gstAVPipeline = nullptr;

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetTextureFromUnity(void* texPtr, bool left, int width, int height)
{
    gstAVPipeline->SetTextureFromUnity((GLuint)(size_t)(texPtr), left, width, height);
}

#endif

static std::unique_ptr<GstMicPipeline> gstMicPipeline = nullptr;
static std::unique_ptr<GstDataPipeline> gstDataPipeline = nullptr;

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API CreateDevice()
{
#if UNITY_WIN
    gstAVPipeline->CreateDevice();
#endif
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API CreatePipeline(const char* uri, const char* remote_peer_id)
{
    Debug::Log("CreatePipeline", Level::Info);
    gstAVPipeline->CreatePipeline(uri, remote_peer_id);
    gstMicPipeline->CreatePipeline(uri, remote_peer_id);
}

extern "C" UNITY_INTERFACE_EXPORT void* UNITY_INTERFACE_API CreateTexture(unsigned int width, unsigned int height, bool left)
{
#if UNITY_WIN
    return gstAVPipeline->CreateTexture(width, height, left);
#elif UNITY_ANDROID
    return gstAVPipeline->CreateTexture(left);
#endif
    return nullptr;
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API ReleaseTexture(void* texPtr)
{
    gstAVPipeline->ReleaseTexture(texPtr);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API DestroyPipeline()
{
    Debug::Log("DestroyPipeline", Level::Info);
    gstAVPipeline->DestroyPipeline();
    gstMicPipeline->DestroyPipeline();
    gst_deinit();
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API DestroyDataPipeline() { gstDataPipeline->DestroyPipeline(); }

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API CreateDataPipeline() { gstDataPipeline->CreatePipeline(); }

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetSDPOffer(const char* sdp_offer)
{
    gstDataPipeline->SetOffer(sdp_offer);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetICECandidate(const char* candidate, int mline_index)
{
    gstDataPipeline->SetICECandidate(candidate, mline_index);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SendBytesChannelService(const unsigned char* data, size_t size)
{
    gstDataPipeline->send_byte_array_channel_service(data, size);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SendBytesChannelReliableCommand(const unsigned char* data,
                                                                                           size_t size)
{
    gstDataPipeline->send_byte_array_channel_command_reliable(data, size);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SendBytesChannelLossyCommand(const unsigned char* data, size_t size)
{
    gstDataPipeline->send_byte_array_channel_command_lossy(data, size);
}

// --------------------------------------------------------------------------
// UnitySetInterfaces

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces* unityInterfaces)
{
    // const char* internalStoragePath = getenv("EXTERNAL_STORAGE");
    // td::string logFilePath = std::string(internalStoragePath) + "/gstreamer.log";
    // setenv("GST_DEBUG_FILE", "/storage/emulated/0/Android/data/com.DefaultCompany.UnityProject/files/gstreamer.log", 1);
    // setenv("GST_DEBUG_NO_COLOR", "1", 1);
    // setenv("GST_DEBUG", "4", 1);
    // gst_debug_set_threshold_for_name("basesink", GST_LEVEL_DEBUG);

#if UNITY_WIN
    gst_init(nullptr, nullptr);
    gstAVPipeline = std::make_unique<GstAVPipelineD3D11>(unityInterfaces);
#elif UNITY_ANDROID
    // gst_init done in the java side
    gstAVPipeline = std::make_unique<GstAVPipelineOpenGLES>(unityInterfaces);
#endif
    gstMicPipeline = std::make_unique<GstMicPipeline>();
    gstDataPipeline = std::make_unique<GstDataPipeline>();
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginUnload()
{
    gst_deinit(); // Move elsewhere if needed. Unity never calls this function.
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
#if UNITY_ANDROID

    else if (eventID == 0)
    {
        gstAVPipeline->SetUnityContext();
    }

#endif
}

// --------------------------------------------------------------------------
// GetRenderEventFunc, an example function we export which is used to get a
// rendering event callback function.

extern "C" UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetRenderEventFunc() { return OnRenderEvent; }
