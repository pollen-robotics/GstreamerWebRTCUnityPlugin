/* Copyright(c) Pollen Robotics, all rights reserved.
 This source code is licensed under the license found in the
 LICENSE file in the root directory of this source tree. */

#include "GstAVPipeline.h"
#include "DebugLog.h"

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

    g_signal_connect(G_OBJECT(webrtcsrc), "pad-added", G_CALLBACK(on_pad_added_wrapper), self);

    gst_bin_add(GST_BIN(pipeline), webrtcsrc);
    return webrtcsrc;
}

void GstAVPipeline::webrtcbin_ready(GstElement* self, gchararray peer_id, GstElement* webrtcbin, gpointer udata)
{
    Debug::Log("Configure webrtcbin", Level::Info);
    g_object_set(webrtcbin, "latency", 1, nullptr);
}

GstAVPipeline::GstAVPipeline(IUnityInterfaces* s_UnityInterfaces)
    : GstBasePipeline("AVPipeline"), _s_UnityInterfaces(s_UnityInterfaces)
{
    // preload plugins before Unity XR plugin
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
    preloaded_plugins.push_back(gst_plugin_load_by_name("rtpmanager"));
    if (!preloaded_plugins.back())
    {
        Debug::Log("Failed to load 'rtpmanager' plugin", Level::Error);
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

    CreateBusThread();
}

void GstAVPipeline::on_pad_added_wrapper(GstElement* src, GstPad* new_pad, gpointer data)
{
    GstAVPipeline* self = static_cast<GstAVPipeline*>(data);
    return self->on_pad_added(src, new_pad, data);
}