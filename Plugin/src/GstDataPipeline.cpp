/* Copyright(c) Pollen Robotics, all rights reserved.
 This source code is licensed under the license found in the
 LICENSE file in the root directory of this source tree. */

#include "GstDataPipeline.h"
#include "DebugLog.h"
#include <gst/sdp/sdp.h>
#include <gst/webrtc/webrtc.h>

GstDataPipeline::GstDataPipeline() : GstBasePipeline("DataPipeline") {
}


void GstDataPipeline::CreatePipeline() 
{
    Debug::Log("GstDataPipeline create pipeline", Level::Info);
    GstBasePipeline::CreatePipeline();

    webrtcbin_ = add_webrtcbin();
    auto state = gst_element_set_state(this->pipeline_, GstState::GST_STATE_READY);    

    CreateBusThread();
}

void GstDataPipeline::DestroyPipeline()
{
    gst_webrtc_data_channel_close(channel_service_);
    gst_webrtc_data_channel_close(channel_command_);
    gst_webrtc_data_channel_close(channel_audit_);

    channel_service_ = nullptr;
    channel_audit_ = nullptr;
    channel_command_ = nullptr;

    GstBasePipeline::DestroyPipeline();
}

void GstDataPipeline::SetOffer(const char* sdp_offer) 
{
    Debug::Log("SDP Offer: " + std::string(sdp_offer));
    GstSDPMessage* sdpmsg = nullptr;
    gst_sdp_message_new_from_text(sdp_offer, &sdpmsg);

    GstWebRTCSDPType sdp_type = GST_WEBRTC_SDP_TYPE_OFFER;
    GstWebRTCSessionDescription* offer = gst_webrtc_session_description_new(sdp_type, sdpmsg);


    GstPromise* promise =
        gst_promise_new_with_change_func(on_offer_set, webrtcbin_, nullptr);

    g_signal_emit_by_name(webrtcbin_, "set-remote-description", offer, promise, nullptr);

    //gst_webrtc_session_description_free(offer); //raise breakpoint?
    gst_sdp_message_free(sdpmsg);
    gst_promise_unref(promise);
}

void GstDataPipeline::SetICECandidate(const char* candidate, int mline_index) 
{
    Debug::Log("Add ICE Candidate: " + std::string(candidate) + " "+ std::to_string(mline_index));
    g_signal_emit_by_name(webrtcbin_, "add-ice-candidate", mline_index, candidate);
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
    if (label == nullptr)
    {
        Debug::Log("Channel has no label", Level::Error);
        return;
    }
    const std::string label_str = std::string(label);
    g_free(label);

    Debug::Log("Received data channel : " + label_str);

    if (label_str == CHANNEL_SERVICE)
    {
        self->channel_service_ = channel;        

        g_signal_connect(channel, "on-message-data", G_CALLBACK(on_message_data_service), nullptr);

        if (callbackChannelServiceOpenInstance != nullptr)        
            callbackChannelServiceOpenInstance();   
        else
            Debug::Log("Fails to notify opening of service channel", Level::Warning);
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
        self->channel_command_ = channel;
        if (callbackChannelCommandOpenInstance != nullptr)        
            callbackChannelCommandOpenInstance();    
        else
            Debug::Log("Fails to notify opening of command channel", Level::Warning);
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
    if (channel_service_ != nullptr)
        send_byte_array(channel_service_, data, size); 
    else
        Debug::Log("channel service is not initialized ", Level::Warning);
}

void GstDataPipeline::send_byte_array_channel_command(const unsigned char* data, size_t size) 
{
    if (channel_command_ != nullptr)
        send_byte_array(channel_command_, data, size);
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

    gst_bin_add(GST_BIN(this->pipeline_), webrtcbin);
    return webrtcbin;
}


// Create a callback delegate
void RegisterICECallback(FuncCallBackICE cb) { callbackICEInstance = cb; }
void RegisterSDPCallback(FuncCallBackSDP cb) { callbackSDPInstance = cb; }
void RegisterChannelCommandOpenCallback(FuncCallBackChannelOpen cb) { callbackChannelCommandOpenInstance = cb; }
void RegisterChannelServiceOpenCallback(FuncCallBackChannelOpen cb) { callbackChannelServiceOpenInstance = cb; }
void RegisterChannelServiceDataCallback(FuncCallBackChannelData cb) { callbackChannelServiceDataInstance = cb; }
void RegisterChannelStateDataCallback(FuncCallBackChannelData cb) { callbackChannelStateDataInstance = cb; }
void RegisterChannelAuditDataCallback(FuncCallBackChannelData cb) { callbackChannelAuditDataInstance = cb; }

//const 
const std::string GstDataPipeline::CHANNEL_SERVICE = "service";
const std::string GstDataPipeline::CHANNEL_REACHY_STATE = "reachy_state";
const std::string GstDataPipeline::CHANNEL_REACHY_COMMAND = "reachy_command";
const std::string GstDataPipeline::CHANNEL_REACHY_AUDIT = "reachy_audit";
