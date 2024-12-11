/* Copyright(c) Pollen Robotics, all rights reserved.
 This source code is licensed under the license found in the
 LICENSE file in the root directory of this source tree. */

#include "GstDataPipeline.h"
#include "Unity/IUnityGraphics.h"
#include "Unity/PlatformBase.h"
// #include <assert.h>

#if UNIY_WIN
#include "GstAVPipeline.h"
#include "GstMicPipeline.h"

static std::unique_ptr<GstMicPipeline> gstMicPipeline = nullptr;
static std::unique_ptr<GstAVPipelineD3D11> gstAVPipeline = nullptr;

#elif UNITY_ANDROID
#include "GstAVPipelineOpenGLES.h"
#include <jni.h>
static std::unique_ptr<GstAVPipelineOpenGLES> gstAVPipeline = nullptr;
//  static JNIEnv* jni_env = nullptr;
static JavaVM* ms2_vm = nullptr;
static jobject gCallbackObject;

jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
    // vm->AttachCurrentThread(&jni_env, 0);
    ms2_vm = vm;
    return JNI_VERSION_1_6;
}

static jobject surface_plugin = nullptr;

#endif

static std::unique_ptr<GstDataPipeline> gstDataPipeline = nullptr;

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API CreateDevice()
{
#if UNIY_WIN
    gstAVPipeline->CreateDevice();
#endif
}

#if UNITY_ANDROID
extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetSurface(jobject surface, bool left)
{
    // GstAVPipelineOpenGLES* avpipeline = static_cast<GstAVPipelineOpenGLES*>(gstAVPipeline.get());
    JNIEnv* jni_env = nullptr;
    ms2_vm->AttachCurrentThread(&jni_env, 0);
    gstAVPipeline->SetNativeWindow(jni_env, surface, left);
}
#endif

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API CreatePipeline(const char* uri, const char* remote_peer_id)
{
    gstAVPipeline->CreatePipeline(uri, remote_peer_id);
#if UNIY_WIN
    gstMicPipeline->CreatePipeline(uri, remote_peer_id);
#endif
}

extern "C" UNITY_INTERFACE_EXPORT void* UNITY_INTERFACE_API CreateTexture(unsigned int width, unsigned int height, bool left)
{
#if UNIY_WIN
    return gstAVPipeline->CreateTexture(width, height, left);
#endif
    return nullptr;
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API ReleaseTexture(void* texPtr)
{
#if UNIY_WIN
    gstAVPipeline->ReleaseTexture((ID3D11Texture2D*)texPtr);
#elif UNITY_ANDROID
    gstAVPipeline->ReleaseTexture(texPtr);
#endif
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API DestroyPipeline()
{
    gstAVPipeline->DestroyPipeline();
#if UNIY_WIN
    gstMicPipeline->DestroyPipeline();
#endif
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

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SendBytesChannelCommand(const unsigned char* data, size_t size)
{
    gstDataPipeline->send_byte_array_channel_command(data, size);
}

#if UNITY_ANDROID
extern "C" JNIEXPORT void JNICALL Java_com_pollenrobotics_gstreamer_RenderingCallbackManager_nativeInit(JNIEnv* env,
                                                                                                        jobject obj)
{
    gCallbackObject = env->NewGlobalRef(obj);
}
#endif

// --------------------------------------------------------------------------
// UnitySetInterfaces

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces* unityInterfaces)
{
    // const char* internalStoragePath = getenv("EXTERNAL_STORAGE");
    // td::string logFilePath = std::string(internalStoragePath) + "/gstreamer.log";
    setenv("GST_DEBUG_FILE", "/storage/emulated/0/Android/data/com.DefaultCompany.UnityProject/files/gstreamer.log", 1);
    setenv("GST_DEBUG_NO_COLOR", "1", 1);
    setenv("GST_DEBUG", "4", 1);

#if UNIY_WIN
    gst_init(nullptr, nullptr);
    gstAVPipeline = std::make_unique<GstAVPipelineD3D11>(unityInterfaces);
    gstMicPipeline = std::make_unique<GstMicPipeline>();
#elif UNITY_ANDROID
    // gst_init done in the java side
    gstAVPipeline = std::make_unique<GstAVPipelineOpenGLES>(unityInterfaces);
#endif

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
#if UNIY_WIN
    if (eventID == 1)
    {
        gstAVPipeline->Draw(true);
        gstAVPipeline->Draw(false);
    }
#elif UNITY_ANDROID
    int status;
    JNIEnv* env;
    int isAttached = 0;

    if ((status = ms2_vm->GetEnv((void**)&env, JNI_VERSION_1_6)) < 0)
    {
        if ((status = ms2_vm->AttachCurrentThread(&env, NULL)) < 0)
        {
            return;
        }
        isAttached = 1;
    }

    jclass cls = env->GetObjectClass(gCallbackObject);
    if (!cls)
    {
        if (isAttached)
            ms2_vm->DetachCurrentThread();
        return;
    }

    jmethodID method = env->GetMethodID(cls, "JavaOnRenderEvent", "(I)V");
    if (!method)
    {
        if (isAttached)
            ms2_vm->DetachCurrentThread();
        return;
    }

    env->CallVoidMethod(gCallbackObject, method, eventID);

    if (isAttached)
        ms2_vm->DetachCurrentThread();
#endif
}

// --------------------------------------------------------------------------
// GetRenderEventFunc, an example function we export which is used to get a
// rendering event callback function.

extern "C" UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetRenderEventFunc() { return OnRenderEvent; }
