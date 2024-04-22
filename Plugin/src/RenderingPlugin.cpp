// Example low level rendering Unity plugin

#include "PlatformBase.h"
#include "RenderAPI.h"

#include <assert.h>

#include "GstAVPipeline.h"

static std::unique_ptr<GstAVPipeline> gstAVPipeline = nullptr;
static IUnityInterfaces* s_UnityInterfaces = NULL;
static IUnityGraphics* s_Graphics = NULL;

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API CreateDevice() { gstAVPipeline->CreateDevice(); }

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API CreatePipeline(const char* uri, const char* remote_peer_id)
{
    gstAVPipeline->CreatePipeline(uri, remote_peer_id);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API CreateTexture(unsigned int width, unsigned int height, bool left)
{
    gstAVPipeline->CreateTexture(width, height, left);
}

extern "C" UNITY_INTERFACE_EXPORT void* UNITY_INTERFACE_API GetTexturePtr(bool left)
{
    return gstAVPipeline->GetTexturePtr(left);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API ReleaseTexture(void* texPtr)
{
    gstAVPipeline->ReleaseTexture((ID3D11Texture2D*)texPtr);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API DestroyPipeline() { gstAVPipeline->DestroyPipeline(); }

/*
extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetTexture(void*
textureHandle, int w, int h)
{
        gstAVPipeline->SetTextureFromUnity(textureHandle, w, h);
}*/

// --------------------------------------------------------------------------
// UnitySetInterfaces

static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType);

static void ConfigureEnvironmentVariables()
{
    // Récupère la valeur actuelle de la variable PATH
    char path[1024];
    GetEnvironmentVariable("PATH", path, sizeof(path));

    // Récupère la valeur de la variable GSTREAMER_1_0_ROOT_MSVC_X86_64
    char gstreamer_root[1024];
    GetEnvironmentVariable("GSTREAMER_1_0_ROOT_MSVC_X86_64", gstreamer_root, sizeof(gstreamer_root));

    // Ajoute le chemin vers le dossier bin de GSTREAMER_1_0_ROOT_MSVC_X86_64 à
    // la variable PATH
    std::string new_path = path;
    new_path += ";";
    new_path += gstreamer_root;
    new_path += "\\bin";

    // Met à jour la variable d'environnement PATH
    if (!SetEnvironmentVariable("PATH", new_path.c_str()))
    {
        // La fonction SetEnvironmentVariable a échoué
        DWORD error_code = GetLastError();

        // std::cout << "Erreur : impossible de mettre à jour la variable
        // d'environnement PATH. Code d'erreur : " << error_code << std::endl;
    }

    // La variable d'environnement PATH a été mise à jour avec succès
    // std::cout << "La variable d'environnement PATH a été mise à jour avec
    // succès." << std::endl;
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces* unityInterfaces)
{
    s_UnityInterfaces = unityInterfaces;
    s_Graphics = s_UnityInterfaces->Get<IUnityGraphics>();
    s_Graphics->RegisterDeviceEventCallback(OnGraphicsDeviceEvent);

#if SUPPORT_VULKAN
    if (s_Graphics->GetRenderer() == kUnityGfxRendererNull)
    {
        extern void RenderAPI_Vulkan_OnPluginLoad(IUnityInterfaces*);
        RenderAPI_Vulkan_OnPluginLoad(unityInterfaces);
    }
#endif // SUPPORT_VULKAN

    // Run OnGraphicsDeviceEvent(initialize) manually on plugin load
    OnGraphicsDeviceEvent(kUnityGfxDeviceEventInitialize);

    ConfigureEnvironmentVariables();
    SetEnvironmentVariable("GST_DEBUG_FILE", "Logs\\gstreamer.log");
    // SetEnvironmentVariable("GST_DEBUG", "h264decoder:6");
    // SetEnvironmentVariable("GST_TRACERS", "buffer - lateness(file =
    // \"buffer_lateness.log\")");
    gst_debug_set_default_threshold(GST_LEVEL_INFO);
    gst_init(nullptr, nullptr);
    gstAVPipeline = std::make_unique<GstAVPipeline>(s_UnityInterfaces);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginUnload()
{
    s_Graphics->UnregisterDeviceEventCallback(OnGraphicsDeviceEvent);
    gstAVPipeline.reset();
    gst_deinit();
}

#if UNITY_WEBGL
typedef void(UNITY_INTERFACE_API* PluginLoadFunc)(IUnityInterfaces* unityInterfaces);
typedef void(UNITY_INTERFACE_API* PluginUnloadFunc)();

extern "C" void UnityRegisterRenderingPlugin(PluginLoadFunc loadPlugin, PluginUnloadFunc unloadPlugin);

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API RegisterPlugin()
{
    UnityRegisterRenderingPlugin(UnityPluginLoad, UnityPluginUnload);
}
#endif

// --------------------------------------------------------------------------
// GraphicsDeviceEvent

static RenderAPI* s_CurrentAPI = NULL;
static UnityGfxRenderer s_DeviceType = kUnityGfxRendererNull;

static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType)
{
    // Create graphics API implementation upon initialization
    if (eventType == kUnityGfxDeviceEventInitialize)
    {
        assert(s_CurrentAPI == NULL);
        s_DeviceType = s_Graphics->GetRenderer();
        s_CurrentAPI = CreateRenderAPI(s_DeviceType);
    }

    // Let the implementation process the device related events
    if (s_CurrentAPI)
    {
        s_CurrentAPI->ProcessDeviceEvent(eventType, s_UnityInterfaces);
    }

    // Cleanup graphics API implementation upon shutdown
    if (eventType == kUnityGfxDeviceEventShutdown)
    {
        delete s_CurrentAPI;
        s_CurrentAPI = NULL;
        s_DeviceType = kUnityGfxRendererNull;
    }
}

// --------------------------------------------------------------------------
// OnRenderEvent
// This will be called for GL.IssuePluginEvent script calls; eventID will
// be the integer passed to IssuePluginEvent. In this example, we just ignore
// that value.

static void UNITY_INTERFACE_API OnRenderEvent(int eventID)
{
    // Unknown / unsupported graphics device type? Do nothing
    if (s_CurrentAPI == NULL)
        return;

    if (eventID == 1)
    {
        gstAVPipeline->Draw(true);
        gstAVPipeline->Draw(false);
        /*ID3D11Texture2D* textures[2];
        textures[0] = gstAVPipeline->GetTexturePtr(true);
        textures[1] = gstAVPipeline->GetTexturePtr(false);
        s_CurrentAPI->Render((void**)textures);*/
    }
}

// --------------------------------------------------------------------------
// GetRenderEventFunc, an example function we export which is used to get a
// rendering event callback function.

extern "C" UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetRenderEventFunc() { return OnRenderEvent; }
