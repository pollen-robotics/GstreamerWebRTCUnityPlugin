/* Copyright(c) Pollen Robotics, all rights reserved.
 This source code is licensed under the license found in the
 LICENSE file in the root directory of this source tree. */

#pragma once
#include "Unity/IUnityInterface.h"
#include "GstBasePipeline.h"
#include <d3d11.h>
#include <gst/app/app.h>
#include <gst/d3d11/gstd3d11.h>
#include <mutex>
#include <vector>
#include <wrl.h>

class GstAVPipeline : GstBasePipeline
{

private:
    std::vector<GstPlugin*> preloaded_plugins;

    GstD3D11Device* _device = nullptr;

    IUnityInterfaces* _s_UnityInterfaces = nullptr;
    GstVideoInfo _render_info;
    struct AppData
    {
        GstAVPipeline* avpipeline = nullptr;
        GstCaps* last_caps = nullptr;
        std::mutex lock;
        GstSample* last_sample = nullptr;
        Microsoft::WRL::ComPtr<IDXGIKeyedMutex> keyed_mutex = nullptr;
        GstBuffer* shared_buffer = nullptr;
        GstD3D11Converter* conv = nullptr;
    };

    std::unique_ptr<AppData> _leftData = nullptr;
    std::unique_ptr<AppData> _rightData = nullptr;

public:
    GstAVPipeline(IUnityInterfaces* s_UnityInterfaces);
    ~GstAVPipeline();

    bool Draw(bool left);

    void CreatePipeline(const char* uri, const char* remote_peer_id);
    void CreateDevice();
    void DestroyPipeline() override;

    ID3D11Texture2D* CreateTexture(unsigned int width, unsigned int height, bool left = true);
    void ReleaseTexture(ID3D11Texture2D* texture);

private:
    static void on_pad_added(GstElement* src, GstPad* new_pad, gpointer data);
    static void webrtcbin_ready(GstElement* self, gchararray peer_id, GstElement* webrtcbin, gpointer udata);
    
    static GstFlowReturn GstAVPipeline::on_new_sample(GstAppSink* appsink, gpointer user_data);

    GstBusSyncReply busSyncHandler(GstBus* bus, GstMessage* msg, gpointer user_data) override;

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
};
