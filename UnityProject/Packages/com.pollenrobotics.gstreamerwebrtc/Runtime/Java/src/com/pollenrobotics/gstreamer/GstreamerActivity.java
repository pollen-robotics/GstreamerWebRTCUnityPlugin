package com.pollenrobotics.gstreamer;
import com.unity3d.player.UnityPlayerActivity;
import android.os.Bundle;
import android.util.Log;
import org.freedesktop.gstreamer.GStreamer;
import android.system.Os;
import android.system.ErrnoException;
import android.view.Surface;

public class GstreamerActivity extends UnityPlayerActivity {

    public interface OnInitializedListener{
        void onInitialized(int textureId);
    }

  private static UnityRenderTexture renderTextureLeft;
  private static UnityRenderTexture renderTextureRight;

  protected void onCreate(Bundle savedInstanceState) {    
    super.onCreate(savedInstanceState);
    
    Log.d("GstreamerActivity", "onCreate called!");
    
    try{
        Os.setenv("GST_DEBUG_FILE", "/storage/emulated/0/Android/data/com.DefaultCompany.UnityProject/files/gstreamer.log", true);
        Os.setenv("GST_DEBUG_NO_COLOR", "1", true);
        Os.setenv("GST_DEBUG", "4", true);
    }
    catch (ErrnoException ex)
    {
        Log.d("OverrideActivity", "ErrnoException caught: " + ex.getMessage());
    }

    System.loadLibrary("gstreamer_android");
    try {
          GStreamer.init(this);
    } catch (Exception e) {
          e.printStackTrace();
    }

  }

  public static void InitExternalTexture(int width, int height, OnInitializedListener listenerLeft, OnInitializedListener listenerRight){
        renderTextureLeft = new UnityRenderTexture(width, height, surface->{
            listenerLeft.onInitialized(renderTextureLeft.getId());
        });
        renderTextureRight = new UnityRenderTexture(width, height, surface->{
            listenerRight.onInitialized(renderTextureRight.getId());
        });
  }

  public static Surface GetSurface(boolean left)
  {
    if(left)
        return renderTextureLeft.GetSurface();
    else
        return renderTextureRight.GetSurface();
  }

  public static void CleanSurfaces()
  {
        if(renderTextureLeft != null)
        {
            renderTextureLeft.release();
            renderTextureLeft = null;
        }
        if(renderTextureRight != null)
        {
            renderTextureRight.release();
            renderTextureRight = null;
        }
  }

}