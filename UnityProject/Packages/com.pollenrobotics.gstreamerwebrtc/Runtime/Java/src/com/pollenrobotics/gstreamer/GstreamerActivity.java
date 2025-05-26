package com.pollenrobotics.gstreamer;

import android.opengl.GLES20;
import android.os.Bundle;
import android.system.ErrnoException;
import android.system.Os;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.widget.FrameLayout;
import com.unity3d.player.UnityPlayerActivity;
import org.freedesktop.gstreamer.GStreamer;
import java.io.File;

public class GstreamerActivity extends UnityPlayerActivity {

    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        Log.d("GstreamerActivity", "onCreate called!");

        try {            
            //File externalFilesDir = getExternalFilesDir(null);
            String path = getExternalFilesDir(null).getAbsolutePath();
            Log.d("GstreamerActivity", "External files directory: " + path);
            /*if (externalFilesDir != null) {
                // Afficher le chemin du répertoire
                String directoryPath = externalFilesDir.getAbsolutePath();
                Log.d("DirectoryPath", "Chemin du répertoire: " + directoryPath);

                // Afficher uniquement le nom du répertoire (dernière partie du chemin)
                String directoryName = externalFilesDir.getName();
                Log.d("DirectoryName", "Nom du répertoire: " + directoryName);
            } else {
                Log.e("DirectoryPath", "Impossible d'obtenir le répertoire de stockage externe.");
            }*/
            /*Os.setenv(
                "GST_DEBUG_FILE",
                path + "/gstreamer.log",
                //"/storage/emulated/0/Android/data/com.DefaultCompany.UnityProject/files/gstreamer.log",
                //"/sdcard/Android/data/com.DefaultCompany.UnityProject/files/gstreamer/gstreamer.log",
                true
            );*/
            Os.setenv(
                "GST_DEBUG_DUMP_DOT_DIR",
                path + "/log_dot",
                true
            );
            Os.setenv(
                "GST_TRACERS",
                //path + "/buffer_lateness.log",
                "buffer-lateness(file=\""+path+"/buffer_lateness.log\")",
                //"/storage/emulated/0/Android/data/com.DefaultCompany.UnityProject/files/buffer_lateness.log",
                //"/sdcard/Android/data/com.DefaultCompany.UnityProject/files/gstreamer/gstreamer.log",
                true
            );  
            Os.setenv("GST_DEBUG_NO_COLOR", "1", true);
            Os.setenv("GST_DEBUG", "3", true);
        } catch (ErrnoException ex) {
            Log.d(
                "OverrideActivity",
                "ErrnoException caught: " + ex.getMessage()
            );
        }
        System.loadLibrary("gstreamer_android");
        try {
            GStreamer.init(this);
        } catch (Exception e) {
            e.printStackTrace();
        }

    }
}
