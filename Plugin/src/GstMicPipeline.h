/* Copyright(c) Pollen Robotics, all rights reserved.
 This source code is licensed under the license found in the
 LICENSE file in the root directory of this source tree. */

#pragma once
#include "GstBasePipeline.h"

class GstMicPipeline : public GstBasePipeline
{

public:
    GstMicPipeline();

    void CreatePipeline(const char* uri, const char* remote_peer_id);

private:
    static void consumer_added_callback(GstElement* consumer_id, gchararray webrtcbin, GstElement* arg1, gpointer udata);
    static GstElement* add_wasapi2src(GstElement* pipeline);
    static GstElement* add_opusenc(GstElement* pipeline);
    static GstElement* add_audio_caps_capsfilter(GstElement* pipeline);
    static GstElement* add_webrtcsink(GstElement* pipeline, const std::string& uri);
    static GstElement* add_webrtcdsp(GstElement* pipeline);
};
