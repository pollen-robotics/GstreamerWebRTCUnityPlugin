/* Copyright(c) Pollen Robotics, all rights reserved.
 This source code is licensed under the license found in the
 LICENSE file in the root directory of this source tree. */

#include "GstAVPipeline.h"
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
ID3D11Texture2D* GstAVPipeline::CreateTexture(unsigned int width, unsigned int height, bool left)
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

    ComPtr<ID3D11Texture2D> texture;
    hr = device->CreateTexture2D(&desc, nullptr, &texture);
    g_assert(SUCCEEDED(hr));

    hr = texture.As(&data->keyed_mutex);
    g_assert(SUCCEEDED(hr));

    hr = data->keyed_mutex->AcquireSync(0, INFINITE);
    g_assert(SUCCEEDED(hr));

    ComPtr<IDXGIResource1> dxgi_resource;
    hr = texture.As(&dxgi_resource);
    g_assert(SUCCEEDED(hr));

    HANDLE shared_handle = nullptr;
    hr = dxgi_resource->CreateSharedHandle(nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, nullptr,
                                           &shared_handle);
    g_assert(SUCCEEDED(hr));

    auto gst_device = gst_d3d11_device_get_device_handle(_device);
    ComPtr<ID3D11Device1> device1;
    hr = gst_device->QueryInterface(IID_PPV_ARGS(&device1));
    g_assert(SUCCEEDED(hr));

    /* if (pDebug == nullptr)
    {
        hr = device1->QueryInterface(IID_PPV_ARGS(&pDebug));
        g_assert(SUCCEEDED(hr));
    }*/
    
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
    return texture.Get();
}

GstFlowReturn GstAVPipeline::on_new_sample(GstAppSink* appsink, gpointer user_data)
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

void GstAVPipeline::Draw(bool left)
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

GstElement* GstAVPipeline::add_rtph264depay(GstElement* pipeline)
{
    GstElement* rtph264depay = gst_element_factory_make("rtph264depay", nullptr);
    if (!rtph264depay)
    {
        Debug::Log("Failed to create rtph264depay", Level::Error);
        return nullptr;
    }
    gst_bin_add(GST_BIN(pipeline), rtph264depay);
    return rtph264depay;
}

GstElement* GstAVPipeline::add_h264parse(GstElement* pipeline)
{
    GstElement* h264parse = gst_element_factory_make("h264parse", nullptr);
    if (!h264parse)
    {
        Debug::Log("Failed to create h264parse", Level::Error);
        return nullptr;
    }
    gst_bin_add(GST_BIN(pipeline), h264parse);
    return h264parse;
}

GstElement* GstAVPipeline::add_d3d11h264dec(GstElement* pipeline)
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

GstElement* GstAVPipeline::add_d3d11convert(GstElement* pipeline)
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

GstElement* GstAVPipeline::add_appsink(GstElement* pipeline)
{
    GstElement* appsink = gst_element_factory_make("appsink", nullptr);
    if (!appsink)
    {
        Debug::Log("Failed to create appsink", Level::Error);
        return nullptr;
    }

    GstCaps* caps = gst_caps_from_string("video/x-raw(memory:D3D11Memory),format=RGBA");
    g_object_set(appsink, "caps", caps, "drop", true, "max-buffers", 1, "processing-deadline", 0, nullptr);
    gst_caps_unref(caps);

    gst_bin_add(GST_BIN(pipeline), appsink);
    return appsink;
}

GstElement* GstAVPipeline::add_rtpopusdepay(GstElement* pipeline)
{
    GstElement* rtpopusdepay = gst_element_factory_make("rtpopusdepay", nullptr);
    if (!rtpopusdepay)
    {
        Debug::Log("Failed to create rtpopusdepay", Level::Error);
        return nullptr;
    }

    gst_bin_add(GST_BIN(pipeline), rtpopusdepay);
    return rtpopusdepay;
}

GstElement* GstAVPipeline::add_queue(GstElement* pipeline)
{
    GstElement* queue = gst_element_factory_make("queue", nullptr);
    if (!queue)
    {
        Debug::Log("Failed to create queue", Level::Error);
        return nullptr;
    }

    gst_bin_add(GST_BIN(pipeline), queue);
    return queue;
}

