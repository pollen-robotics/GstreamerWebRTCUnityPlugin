#pragma once
#include "GstAVPipeline.h"
// #include "Unity/IUnityInterface.h"
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <android/native_window_jni.h>
#include <jni.h>
#include <mutex>

class GstAVPipelineOpenGLES : public GstAVPipeline
{

public:
    GstAVPipelineOpenGLES(IUnityInterfaces* s_UnityInterfaces);
    void SetNativeWindow(JNIEnv* env, jobject surface, bool left);
    void ReleaseTexture(void* texture) override;

private:
    ANativeWindow* _nativeWindow_left = nullptr;
    ANativeWindow* _nativeWindow_right = nullptr;
    std::mutex _lock;

private:
    void on_pad_added(GstElement* src, GstPad* new_pad, gpointer data) override;
};
