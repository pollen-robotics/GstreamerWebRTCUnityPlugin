#pragma once
#include "Unity/IUnityInterface.h"
#include <gst/app/app.h>
#include <gst/gst.h>
#include <mutex>
#include <vector>

class GstAVPipeline
{

private:
    std::vector<GstPlugin*> preloaded_plugins;
    GstElement* audiomixer = nullptr;

    GstElement* _pipeline = nullptr;
    GMainContext* main_context_ = nullptr;
    GMainLoop* main_loop_ = nullptr;
    GThread* thread_ = nullptr;

    IUnityInterfaces* _s_UnityInterfaces = nullptr;
    // GstVideoInfo _render_info;
    /*struct AppData
    {
        GstAVPipeline* avpipeline = nullptr;
        GstCaps* last_caps = nullptr;
        std::mutex lock;
        GstSample* last_sample = nullptr;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture = nullptr;
        Microsoft::WRL::ComPtr<IDXGIKeyedMutex> keyed_mutex = nullptr;
        GstBuffer* shared_buffer = nullptr;
        GstD3D11Converter* conv = nullptr;
    };

    std::unique_ptr<AppData> _leftData = nullptr;
    std::unique_ptr<AppData> _rightData = nullptr;*/

public:
    GstAVPipeline(IUnityInterfaces* s_UnityInterfaces);
    ~GstAVPipeline();

    virtual void* GetTexturePtr(bool left = true) = 0;
    void Draw(bool left);
    // void EndDraw(bool left);

    void CreatePipeline(const char* uri, const char* remote_peer_id);
    virtual void CreateDevice() = 0;
    virtual void DestroyPipeline();

    bool CreateTexture(unsigned int width, unsigned int height, bool left = true);
    virtual void ReleaseTexture(void* texture) = 0;

private:
    static void on_pad_added(GstElement* src, GstPad* new_pad, gpointer data);
    static void webrtcbin_ready(GstElement* self, gchararray peer_id, GstElement* webrtcbin, gpointer udata);

    static gboolean dumpLatencyCallback(GstAVPipeline* self);

    static gpointer main_loop_func(gpointer data);
    static gboolean busHandler(GstBus* bus, GstMessage* msg, gpointer data);
    static GstBusSyncReply busSyncHandler(GstBus* bus, GstMessage* msg, gpointer user_data);
    virtual void busSyncHandler_context(GstMessage* msg) = 0;

    static GstElement* add_rtph264depay(GstElement* pipeline);
    static GstElement* add_h264parse(GstElement* pipeline);
    static GstElement* add_rtpopusdepay(GstElement* pipeline);
    static GstElement* add_queue(GstElement* pipeline);
    static GstElement* add_opusdec(GstElement* pipeline);
    static GstElement* add_audioconvert(GstElement* pipeline);
    static GstElement* add_audioresample(GstElement* pipeline);
    static GstElement* add_webrtcsrc(GstElement* pipeline, const std::string& remote_peer_id, const std::string& uri,
                                     GstAVPipeline* self);
    static GstElement* add_opusenc(GstElement* pipeline);
    static GstElement* add_audio_caps_capsfilter(GstElement* pipeline);
    static GstElement* add_webrtcsink(GstElement* pipeline, const std::string& uri);
    static GstElement* add_audiotestsrc(GstElement* pipeline);
    static GstElement* add_audiomixer(GstElement* pipeline);
    static GstElement* add_webrtcechoprobe(GstElement* pipeline);
    static GstElement* add_webrtcdsp(GstElement* pipeline);
    static GstElement* add_fakesink(GstElement* pipeline);
    static GstElement* add_tee(GstElement* pipeline);
    static GstElement* add_audiosink(GstAVPipeline* self);
    static GstElement* add_audiosrc(GstAVPipeline* self);
    virtual GstElement* make_audiosink() = 0;
    virtual GstElement* make_audiosrc() = 0;
    virtual void configure_videopad() = 0;
};
