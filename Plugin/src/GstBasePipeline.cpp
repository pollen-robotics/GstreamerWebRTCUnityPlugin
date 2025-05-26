/* Copyright(c) Pollen Robotics, all rights reserved.
 This source code is licensed under the license found in the
 LICENSE file in the root directory of this source tree. */

#include "GstBasePipeline.h"
#include "DebugLog.h"

GstBasePipeline::GstBasePipeline(const std::string& pipename) : PIPENAME(pipename)
{
    main_context_ = g_main_context_new();
    main_loop_ = g_main_loop_new(main_context_, FALSE);
}

GstBasePipeline::~GstBasePipeline()
{
    g_main_context_unref(main_context_);
    g_main_loop_unref(main_loop_);
}

void GstBasePipeline::CreatePipeline() { pipeline_ = gst_pipeline_new(PIPENAME.c_str()); }

void GstBasePipeline::DestroyPipeline()
{
    if (main_loop_ != nullptr)
        g_main_loop_quit(main_loop_);

    if (bus_thread_ != nullptr)
    {
        Debug::Log("Wait for " + PIPENAME + " thread to close ...", Level::Info);
        g_thread_join(bus_thread_);
        g_thread_unref(bus_thread_);
        bus_thread_ = nullptr;
    }

    if (pipeline_ != nullptr)
    {
        Debug::Log(PIPENAME + " pipeline released", Level::Info);
        gst_object_unref(pipeline_);
        pipeline_ = nullptr;
    }
    else
    {
        Debug::Log(PIPENAME + " pipeline already released", Level::Warning);
    }
}

gpointer GstBasePipeline::main_loop_func(gpointer data)
{
    GstBasePipeline* self = static_cast<GstBasePipeline*>(data);
    Debug::Log("Entering main loop " + self->PIPENAME);

    g_main_context_push_thread_default(self->main_context_);

    GstBus* bus = gst_element_get_bus(self->pipeline_);
    gst_bus_add_watch(bus, busHandler, self);
    gst_bus_set_sync_handler(bus, busSyncHandlerWrapper, self, nullptr);

    auto state = gst_element_set_state(self->pipeline_, GstState::GST_STATE_PLAYING);
    if (state == GstStateChangeReturn::GST_STATE_CHANGE_FAILURE)
    {
        Debug::Log("Cannot set pipeline to playing state", Level::Error);
        gst_object_unref(self->pipeline_);
        self->pipeline_ = nullptr;
        return nullptr;
    }

    g_main_loop_run(self->main_loop_);

    gst_element_set_state(self->pipeline_, GST_STATE_NULL);

    gst_bus_set_sync_handler(bus, nullptr, nullptr, nullptr);
    gst_bus_remove_watch(bus);
    gst_object_unref(bus);
    g_main_context_pop_thread_default(self->main_context_);
    Debug::Log("Quitting main loop " + self->PIPENAME);

    return nullptr;
}

GstBusSyncReply GstBasePipeline::busSyncHandlerWrapper(GstBus* bus, GstMessage* msg, gpointer user_data)
{
    GstBasePipeline* self = static_cast<GstBasePipeline*>(user_data);
    return self->busSyncHandler(bus, msg, user_data);
}

GstBusSyncReply GstBasePipeline::busSyncHandler(GstBus* bus, GstMessage* msg, gpointer user_data) { return GST_BUS_PASS; }

gboolean GstBasePipeline::busHandler(GstBus* bus, GstMessage* msg, gpointer data)
{
    auto self = (GstBasePipeline*)data;

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
            Debug::Log("Got EOS " + self->PIPENAME);
            g_main_loop_quit(self->main_loop_);
            break;
        case GST_MESSAGE_LATENCY:
        {
            // Debug::Log("Redistribute latency ...");
            gst_bin_recalculate_latency(GST_BIN(self->pipeline_));
            GstBasePipeline::dumpLatencyCallback(self);
            break;
        }
        default:
            // Debug::Log(GST_MESSAGE_TYPE_NAME(msg));
            break;
    }

    return G_SOURCE_CONTINUE;
}

gboolean GstBasePipeline::dumpLatencyCallback(GstBasePipeline* self)
{
    if (self)
    {
        GstQuery* query = gst_query_new_latency();
        gboolean res = gst_element_query(self->pipeline_, query);
        if (res)
        {
            gboolean live;
            GstClockTime min_latency, max_latency;
            gst_query_parse_latency(query, &live, &min_latency, &max_latency);
            std::string msg = "Pipeline " + self->PIPENAME + " latency: live=" + std::to_string(live) +
                              ", min=" + std::to_string(min_latency) + ", max=" + std::to_string(max_latency);
            Debug::Log(msg);
            GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(self->pipeline_), GST_DEBUG_GRAPH_SHOW_ALL, self->PIPENAME.c_str());
        }
        gst_query_unref(query);
        return true;
    }
    return false;
}

void GstBasePipeline::CreateBusThread()
{
    const std::string name = "bus thread " + PIPENAME;
    bus_thread_ = g_thread_new(name.c_str(), main_loop_func, this);
    if (!bus_thread_)
    {
        Debug::Log("Failed to create GLib main thread", Level::Error);
    }
}

// generic method to add element with default options
GstElement* GstBasePipeline::add_by_name(GstElement* pipeline, const std::string& name)
{
    GstElement* element = gst_element_factory_make(name.c_str(), nullptr);
    if (!element)
    {
        Debug::Log("Failed to create " + name, Level::Error);
        return nullptr;
    }
    gst_bin_add(GST_BIN(pipeline), element);
    return element;
}