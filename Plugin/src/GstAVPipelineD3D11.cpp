#include "GstAVPipelineD3D11.h"
#include "DebugLog.h"

#include <d3d11_1.h>
#include <d3d11sdklayers.h>
#include <dxgi1_2.h>
#include <dxgiformat.h>

#include "Unity/IUnityGraphicsD3D11.h"
using namespace Microsoft::WRL;

// Call only on plugin thread
// Creates the underlying D3D11 texture using the provided unity device.
// This texture can then be turned into a proper Unity texture on the
// managed side using Texture2D.CreateExternalTexture()
bool GstAVPipelineD3D11::CreateTexture(unsigned int width, unsigned int height, bool left)
{
    auto device = _s_UnityInterfaces->Get<IUnityGraphicsD3D11>()->GetDevice();
    HRESULT hr = S_OK;

    gst_video_info_set_format(&_render_info, GST_VIDEO_FORMAT_RGBA, width, height);

    std::unique_ptr<AppData> data = std::make_unique<AppData>();
    data->avpipeline = this;

    // Create a texture 2D that can be shared
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX | D3D11_RESOURCE_MISC_SHARED_NTHANDLE;
    // desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

    hr = device->CreateTexture2D(&desc, nullptr, &data->texture);
    g_assert(SUCCEEDED(hr));

    hr = data->texture.As(&data->keyed_mutex);
    g_assert(SUCCEEDED(hr));

    hr = data->keyed_mutex->AcquireSync(0, INFINITE);
    g_assert(SUCCEEDED(hr));

    ComPtr<IDXGIResource1> dxgi_resource;
    hr = data->texture.As(&dxgi_resource);
    g_assert(SUCCEEDED(hr));

    HANDLE shared_handle = nullptr;
    hr = dxgi_resource->CreateSharedHandle(nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, nullptr,
                                           &shared_handle);
    g_assert(SUCCEEDED(hr));

    auto gst_device = gst_d3d11_device_get_device_handle(_device);
    ComPtr<ID3D11Device1> device1;
    hr = gst_device->QueryInterface(IID_PPV_ARGS(&device1));
    g_assert(SUCCEEDED(hr));

    /* Open shared texture at GStreamer device side */
    ComPtr<ID3D11Texture2D> gst_texture;
    hr = device1->OpenSharedResource1(shared_handle, IID_PPV_ARGS(&gst_texture));
    g_assert(SUCCEEDED(hr));
    /* Can close NT handle now */
    CloseHandle(shared_handle);

    /* Wrap shared texture with GstD3D11Memory in order to convert texture
     * using converter API */
    GstMemory* mem = gst_d3d11_allocator_alloc_wrapped(nullptr, _device, gst_texture.Get(),
                                                       /* CPU accessible (staging texture) memory size is unknown.
                                                        * Pass zero here, then GStreamer will calculate it */
                                                       0, nullptr, nullptr);
    g_assert(mem);

    data->shared_buffer = gst_buffer_new();
    gst_buffer_append_memory(data->shared_buffer, mem);

    if (left)
        _leftData = std::move(data);
    else
        _rightData = std::move(data);

    return true;
}

GstFlowReturn GstAVPipelineD3D11::on_new_sample(GstAppSink* appsink, gpointer user_data)
{
    AppData* data = static_cast<AppData*>(user_data);
    GstSample* sample = gst_app_sink_pull_sample(appsink);

    if (!sample)
        return GST_FLOW_ERROR;

    GstCaps* caps = gst_sample_get_caps(sample);
    if (!caps)
    {
        gst_sample_unref(sample);
        Debug::Log("Sample without caps", Level::Error);
        return GST_FLOW_ERROR;
    }

    std::lock_guard<std::mutex> lk(data->lock);
    // data->avpipeline->Enter();
    /* Caps updated, recreate converter */
    if (data->last_caps && !gst_caps_is_equal(data->last_caps, caps))
        gst_clear_object(&data->conv);

    if (!data->conv)
    {
        Debug::Log("Create new converter");
        GstVideoInfo in_info;
        gst_video_info_from_caps(&in_info, caps);

        /* In case of shared texture, video processor might not behave as expected.
         * Use only pixel shader */
        auto config = gst_structure_new("converter-config", GST_D3D11_CONVERTER_OPT_BACKEND, GST_TYPE_D3D11_CONVERTER_BACKEND,
                                        GST_D3D11_CONVERTER_BACKEND_SHADER, nullptr);

        data->conv = gst_d3d11_converter_new(data->avpipeline->_device, &in_info, &data->avpipeline->_render_info, config);
    }

    gst_caps_replace(&data->last_caps, caps);
    gst_clear_sample(&data->last_sample);
    data->last_sample = sample;

    return GST_FLOW_OK;
}

