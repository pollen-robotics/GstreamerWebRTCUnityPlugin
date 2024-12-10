package com.pollenrobotics.gstreamer;

import java.util.ArrayList;

public class RenderingCallbackManager {
    @SuppressWarnings("JniMissingFunction")
    public native void nativeInit();

    private RenderingCallbackManager(){}
    private static RenderingCallbackManager _instance;
    public static RenderingCallbackManager Instance(){
        if(_instance == null)
        {
            _instance = new RenderingCallbackManager();
            _instance.nativeInit();
        }
        return _instance;
    }
    private final ArrayList<OnUnityRenderListener> listeners = new ArrayList<>();
    private final ArrayList<OnUnityRenderListener> oneShotListeners = new ArrayList<>();
    public interface OnUnityRenderListener{
        void onUnityRender(int eventCode);
    }
    public void SubscribeRenderEvent(OnUnityRenderListener listener){
        synchronized (listeners)
        {
            listeners.add(listener);
        }
    }
    public void UnsubscribeRenderEvent(OnUnityRenderListener listener){
        synchronized (listeners)
        {
            listeners.remove(listener);
        }
    }
    public void SubscribeOneShot(OnUnityRenderListener listener){
        synchronized (oneShotListeners)
        {
            oneShotListeners.add(listener);
        }
    }
    private void JavaOnRenderEvent(int eventCode){
        synchronized (listeners){
            for (OnUnityRenderListener listener:listeners) {
                listener.onUnityRender(eventCode);
            }
        }
        synchronized (oneShotListeners){
            for (OnUnityRenderListener listener:oneShotListeners) {
                listener.onUnityRender(eventCode);
            }
            oneShotListeners.clear();
        }
    }
}
