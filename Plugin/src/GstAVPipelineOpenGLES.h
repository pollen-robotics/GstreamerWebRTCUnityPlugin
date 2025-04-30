#pragma once
#include "GstAVPipeline.h"
// #include "Unity/IUnityInterface.h"
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <android/native_window_jni.h>
// #include <gst/gl/gstglcontext.h>
#include <gst/app/app.h>
#include <gst/gl/egl/gstgldisplay_egl.h>
#include <gst/gl/gl.h>
#include <jni.h>
#include <mutex>

class GstAVPipelineOpenGLES : public GstAVPipeline
{

public:
    GstAVPipelineOpenGLES(IUnityInterfaces* s_UnityInterfaces);
    // void CreateTextureAndSurfaces(JNIEnv* env, int width, int height, bool left);
    //  void CreateSurfaces(JNIEnv* env, void* textureHandle, int width, int height, bool left);
    void* CreateTexture(bool left);
    // GLuint CreateTexture(bool left);
    void Draw(JNIEnv* env, bool left);
    void ReleaseTexture(void* texture) override;
    void SetTextureFromUnity(GLuint texPtr, bool left);
    void SetUnityContext();

private:
    jmethodID _updateTexImageMethod = nullptr;

    GstGLContext* gl_context_unity = nullptr;
    // GstGLDisplay* gl_display = nullptr;

    struct AppData
    {
        // GstGLContext* gl_context = nullptr;
        // GstGLDisplay* gl_display = nullptr;
        GLuint textureID = -1;
        ANativeWindow* nativeWindow = nullptr;
        jobject surfaceTexture = nullptr;
        GstCaps* last_caps = nullptr;
        GstSample* last_sample = nullptr;
        std::mutex lock;
    };
    std::unique_ptr<AppData> _leftData = nullptr;
    std::unique_ptr<AppData> _rightData = nullptr;

private:
    // ANativeWindow* SetNativeWindow(JNIEnv* env, jobject surface);
    GstBusSyncReply busSyncHandler(GstBus* bus, GstMessage* msg, gpointer user_data) override;
    virtual void createCustomPipeline() override;
    void fillTextureWithColor(GLuint textureID, GLsizei width, GLsizei height, GLubyte r, GLubyte g, GLubyte b, GLubyte a);
    GstElement* add_appsink(GstElement* pipeline);
    static GstFlowReturn on_new_sample(GstAppSink* appsink, gpointer user_data);
    void copyGStreamerTextureToFramebuffer(GLuint sourceTexture, GLuint destinationTexture, GLsizei width, GLsizei height);
    void InitFullscreenQuadShader();
};
