#pragma once
#include "Unity/IUnityInterface.h"
#include <d3d11.h>
#include <gst/app/app.h>
#include <gst/d3d11/gstd3d11.h>
#include <gst/gst.h>
#include <mutex>
#include <vector>
#include <wrl.h>

class GstAVPipeline
{

private:
    std::vector<GstPlugin*> preloaded_plugins;
    GstElement* audiomixer = nullptr;

    GstElement* _pipeline = nullptr;
    GstD3D11Device* _device = nullptr;
    GMainContext* main_context_ = nullptr;
    GMainLoop* main_loop_ = nullptr;
    GThread* thread_ = nullptr;

    IUnityInterfaces* _s_UnityInterfaces = nullptr;
    GstVideoInfo _render_info;
    struct AppData
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
    std::unique_ptr<AppData> _rightData = nullptr;

public:
    GstAVPipeline(IUnityInterfaces* s_UnityInterfaces);
    ~GstAVPipeline();

    ID3D11Texture2D* GetTexturePtr(bool left = true);
    void Draw(bool left);
    // void EndDraw(bool left);

    void CreatePipeline(const char* uri, const char* remote_peer_id);
    void CreateDevice();
    void DestroyPipeline();

    bool CreateTexture(unsigned int width, unsigned int height, bool left = true);
    void ReleaseTexture(ID3D11Texture2D* texture);

private:
    static void on_pad_added(GstElement* src, GstPad* new_pad, gpointer data);
    static void webrtcbin_ready(GstElement* self, gchararray peer_id, GstElement* webrtcbin, gpointer udata);

    static GstFlowReturn GstAVPipeline::on_new_sample(GstAppSink* appsink, gpointer user_data);

    static gboolean dumpLatencyCallback(GstAVPipeline* self);

    static gpointer main_loop_func(gpointer data);
    static gboolean busHandler(GstBus* bus, GstMessage* msg, gpointer data);
    static GstBusSyncReply busSyncHandler(GstBus* bus, GstMessage* msg, gpointer user_data);

    static GstElement* add_rtph264depay(GstElement* pipeline);
    static GstElement* add_h264parse(GstElement* pipeline);
    static GstElement* add_d3d11h264dec(GstElement* pipeline);
    static GstElement* add_d3d11convert(GstElement* pipeline);
    static GstElement* add_appsink(GstElement* pipeline);
    static GstElement* add_rtpopusdepay(GstElement* pipeline);
    static GstElement* add_queue(GstElement* pipeline);
    static GstElement* add_opusdec(GstElement* pipeline);
    static GstElement* add_audioconvert(GstElement* pipeline);
    static GstElement* add_audioresample(GstElement* pipeline);
    static GstElement* add_wasapi2sink(GstElement* pipeline);
    static GstElement* add_webrtcsrc(GstElement* pipeline, const std::string& remote_peer_id, const std::string& uri,
                                     GstAVPipeline* self);
    static GstElement* add_wasapi2src(GstElement* pipeline);
    static GstElement* add_opusenc(GstElement* pipeline);
    static GstElement* add_audio_caps_capsfilter(GstElement* pipeline);
    static GstElement* add_webrtcsink(GstElement* pipeline, const std::string& uri);
    static GstElement* add_audiotestsrc(GstElement* pipeline);
    static GstElement* add_audiomixer(GstElement* pipeline);
    static GstElement* add_webrtcechoprobe(GstElement* pipeline);
    static GstElement* add_webrtcdsp(GstElement* pipeline);
    static GstElement* add_fakesink(GstElement* pipeline);
    static GstElement* add_tee(GstElement* pipeline);
};
