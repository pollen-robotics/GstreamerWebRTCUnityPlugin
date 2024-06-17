#include "GstAVPipeline.h"
#include "DebugLog.h"

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

    g_object_set(webrtcsrc, "stun-server", nullptr, nullptr);

    g_signal_connect(G_OBJECT(webrtcsrc), "pad-added", G_CALLBACK(on_pad_added), self);

    gst_bin_add(GST_BIN(pipeline), webrtcsrc);
    return webrtcsrc;
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

    g_object_set(webrtcsink, "stun-server", nullptr, nullptr);

    gst_bin_add(GST_BIN(pipeline), webrtcsink);
    return webrtcsink;
}

GstElement* GstAVPipeline::add_audiotestsrc(GstElement* pipeline)
{
    GstElement* audiotestsrc = gst_element_factory_make("audiotestsrc", nullptr);
    if (!audiotestsrc)
    {
        Debug::Log("Failed to create audiotestsrc", Level::Error);
        return nullptr;
    }

    g_object_set(audiotestsrc, "wave", "silence", "is-live", true, nullptr);

    gst_bin_add(GST_BIN(pipeline), audiotestsrc);
    return audiotestsrc;
}

GstElement* GstAVPipeline::add_audiomixer(GstElement* pipeline)
{
    GstElement* audiomixer = gst_element_factory_make("audiomixer", nullptr);
    if (!audiomixer)
    {
        Debug::Log("Failed to create audiomixer", Level::Error);
        return nullptr;
    }

    gst_bin_add(GST_BIN(pipeline), audiomixer);
    return audiomixer;
}

GstElement* GstAVPipeline::add_webrtcechoprobe(GstElement* pipeline)
{
    GstElement* webrtcechoprobe = gst_element_factory_make("webrtcechoprobe", nullptr);
    if (!webrtcechoprobe)
    {
        Debug::Log("Failed to create webrtcechoprobe", Level::Error);
        return nullptr;
    }

    gst_bin_add(GST_BIN(pipeline), webrtcechoprobe);
    return webrtcechoprobe;
}

GstElement* GstAVPipeline::add_webrtcdsp(GstElement* pipeline)
{
    GstElement* webrtcdsp = gst_element_factory_make("webrtcdsp", nullptr);
    if (!webrtcdsp)
    {
        Debug::Log("Failed to create webrtcdsp", Level::Error);
        return nullptr;
    }

    // ToDo: seems to cut user's voice
    g_object_set(webrtcdsp, "echo-cancel", false, nullptr);

    gst_bin_add(GST_BIN(pipeline), webrtcdsp);
    return webrtcdsp;
}

GstElement* GstAVPipeline::add_fakesink(GstElement* pipeline)
{
    GstElement* fakesink = gst_element_factory_make("fakesink", nullptr);
    if (!fakesink)
    {
        Debug::Log("Failed to create fakesink", Level::Error);
        return nullptr;
    }

    gst_bin_add(GST_BIN(pipeline), fakesink);
    return fakesink;
}

GstElement* GstAVPipeline::add_tee(GstElement* pipeline)
{
    GstElement* tee = gst_element_factory_make("tee", nullptr);
    if (!tee)
    {
        Debug::Log("Failed to create tee", Level::Error);
        return nullptr;
    }

    gst_bin_add(GST_BIN(pipeline), tee);
    return tee;
}

GstElement* GstAVPipeline::add_audiosink(GstAVPipeline* self)
{
    GstElement* audiosink = self->make_audiosink();
    if (!audiosink)
        gst_bin_add(GST_BIN(self->_pipeline), audiosink);

    return audiosink;
}

GstElement* GstAVPipeline::add_audiosrc(GstAVPipeline* self)
{
    GstElement* audiosrc = self->make_audiosrc();
    if (!audiosrc)
        gst_bin_add(GST_BIN(self->_pipeline), audiosrc);

    return audiosrc;
}

