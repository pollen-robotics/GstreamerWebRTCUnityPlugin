package com.pollenrobotics.gstreamer;

import android.graphics.SurfaceTexture;
import android.opengl.EGL14;
import android.opengl.EGLContext;
import android.opengl.EGLDisplay;
import android.opengl.EGLSurface;
import android.opengl.GLES11Ext;
import android.opengl.GLES20;
import android.view.Surface;
import android.util.Log;

public class UnityRenderTexture implements SurfaceTexture.OnFrameAvailableListener {
    public interface OnInitializedListener{
        void onInitialized(Surface surface);
    }

    private SurfaceTexture surfaceTexture;
    private Surface surface;
    private final int width;
    private final int height;
    private int renderTextureId = -1;


    public UnityRenderTexture(int width, int height, OnInitializedListener listener){
        this.width              = width;
        this.height             = height;
        RenderingCallbackManager.Instance().SubscribeOneShot(eventCode->{
            int[] texIds = new int[1];
            GLES20.glGenTextures(1,texIds,0);
            renderTextureId = texIds[0];
            
            EGLContext unityContext = EGL14.eglGetCurrentContext();
            EGLDisplay unityDisplay = EGL14.eglGetCurrentDisplay();
            EGLSurface unityDrawSurface = EGL14.eglGetCurrentSurface(EGL14.EGL_DRAW);
            EGLSurface unityReadSurface = EGL14.eglGetCurrentSurface(EGL14.EGL_READ);
            EGL14.eglMakeCurrent(unityDisplay, unityDrawSurface, unityReadSurface, unityContext);

            GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
            GLES20.glBindTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, renderTextureId);            
            GLES20.glTexParameterf(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, GLES20.GL_TEXTURE_MIN_FILTER, GLES20.GL_LINEAR);
            GLES20.glTexParameterf(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, GLES20.GL_TEXTURE_MAG_FILTER, GLES20.GL_LINEAR);
            GLES20.glTexParameterf(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, GLES20.GL_TEXTURE_WRAP_S, GLES20.GL_REPEAT);
            GLES20.glTexParameterf(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, GLES20.GL_TEXTURE_WRAP_T, GLES20.GL_REPEAT);

            surfaceTexture = new SurfaceTexture(renderTextureId);
            surfaceTexture.setDefaultBufferSize(width, height);
            surfaceTexture.setOnFrameAvailableListener(this);

            surface = new Surface(surfaceTexture);
            listener.onInitialized(surface);

            /*RenderingCallbackManager.Instance().SubscribeRenderEvent(eventCode2->{
                surfaceTexture.updateTexImage();
            });*/

        });
    }


    public int getId(){
        return renderTextureId;        
    }
    public int getWidth(){
        return width;
    }
    public int getHeight() {
        return height;
    }
    public void release() {
        surface.release();
        surfaceTexture.release();
    }
    public SurfaceTexture GetTexture(){
            return surfaceTexture;
    }
    public Surface GetSurface(){
            return surface;
    }

    @Override
    public void onFrameAvailable(SurfaceTexture surfaceTexture) {
        RenderingCallbackManager.Instance().SubscribeOneShot(eventCode->{
                surfaceTexture.updateTexImage();            
        });
    }
}
