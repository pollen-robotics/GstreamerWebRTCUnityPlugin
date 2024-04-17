using UnityEngine;
using System;
using System.Runtime.InteropServices;
using UnityEngine.Rendering;
using UnityEngine.UI;

namespace GstreamerWebRTC
{
    public class GStreamerRenderingPlugin
    {

#if PLATFORM_SWITCH && !UNITY_EDITOR
    [DllImport("__Internal")]
    private static extern void RegisterPlugin();
#endif

#if (PLATFORM_IOS || PLATFORM_TVOS || PLATFORM_BRATWURST || PLATFORM_SWITCH) && !UNITY_EDITOR
    [DllImport("__Internal")]
#else
        [DllImport("UnityGStreamerPlugin")]
#endif
        private static extern void CreatePipeline(string uri, string remote_peer_id);

#if (PLATFORM_IOS || PLATFORM_TVOS || PLATFORM_BRATWURST || PLATFORM_SWITCH) && !UNITY_EDITOR
    [DllImport("__Internal")]
#else
        [DllImport("UnityGStreamerPlugin")]
#endif
        private static extern void CreateDevice();

#if (PLATFORM_IOS || PLATFORM_TVOS || PLATFORM_BRATWURST || PLATFORM_SWITCH) && !UNITY_EDITOR
    [DllImport("__Internal")]
#else
        [DllImport("UnityGStreamerPlugin")]
#endif
        private static extern void DestroyPipeline();

#if (PLATFORM_IOS || PLATFORM_TVOS || PLATFORM_BRATWURST || PLATFORM_SWITCH) && !UNITY_EDITOR
    [DllImport("__Internal")]
#else
        [DllImport("UnityGStreamerPlugin")]
#endif
        private static extern IntPtr GetTexturePtr(bool left);

#if (PLATFORM_IOS || PLATFORM_TVOS || PLATFORM_BRATWURST || PLATFORM_SWITCH) && !UNITY_EDITOR
    [DllImport("__Internal")]
#else
        [DllImport("UnityGStreamerPlugin")]
#endif
        private static extern void CreateTexture(uint width, uint height, bool left);

#if (PLATFORM_IOS || PLATFORM_TVOS || PLATFORM_BRATWURST || PLATFORM_SWITCH) && !UNITY_EDITOR
    [DllImport("__Internal")]
#else
        [DllImport("UnityGStreamerPlugin")]
#endif
        private static extern void ReleaseTexture(IntPtr texture);

#if (PLATFORM_IOS || PLATFORM_TVOS || PLATFORM_BRATWURST || PLATFORM_SWITCH) && !UNITY_EDITOR
    [DllImport("__Internal")]
#else
        [DllImport("UnityGStreamerPlugin")]
#endif
        private static extern IntPtr GetRenderEventFunc();

#if (PLATFORM_IOS || PLATFORM_TVOS || PLATFORM_BRATWURST || PLATFORM_SWITCH) && !UNITY_EDITOR
    [DllImport("__Internal")]
#else
        [DllImport("UnityGStreamerPlugin")]
#endif
        private static extern IntPtr GetTextureUpdateCallback();

        private IntPtr leftTextureNativePtr;

        private IntPtr rightTextureNativePtr;

        private string _signallingServerURL;
        private Signalling _signalling;

        public bool producer = false;
        public string remote_producer_name = "robot";

        const uint width = 960;
        const uint height = 720;

        //CommandBuffer _command = null;

        public GStreamerRenderingPlugin(string ip_address, ref RawImage leftRawImage, ref RawImage rightRawImage)
        {
            _signallingServerURL = "ws://" + ip_address + ":8443";

            _signalling = new Signalling(_signallingServerURL, producer, remote_producer_name);

            _signalling.event_OnRemotePeerId.AddListener(StartPipeline);

            CreateDevice();
            CreateRenderTexture(true, ref leftTextureNativePtr, ref leftRawImage);
            CreateRenderTexture(false, ref rightTextureNativePtr, ref rightRawImage);
            _signalling.Connect();
        }

        void CreateRenderTexture(bool left, ref IntPtr textureNativePtr, ref RawImage rawImage)
        {
            CreateTexture(width, height, left);
            textureNativePtr = GetTexturePtr(left);

            if (textureNativePtr != IntPtr.Zero)
            {
                var texture = Texture2D.CreateExternalTexture((int)width, (int)height, TextureFormat.RGBA32, mipChain: false, linear: true, textureNativePtr);
                rawImage.texture = texture;
            }
            else
            {
                Debug.LogError("Texture is null");
            }

        }

        void StartPipeline(string remote_peer_id)
        {
            Debug.Log("start pipe " + remote_peer_id);
            CreatePipeline(_signallingServerURL, remote_peer_id);
        }

        public void Cleanup()
        {
            //_command = null;
            _signalling.Close();
            DestroyPipeline();
            if (leftTextureNativePtr != IntPtr.Zero)
            {
                ReleaseTexture(leftTextureNativePtr);
                leftTextureNativePtr = IntPtr.Zero;
            }
            if (rightTextureNativePtr != IntPtr.Zero)
            {
                ReleaseTexture(rightTextureNativePtr);
                rightTextureNativePtr = IntPtr.Zero;
            }
            //_command.Dispose();
        }


        public void Render()
        {
            CommandBuffer _command = new CommandBuffer();
            _command.IssuePluginEvent(GetRenderEventFunc(), 1);
            Graphics.ExecuteCommandBuffer(_command);
        }

    }
}