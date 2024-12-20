# GStreamer plugin for Unity

This GStreamer-based plugin enables Unity to communicate with a Reachy 2 through WebRTC channels. It can receive stereo video streams with audio, as well as data.

## Requirements

Currently, only the Windows platform is supported. 
The GStreamer library 1.24 is required. Download the [runtime](https://gstreamer.freedesktop.org/data/pkg/windows/1.24.8/msvc/gstreamer-1.0-msvc-x86_64-1.24.8.msi) installer.

For developing, the project requires [CMake](https://cmake.org/) to be generated and then Visual Studio to be built (tested with VS 2022). The [gstreamer development installer](https://gstreamer.freedesktop.org/data/pkg/windows/1.24.8/msvc/gstreamer-1.0-devel-msvc-x86_64-1.24.8.msi) is also required.


## Installation

From the *Window/Package Manager*, hit the + button and *add package from git URL*:
```
https://github.com/pollen-robotics/GstreamerWebRTCUnityPlugin.git?path=/UnityProject/Packages/com.pollenrobotics.gstreamerwebrtc#main
```

## Build

### Clone the project

```console
git clone git@github.com:pollen-robotics/GstreamerWebRTCUnityPlugin.git
```

### Generate project

```console
cd GstreamerWebRTCUnityPlugin\Plugin
mkdir build
cd build
cmake -G "Visual Studio 17 2022" ..
````

(The same operation can also be performed with the CMake GUI)

### Compilation

```
cmake --build . --config Release
```

Alternatively, you can open the `*.sln` file in the build folder and build the project with Visual Studio (or press Ctrl+Shift+B).

## Testing

The `GstreamerWebRTCUnityPlugin\UnityProject` is an example project that uses this plugin. It receives and displays audiovisual streams while sending sound from the microphone. The connection to the data is limited to the opening of the service channel.

A running [docker_webrtc](https://github.com/pollen-robotics/docker_webrtc) service is required. The IP address of the machine running this service needs to be manually set in the properties of the GStreamer object in the gstreamer_scene.

## Debugging

The following environment variable may be set to get more information on the plugin.

```
GST_DEBUG_FILE=%userprofile%\gstreamer_debug.log
GST_DEBUG=4
GST_DEBUG_DUMP_DOT_DIR=%userprofile%\log_dot
GST_TRACERS=buffer-lateness(file="C:\\Users\\<name>\\buffer_lateness.log")
```

The tracers logs can be analyzed with [gstreamer scripts](https://gitlab.freedesktop.org/gstreamer/gst-plugins-rs/-/tree/main/utils/tracers/scripts?ref_type=heads). Make sure that the rstracers plugin is available : `gst-inspect-1.0.exe rstracers`

To generate the pipeline diagrams you need to call this line somewhere in the code
```cpp
GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(self->_pipeline), GST_DEBUG_GRAPH_SHOW_ALL, "pipeline");
```
Figure can be generated with [graphviz](https://graphviz.org/)
```console
dot -Tpng -O pipeline.dot
```

### Debug from Visual Studio

In the solution explorer, select **UnityGstreamerPlugin** as the startup project. In the *Confguration Properties/Debugging* of this project set the *Command* as *D:\UnityHub\2022.3.41f1\Editor\Unity.exe* and *Commands Arguments* as *-projectPath C:\Users\Fabien\Dev\UnityGstreamerPlugin\UnityProject* (with the right paths).

Hit F5 to start the debugging. The Unity project should start.