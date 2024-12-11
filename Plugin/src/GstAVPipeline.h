/* Copyright(c) Pollen Robotics, all rights reserved.
 This source code is licensed under the license found in the
 LICENSE file in the root directory of this source tree. */

#pragma once
#include "GstBasePipeline.h"
#include "Unity/IUnityInterface.h"
#include <vector>

class GstAVPipeline : public GstBasePipeline
{
protected:
    std::vector<GstPlugin*> preloaded_plugins;
    IUnityInterfaces* _s_UnityInterfaces = nullptr;

public:
    GstAVPipeline(IUnityInterfaces* s_UnityInterfaces);
    ~GstAVPipeline();

    void CreatePipeline(const char* uri, const char* remote_peer_id);
    virtual void ReleaseTexture(void* texture) = 0;

protected:
    virtual void on_pad_added(GstElement* src, GstPad* new_pad, gpointer data) = 0;
    static void on_pad_added_wrapper(GstElement* src, GstPad* new_pad, gpointer data);
    static void webrtcbin_ready(GstElement* self, gchararray peer_id, GstElement* webrtcbin, gpointer udata);

    static GstElement* add_webrtcsrc(GstElement* pipeline, const std::string& remote_peer_id, const std::string& uri,
                                     GstAVPipeline* self);
};
