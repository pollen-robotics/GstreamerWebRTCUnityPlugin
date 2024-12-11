/* Copyright(c) Pollen Robotics, all rights reserved.
 This source code is licensed under the license found in the
 LICENSE file in the root directory of this source tree. */

#pragma once
#include "GstAVPipeline.h"
#include "Unity/IUnityInterface.h"
#include <d3d11.h>
#include <gst/app/app.h>
#include <gst/d3d11/gstd3d11.h>
#include <mutex>
#include <vector>
#include <wrl.h>

class GstAVPipelineD3D11 : public GstAVPipeline
{

private:
    GstD3D11Device* _device = nullptr;
    GstVideoInfo _render_info;
    struct AppData
    {
        GstAVPipelineD3D11* avpipeline = nullptr;
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
    GstAVPipelineD3D11(IUnityInterfaces* s_UnityInterfaces);

    void Draw(bool left);

    void CreateDevice();
    void DestroyPipeline() override;

    ID3D11Texture2D* CreateTexture(unsigned int width, unsigned int height, bool left = true);
    void ReleaseTexture(void* texture) override;

private:
    void on_pad_added(GstElement* src, GstPad* new_pad, gpointer data) override;

    static GstFlowReturn GstAVPipelineD3D11::on_new_sample(GstAppSink* appsink, gpointer user_data);

    GstBusSyncReply busSyncHandler(GstBus* bus, GstMessage* msg, gpointer user_data) override;

    static GstElement* add_appsink(GstElement* pipeline);
    static GstElement* add_wasapi2sink(GstElement* pipeline);
};
