/* Copyright(c) Pollen Robotics, all rights reserved.
 This source code is licensed under the license found in the
 LICENSE file in the root directory of this source tree. */

#pragma once
#include "GstBasePipeline.h"
#include <gst/gst.h>
#include <gst/webrtc/datachannel.h>
#include <string>

#define DLLExport __declspec(dllexport)

extern "C"
{
    // Create a callback delegate
    typedef void (*FuncCallBackICE)(const char* candidate, int size_candidate_str, int mline_index);
    static FuncCallBackICE callbackICEInstance = nullptr;
    DLLExport void RegisterICECallback(FuncCallBackICE cb);

    typedef void (*FuncCallBackSDP)(const char* message, int size);
    static FuncCallBackSDP callbackSDPInstance = nullptr;
    DLLExport void RegisterSDPCallback(FuncCallBackSDP cb);

    typedef void (*FuncCallBackChannelOpen)();
    static FuncCallBackChannelOpen callbackChannelServiceOpenInstance = nullptr;
    DLLExport void RegisterChannelServiceOpenCallback(FuncCallBackChannelOpen cb);

    static FuncCallBackChannelOpen callbackChannelCommandOpenInstance = nullptr;
    DLLExport void RegisterChannelCommandOpenCallback(FuncCallBackChannelOpen cb);

    typedef void (*FuncCallBackChannelData)(const uint8_t * message, int size);
    static FuncCallBackChannelData callbackChannelServiceDataInstance = nullptr;
    DLLExport void RegisterChannelServiceDataCallback(FuncCallBackChannelData cb);

    static FuncCallBackChannelData callbackChannelStateDataInstance = nullptr;
    DLLExport void RegisterChannelStateDataCallback(FuncCallBackChannelData cb);

    static FuncCallBackChannelData callbackChannelAuditDataInstance = nullptr;
    DLLExport void RegisterChannelAuditDataCallback(FuncCallBackChannelData cb);
}

class GstDataPipeline : GstBasePipeline
{
private:
    GstElement* webrtcbin_ = nullptr;
    static const std::string CHANNEL_SERVICE;
    static const std::string CHANNEL_REACHY_STATE;
    static const std::string CHANNEL_REACHY_COMMAND;
    static const std::string CHANNEL_REACHY_AUDIT;
    GstWebRTCDataChannel* channel_service_ = nullptr;    
    GstWebRTCDataChannel* channel_command_ = nullptr;
    GstWebRTCDataChannel* channel_audit_ = nullptr;


public:
    GstDataPipeline();
    void CreatePipeline();
    void DestroyPipeline() override;
    void SetOffer(const char* sdp_offer);
    void SetICECandidate(const char* candidate, int mline_index);
    void send_byte_array_channel_service(const unsigned char * data, size_t size);
    void send_byte_array_channel_command(const unsigned char* data, size_t size);

private:
    GstElement* add_webrtcbin();
    static void on_ice_candidate(GstElement* webrtcbin, guint mline_index, gchararray candidate, gpointer user_data);
    static void on_data_channel(GstElement* webrtcbin, GstWebRTCDataChannel* channel, gpointer udata);
    //static void on_message_data(GstWebRTCDataChannel* channel, GBytes* data, gpointer user_data);
    static void on_message_data_service(GstWebRTCDataChannel* channel, GBytes* data, gpointer user_data);
    static void on_message_data_state(GstWebRTCDataChannel* channel, GBytes* data, gpointer user_data);
    static void on_message_data_audit(GstWebRTCDataChannel* channel, GBytes* data, gpointer user_data);
    static void on_offer_set(GstPromise* promise, gpointer user_data);
    static void on_answer_created(GstPromise* promise, gpointer user_data);
    static void on_ice_gathering_state_notify(GstElement* webrtcbin, GParamSpec* pspec, gpointer user_data);
    void send_byte_array(GstWebRTCDataChannel* channel, const unsigned char* data, size_t size);
    static bool starts_with(const std::string& str, const std::string& prefix);
};