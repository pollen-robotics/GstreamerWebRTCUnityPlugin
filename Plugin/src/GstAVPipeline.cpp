/* Copyright(c) Pollen Robotics, all rights reserved.
 This source code is licensed under the license found in the
 LICENSE file in the root directory of this source tree. */

#include "GstAVPipeline.h"
#include "DebugLog.h"

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

void GstAVPipeline::CreatePipeline(/*const char* uri, const char* remote_peer_id*/)
{
    Debug::Log("GstAVPipeline create pipeline", Level::Info);
    /*Debug::Log(uri, Level::Info);
    Debug::Log(remote_peer_id, Level::Info);*/

    GstBasePipeline::CreatePipeline();

    // GstElement* webrtcsrc = add_webrtcsrc(pipeline_, remote_peer_id, uri, this);

    createCustomPipeline();

    CreateBusThread();
}

/*void GstAVPipeline::on_pad_added_wrapper(GstElement* src, GstPad* new_pad, gpointer data)
{
    GstAVPipeline* self = static_cast<GstAVPipeline*>(data);
    return self->on_pad_added(src, new_pad, data);
}*/
