#include "GstDataPipeline.h"
#include "DebugLog.h"
#include <gst/sdp/sdp.h>
#include <gst/webrtc/webrtc.h>

GstDataPipeline::GstDataPipeline()
{
    main_context_ = g_main_context_new();
    main_loop_ = g_main_loop_new(main_context_, FALSE);
}

GstDataPipeline::~GstDataPipeline()
{
    g_main_context_unref(main_context_);
    g_main_loop_unref(main_loop_);
}

void GstDataPipeline::CreatePipeline() 
{
    Debug::Log("GstDataPipeline create pipeline", Level::Info);
    _pipeline = gst_pipeline_new("Plugin Data Pipeline");

    _webrtcbin = add_webrtcbin();
    auto state = gst_element_set_state(_pipeline, GstState::GST_STATE_READY);    

    thread_ = g_thread_new("bus thread", main_loop_func, this);
    if (!thread_)
    {
        Debug::Log("Failed to create GLib main thread", Level::Error);
    }
}

void GstDataPipeline::DestroyPipeline()
{
    gst_webrtc_data_channel_close(_channel_service);
    gst_webrtc_data_channel_close(_channel_command);
    gst_webrtc_data_channel_close(_channel_audit);

    if (main_loop_ != nullptr)
        g_main_loop_quit(main_loop_);

    if (thread_ != nullptr)
    {
        Debug::Log("Wait for data thread to close ...", Level::Info);
        g_thread_join(thread_);
        g_thread_unref(thread_);
        thread_ = nullptr;
    }

     if (_pipeline != nullptr)
    {
        Debug::Log("GstDataPipeline pipeline released", Level::Info);
        gst_object_unref(_pipeline);
        _pipeline = nullptr;
    }
    else
    {
        Debug::Log("GstDataPipeline pipeline already released", Level::Warning);
    }
}

void GstDataPipeline::SetOffer(const char* sdp_offer) 
{
    Debug::Log("SDP Offer: " + std::string(sdp_offer));
    GstSDPMessage* sdpmsg = nullptr;
    gst_sdp_message_new_from_text(sdp_offer, &sdpmsg);

    GstWebRTCSDPType sdp_type = GST_WEBRTC_SDP_TYPE_OFFER;
    GstWebRTCSessionDescription* offer = gst_webrtc_session_description_new(sdp_type, sdpmsg);


    GstPromise* promise =
        gst_promise_new_with_change_func(on_offer_set, _webrtcbin, nullptr);

    g_signal_emit_by_name(_webrtcbin, "set-remote-description", offer, promise, nullptr);

    //gst_webrtc_session_description_free(offer); //raise breakpoint?
    gst_sdp_message_free(sdpmsg);
    gst_promise_unref(promise);
}

void GstDataPipeline::SetICECandidate(const char* candidate, int mline_index) 
{
    Debug::Log("Add ICE Candidate: " + std::string(candidate) + " "+ std::to_string(mline_index));
    g_signal_emit_by_name(_webrtcbin, "add-ice-candidate", mline_index, candidate);
}

void GstDataPipeline::on_offer_set(GstPromise* promise, gpointer user_data)
{
    g_assert(gst_promise_wait(promise) == GST_PROMISE_RESULT_REPLIED);

    GstElement* webrtc = GST_ELEMENT(user_data);

    promise = gst_promise_new_with_change_func(on_answer_created, webrtc, nullptr);
    g_signal_emit_by_name(webrtc, "create-answer", nullptr, promise, nullptr);
    gst_promise_unref(promise);
}


void GstDataPipeline::on_answer_created(GstPromise* promise, gpointer user_data)
{
    Debug::Log("create answer");
    g_assert(gst_promise_wait(promise) == GST_PROMISE_RESULT_REPLIED);

    GstElement* webrtc = GST_ELEMENT(user_data);

    const GstStructure* reply = gst_promise_get_reply(promise);
    GstWebRTCSessionDescription* answer = nullptr;
    gst_structure_get(reply, "answer", GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &answer, nullptr);
    gst_promise_unref(promise);

    //promise = gst_promise_new_with_change_func(on_answer_set, webrtc, nullptr);
    g_signal_emit_by_name(webrtc, "set-local-description", answer, nullptr);
    //gst_promise_interrupt(promise);
   // gst_promise_unref(promise);


    if (callbackICEInstance != nullptr)
    {
        gchar* desc = gst_sdp_message_as_text(answer->sdp);
        callbackSDPInstance(desc, (int)strlen(desc));
        g_free(desc);
    }

    gst_webrtc_session_description_free(answer);
}

void GstDataPipeline::on_ice_candidate(GstElement* webrtcbin, guint mline_index, gchararray candidate, gpointer user_data)
{
    if (callbackICEInstance != nullptr)
    {
        const std::string tmp = candidate;
        const char* tmsg = tmp.c_str();
        callbackICEInstance(tmsg, (int)strlen(tmsg), mline_index);
    }
    else
    {
        Debug::Log("callbackICEInstance is not initialized", Level::Error);
    }
}


