#include "GstAVPipelineOpenGLES.h"
#include "DebugLog.h"
#include <EGL/egl.h>
// #include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES3/gl3.h>
#include <gst/video/videooverlay.h>
#include <string>

std::string GetGLErrorString(GLenum error)
{
    switch (error)
    {
        case GL_NO_ERROR:
            return "GL_NO_ERROR";
        case GL_INVALID_ENUM:
            return "GL_INVALID_ENUM: An unacceptable value is specified for an enumerated argument.";
        case GL_INVALID_VALUE:
            return "GL_INVALID_VALUE: A numeric argument is out of range.";
        case GL_INVALID_OPERATION:
            return "GL_INVALID_OPERATION: The specified operation is not allowed in the current state.";
        case GL_OUT_OF_MEMORY:
            return "GL_OUT_OF_MEMORY: There is not enough memory left to execute the command.";
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            return "GL_INVALID_FRAMEBUFFER_OPERATION: The framebuffer object is not complete.";
        default:
            return "Unknown error";
    }
}

// Wrapper function to check and log OpenGL errors after a call
void CheckOpenGLError(const std::string& function)
{
    GLenum error = glGetError();
    if (error != GL_NO_ERROR)
    {
        std::string errorString = GetGLErrorString(error);
        Debug::Log("OpenGL Error in " + function + ": " + errorString, Level::Error);
    }
}

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
    Debug::Log("Sample buffer received in Draw " + std::to_string(texture));
    copyGStreamerTextureToFramebuffer(texture, data->textureID, 960, 720);
    // fillTextureWithColor(data->textureID, 960, 720, 0, 255, 0, 255);

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

void GstAVPipelineOpenGLES::fillTextureWithColor(GLuint textureID, GLsizei width, GLsizei height, GLubyte r, GLubyte g,
                                                 GLubyte b, GLubyte a)
{
    // Step 1: Bind the texture
    glBindTexture(GL_TEXTURE_2D, textureID);

    // Step 2: Create a solid color pattern (e.g., blue color)
    std::vector<GLubyte> colorData(width * height * 4, 0); // 4 bytes per pixel (RGBA)
    for (size_t i = 0; i < colorData.size(); i += 4)
    {
        colorData[i] = r;     // Red
        colorData[i + 1] = g; // Green
        colorData[i + 2] = b; // Blue
        colorData[i + 3] = a; // Alpha
    }

    // Step 3: Update texture data
    // glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, colorData.data());
    glTexSubImage2D(GL_TEXTURE_2D,    // target
                    0,                // level
                    0, 0,             // xoffset, yoffset
                    width, height,    // width, height
                    GL_RGBA,          // format
                    GL_UNSIGNED_BYTE, // type
                    colorData.data()  // pixels
    );

    // Step 4: Unbind texture
    glBindTexture(GL_TEXTURE_2D, 0);

    // Check for OpenGL errors
    GLenum error = glGetError();
    if (error != GL_NO_ERROR)
    {
        Debug::Log("Error while filling texture with color: " + std::to_string(error), Level::Error);
    }
    Debug::Log("Texture filled with color: " + std::to_string(textureID));
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

// to do proper release of the _surface_left / java method
void GstAVPipelineOpenGLES::ReleaseTexture(void* texture)
{
    _leftData->nativeWindow = nullptr;
    _rightData->nativeWindow = nullptr;
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
            Debug::Log("Got need context " + std::string(context_type) + " " + msg->src->name + "  " + msg->src->parent->name);

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

    Debug::Log("Sample received in on_new_sample");

    return GST_FLOW_OK;
}

GstElement* GstAVPipelineOpenGLES::add_appsink(GstElement* pipeline)
{
    GstElement* appsink = gst_element_factory_make("appsink", nullptr);
    if (!appsink)
    {
        Debug::Log("Failed to create appsink", Level::Error);
        return nullptr;
    }

    GstCaps* caps = gst_caps_from_string(
        "video/x-raw(memory:GLMemory),format=RGBA,width=960,height=720,framerate=(fraction)30/1,texture-target=2D");
    g_object_set(appsink, "caps", caps, /*"drop", true, "max-buffers", 1, "processing-deadline", 0,*/ nullptr);
    gst_caps_unref(caps);

    gst_bin_add(GST_BIN(pipeline), appsink);
    return appsink;
}

void GstAVPipelineOpenGLES::createCustomPipeline()
{
    Debug::Log("Create custom pipeline");
    GstElement* gltestsrc = add_by_name(pipeline_, "gltestsrc");
    g_object_set(gltestsrc, "pattern", 0, "is-live", true, nullptr);

    GstElement* queue = add_by_name(pipeline_, "queue");

    GstElement* appsink = add_appsink(pipeline_);

    GstAppSinkCallbacks callbacks = {nullptr};
    callbacks.new_sample = on_new_sample;

    gst_app_sink_set_callbacks(GST_APP_SINK(appsink), &callbacks, _leftData.get(), nullptr);

    if (!gst_element_link_many(gltestsrc, queue, appsink, nullptr))
    {
        Debug::Log("Elements filler could not be linked.");
    }

    GstElement* gltestsrc2 = add_by_name(pipeline_, "gltestsrc");
    g_object_set(gltestsrc2, "pattern", 13, "is-live", true, nullptr);

    GstElement* queue2 = add_by_name(pipeline_, "queue");

    GstElement* appsink2 = add_appsink(pipeline_);

    gst_app_sink_set_callbacks(GST_APP_SINK(appsink2), &callbacks, _rightData.get(), nullptr);

    if (!gst_element_link_many(gltestsrc2, queue2, appsink2, nullptr))
    {
        Debug::Log("Elements filler could not be linked.");
    }

    Debug::Log("End creating");
}
