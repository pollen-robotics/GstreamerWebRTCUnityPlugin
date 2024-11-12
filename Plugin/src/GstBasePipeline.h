/* Copyright(c) Pollen Robotics, all rights reserved.
 This source code is licensed under the license found in the
 LICENSE file in the root directory of this source tree. */

#pragma once
#include <gst/gst.h>
#include <string>

class GstBasePipeline
{
protected:
    const std::string PIPENAME;
    GstElement* pipeline_ = nullptr;
    GThread* bus_thread_ = nullptr;
    GMainContext* main_context_ = nullptr;
    GMainLoop* main_loop_ = nullptr;

public:
    GstBasePipeline(const std::string& pipename);
    virtual ~GstBasePipeline();
    virtual void CreatePipeline();
    virtual void DestroyPipeline();    

protected:
    static gpointer main_loop_func(gpointer data);
    static GstBusSyncReply busSyncHandlerWrapper(GstBus* bus, GstMessage* msg, gpointer user_data);
    virtual GstBusSyncReply busSyncHandler(GstBus* bus, GstMessage* msg, gpointer user_data);
    static gboolean busHandler(GstBus* bus, GstMessage* msg, gpointer data);
    static gboolean dumpLatencyCallback(GstBasePipeline* self);

    void CreateBusThread();
};