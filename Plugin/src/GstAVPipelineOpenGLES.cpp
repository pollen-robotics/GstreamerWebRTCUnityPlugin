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

void GstAVPipelineOpenGLES::CreateTextureAndSurfaces(JNIEnv* env, int width, int height, bool left)
{
    std::unique_ptr<AppData> data = std::make_unique<AppData>();

    GLuint renderTextureId;

    // Generate texture
    glGenTextures(1, &renderTextureId);

    CheckOpenGLError("glBindTexture");

    // Bind current EGL context and surfaces

    EGLContext unityContext = eglGetCurrentContext();
    EGLDisplay unityDisplay = eglGetCurrentDisplay();
    EGLSurface unityDrawSurface = eglGetCurrentSurface(EGL_DRAW);
    EGLSurface unityReadSurface = eglGetCurrentSurface(EGL_READ);

    if (unityContext == EGL_NO_CONTEXT)
    {
        Debug::Log("UnityEGLContext is invalid", Level::Error);
    }

    // eglMakeCurrent(unityDisplay, unityDrawSurface, unityReadSurface, unityContext);

    data->gl_display = gst_gl_display_egl_new_with_egl_display(unityDisplay);
    GstGLDisplay* display = gst_gl_display_new_with_type(GST_GL_DISPLAY_TYPE_EGL);
    data->gl_context =
        gst_gl_context_new_wrapped(&data->gl_display->parent, (guintptr)unityContext, GST_GL_PLATFORM_EGL, GST_GL_API_GLES2);
    // gst_gl_context_new(display);
    // gst_gl_context_activate(data->gl_context, true);

    glBindTexture(GL_TEXTURE_EXTERNAL_OES, renderTextureId);
    // Set texture parameters
    glTexParameterf(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameterf(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_REPEAT);

    CheckOpenGLError("glTexParameterf");

    Debug::Log("Texture created with ID: " + std::to_string(renderTextureId));

    data->textureID = renderTextureId;

    jclass surfaceTextureClass = env->FindClass("android/graphics/SurfaceTexture");
    if (!surfaceTextureClass)
    {
        Debug::Log("Unable to find SurfaceTexture class", Level::Error);
        return;
    }

    jmethodID surfaceTextureCtor = env->GetMethodID(surfaceTextureClass, "<init>", "(I)V");
    if (!surfaceTextureCtor)
    {
        Debug::Log("Unable to find SurfaceTexture constructor", Level::Error);
        return;
    }

    data->surfaceTexture = env->NewObject(surfaceTextureClass, surfaceTextureCtor, renderTextureId);
    if (!data->surfaceTexture || env->ExceptionCheck())
    {
        env->ExceptionDescribe();
        env->ExceptionClear();
        Debug::Log("Failed to create SurfaceTexture", Level::Error);
        return;
    }

    _updateTexImageMethod = env->GetMethodID(surfaceTextureClass, "updateTexImage", "()V");
    if (!_updateTexImageMethod)
    {
        Debug::Log("Unable to find updateTexImage method", Level::Error);
        return;
    }

    jmethodID setDefaultBufferSizeMethod = env->GetMethodID(surfaceTextureClass, "setDefaultBufferSize", "(II)V");
    if (!setDefaultBufferSizeMethod)
    {
        Debug::Log("Unable to find setDefaultBufferSize method", Level::Error);
        return;
    }

    env->CallVoidMethod(data->surfaceTexture, setDefaultBufferSizeMethod, width, height);
    if (env->ExceptionCheck())
    {
        env->ExceptionDescribe();
        env->ExceptionClear();
        Debug::Log("Failed to set default buffer size on SurfaceTexture", Level::Error);
        return;
    }

    jclass surfaceClass = env->FindClass("android/view/Surface");
    if (!surfaceClass)
    {
        Debug::Log("Unable to find Surface class", Level::Error);
        return;
    }

    jmethodID surfaceCtor = env->GetMethodID(surfaceClass, "<init>", "(Landroid/graphics/SurfaceTexture;)V");
    if (!surfaceCtor)
    {
        Debug::Log("Unable to find Surface constructor", Level::Error);
        return;
    }

    jobject surfaceObject = env->NewObject(surfaceClass, surfaceCtor, data->surfaceTexture);
    if (!surfaceObject || env->ExceptionCheck())
    {
        env->ExceptionDescribe();
        env->ExceptionClear();
        Debug::Log("Failed to create Surface", Level::Error);
        return;
    }

    data->nativeWindow = SetNativeWindow(env, surfaceObject);

    if (left)
        _leftData = std::move(data);
    else
        _rightData = std::move(data);

    // eglMakeCurrent(unityDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    // eglMakeCurrent(unityDisplay, unityDrawSurface, unityReadSurface, unityContext);
}

void GstAVPipelineOpenGLES::Draw(JNIEnv* env, bool left)
{
    /*EGLDisplay unityDisplay = eglGetCurrentDisplay();
    EGLContext unityContext = eglGetCurrentContext();
    EGLSurface unityDrawSurface = eglGetCurrentSurface(EGL_DRAW);
    EGLSurface unityReadSurface = eglGetCurrentSurface(EGL_READ);
    // eglMakeCurrent(unityDisplay, unityDrawSurface, unityReadSurface, unityContext);*/
    jobject surfaceTexture;
    if (left)
    {

        surfaceTexture = _leftData->surfaceTexture;
        if (surfaceTexture == nullptr)
        {
            Debug::Log("SurfaceTexture left is null ", Level::Error);
            return;
        }
        // gst_gl_context_activate(_leftData->gl_context, true);
    }
    else
    {
        surfaceTexture = _rightData->surfaceTexture;
        if (surfaceTexture == nullptr)
        {
            Debug::Log("SurfaceTexture right is null ", Level::Error);
            return;
        }
        // gst_gl_context_activate(_rightData->gl_context, true);
    }

    // Call the updateTexImage method
    env->CallVoidMethod(surfaceTexture, _updateTexImageMethod);

    // Check for JNI exceptions
    if (env->ExceptionCheck())
    {
        env->ExceptionDescribe();
        env->ExceptionClear();
        Debug::Log("Exception occurred while updating the SurfaceTexture image", Level::Error);
    }
    // eglMakeCurrent(unityDisplay, unityDrawSurface, unityReadSurface, unityContext);
    //  eglMakeCurrent(unityDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    //   Debug::Log("Draw");
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

// GLuint GstAVPipelineOpenGLES::CreateTexture(bool left) { return left ? _textureIDLeft : _textureIDRight; }

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

ANativeWindow* GstAVPipelineOpenGLES::SetNativeWindow(JNIEnv* env, jobject surface)
{
    Debug::Log("Set Native Window ");
    if (!isSurfaceValid(env, surface))
    {
        Debug::Log("Surface is not valid", Level::Error);
        return nullptr;
    }
    Debug::Log("Surface is valid. Getting window");
    ANativeWindow* nativeWindow = ANativeWindow_fromSurface(env, surface);
    if (nativeWindow == nullptr)
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
        return nullptr;
    }

    return nativeWindow;
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
                GstGLContext* gl_context = nullptr;
                if (g_strcmp0(msg->src->parent->name, "glimagesink_left") == 0)
                {
                    Debug::Log("set left context");
                    gl_context = self->_leftData->gl_context;
                }
                else if (g_strcmp0(msg->src->parent->name, "glimagesink_right") == 0)
                {
                    Debug::Log("set right context");
                    gl_context = self->_rightData->gl_context;
                }

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

void GstAVPipelineOpenGLES::createCustomPipeline()
{
    Debug::Log("Create custom pipeline");
    GstElement* gltestsrc = add_by_name(pipeline_, "gltestsrc");
    g_object_set(gltestsrc, "pattern", 0, "is-live", true, nullptr);
    GstElement* capsfilter = gst_element_factory_make("capsfilter", nullptr);
    GstCaps* caps = gst_caps_from_string("video/x-raw(memory:GLMemory),width=960,height=720");
    g_object_set(capsfilter, "caps", caps, nullptr);
    gst_bin_add(GST_BIN(pipeline_), capsfilter);
    gst_caps_unref(caps);

    GstElement* queue = add_by_name(pipeline_, "queue");

    GstElement* glimagesink = gst_element_factory_make("glimagesink", "glimagesink_left");
    if (!glimagesink)
        Debug::Log("Failed to create glimagesink_left", Level::Error);

    gst_bin_add(GST_BIN(pipeline_), glimagesink);

    if (_leftData->nativeWindow == nullptr)
    {
        Debug::Log("Native left window is null", Level::Error);
    }

    gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(glimagesink), (guintptr)_leftData->nativeWindow);

    if (!gst_element_link_many(gltestsrc, capsfilter, queue, glimagesink, nullptr))
    {
        Debug::Log("Elements filler could not be linked.");
    }

    GstElement* gltestsrc2 = add_by_name(pipeline_, "gltestsrc");
    g_object_set(gltestsrc2, "pattern", 13, "is-live", true, nullptr);
    GstElement* capsfilter2 = gst_element_factory_make("capsfilter", nullptr);
    GstCaps* caps2 = gst_caps_from_string("video/x-raw(memory:GLMemory),width=960,height=720");
    g_object_set(capsfilter2, "caps", caps2, nullptr);
    gst_bin_add(GST_BIN(pipeline_), capsfilter2);
    gst_caps_unref(caps2);

    GstElement* queue2 = add_by_name(pipeline_, "queue");

    GstElement* glimagesink2 = gst_element_factory_make("glimagesink", "glimagesink_right");
    if (!glimagesink2)
        Debug::Log("Failed to create glimagesink_right", Level::Error);

    gst_bin_add(GST_BIN(pipeline_), glimagesink2);

    if (_rightData->nativeWindow == nullptr)
    {
        Debug::Log("Native right window is null", Level::Error);
    }

    gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(glimagesink2), (guintptr)_rightData->nativeWindow);

    if (!gst_element_link_many(gltestsrc2, capsfilter2, queue2, glimagesink2, nullptr))
    {
        Debug::Log("Elements filler could not be linked.");
    }

    gst_element_sync_state_with_parent(gltestsrc2);
    gst_element_sync_state_with_parent(capsfilter2);
    gst_element_sync_state_with_parent(glimagesink2);

    Debug::Log("End creating");
}