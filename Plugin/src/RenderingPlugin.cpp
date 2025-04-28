/* Copyright(c) Pollen Robotics, all rights reserved.
 This source code is licensed under the license found in the
 LICENSE file in the root directory of this source tree. */

#include "Unity/IUnityGraphics.h"
#include "Unity/PlatformBase.h"
// #include <assert.h>

#if UNITY_WIN
#include "GstAVPipelineD3D11.h"

static std::unique_ptr<GstAVPipelineD3D11> gstAVPipeline = nullptr;

#elif UNITY_ANDROID
#include "GstAVPipelineOpenGLES.h"
#include <jni.h>
static std::unique_ptr<GstAVPipelineOpenGLES> gstAVPipeline = nullptr;
static JavaVM* ms2_vm = nullptr;
static void* g_TextureHandle_left = nullptr;
static void* g_TextureHandle_right = nullptr;

jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
    // vm->AttachCurrentThread(&jni_env, 0);
    ms2_vm = vm;
    return JNI_VERSION_1_6;
}

#endif

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API CreateDevice()
{
#if UNITY_WIN
    gstAVPipeline->CreateDevice();
#endif
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API CreatePipeline() { gstAVPipeline->CreatePipeline(); }

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

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API DestroyPipeline() { gstAVPipeline->DestroyPipeline(); }

// --------------------------------------------------------------------------
// UnitySetInterfaces

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces* unityInterfaces)
{
    // const char* internalStoragePath = getenv("EXTERNAL_STORAGE");
    // td::string logFilePath = std::string(internalStoragePath) + "/gstreamer.log";
    /*setenv("GST_DEBUG_FILE", "/storage/emulated/0/Android/data/com.DefaultCompany.UnityProject/files/gstreamer/gstreamer.log",
           1);
    setenv("GST_DEBUG_NO_COLOR", "1", 1);
    setenv("GST_DEBUG", "4", 1);*/

#if UNITY_WIN
    gst_init(nullptr, nullptr);
    gstAVPipeline = std::make_unique<GstAVPipelineD3D11>(unityInterfaces);
#elif UNITY_ANDROID
    // gst_init done in the java side
    gstAVPipeline = std::make_unique<GstAVPipelineOpenGLES>(unityInterfaces);
#endif
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
#if UNITY_WIN
    if (eventID == 1)
    {
        gstAVPipeline->Draw(true);
        gstAVPipeline->Draw(false);
    }
#elif UNITY_ANDROID

    int status;
    JNIEnv* env;

    if ((status = ms2_vm->GetEnv((void**)&env, JNI_VERSION_1_6)) < 0)
    {
        if ((status = ms2_vm->AttachCurrentThread(&env, NULL)) < 0)
        {
            return;
        }
    }

    if (eventID == 0)
    {
        int height = 720;
        int width = 960;
        gstAVPipeline->CreateTextureAndSurfaces(env, width, height, true);
        gstAVPipeline->CreateTextureAndSurfaces(env, width, height, false);
    }
    else if (eventID == 1)
    {
        gstAVPipeline->Draw(env, true);
        gstAVPipeline->Draw(env, false);
    }

#endif
}

// --------------------------------------------------------------------------
// GetRenderEventFunc, an example function we export which is used to get a
// rendering event callback function.

extern "C" UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetRenderEventFunc() { return OnRenderEvent; }