GstElement* GstAVPipeline::add_opusdec(GstElement* pipeline)
{
    GstElement* opusdec = gst_element_factory_make("opusdec", nullptr);
    if (!opusdec)
    {
        Debug::Log("Failed to create opusdec", Level::Error);
        return nullptr;
    }

    gst_bin_add(GST_BIN(pipeline), opusdec);
    return opusdec;
}

GstElement* GstAVPipeline::add_audioconvert(GstElement* pipeline)
{
    GstElement* audioconvert = gst_element_factory_make("audioconvert", nullptr);
    if (!audioconvert)
    {
        Debug::Log("Failed to create audioconvert", Level::Error);
        return nullptr;
    }

    gst_bin_add(GST_BIN(pipeline), audioconvert);
    return audioconvert;
}

GstElement* GstAVPipeline::add_audioresample(GstElement* pipeline)
{
    GstElement* audioresample = gst_element_factory_make("audioresample", nullptr);
    if (!audioresample)
    {
        Debug::Log("Failed to create audioresample", Level::Error);
        return nullptr;
    }

    gst_bin_add(GST_BIN(pipeline), audioresample);
    return audioresample;
}

GstElement* GstAVPipeline::add_wasapi2sink(GstElement* pipeline)
{
    GstElement* wasapi2sink = gst_element_factory_make("wasapi2sink", nullptr);
    if (!wasapi2sink)
    {
        Debug::Log("Failed to create wasapi2sink", Level::Error);
        return nullptr;
    }
    g_object_set(wasapi2sink, "low-latency", true, "provide-clock", false, "processing-deadline", 0, nullptr);

    gst_bin_add(GST_BIN(pipeline), wasapi2sink);
    return wasapi2sink;
}

GstElement* GstAVPipeline::add_webrtcsrc(GstElement* pipeline, const std::string& remote_peer_id, const std::string& uri,
                                         GstAVPipeline* self)
{
    GstElement* webrtcsrc = gst_element_factory_make("webrtcsrc", nullptr);
    if (!webrtcsrc)
    {
        Debug::Log("Failed to create webrtcsrc", Level::Error);
        return nullptr;
    }

    GObject* signaller;
    g_object_get(webrtcsrc, "signaller", &signaller, nullptr);
    if (signaller)
    {
        g_object_set(signaller, "producer-peer-id", remote_peer_id.c_str(), "uri", uri.c_str(), nullptr);
        g_signal_connect(G_OBJECT(signaller), "webrtcbin-ready", G_CALLBACK(webrtcbin_ready), self);
        g_object_unref(signaller); // Unref signaller when done
    }
    else
    {
        Debug::Log("Failed to get signaller property from webrtcsrc.", Level::Error);
    }

    g_object_set(webrtcsrc, "stun-server", nullptr, "do-retransmission", false, nullptr);

    g_signal_connect(G_OBJECT(webrtcsrc), "pad-added", G_CALLBACK(on_pad_added), self);

    gst_bin_add(GST_BIN(pipeline), webrtcsrc);
    return webrtcsrc;
}

GstElement* GstAVPipeline::add_wasapi2src(GstElement* pipeline)
{
    GstElement* wasapi2src = gst_element_factory_make("wasapi2src", nullptr);
    if (!wasapi2src)
    {
        Debug::Log("Failed to create wasapi2src", Level::Error);
        return nullptr;
    }
    g_object_set(wasapi2src, "low-latency", true, "provide-clock", false, nullptr);

    gst_bin_add(GST_BIN(pipeline), wasapi2src);
    return wasapi2src;
}

GstElement* GstAVPipeline::add_opusenc(GstElement* pipeline)
{
    GstElement* opusenc = gst_element_factory_make("opusenc", nullptr);
    if (!opusenc)
    {
        Debug::Log("Failed to create opusenc", Level::Error);
        return nullptr;
    }

    g_object_set(opusenc, "audio-type", "restricted-lowdelay", /* "frame-size", 10,*/ nullptr);

    gst_bin_add(GST_BIN(pipeline), opusenc);
    return opusenc;
}

GstElement* GstAVPipeline::add_audio_caps_capsfilter(GstElement* pipeline)
{
    GstElement* audio_caps_capsfilter = gst_element_factory_make("capsfilter", nullptr);
    if (!audio_caps_capsfilter)
    {
        Debug::Log("Failed to create capsfilter", Level::Error);
        return nullptr;
    }

    GstCaps* audio_caps = gst_caps_from_string("audio/x-opus");
    gst_caps_set_simple(audio_caps, "channels", G_TYPE_INT, 1, "rate", G_TYPE_INT, 48000, nullptr);
    g_object_set(audio_caps_capsfilter, "caps", audio_caps, nullptr);

    gst_bin_add(GST_BIN(pipeline), audio_caps_capsfilter);
    gst_caps_unref(audio_caps);
    return audio_caps_capsfilter;
}

