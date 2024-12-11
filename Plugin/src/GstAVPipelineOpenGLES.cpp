#include "GstAVPipelineOpenGLES.h"
#include "DebugLog.h"
#include <gst/video/videooverlay.h>

bool isSurfaceValid(JNIEnv* env, jobject surface)
{
    if (surface == nullptr)
        return false;
    Debug::Log("Check surface 1");
    jclass surfaceClass = env->GetObjectClass(surface);
    if (surfaceClass == nullptr)
    {
        Debug::Log("Failed to get Surface class");
        return false;
    }

    jclass classClass = env->FindClass("java/lang/Class");
    jmethodID getNameMethod = env->GetMethodID(classClass, "getName", "()Ljava/lang/String;");
    jstring className = (jstring)env->CallObjectMethod(surfaceClass, getNameMethod);
    const char* classNameChars = env->GetStringUTFChars(className, nullptr);
    Debug::Log("Surface class name: " + std::string(classNameChars));
    env->ReleaseStringUTFChars(className, classNameChars);

    Debug::Log("Check surface 2");
    jmethodID isValidMethod = env->GetMethodID(surfaceClass, "isValid", "()Z");
    if (isValidMethod == nullptr)
    {
        Debug::Log("isValid method not found");
        return false;
    }
    Debug::Log("Check surface 3");
    jboolean isValid = env->CallBooleanMethod(surface, isValidMethod);
    return isValid;
}

void GstAVPipelineOpenGLES::SetNativeWindow(JNIEnv* env, jobject surface, bool left)
{
    Debug::Log("Set Native Window");
    if (!isSurfaceValid(env, surface))
    {
        Debug::Log("Surface is not valid", Level::Error);
        return;
    }
    Debug::Log("Surface is valid. Getting window");
    ANativeWindow* _nativeWindow = ANativeWindow_fromSurface(env, surface);
    if (_nativeWindow == nullptr)
    {
        // ANativeWindow_fromSurface() failed
        if (env->ExceptionCheck())
        {
            // An exception was thrown by ANativeWindow_fromSurface()
            env->ExceptionDescribe();
            env->ExceptionClear();
            Debug::Log("ANativeWindow_fromSurface() failed with exception", Level::Error);
        }
        else
        {
            // ANativeWindow_fromSurface() failed for some other reason
            Debug::Log("ANativeWindow_fromSurface() failed", Level::Error);
        }
        return;
    }

    if (left)
    {
        Debug::Log("Left Native Window set");
        _nativeWindow_left = _nativeWindow;
    }
    else
    {
        Debug::Log("Right Native Window set");
        _nativeWindow_right = _nativeWindow;
    }
}

void GstAVPipelineOpenGLES::ReleaseTexture(void* texture)
{
    _nativeWindow_left = nullptr;
    _nativeWindow_right = nullptr;
}

GstElement* GstAVPipelineOpenGLES::add_autoaudiosink(GstElement* pipeline)
{
    GstElement* autoaudiosink = gst_element_factory_make("autoaudiosink", nullptr);
    if (!autoaudiosink)
    {
        Debug::Log("Failed to create audiosink", Level::Error);
        return nullptr;
    }
    gst_bin_add(GST_BIN(pipeline), autoaudiosink);
    return autoaudiosink;
}

GstElement* GstAVPipelineOpenGLES::add_videoconvert(GstElement* pipeline)
{
    GstElement* videoconvert = gst_element_factory_make("videoconvert", nullptr);
    if (!videoconvert)
    {
        Debug::Log("Failed to create videoconvert", Level::Error);
        return nullptr;
    }
    gst_bin_add(GST_BIN(pipeline), videoconvert);
    return videoconvert;
}

GstElement* GstAVPipelineOpenGLES::add_audioconvert(GstElement* pipeline)
{
    GstElement* audioconvert = gst_element_factory_make("audioconvert", nullptr);
    if (!audioconvert)
    {
        Debug::Log("Failed to create audioconvert", Level::Error);
        return nullptr;
    }
    gst_bin_add(GST_BIN(pipeline), audioconvert);
    return audioconvert;
}

GstElement* GstAVPipelineOpenGLES::add_glimagesink(GstElement* pipeline)
{
    GstElement* glimagesink = gst_element_factory_make("glimagesink", nullptr);
    if (!glimagesink)
    {
        Debug::Log("Failed to create glimagesink", Level::Error);
        return nullptr;
    }
    gst_bin_add(GST_BIN(pipeline), glimagesink);
    return glimagesink;
}

void GstAVPipelineOpenGLES::on_pad_added(GstElement* src, GstPad* new_pad, gpointer data)
{
    gchar* pad_name = gst_pad_get_name(new_pad);
    GstAVPipelineOpenGLES* avpipeline = static_cast<GstAVPipelineOpenGLES*>(data);

    Debug::Log("Adding pad ");
    if (g_str_has_prefix(pad_name, "video"))
    {
        std::lock_guard<std::mutex> lk(avpipeline->_lock);
        Debug::Log("Adding video pad " + std::string(pad_name));
        GstElement* queue = add_queue(avpipeline->pipeline_);
        GstElement* videoconvert = add_videoconvert(avpipeline->pipeline_);
        GstElement* glimagesink = add_glimagesink(avpipeline->pipeline_);

        if (g_str_has_prefix(pad_name, "video_0"))
        {
            Debug::Log("Connecting left video pad " + std::string(pad_name));
            gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(glimagesink), (guintptr)avpipeline->_nativeWindow_left);
        }
        else
        {
            Debug::Log("Connecting right video pad " + std::string(pad_name));
            gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(glimagesink), (guintptr)avpipeline->_nativeWindow_right);
        }

        if (!gst_element_link_many(queue, videoconvert, glimagesink, nullptr))
        {
            Debug::Log("Elements could not be linked.");
        }

        GstPad* sinkpad = gst_element_get_static_pad(queue, "sink");
        if (gst_pad_link(new_pad, sinkpad) != GST_PAD_LINK_OK)
        {
            Debug::Log("Could not link dynamic video pad to queue", Level::Error);
        }
        gst_object_unref(sinkpad);

        gst_element_sync_state_with_parent(queue);
        gst_element_sync_state_with_parent(videoconvert);
        gst_element_sync_state_with_parent(glimagesink);
    }
    else if (g_str_has_prefix(pad_name, "audio"))
    {
        Debug::Log("Adding audio pad " + std::string(pad_name));
        GstElement* queue = add_queue(avpipeline->pipeline_);
        GstElement* audioconvert = add_audioconvert(avpipeline->pipeline_);
        GstElement* audioresample = add_audioresample(avpipeline->pipeline_);
        GstElement* autoaudiosink = add_autoaudiosink(avpipeline->pipeline_);

        if (!gst_element_link_many(queue, audioconvert, audioresample, autoaudiosink, nullptr))
        {
            Debug::Log("Audio elements could not be linked.", Level::Error);
        }

        GstPad* sinkpad = gst_element_get_static_pad(queue, "sink");
        if (gst_pad_link(new_pad, sinkpad) != GST_PAD_LINK_OK)
        {
            Debug::Log("Could not link dynamic audio pad to queue", Level::Error);
        }
        gst_object_unref(sinkpad);

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
