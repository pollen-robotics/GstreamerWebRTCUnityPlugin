#include "GstAVPipelineOpenGLES.h"
#include "DebugLog.h"
#include <EGL/egl.h>
#include <gst/gl/egl/gstgldisplay_egl.h>

#include <string>

void GstAVPipelineOpenGLES::SetUnityContext()
{
    EGLContext unityContext = eglGetCurrentContext();
    EGLDisplay unityDisplay = eglGetCurrentDisplay();
    EGLSurface unityDrawSurface = eglGetCurrentSurface(EGL_DRAW);
    EGLSurface unityReadSurface = eglGetCurrentSurface(EGL_READ);

    if (unityContext == EGL_NO_CONTEXT)
    {
        Debug::Log("UnityEGLContext is invalid", Level::Error);
    }

    eglMakeCurrent(unityDisplay, unityDrawSurface, unityReadSurface, unityContext);

    Debug::Log("Set Unity context wrapper");

    GstGLDisplay* gl_display = (GstGLDisplay*)gst_gl_display_egl_new_with_egl_display(unityDisplay);
    gl_context_unity = gst_gl_context_new_wrapped(gl_display, (guintptr)unityContext, GST_GL_PLATFORM_EGL, GST_GL_API_GLES2);
    gst_gl_context_activate(gl_context_unity, true);
}

void GstAVPipelineOpenGLES::SetTextureFromUnity(GLuint texPtr, bool left)
{
    std::unique_ptr<AppData> data = std::make_unique<AppData>();
    data->textureID = texPtr;

    if (left)
        _leftData = std::move(data);
    else
        _rightData = std::move(data);
}

void GstAVPipelineOpenGLES::Draw(JNIEnv* env, bool left)
{
    AppData* data;
    if (left)
        data = _leftData.get();
    else
        data = _rightData.get();

    if (data == nullptr)
    {
        Debug::Log("data is null", Level::Warning);
        return;
    }

    GstSample* sample = nullptr;

    std::lock_guard<std::mutex> lk(data->lock);
    if (!data->last_sample)
        return;

    sample = data->last_sample;
    data->last_sample = nullptr;

    auto buf = gst_sample_get_buffer(sample);
    if (!buf)
    {
        Debug::Log("Sample without buffer", Level::Error);
        gst_sample_unref(sample);
        return;
    }

    GstCaps* caps = gst_sample_get_caps(sample);
    if (!caps)
    {
        gst_sample_unref(sample);
        Debug::Log("Sample without caps", Level::Error);
        return;
    }

    GstVideoFrame v_frame;
    GstVideoInfo v_info;
    guint texture = 0;

    gst_video_info_from_caps(&v_info, caps);

    if (!gst_video_frame_map(&v_frame, &v_info, buf, (GstMapFlags)(GST_MAP_READ | GST_MAP_GL)))
    {
        Debug::Log("Failed to map the video buffer", Level::Warning);
        return;
    }

    texture = *(guint*)v_frame.data[0];
    // Debug::Log("Sample buffer received in Draw " + std::to_string(texture));
    copyGStreamerTextureToFramebuffer(texture, data->textureID, 960, 720);

    gst_video_frame_unmap(&v_frame);
    gst_sample_unref(sample);
}

void GstAVPipelineOpenGLES::copyGStreamerTextureToFramebuffer(GLuint srcTexture, GLuint dstTexture, int width, int height)
{
    GLuint fboRead = 0;
    GLuint fboDraw = 0;

    glGenFramebuffers(1, &fboRead);
    glGenFramebuffers(1, &fboDraw);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, fboRead);
    /*GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
        Debug::Log("Src FBO incomplete: 0x" + std::to_string(status), Level::Error);*/

    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, srcTexture, 0);

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fboDraw);
    /*status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
        Debug::Log("Dest FBO incomplete: 0x" + std::to_string(status), Level::Error);*/
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dstTexture, 0);

    /*status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
        Debug::Log("FBO incomplete: 0x" + std::to_string(status), Level::Error);*/

    glBlitFramebuffer(0, 0, width, height, // Source area
                      0, 0, width, height, // Dest area
                      GL_COLOR_BUFFER_BIT, // buffer à blitter
                      GL_NEAREST           // interpolation
    );

    // Clean up
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fboRead);
    glDeleteFramebuffers(1, &fboDraw);
}

