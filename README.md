# GStreamer plugin for Unity

This GStreamer-based plugin enables Unity to communicate with a Reachy 2 through WebRTC channels. It can receive stereo video streams with audio, as well as data.

## Requirements

Currently, only the Windows platform is supported. The project requires [CMake](https://cmake.org/) to be generated and then Visual Studio to be built (tested with VS 2022).

The GStreamer library 1.24 is required. Download the [runtime](https://gstreamer.freedesktop.org/data/pkg/windows/1.24.8/msvc/gstreamer-1.0-msvc-x86_64-1.24.8.msi) and [development](https://gstreamer.freedesktop.org/data/pkg/windows/1.24.8/msvc/gstreamer-1.0-devel-msvc-x86_64-1.24.8.msi) installers.

## Build

### Clone the project

```console
git clone git@github.com:pollen-robotics/GstreamerWebRTCUnityPlugin.git
```

### Generate project

```console
cd GstreamerWebRTCUnityPlugin\Plugin
mkdir build
cmake -G "Visual Studio 17 2022" ..
````

(The same operation can also be performed with the CMake GUI)

### Compilation

```
cmake --build . --config Release
```

Alternatively, you can open the `*.sln` file in the build folder and build the project with Visual Studio (or press Ctrl+Shift+B).

## Installation

The built DLL and script files are located in the `GstreamerWebRTCUnityPlugin\UnityProject\Packages\com.pollenrobotics.gstreamerwebrtc` folder.

Simply copy and paste this folder into the Packages folder of your Unity app.

## Testing

The `GstreamerWebRTCUnityPlugin\UnityProject` is an example project that uses this plugin. It receives and displays audiovisual streams while sending sound from the microphone. The connection to the data is limited to the opening of the service channel.

A running [docker_webrtc](https://github.com/pollen-robotics/docker_webrtc) service is required. The IP address of the machine running this service needs to be manually set in the properties of the GStreamer object in the gstreamer_scene.