/* void GstDataPipeline::on_message_data(GstWebRTCDataChannel* channel, GBytes* data, gpointer user_data)
{
    Debug::Log("Data channel message received", Level::Info);
}*/

/* void GstDataPipeline::add_data_channel(GstElement* webrtcbin)
{
    const gchar* label = "service";
    GstStructure* options = gst_structure_new_empty("options");
    gst_structure_set(options, "ordered", G_TYPE_BOOLEAN, TRUE, nullptr);

    GstWebRTCDataChannel* data_channel = nullptr;
    g_signal_emit_by_name(webrtcbin, "create-data-channel", label, options, &data_channel);
    if (!data_channel)
    {
        Debug::Log("Failed to create data channel", Level::Error);
        return;
    }
    else
    {
        g_signal_connect(data_channel, "on-message-data", G_CALLBACK(on_message_data), nullptr);
    }

    gst_structure_free(options);
}*/


void GstDataPipeline::on_ice_gathering_state_notify(GstElement* webrtcbin, GParamSpec* pspec, gpointer user_data)
{
    GstWebRTCICEGatheringState ice_gather_state;
    std::string new_state = "unknown";

    g_object_get(webrtcbin, "ice-gathering-state", &ice_gather_state, NULL);
    switch (ice_gather_state)
    {
        case GST_WEBRTC_ICE_GATHERING_STATE_NEW:
            new_state = "new";
            break;
        case GST_WEBRTC_ICE_GATHERING_STATE_GATHERING:
            new_state = "gathering";
            break;
        case GST_WEBRTC_ICE_GATHERING_STATE_COMPLETE:
            new_state = "complete";
            break;
    }
    Debug::Log("ICE gathering state changed to "+ new_state);
}

bool GstDataPipeline::starts_with(const std::string& str, const std::string& prefix)
{
    return str.size() >= prefix.size() && str.substr(0, prefix.size()) == prefix;
}

void GstDataPipeline::on_data_channel(GstElement* webrtcbin, GstWebRTCDataChannel* channel, gpointer udata)
{
    GstDataPipeline* self = static_cast<GstDataPipeline*>(udata);
    gchar* label = nullptr;
    g_object_get(channel, "label", &label, nullptr);
    std::string label_str = std::string(label);
    g_free(label);

    Debug::Log("Received data channel : " + label_str);

    if (label_str == CHANNEL_SERVICE)
    {
        self->_channel_service = channel;        

        g_signal_connect(channel, "on-message-data", G_CALLBACK(on_message_data_service), nullptr);

        if (callbackChannelServiceOpenInstance != nullptr)
        {
            callbackChannelServiceOpenInstance();
        }   
    }
    else if (starts_with(label_str,CHANNEL_REACHY_STATE))
    {
        g_signal_connect(channel, "on-message-data", G_CALLBACK(on_message_data_state), nullptr);
    }
    else if (starts_with(label_str, CHANNEL_REACHY_AUDIT))
    {
        g_signal_connect(channel, "on-message-data", G_CALLBACK(on_message_data_audit), nullptr);
    }
    else if (starts_with(label_str, CHANNEL_REACHY_COMMAND))
    {
        self->_channel_command = channel;
    }
    else
    {
        Debug::Log("unknown data channel : " + label_str, Level::Warning);
    }

}

void GstDataPipeline::send_byte_array(GstWebRTCDataChannel* channel, const unsigned char* data, size_t size)
{
    g_assert(data != nullptr);
    GBytes* bytes = g_bytes_new(data, size);

    gst_webrtc_data_channel_send_data(channel, bytes);
    g_bytes_unref(bytes);
}

void GstDataPipeline::send_byte_array_channel_service(const unsigned char* data, size_t size) 
{
    if (_channel_service != nullptr)
        send_byte_array(_channel_service, data, size); 
    else
        Debug::Log("channel service is not initialized ", Level::Warning);
}

void GstDataPipeline::send_byte_array_channel_command(const unsigned char* data, size_t size) 
{
    if (_channel_command != nullptr)
        send_byte_array(_channel_command, data, size);
    else
        Debug::Log("channel command is not initialized ", Level::Warning);
}

void GstDataPipeline::on_message_data_service(GstWebRTCDataChannel* channel, GBytes* data, gpointer user_data)
{
    //Debug::Log("Data channel service message received", Level::Info);
    if (callbackChannelServiceDataInstance != nullptr)
    {
        gsize size = g_bytes_get_size(data);
        const uint8_t* message = static_cast<uint8_t*>(const_cast<gpointer>(g_bytes_get_data(data, &size)));
        callbackChannelServiceDataInstance(message, (int)size);
    }
}