void GstAVPipeline::consumer_added_callback(GstElement* consumer_id, gchararray webrtcbin, GstElement* arg1, gpointer udata)
{
    Debug::Log("Consumer added");
    GstIterator* sinks = gst_bin_iterate_sinks(GST_BIN(consumer_id));
    gboolean done = FALSE;
    while (!done)
    {
        GValue item = G_VALUE_INIT;
        switch (gst_iterator_next(sinks, &item))
        {
            case GST_ITERATOR_OK:
            {
                GstElement* sink = GST_ELEMENT(g_value_get_object(&item));

                // Log a message indicating that the processing deadline is being set for the sink
                std::string name = GST_ELEMENT_NAME(sink);
                Debug::Log("Setting processing deadline for " + name);

                // Set the processing deadline for the sink
                g_object_set(sink, "processing-deadline", 1000000, nullptr);

                // Unref the sink to free its resources
                g_object_unref(sink);

                // Free the item value
                g_value_unset(&item);

                break;
            }
            case GST_ITERATOR_RESYNC:
                // Resync the iterator
                gst_iterator_resync(sinks);
                break;
            case GST_ITERATOR_ERROR:
                // Handle the error
                g_warning("Error iterating sinks");
                done = TRUE;
                break;
            case GST_ITERATOR_DONE:
                // We're done iterating
                done = TRUE;
                break;
        }
    }

    // Free the iterator
    gst_iterator_free(sinks);
}

GstElement* GstAVPipeline::add_webrtcsink(GstElement* pipeline, const std::string& uri)
{
    GstElement* webrtcsink = gst_element_factory_make("webrtcsink", nullptr);
    if (!webrtcsink)
    {
        Debug::Log("Failed to create webrtcsink", Level::Error);
        return nullptr;
    }

    GObject* signaller;
    g_object_get(webrtcsink, "signaller", &signaller, nullptr);
    if (signaller)
    {
        g_object_set(signaller, "uri", uri.c_str(), nullptr);
        g_object_unref(signaller); // Unref signaller when done
    }
    else
    {
        Debug::Log("Failed to get signaller property from webrtcsink.", Level::Error);
    }

    GstStructure* meta_structure = gst_structure_new_empty("meta");
    gst_structure_set(meta_structure, "name", G_TYPE_STRING, "UnityClient", nullptr);
    GValue meta_value = G_VALUE_INIT;
    g_value_init(&meta_value, GST_TYPE_STRUCTURE);
    gst_value_set_structure(&meta_value, meta_structure);
    g_object_set_property(G_OBJECT(webrtcsink), "meta", &meta_value);
    gst_structure_free(meta_structure);
    g_value_unset(&meta_value);

    g_object_set(webrtcsink, "stun-server", nullptr, "do-restransmission", false, nullptr);

    //g_signal_connect(webrtcsink, "consumer-added", G_CALLBACK(consumer_added_callback), nullptr);

    gst_bin_add(GST_BIN(pipeline), webrtcsink);
    return webrtcsink;
}


GstElement* GstAVPipeline::add_webrtcdsp(GstElement* pipeline)
{
    GstElement* webrtcdsp = gst_element_factory_make("webrtcdsp", nullptr);
    if (!webrtcdsp)
    {
        Debug::Log("Failed to create webrtcdsp", Level::Error);
        return nullptr;
    }

    // Echo cancel done on the robot side
    g_object_set(webrtcdsp, "echo-cancel", false, nullptr);

    gst_bin_add(GST_BIN(pipeline), webrtcdsp);
    return webrtcdsp;
}