void* GstAVPipelineOpenGLES::CreateTexture(bool left)
{
    if (left)
    {
        Debug::Log("return texture left " + std::to_string(_leftData->textureID));
        return &_leftData->textureID;
    }
    else
    {
        Debug::Log("return texture right " + std::to_string(_rightData->textureID));
        return &_rightData->textureID;
    }
}

void GstAVPipelineOpenGLES::ReleaseTexture(void* texture)
{
    _leftData->textureID = -1;
    if (_leftData->last_sample != nullptr)
    {
        gst_sample_unref(_leftData->last_sample);
        _leftData->last_sample = nullptr;
        gst_caps_unref(_leftData->last_caps);
        _leftData->last_caps = nullptr;
    }

    _rightData->textureID = -1;
    if (_rightData->last_sample != nullptr)
    {
        gst_sample_unref(_rightData->last_sample);
        _rightData->last_sample = nullptr;
        gst_caps_unref(_rightData->last_caps);
        _rightData->last_caps = nullptr;
    }
}

GstElement* GstAVPipelineOpenGLES::add_appsink(GstElement* pipeline)
{
    GstElement* appsink = gst_element_factory_make("appsink", nullptr);
    if (!appsink)
    {
        Debug::Log("Failed to create appsink", Level::Error);
        return nullptr;
    }

    GstCaps* caps = gst_caps_from_string("video/x-raw(memory:GLMemory),format=RGBA,texture-target=2D");
    g_object_set(appsink, "caps", caps, "drop", true, "max-buffers", 1, /*"processing-deadline", 0,*/ nullptr);
    gst_caps_unref(caps);

    gst_bin_add(GST_BIN(pipeline), appsink);
    return appsink;
}

GstFlowReturn GstAVPipelineOpenGLES::on_new_sample(GstAppSink* appsink, gpointer user_data)
{
    AppData* data = static_cast<AppData*>(user_data);
    GstSample* sample = gst_app_sink_pull_sample(appsink);

    if (!sample)
        return GST_FLOW_ERROR;

    GstCaps* caps = gst_sample_get_caps(sample);
    if (!caps)
    {
        gst_sample_unref(sample);
        Debug::Log("Sample without caps", Level::Error);
        return GST_FLOW_ERROR;
    }

    std::lock_guard<std::mutex> lk(data->lock);

    gst_caps_replace(&data->last_caps, caps);
    gst_clear_sample(&data->last_sample);
    data->last_sample = sample;

    // Debug::Log("Sample received in on_new_sample");

    return GST_FLOW_OK;
}

