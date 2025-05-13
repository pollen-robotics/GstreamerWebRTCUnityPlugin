/* Copyright(c) Pollen Robotics, all rights reserved.
 This source code is licensed under the license found in the
 LICENSE file in the root directory of this source tree. */

#include "GstMicPipeline.h"
#include "DebugLog.h"

GstMicPipeline::GstMicPipeline() : GstBasePipeline("MicPipeline") {}

void GstMicPipeline::CreatePipeline(const char* uri, const char* remote_peer_id)
{
    Debug::Log("GstMicPipeline create pipeline", Level::Info);
    Debug::Log(uri, Level::Info);
    Debug::Log(remote_peer_id, Level::Info);

    GstBasePipeline::CreatePipeline();

#if UNITY_WIN
    GstElement* audiosrc = add_wasapi2src(pipeline_);
#elif UNITY_ANDROID
    GstElement* audiosrc = add_openslessrc(pipeline_);
#endif
    GstElement* webrtcdsp = add_webrtcdsp(pipeline_);
    GstElement* audioconvert = add_by_name(pipeline_, "audioconvert");
    GstElement* audioresample = add_by_name(pipeline_, "audioresample");
    GstElement* queue = add_by_name(pipeline_, "queue");
    GstElement* opusenc = add_opusenc(pipeline_);
    GstElement* audio_caps_capsfilter = add_audio_caps_capsfilter(pipeline_);
    GstElement* webrtcsink = add_webrtcsink(pipeline_, uri);

    if (!gst_element_link_many(audiosrc, queue, audioconvert, audioresample, webrtcdsp, opusenc, audio_caps_capsfilter,
                               webrtcsink, nullptr))
    {
        Debug::Log("Audio sending elements could not be linked.", Level::Error);
    }

    CreateBusThread();
}

#if UNITY_WIN
GstElement* GstMicPipeline::add_wasapi2src(GstElement* pipeline)
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
#elif UNITY_ANDROID
GstElement* GstMicPipeline::add_openslessrc(GstElement* pipeline)
{
    GstElement* openslessrc = gst_element_factory_make("openslessrc", nullptr);
    if (!openslessrc)
    {
        Debug::Log("Failed to create openslessrc", Level::Error);
        return nullptr;
    }
    // g_object_set(openslessrc, "buffer-time", 40000, nullptr);

    gst_bin_add(GST_BIN(pipeline), openslessrc);
    return openslessrc;
}
#endif

GstElement* GstMicPipeline::add_opusenc(GstElement* pipeline)
{
    GstElement* opusenc = gst_element_factory_make("opusenc", nullptr);
    if (!opusenc)
    {
        Debug::Log("Failed to create opusenc", Level::Error);
        return nullptr;
    }

    g_object_set(opusenc, "audio-type", /*"restricted-lowdelay"*/ 2051, "frame-size", 10, nullptr);

    gst_bin_add(GST_BIN(pipeline), opusenc);
    return opusenc;
}

GstElement* GstMicPipeline::add_audio_caps_capsfilter(GstElement* pipeline)
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

GstElement* GstMicPipeline::add_webrtcsink(GstElement* pipeline, const std::string& uri)
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

    g_object_set(webrtcsink, "stun-server", nullptr, "do-retransmission", false, nullptr);

    g_signal_connect(webrtcsink, "consumer-added", G_CALLBACK(consumer_added_callback), nullptr);

    gst_bin_add(GST_BIN(pipeline), webrtcsink);
    return webrtcsink;
}

GstElement* GstMicPipeline::add_webrtcdsp(GstElement* pipeline)
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

void GstMicPipeline::consumer_added_callback(GstElement* consumer_id, gchararray webrtcbin, GstElement* arg1, gpointer udata)
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