void GstAVPipeline::on_pad_added(GstElement* src, GstPad* new_pad, gpointer data)
{
    GstAVPipeline* avpipeline = static_cast<GstAVPipeline*>(data);

    gchar* pad_name = gst_pad_get_name(new_pad);
    Debug::Log("Adding pad ");
    if (g_str_has_prefix(pad_name, "video"))
    {
        Debug::Log("Adding video pad " + std::string(pad_name));
        GstElement* rtph264depay = add_rtph264depay(avpipeline->pipeline_);
        GstElement* h264parse = add_h264parse(avpipeline->pipeline_);
        GstElement* d3d11h264dec = add_d3d11h264dec(avpipeline->pipeline_);
        GstElement* d3d11convert = add_d3d11convert(avpipeline->pipeline_);
        GstElement* appsink = add_appsink(avpipeline->pipeline_);

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
    else if (g_str_has_prefix(pad_name, "audio"))
    {
        Debug::Log("Adding audio pad " + std::string(pad_name));
        GstElement* rtpopusdepay = add_rtpopusdepay(avpipeline->pipeline_);
        GstElement* queue = add_queue(avpipeline->pipeline_);
        GstElement* opusdec = add_opusdec(avpipeline->pipeline_);
        GstElement* audioconvert = add_audioconvert(avpipeline->pipeline_);
        GstElement* audioresample = add_audioresample(avpipeline->pipeline_);
        GstElement* wasapi2sink = add_wasapi2sink(avpipeline->pipeline_);
       // GstElement* tee = add_tee(avpipeline->_pipeline);

        if (!gst_element_link_many(rtpopusdepay, opusdec, /* tee,*/ queue, audioconvert, audioresample, wasapi2sink, nullptr))
        {
            Debug::Log("Audio elements could not be linked.", Level::Error);
        }

        /* GstElement* queue2 = add_queue(avpipeline->_pipeline);
        if (!gst_element_link_many(tee, queue2, avpipeline->audiomixer, nullptr))
            Debug::Log("Received audio could not be linked to audiomixer.", Level::Error);*/

        GstPad* sinkpad = gst_element_get_static_pad(rtpopusdepay, "sink");
        if (gst_pad_link(new_pad, sinkpad) != GST_PAD_LINK_OK)
        {
            Debug::Log("Could not link dynamic audio pad to rtpopusdepay", Level::Error);
        }
        gst_object_unref(sinkpad);

        gst_element_sync_state_with_parent(rtpopusdepay);
        gst_element_sync_state_with_parent(opusdec);
        //gst_element_sync_state_with_parent(tee);
        gst_element_sync_state_with_parent(queue);
        gst_element_sync_state_with_parent(audioconvert);
        gst_element_sync_state_with_parent(audioresample);
        gst_element_sync_state_with_parent(wasapi2sink);
        //gst_element_sync_state_with_parent(queue2);
        //gst_element_sync_state_with_parent(avpipeline->audiomixer);
    }
    g_free(pad_name);
}

void GstAVPipeline::webrtcbin_ready(GstElement* self, gchararray peer_id, GstElement* webrtcbin, gpointer udata)
{
    Debug::Log("Configure webrtcbin", Level::Info);
    g_object_set(webrtcbin, "latency", 10, nullptr);
}

void GstAVPipeline::ReleaseTexture(ID3D11Texture2D* texture)
{
    if (texture != nullptr)
    {  
        texture->Release();
        texture = nullptr;        
    }
}

GstAVPipeline::GstAVPipeline(IUnityInterfaces* s_UnityInterfaces) : GstBasePipeline("AVPipeline"), _s_UnityInterfaces(s_UnityInterfaces)
{
    preloaded_plugins.push_back(gst_plugin_load_by_name("rswebrtc"));
    if (!preloaded_plugins.back())
    {
        Debug::Log("Failed to load 'rswebrtc' plugin", Level::Error);
    }
    preloaded_plugins.push_back(gst_plugin_load_by_name("webrtc"));
    if (!preloaded_plugins.back())
    {
        Debug::Log("Failed to load 'webrtc' plugin", Level::Error);
    }
    preloaded_plugins.push_back(gst_plugin_load_by_name("webrtcdsp"));
    if (!preloaded_plugins.back())
    {
        Debug::Log("Failed to load 'webrtcdsp' plugin", Level::Error);
    }
    preloaded_plugins.push_back(gst_plugin_load_by_name("d3d11"));
    if (!preloaded_plugins.back())
    {
        Debug::Log("Failed to load 'd3d11' plugin", Level::Error);
    }
    preloaded_plugins.push_back(gst_plugin_load_by_name("rtpmanager"));
    if (!preloaded_plugins.back())
    {
        Debug::Log("Failed to load 'rtpmanager' plugin", Level::Error);
    }
    preloaded_plugins.push_back(gst_plugin_load_by_name("opus"));
    if (!preloaded_plugins.back())
    {
        Debug::Log("Failed to load 'opus' plugin", Level::Error);
    }
    preloaded_plugins.push_back(gst_plugin_load_by_name("wasapi2"));
    if (!preloaded_plugins.back())
    {
        Debug::Log("Failed to load 'wasapi2' plugin", Level::Error);
    }
    preloaded_plugins.push_back(gst_plugin_load_by_name("dtls"));
    if (!preloaded_plugins.back())
    {
        Debug::Log("Failed to load 'dtls' plugin", Level::Error);
    }
    preloaded_plugins.push_back(gst_plugin_load_by_name("srtp"));
    if (!preloaded_plugins.back())
    {
        Debug::Log("Failed to load 'srtp' plugin", Level::Error);
    }
}

GstAVPipeline::~GstAVPipeline()
{
    gst_clear_object(&_device);
    gst_object_unref(_device);

    for (auto& plugin : preloaded_plugins)
    {
        gst_object_unref(plugin);
    }
    preloaded_plugins.clear();
}

void GstAVPipeline::CreatePipeline(const char* uri, const char* remote_peer_id)
{
    Debug::Log("GstAVPipeline create pipeline", Level::Info);
    Debug::Log(uri, Level::Info);
    Debug::Log(remote_peer_id, Level::Info);

    GstBasePipeline::CreatePipeline();

    GstElement* webrtcsrc = add_webrtcsrc(pipeline_, remote_peer_id, uri, this);
    GstElement* wasapi2src = add_wasapi2src(pipeline_);
    GstElement* webrtcdsp = add_webrtcdsp(pipeline_);
    GstElement* audioconvert = add_audioconvert(pipeline_);
    GstElement* queue = add_queue(pipeline_);
    GstElement* opusenc = add_opusenc(pipeline_);
    GstElement* audio_caps_capsfilter = add_audio_caps_capsfilter(pipeline_);
    GstElement* webrtcsink = add_webrtcsink(pipeline_, uri);

    if (!gst_element_link_many(wasapi2src, audioconvert, webrtcdsp, queue, opusenc, audio_caps_capsfilter, webrtcsink,
                               nullptr))
    {
        Debug::Log("Audio sending elements could not be linked.", Level::Error);
    }

    CreateBusThread();
}

void GstAVPipeline::CreateDevice()
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
        _device =
            gst_d3d11_device_new_for_adapter_luid(luid, D3D11_CREATE_DEVICE_BGRA_SUPPORT /* | D3D11_CREATE_DEVICE_DEBUG*/);
        g_assert(_device);
    }
    else
    {
        Debug::Log("Device already created", Level::Warning);
    }
}

