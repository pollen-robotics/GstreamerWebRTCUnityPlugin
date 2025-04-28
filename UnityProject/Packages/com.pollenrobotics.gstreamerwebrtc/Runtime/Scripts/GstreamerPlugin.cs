/* Copyright(c) Pollen Robotics, all rights reserved.
 This source code is licensed under the license found in the
 LICENSE file in the root directory of this source tree. */

using UnityEngine;
using UnityEngine.UI;
using System.Threading;
using UnityEngine.Events;
using System.Collections;
using System;
using UnityEngine.Rendering;
using System.Runtime.InteropServices;
using System.IO;

namespace GstreamerWebRTC
{
    public class GStreamerPlugin : MonoBehaviour
    {
        [Tooltip("RawImage on which the left texture will be rendered")]
        public RawImage leftRawImage;
        [Tooltip("RawImage on which the right texture will be rendered")]
        public RawImage rightRawImage;


        protected GStreamerRenderingPlugin renderingPlugin = null;


        protected DebugFromPlugin debug = null;



        private Thread cleaning_thread = null;
        private Thread init_thread = null;

        public UnityEvent<bool> event_OnPipelineRenderingRunning = new UnityEvent<bool>();
        public UnityEvent<bool> event_OnPipelineDataRunning = new UnityEvent<bool>();

        private IntPtr nativeTexPtr;
        private bool nativeTexPtrSet;

        void OnEnable()
        {
            debug = new DebugFromPlugin();
        }

        void OnDestroy()
        {
            cleaning_thread?.Join();
        }

        void Start()
        {
            if (cleaning_thread != null)
            {
                cleaning_thread.Join();
            }



            //GStreamerRenderingPlugin has to run in main thread
            InitAV();
        }

        protected virtual void InitAV()
        {
            if (leftRawImage == null)
                Debug.LogError("Left image is not assigned!");

            if (rightRawImage == null)
                Debug.LogError("Right image is not assigned!");

            Texture left = null, right = null;
            renderingPlugin = new GStreamerRenderingPlugin(ref left, ref right);
#if UNITY_ANDROID
            StartCoroutine(WaitForNativePointer(renderingPlugin));
#elif (UNITY_EDITOR_WIN || UNITY_STANDALONE_WIN)
            leftRawImage.texture = left;
            rightRawImage.texture = right;
            renderingPlugin.event_OnPipelineStarted.AddListener(PipelineStarted);
            renderingPlugin.event_OnPipelineStopped.AddListener(PipelineStopped);
            renderingPlugin.Connect();
#endif
        }

#if UNITY_ANDROID
        private IEnumerator WaitForNativePointer(GStreamerRenderingPlugin renderingPlugin)
        {
            yield return new WaitUntil(() => renderingPlugin.IsNativePtrSet());
            leftRawImage.texture = renderingPlugin.SetTextures(true);
            rightRawImage.texture = renderingPlugin.SetTextures(false);
            //leftRawImage.material.SetTexture("_MainTex", renderingPlugin.SetTextures(true));
            //rightRawImage.material.SetTexture("_RightTex", renderingPlugin.SetTextures(false));
            renderingPlugin.event_OnPipelineStarted.AddListener(PipelineStarted);
            renderingPlugin.event_OnPipelineStopped.AddListener(PipelineStopped);
            //renderingPlugin.Connect();
            renderingPlugin.StartPipeline();
        }
#endif



        protected virtual void PipelineStarted()
        {
            Debug.Log("Pipeline started");
            event_OnPipelineRenderingRunning.Invoke(true);
        }

        protected virtual void PipelineDataStarted()
        {
            Debug.Log("Pipeline data started");
            event_OnPipelineDataRunning.Invoke(true);
        }

        protected virtual void PipelineStopped()
        {
            Debug.Log("Pipeline stopped");
            event_OnPipelineRenderingRunning.Invoke(false);
        }

        protected virtual void PipelineDataStopped()
        {
            Debug.Log("Pipeline data stopped");
            event_OnPipelineDataRunning.Invoke(false);
        }

        protected virtual void OnDisable()
        {
            if (init_thread != null)
            {
                init_thread.Join();
            }

            cleaning_thread = new Thread(Cleanup);
            cleaning_thread.Start();
        }

        void Cleanup()
        {
            renderingPlugin.Cleanup();
        }

        void Update()
        {
            renderingPlugin.Render();
        }

    }
}