void GstAVPipelineD3D11::Draw(bool left)
{
    AppData* data;
    if (left)
        data = _leftData.get();
    else
        data = _rightData.get();

    if (data == nullptr)
    {
        Debug::Log("data is null", Level::Warning);
        return;
    }

    GstSample* sample = nullptr;

    /* Steal sample pointer */
    std::lock_guard<std::mutex> lk(data->lock);
    /* If there's no updated sample, don't need to render again */
    if (!data->last_sample)
        return;

    sample = data->last_sample;
    data->last_sample = nullptr;

    auto buf = gst_sample_get_buffer(sample);
    if (!buf)
    {
        Debug::Log("Sample without buffer", Level::Error);
        gst_sample_unref(sample);
        return;
    }

    data->keyed_mutex->ReleaseSync(0);
    /* Converter will take gst_d3d11_device_lock() and acquire sync */
    gst_d3d11_converter_convert_buffer(data->conv, buf, data->shared_buffer);
    data->keyed_mutex->AcquireSync(0, INFINITE);
    gst_sample_unref(sample);
}

GstElement* GstAVPipelineD3D11::make_audiosink()
{
    GstElement* wasapi2sink = gst_element_factory_make("wasapi2sink", nullptr);
    if (!wasapi2sink)
    {
        Debug::Log("Failed to create wasapi2sink", Level::Error);
        return nullptr;
    }
    g_object_set(wasapi2sink, "low-latency", true, "provide-clock", false, nullptr);

    return wasapi2sink;
}

GstElement* GstAVPipelineD3D11::make_audiosrc(GstElement* pipeline)
{
    GstElement* wasapi2src = gst_element_factory_make("wasapi2src", nullptr);
    if (!wasapi2src)
    {
        Debug::Log("Failed to create wasapi2src", Level::Error);
        return nullptr;
    }
    g_object_set(wasapi2src, "low-latency", true, "provide-clock", false, nullptr);

    return wasapi2src;
}

GstElement* GstAVPipelineD3D11::add_d3d11h264dec(GstElement* pipeline)
{
    GstElement* d3d11h264dec = gst_element_factory_make("d3d11h264dec", nullptr);
    if (!d3d11h264dec)
    {
        Debug::Log("Failed to create d3d11h264dec", Level::Error);
        return nullptr;
    }
    gst_bin_add(GST_BIN(pipeline), d3d11h264dec);
    return d3d11h264dec;
}

GstElement* GstAVPipelineD3D11::add_d3d11convert(GstElement* pipeline)
{
    GstElement* d3d11convert = gst_element_factory_make("d3d11convert", nullptr);
    if (!d3d11convert)
    {
        Debug::Log("Failed to create d3d11convert", Level::Error);
        return nullptr;
    }
    gst_bin_add(GST_BIN(pipeline), d3d11convert);
    return d3d11convert;
}

GstElement* GstAVPipelineD3D11::add_appsink(GstElement* pipeline)
{
    GstElement* appsink = gst_element_factory_make("appsink", nullptr);
    if (!appsink)
    {
        Debug::Log("Failed to create appsink", Level::Error);
        return nullptr;
    }

    GstCaps* caps = gst_caps_from_string("video/x-raw(memory:D3D11Memory),format=RGBA");
    g_object_set(appsink, "caps", caps, "drop", true, "max-buffers", 3, nullptr);
    gst_caps_unref(caps);

    gst_bin_add(GST_BIN(pipeline), appsink);
    return appsink;
}