void GstAVPipeline::DestroyPipeline()
{
    GstBasePipeline::DestroyPipeline();
    
    if (_leftData != nullptr)
    {
        gst_clear_sample(&_leftData->last_sample);
        gst_clear_caps(&_leftData->last_caps);
        gst_clear_buffer(&_leftData->shared_buffer);
        gst_clear_object(&_leftData->conv);
        _leftData->keyed_mutex = nullptr;
    }
    if (_rightData != nullptr)
    {
        gst_clear_sample(&_rightData->last_sample);
        gst_clear_caps(&_rightData->last_caps);
        gst_clear_buffer(&_rightData->shared_buffer);
        gst_clear_object(&_rightData->conv);
        _rightData->keyed_mutex = nullptr;
    }

    //pDebug->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL | D3D11_RLDO_IGNORE_INTERNAL);
    //pDebug = nullptr;
}

GstBusSyncReply GstAVPipeline::busSyncHandler(GstBus* bus, GstMessage* msg, gpointer user_data)
{
    auto self = (GstAVPipeline*)user_data;

    switch (GST_MESSAGE_TYPE(msg))
    {
        case GST_MESSAGE_NEED_CONTEXT:
        {
            const gchar* ctx_type;
            if (!gst_message_parse_context_type(msg, &ctx_type))
                break;

            /* non-d3d11 context message is not interested */
            if (g_strcmp0(ctx_type, GST_D3D11_DEVICE_HANDLE_CONTEXT_TYPE) != 0)
                break;

            /* Pass our device to the message source element.
             * Otherwise pipeline will create another device */
            auto context = gst_d3d11_context_new(self->_device);
            gst_element_set_context(GST_ELEMENT(msg->src), context);
            gst_context_unref(context);
            break;
        }
        default:
            break;
    }
    return GST_BUS_PASS;
}