void GstAVPipeline::on_pad_added(GstElement* src, GstPad* new_pad, gpointer data)
{
    GstAVPipeline* avpipeline = static_cast<GstAVPipeline*>(data);

    gchar* pad_name = gst_pad_get_name(new_pad);
    Debug::Log("Adding pad ");
    if (g_str_has_prefix(pad_name, "video"))
    {
        Debug::Log("Adding video pad " + std::string(pad_name));
        avpipeline->configure_videopad();
    }
    else if (g_str_has_prefix(pad_name, "audio"))
    {
        Debug::Log("Adding audio pad " + std::string(pad_name));
        GstElement* rtpopusdepay = add_rtpopusdepay(avpipeline->_pipeline);
        GstElement* queue = add_queue(avpipeline->_pipeline);
        GstElement* opusdec = add_opusdec(avpipeline->_pipeline);
        GstElement* audioconvert = add_audioconvert(avpipeline->_pipeline);
        GstElement* audioresample = add_audioresample(avpipeline->_pipeline);
        GstElement* audiosink = add_audiosink(avpipeline);
        GstElement* tee = add_tee(avpipeline->_pipeline);

        if (!gst_element_link_many(rtpopusdepay, opusdec, tee, queue, audioconvert, audioresample, audiosink, nullptr))
        {
            Debug::Log("Audio elements could not be linked.", Level::Error);
        }

        GstElement* queue2 = add_queue(avpipeline->_pipeline);
        if (!gst_element_link_many(tee, queue2, avpipeline->audiomixer, nullptr))
            Debug::Log("Received audio could not be linked to audiomixer.", Level::Error);

        GstPad* sinkpad = gst_element_get_static_pad(rtpopusdepay, "sink");
        if (gst_pad_link(new_pad, sinkpad) != GST_PAD_LINK_OK)
        {
            Debug::Log("Could not link dynamic audio pad to rtpopusdepay", Level::Error);
        }
        gst_object_unref(sinkpad);

        gst_element_sync_state_with_parent(rtpopusdepay);
        gst_element_sync_state_with_parent(opusdec);
        gst_element_sync_state_with_parent(tee);
        gst_element_sync_state_with_parent(queue);
        gst_element_sync_state_with_parent(audioconvert);
        gst_element_sync_state_with_parent(audioresample);
        gst_element_sync_state_with_parent(audiosink);
        gst_element_sync_state_with_parent(queue2);
        gst_element_sync_state_with_parent(avpipeline->audiomixer);
    }
    g_free(pad_name);
}

void GstAVPipeline::webrtcbin_ready(GstElement* self, gchararray peer_id, GstElement* webrtcbin, gpointer udata)
{
    Debug::Log("Configure webrtcbin", Level::Info);
    g_object_set(webrtcbin, "latency", 50, nullptr);
}

GstAVPipeline::GstAVPipeline(IUnityInterfaces* s_UnityInterfaces) : _s_UnityInterfaces(s_UnityInterfaces)
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

    main_context_ = g_main_context_new();
    main_loop_ = g_main_loop_new(main_context_, FALSE);
}

GstAVPipeline::~GstAVPipeline()
{
    g_main_context_unref(main_context_);
    g_main_loop_unref(main_loop_);
    for (auto& plugin : preloaded_plugins)
    {
        gst_object_unref(plugin);
    }
    preloaded_plugins.clear();
}

gboolean GstAVPipeline::dumpLatencyCallback(GstAVPipeline* self)
{
    if (self)
    {
        GstQuery* query = gst_query_new_latency();
        gboolean res = gst_element_query(self->_pipeline, query);
        if (res)
        {
            gboolean live;
            GstClockTime min_latency, max_latency;
            gst_query_parse_latency(query, &live, &min_latency, &max_latency);
            std::string msg = "Pipeline latency: live=" + std::to_string(live) + ", min=" + std::to_string(min_latency) +
                              ", max=" + std::to_string(max_latency);
            Debug::Log(msg);
        }
        gst_query_unref(query);
        return true;
    }
    return false;
}

void GstAVPipeline::CreatePipeline(const char* uri, const char* remote_peer_id)
{
    Debug::Log("GstAVPipeline create pipeline", Level::Info);
    Debug::Log(uri, Level::Info);
    Debug::Log(remote_peer_id, Level::Info);

    _pipeline = gst_pipeline_new("Plugin AV Pipeline");

    GstElement* webrtcsrc = add_webrtcsrc(_pipeline, remote_peer_id, uri, this);
    // GstElement* wasapi2src = add_wasapi2src(_pipeline);
    GstElement* audiosrc = add_audiosrc(this);
    GstElement* webrtcdsp = add_webrtcdsp(_pipeline);
    GstElement* audioconvert = add_audioconvert(_pipeline);
    GstElement* queue = add_queue(_pipeline);
    GstElement* opusenc = add_opusenc(_pipeline);
    GstElement* audio_caps_capsfilter = add_audio_caps_capsfilter(_pipeline);
    GstElement* webrtcsink = add_webrtcsink(_pipeline, uri);

    if (!gst_element_link_many(audiosrc, audioconvert, webrtcdsp, queue, opusenc, audio_caps_capsfilter, webrtcsink, nullptr))
    {
        Debug::Log("Audio sending elements could not be linked.", Level::Error);
    }

    GstElement* audiotestsrc = add_audiotestsrc(_pipeline);
    audiomixer = add_audiomixer(_pipeline);
    GstElement* audioconvert2 = add_audioconvert(_pipeline);
    GstElement* audioresample = add_audioresample(_pipeline);
    GstElement* webrtcechoprobe = add_webrtcechoprobe(_pipeline);
    GstElement* fakesink = add_fakesink(_pipeline);

    if (!gst_element_link_many(audiotestsrc, audiomixer, audioresample, webrtcechoprobe, fakesink, nullptr))
    {
        Debug::Log("Audio dsp elements could not be linked.", Level::Error);
    }

    // g_timeout_add_seconds(3, G_SOURCE_FUNC(GstAVPipeline::dumpLatencyCallback), this);

    thread_ = g_thread_new("bus thread", main_loop_func, this);
    if (!thread_)
    {
        Debug::Log("Failed to create GLib main thread", Level::Error);
    }
}

