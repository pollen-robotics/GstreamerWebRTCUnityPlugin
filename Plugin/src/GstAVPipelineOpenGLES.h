#pragma once
#include "GstBasePipeline.h"
#include <GLES2/gl2.h>
// #include <android/native_window.h>
#include "Unity/IUnityInterface.h"
#include <EGL/egl.h>
#include <android/native_window_jni.h>
#include <gst/app/app.h>
#include <jni.h>
#include <mutex>
#include <vector>

class GstAVPipelineOpenGLES : public GstBasePipeline
{

public:
    GstAVPipelineOpenGLES(IUnityInterfaces* s_UnityInterfaces);

    void CreatePipeline(const char* uri, const char* remote_peer_id);
    void DestroyPipeline() override;

    void SetNativeWindow(JNIEnv* env, jobject surface, bool left);

private:
    ANativeWindow* _nativeWindow_left = nullptr;
    EGLDisplay _display;
    EGLContext _context;
    EGLSurface _surface;
    ANativeWindow* _nativeWindow_right = nullptr;
    GLuint _textureId_left;
    GLuint _textureId_right;
    IUnityInterfaces* _s_UnityInterfaces = nullptr;
    std::vector<GstPlugin*> preloaded_plugins;

    /*struct AppData
    {
        GstAVPipelineOpenGLES* avpipeline = nullptr;
        GstCaps* last_caps = nullptr;
        std::mutex lock;
        GstSample* last_sample = nullptr;
        GLuint textureId = 0;
    };

    std::unique_ptr<AppData> _leftData = nullptr;
    std::unique_ptr<AppData> _rightData = nullptr;*/

private:
    GstElement* make_audiosink();
    GstElement* make_audiosrc();
    static GstElement* add_videoconvert(GstElement* pipeline);
    static GstElement* add_glimagesink(GstElement* pipeline);
    static GstElement* add_appsink(GstElement* pipeline);
    static void on_pad_added(GstElement* src, GstPad* new_pad, gpointer data);
    static void webrtcbin_ready(GstElement* self, gchararray peer_id, GstElement* webrtcbin, gpointer udata);
    // void busSyncHandler_context(GstMessage* msg) override;
    void set_custom_opusenc_settings(GstElement* opusenc);
    static GstElement* add_webrtcsrc(GstElement* pipeline, const std::string& remote_peer_id, const std::string& uri,
                                     GstAVPipelineOpenGLES* self);
    static GstElement* add_rtph264depay(GstElement* pipeline);
    static GstElement* add_h264parse(GstElement* pipeline);
    static GstElement* add_queue(GstElement* pipeline);
    static GstFlowReturn on_new_sample(GstAppSink* appsink, gpointer user_data);
};