void GstDataPipeline::on_message_data_state(GstWebRTCDataChannel* channel, GBytes* data, gpointer user_data)
{
    //Debug::Log("Data channel state message received", Level::Info);
    if (callbackChannelStateDataInstance != nullptr)
    {
        gsize size = g_bytes_get_size(data);
        const uint8_t* message = static_cast<uint8_t*>(const_cast<gpointer>(g_bytes_get_data(data, &size)));
        callbackChannelStateDataInstance(message, (int)size);
    }
}

void GstDataPipeline::on_message_data_audit(GstWebRTCDataChannel* channel, GBytes* data, gpointer user_data)
{
    // Debug::Log("Data channel audit message received", Level::Info);
    if (callbackChannelAuditDataInstance != nullptr)
    {
        gsize size = g_bytes_get_size(data);
        const uint8_t* message = static_cast<uint8_t*>(const_cast<gpointer>(g_bytes_get_data(data, &size)));
        callbackChannelAuditDataInstance(message, (int)size);
    }
}

GstElement* GstDataPipeline::add_webrtcbin()
{
    GstElement* webrtcbin = gst_element_factory_make("webrtcbin", nullptr);
    if (!webrtcbin)
    {
        Debug::Log("Failed to create webrtcbin", Level::Error);
        return nullptr;
    }

    g_object_set(G_OBJECT(webrtcbin), "bundle-policy", GST_WEBRTC_BUNDLE_POLICY_MAX_BUNDLE, nullptr);
    g_signal_connect(webrtcbin, "on-ice-candidate", G_CALLBACK(on_ice_candidate), nullptr);
    g_signal_connect(webrtcbin, "on-data-channel", G_CALLBACK(on_data_channel), this);
    g_signal_connect(webrtcbin, "notify::ice-gathering-state", G_CALLBACK(on_ice_gathering_state_notify), nullptr);

    gst_bin_add(GST_BIN(this->_pipeline), webrtcbin);
    return webrtcbin;
}

gpointer GstDataPipeline::main_loop_func(gpointer data)
{
    Debug::Log("Entering main loop");
    GstDataPipeline* self = static_cast<GstDataPipeline*>(data);

    g_main_context_push_thread_default(self->main_context_);

    GstBus* bus = gst_element_get_bus(self->_pipeline);
    gst_bus_add_watch(bus, busHandler, self);

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

    gst_bus_remove_watch(bus);
    gst_object_unref(bus);
    g_main_context_pop_thread_default(self->main_context_);
    Debug::Log("Quitting main loop");

    return nullptr;
}

gboolean GstDataPipeline::busHandler(GstBus* bus, GstMessage* msg, gpointer data)
{
    auto self = (GstDataPipeline*)data;

    switch (GST_MESSAGE_TYPE(msg))
    {
        case GST_MESSAGE_ERROR:
        {
            GError* err;
            gchar* dbg;

            gst_message_parse_error(msg, &err, &dbg);
            Debug::Log(err->message, Level::Error);
            if (dbg != nullptr)
                Debug::Log(dbg);
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
            Debug::Log("Redistribute latency data...");
            gst_bin_recalculate_latency(GST_BIN(self->_pipeline));
            GstDataPipeline::dumpLatencyCallback(self);
            break;
        }
        default:
            //Debug::Log(GST_MESSAGE_TYPE_NAME(msg));
            break;
    }

    return G_SOURCE_CONTINUE;
}

gboolean GstDataPipeline::dumpLatencyCallback(GstDataPipeline* self) 
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
            // Log or std::cout your latency here
            std::string msg = "Pipeline latency: live=" + std::to_string(live) + ", min=" + std::to_string(min_latency) +
                              ", max=" + std::to_string(max_latency);
            Debug::Log(msg);
        }
        gst_query_unref(query);
        return true;
    }
    return false;
}

// Create a callback delegate
void RegisterICECallback(FuncCallBackICE cb) { callbackICEInstance = cb; }
void RegisterSDPCallback(FuncCallBackSDP cb) { callbackSDPInstance = cb; }
void RegisterChannelServiceOpenCallback(FuncCallBackChannelServiceOpen cb) { callbackChannelServiceOpenInstance = cb; }
void RegisterChannelServiceDataCallback(FuncCallBackChannelData cb) { callbackChannelServiceDataInstance = cb; }
void RegisterChannelStateDataCallback(FuncCallBackChannelData cb) { callbackChannelStateDataInstance = cb; }
void RegisterChannelAuditDataCallback(FuncCallBackChannelData cb) { callbackChannelAuditDataInstance = cb; }

//const 
const std::string GstDataPipeline::CHANNEL_SERVICE = "service";
const std::string GstDataPipeline::CHANNEL_REACHY_STATE = "reachy_state";
const std::string GstDataPipeline::CHANNEL_REACHY_COMMAND = "reachy_command";
const std::string GstDataPipeline::CHANNEL_REACHY_AUDIT = "reachy_audit";