void GstAVPipelineOpenGLES::on_pad_added(GstElement* src, GstPad* new_pad, gpointer data)
{
    gchar* pad_name = gst_pad_get_name(new_pad);
    GstAVPipelineOpenGLES* avpipeline = static_cast<GstAVPipelineOpenGLES*>(data);

    Debug::Log("Adding pad ");
    if (g_str_has_prefix(pad_name, "video"))
    {
        Debug::Log("Adding video pad " + std::string(pad_name));
        // GstElement* queue = add_by_name(avpipeline->pipeline_, "queue");
        //  decoder output texture-target=external-eos. converts to exture-target=2D
        GstElement* glcolorconvert = add_by_name(avpipeline->pipeline_, "glcolorconvert");
        GstElement* appsink = add_appsink(avpipeline->pipeline_);

        GstAppSinkCallbacks callbacks = {nullptr};
        callbacks.new_sample = on_new_sample;

        if (g_str_has_suffix(pad_name, "_0"))
        {
            Debug::Log("Connecting left video pad " + std::string(pad_name));
            gst_app_sink_set_callbacks(GST_APP_SINK(appsink), &callbacks, _leftData.get(), nullptr);
        }
        else if (g_str_has_suffix(pad_name, "_1"))
        {
            Debug::Log("Connecting right video pad " + std::string(pad_name));
            gst_app_sink_set_callbacks(GST_APP_SINK(appsink), &callbacks, _rightData.get(), nullptr);
        }
        else
        {
            Debug::Log("Unknown video pad", Level::Warning);
        }

        if (!gst_element_link_many(/*queue,*/ glcolorconvert, appsink, nullptr))
        {
            Debug::Log("Elements could not be linked.");
        }

        GstPad* sinkpad = gst_element_get_static_pad(glcolorconvert, "sink");
        if (gst_pad_link(new_pad, sinkpad) != GST_PAD_LINK_OK)
        {
            Debug::Log("Could not link dynamic video pad to queue", Level::Error);
        }
        gst_object_unref(sinkpad);

        // gst_element_sync_state_with_parent(queue);
        gst_element_sync_state_with_parent(glcolorconvert);
        gst_element_sync_state_with_parent(appsink);
    }
    else if (g_str_has_prefix(pad_name, "audio"))
    {
        Debug::Log("Adding audio pad " + std::string(pad_name));
        GstElement* queue = add_by_name(avpipeline->pipeline_, "queue");
        GstElement* rtpopusdepay = add_by_name(avpipeline->pipeline_, "rtpopusdepay");
        GstElement* opusdec = add_by_name(avpipeline->pipeline_, "opusdec");
        GstElement* audioconvert = add_by_name(avpipeline->pipeline_, "audioconvert");
        GstElement* audioresample = add_by_name(avpipeline->pipeline_, "audioresample");
        GstElement* autoaudiosink = add_by_name(avpipeline->pipeline_, "openslessink");
        g_object_set(autoaudiosink, "processing-deadline", 0, nullptr);

        if (!gst_element_link_many(queue, rtpopusdepay, opusdec, audioconvert, audioresample, autoaudiosink, nullptr))
        {
            Debug::Log("Audio elements could not be linked.", Level::Error);
        }

        GstPad* sinkpad = gst_element_get_static_pad(queue, "sink");
        if (gst_pad_link(new_pad, sinkpad) != GST_PAD_LINK_OK)
        {
            Debug::Log("Could not link dynamic audio pad to queue", Level::Error);
        }
        gst_object_unref(sinkpad);

        gst_element_sync_state_with_parent(rtpopusdepay);
        gst_element_sync_state_with_parent(opusdec);
        gst_element_sync_state_with_parent(queue);
        gst_element_sync_state_with_parent(audioconvert);
        gst_element_sync_state_with_parent(audioresample);
        gst_element_sync_state_with_parent(autoaudiosink);
    }
    g_free(pad_name);
}

GstAVPipelineOpenGLES::GstAVPipelineOpenGLES(IUnityInterfaces* s_UnityInterfaces) : GstAVPipeline(s_UnityInterfaces)
{
    preloaded_plugins.push_back(gst_plugin_load_by_name("opengl"));
    if (!preloaded_plugins.back())
    {
        Debug::Log("Failed to load 'opengl' plugin", Level::Error);
    }
}

GstBusSyncReply GstAVPipelineOpenGLES::busSyncHandler(GstBus* bus, GstMessage* msg, gpointer user_data)
{
    auto self = (GstAVPipelineOpenGLES*)user_data;

    switch (GST_MESSAGE_TYPE(msg))
    {
        case GST_MESSAGE_NEED_CONTEXT:
        {
            const gchar* context_type;
            GstContext* context = NULL;

            gst_message_parse_context_type(msg, &context_type);
            Debug::Log("Got need context " + std::string(context_type) + " " + msg->src->name);

            if (g_strcmp0(context_type, "gst.gl.app_context") == 0)
            {
                GstGLContext* gl_context = self->gl_context_unity;
                GstStructure* s;

                context = gst_context_new("gst.gl.app_context", TRUE);
                s = gst_context_writable_structure(context);
                gst_structure_set(s, "context", GST_TYPE_GL_CONTEXT, gl_context, NULL);

                gst_element_set_context(GST_ELEMENT(msg->src), context);
            }
            if (context)
                gst_context_unref(context);
            break;
        }
        default:
            break;
    }

    return GST_BUS_PASS;
}
