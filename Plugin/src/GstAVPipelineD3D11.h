#pragma once
#include "GstAVPipeline.h"
#include "Unity/IUnityInterface.h"
#include <d3d11.h>
#include <gst/d3d11/gstd3d11.h>
#include <wrl.h>

class GstAVPipelineD3D11 : public GstAVPipeline
{

private:
    GstElement* audiomixer = nullptr;

    GstElement* _pipeline = nullptr;
    GstD3D11Device* _device = nullptr;
    GThread* thread_ = nullptr;

    IUnityInterfaces* _s_UnityInterfaces = nullptr;
    GstVideoInfo _render_info;
    struct AppData
    {
        GstAVPipelineD3D11* avpipeline = nullptr;
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
    GstAVPipelineD3D11(IUnityInterfaces* s_UnityInterfaces);
    ~GstAVPipelineD3D11();

    ID3D11Texture2D* GetTexturePtr(bool left = true) override;
    void Draw(bool left);

    void CreatePipeline(const char* uri, const char* remote_peer_id);
    void CreateDevice() override;
    void DestroyPipeline() override;

    bool CreateTexture(unsigned int width, unsigned int height, bool left = true);
    void ReleaseTexture(ID3D11Texture2D* texture) override;

private:
    static GstFlowReturn GstAVPipelineD3D11::on_new_sample(GstAppSink* appsink, gpointer user_data);
    GstElement* make_audiosink() override;
    GstElement* make_audiosrc() override;
    static GstElement* add_d3d11h264dec(GstElement* pipeline);
    static GstElement* add_d3d11convert(GstElement* pipeline);
    static GstElement* add_appsink(GstElement* pipeline);
    void configure_videopad() override;
    void busSyncHandler_context(GstMessage* msg) override;
};