void GstAVPipeline::DestroyPipeline()
{
    if (main_loop_ != nullptr)
        g_main_loop_quit(main_loop_);

    if (thread_ != nullptr)
    {
        g_thread_join(thread_);
        thread_ = nullptr;
    }

    if (_pipeline != nullptr)
    {
        Debug::Log("GstAVPipeline pipeline released", Level::Info);
        gst_object_unref(_pipeline);
        _pipeline = nullptr;
    }
    else
    {
        Debug::Log("GstAVPipeline pipeline already released", Level::Warning);
    }
}

gpointer GstAVPipeline::main_loop_func(gpointer data)
{
    Debug::Log("Entering main loop");
    GstAVPipeline* self = static_cast<GstAVPipeline*>(data);

    g_main_context_push_thread_default(self->main_context_);

    GstBus* bus = gst_element_get_bus(self->_pipeline);
    gst_bus_add_watch(bus, busHandler, self);
    gst_bus_set_sync_handler(bus, busSyncHandler, self, nullptr);

    auto state = gst_element_set_state(self->_pipeline, GstState::GST_STATE_PLAYING);
    if (state == GstStateChangeReturn::GST_STATE_CHANGE_FAILURE)
    {
        Debug::Log("Cannot set pipeline to playing state", Level::Error);
        gst_object_unref(self->_pipeline);
        self->_pipeline = nullptr;
        return nullptr;
    }

    g_main_loop_run(self->main_loop_);

    gst_element_set_state(self->_pipeline, GST_STATE_NULL);

    gst_bus_set_sync_handler(bus, nullptr, nullptr, nullptr);
    gst_bus_remove_watch(bus);
    gst_object_unref(bus);
    g_main_context_pop_thread_default(self->main_context_);
    Debug::Log("Quitting main loop");

    return nullptr;
}

gboolean GstAVPipeline::busHandler(GstBus* bus, GstMessage* msg, gpointer data)
{
    auto self = (GstAVPipeline*)data;

    switch (GST_MESSAGE_TYPE(msg))
    {
        case GST_MESSAGE_ERROR:
        {
            GError* err;
            gchar* dbg;

            gst_message_parse_error(msg, &err, &dbg);
            // gst_printerrln("ERROR %s", err->message);
            Debug::Log(err->message, Level::Error);
            if (dbg != nullptr)
                Debug::Log(dbg);
            // gst_printerrln("ERROR debug information: %s", dbg);
            g_clear_error(&err);
            g_free(dbg);
            g_main_loop_quit(self->main_loop_);
            break;
        }
        case GST_MESSAGE_EOS:
            Debug::Log("Got EOS");
            g_main_loop_quit(self->main_loop_);
            break;
        case GST_MESSAGE_LATENCY:
        {
            Debug::Log("Redistribute latency...");
            gst_bin_recalculate_latency(GST_BIN(self->_pipeline));
            GstAVPipeline::dumpLatencyCallback(self);
            break;
        }
        default:
            Debug::Log(GST_MESSAGE_TYPE_NAME(msg));
            break;
    }

    return G_SOURCE_CONTINUE;
}

GstBusSyncReply GstAVPipeline::busSyncHandler(GstBus* bus, GstMessage* msg, gpointer user_data)
{
    auto self = (GstAVPipeline*)user_data;

    switch (GST_MESSAGE_TYPE(msg))
    {
        case GST_MESSAGE_NEED_CONTEXT:
        {
            self->busSyncHandler_context(msg);
            break;
        }
        default:
            break;
    }
    return GST_BUS_PASS;
}