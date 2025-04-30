#pragma once
#include "GstAVPipeline.h"
#include <GLES3/gl3.h>
#include <gst/app/app.h>
#include <gst/gl/gl.h>
#include <jni.h>
#include <mutex>

class GstAVPipelineOpenGLES : public GstAVPipeline
{

public:
    GstAVPipelineOpenGLES(IUnityInterfaces* s_UnityInterfaces);
    void* CreateTexture(bool left);
    void Draw(JNIEnv* env, bool left);
    void ReleaseTexture(void* texture) override;
    void SetTextureFromUnity(GLuint texPtr, bool left);
    void SetUnityContext();

private:
    GstGLContext* gl_context_unity = nullptr;

    struct AppData
    {
        GLuint textureID = -1;
        GstCaps* last_caps = nullptr;
        GstSample* last_sample = nullptr;
        std::mutex lock;
    };
    std::unique_ptr<AppData> _leftData = nullptr;
    std::unique_ptr<AppData> _rightData = nullptr;

private:
    void on_pad_added(GstElement* src, GstPad* new_pad, gpointer data) override;
    GstBusSyncReply busSyncHandler(GstBus* bus, GstMessage* msg, gpointer user_data) override;
    GstElement* add_appsink(GstElement* pipeline);
    static GstFlowReturn on_new_sample(GstAppSink* appsink, gpointer user_data);
    void copyGStreamerTextureToFramebuffer(GLuint sourceTexture, GLuint destinationTexture, GLsizei width, GLsizei height);
};