void GstAVPipelineD3D11::configure_videopad()
{
    GstElement* rtph264depay = add_rtph264depay(avpipeline->_pipeline);
    GstElement* h264parse = add_h264parse(avpipeline->_pipeline);
    GstElement* d3d11h264dec = add_d3d11h264dec(avpipeline->_pipeline);
    GstElement* d3d11convert = add_d3d11convert(avpipeline->_pipeline);
    GstElement* appsink = add_appsink(avpipeline->_pipeline);

    GstAppSinkCallbacks callbacks = {nullptr};
    callbacks.new_sample = on_new_sample;

    if (g_str_has_prefix(pad_name, "video_0"))
    {
        Debug::Log("Connecting left video pad " + std::string(pad_name));
        gst_app_sink_set_callbacks(GST_APP_SINK(appsink), &callbacks, avpipeline->_leftData.get(), nullptr);
    }
    else
    {
        Debug::Log("Connecting right video pad " + std::string(pad_name));
        gst_app_sink_set_callbacks(GST_APP_SINK(appsink), &callbacks, avpipeline->_rightData.get(), nullptr);
    }

    if (!gst_element_link_many(rtph264depay, h264parse, d3d11h264dec, d3d11convert, appsink, nullptr))
    {
        Debug::Log("Elements could not be linked.");
    }

    GstPad* sinkpad = gst_element_get_static_pad(rtph264depay, "sink");
    if (gst_pad_link(new_pad, sinkpad) != GST_PAD_LINK_OK)
    {
        Debug::Log("Could not link dynamic video pad to rtph264depay", Level::Error);
    }
    gst_object_unref(sinkpad);

    gst_element_sync_state_with_parent(rtph264depay);
    gst_element_sync_state_with_parent(h264parse);
    gst_element_sync_state_with_parent(d3d11h264dec);
    gst_element_sync_state_with_parent(d3d11convert);
    gst_element_sync_state_with_parent(appsink);
}

void GstAVPipelineD3D11::ReleaseTexture(ID3D11Texture2D* texture)
{
    if (texture != nullptr)
    {
        texture->Release();
        texture = nullptr;
    }
}

GstAVPipelineD3D11::GstAVPipelineD3D11(IUnityInterfaces* s_UnityInterfaces) : GstAVPipeline(s_UnityInterfaces)
{
    preloaded_plugins.push_back(gst_plugin_load_by_name("wasapi2"));
    if (!preloaded_plugins.back())
    {
        Debug::Log("Failed to load 'wasapi2' plugin", Level::Error);
    }
}

GstAVPipelineD3D11::~GstAVPipelineD3D11()
{
    gst_clear_object(&_device);
    gst_object_unref(_device);
    ~GstAVPipeline();
}

void GstAVPipelineD3D11::CreateDevice()
{
    if (_device == nullptr)
    {
        /* Find adapter LUID of render device, then create our device with the same
         * adapter */
        ComPtr<IDXGIDevice> dxgi_device;
        auto hr = _s_UnityInterfaces->Get<IUnityGraphicsD3D11>()->GetDevice()->QueryInterface(IID_PPV_ARGS(&dxgi_device));
        g_assert(SUCCEEDED(hr));

        ComPtr<IDXGIAdapter> adapter;
        hr = dxgi_device->GetAdapter(&adapter);
        g_assert(SUCCEEDED(hr));

        DXGI_ADAPTER_DESC adapter_desc;
        hr = adapter->GetDesc(&adapter_desc);
        g_assert(SUCCEEDED(hr));

        auto luid = gst_d3d11_luid_to_int64(&adapter_desc.AdapterLuid);

        /* This device will be used by our pipeline */
        _device = gst_d3d11_device_new_for_adapter_luid(luid, D3D11_CREATE_DEVICE_BGRA_SUPPORT);
        g_assert(_device);
    }
    else
    {
        Debug::Log("Device already created", Level::Warning);
    }
}

void GstAVPipelineD3D11::DestroyPipeline()
{
    GstAVPipeline::DestroyPipeline();

    if (_leftData != nullptr)
    {
        gst_clear_sample(&_leftData->last_sample);
        gst_clear_caps(&_leftData->last_caps);
        gst_clear_buffer(&_leftData->shared_buffer);
        gst_clear_object(&_leftData->conv);
        _leftData.reset(nullptr);
    }
    if (_rightData != nullptr)
    {
        gst_clear_sample(&_rightData->last_sample);
        gst_clear_caps(&_rightData->last_caps);
        gst_clear_buffer(&_rightData->shared_buffer);
        gst_clear_object(&_rightData->conv);
        _rightData.reset(nullptr);
    }
}

ID3D11Texture2D* GstAVPipelineD3D11::GetTexturePtr(bool left)
{
    if (left)
        return _leftData->texture.Get();
    else
        return _rightData->texture.Get();
}

void GstAVPipelineD3D11::busSyncHandler_context(GstMessage* msg)
{
    const gchar* ctx_type;
    if (!gst_message_parse_context_type(msg, &ctx_type))
        return;

    /* non-d3d11 context message is not interested */
    if (g_strcmp0(ctx_type, GST_D3D11_DEVICE_HANDLE_CONTEXT_TYPE) != 0)
        return;

    /* Pass our device to the message source element.
     * Otherwise pipeline will create another device */
    auto context = gst_d3d11_context_new(_device);
    gst_element_set_context(GST_ELEMENT(msg->src), context);
    gst_context_unref(context);
    return;
}