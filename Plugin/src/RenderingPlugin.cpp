/* Copyright(c) Pollen Robotics, all rights reserved.
 This source code is licensed under the license found in the
 LICENSE file in the root directory of this source tree. */

#include "Unity/IUnityGraphics.h"
#include <assert.h>
#include "GstAVPipeline.h"
#include "GstDataPipeline.h"
#include "GstMicPipeline.h"

static std::unique_ptr<GstAVPipeline> gstAVPipeline = nullptr;
static std::unique_ptr<GstDataPipeline> gstDataPipeline = nullptr;
static std::unique_ptr<GstMicPipeline> gstMicPipeline = nullptr;

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API CreateDevice() { gstAVPipeline->CreateDevice(); }

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API CreatePipeline(const char* uri, const char* remote_peer_id)
{
    gstAVPipeline->CreatePipeline(uri, remote_peer_id);
    gstMicPipeline->CreatePipeline(uri, remote_peer_id);
}

extern "C" UNITY_INTERFACE_EXPORT void* UNITY_INTERFACE_API CreateTexture(unsigned int width, unsigned int height, bool left)
{
    return gstAVPipeline->CreateTexture(width, height, left);
}

/* extern "C" UNITY_INTERFACE_EXPORT void* UNITY_INTERFACE_API GetTexturePtr(bool left)
{
    return gstAVPipeline->GetTexturePtr(left);
}*/

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API ReleaseTexture(void* texPtr)
{
    gstAVPipeline->ReleaseTexture((ID3D11Texture2D*)texPtr);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API DestroyPipeline() 
{
    gstAVPipeline->DestroyPipeline(); 
    gstMicPipeline->DestroyPipeline();
}

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

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SendBytesChannelCommand(const unsigned char* data, size_t size)
{
    gstDataPipeline->send_byte_array_channel_command(data, size);
}

extern "C"
{
    // Create a callback delegate to notify when a texture was drawn
    typedef void (*FuncCallBack)(void);
    static FuncCallBack callbackInstanceDrawn = nullptr;
    void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API RegisterDrawnCallback(FuncCallBack cb) { callbackInstanceDrawn = cb; };
}

// --------------------------------------------------------------------------
// UnitySetInterfaces


extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces* unityInterfaces)
{
    gst_init(nullptr, nullptr);
    gstAVPipeline = std::make_unique<GstAVPipeline>(unityInterfaces);
    gstMicPipeline = std::make_unique<GstMicPipeline>();
    gstDataPipeline = std::make_unique<GstDataPipeline>();
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginUnload()
{
    gst_deinit(); //Move elsewhere if needed. Unity never calls this function.
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
       bool left = gstAVPipeline->Draw(true);
       bool right = gstAVPipeline->Draw(false);
       if (left || right)
       {
           callbackInstanceDrawn();
       }
    }
}

// --------------------------------------------------------------------------
// GetRenderEventFunc, an example function we export which is used to get a
// rendering event callback function.

extern "C" UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetRenderEventFunc() { return OnRenderEvent; }
