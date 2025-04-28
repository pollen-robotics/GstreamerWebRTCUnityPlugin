#!/bin/bash

export GSTREAMER_ROOT_ANDROID=/mnt/data/fabien/android/gstreamer-1.0-android-universal-1.24.9/
#export ANDROID_NDK_ROOT=/mnt/data/fabien/android/Sdk/ndk/25.2.9519653
export ANDROID_NDK_ROOT=/mnt/data/fabien/unity/2022.3.61f1/Editor/Data/PlaybackEngines/AndroidPlayer/NDK

$ANDROID_NDK_ROOT/ndk-build NDK_PROJECT_PATH=. NDK_APPLICATION_MK=Application.mk

echo "copying libraries .."
cp -vR ./libs/arm64-v8a ../../UnityProject/Packages/com.pollenrobotics.gstreamerwebrtc/Runtime/Plugins/
#cp -vR ./src ../../UnityProject/Packages/com.pollenrobotics.gstreamerwebrtc/Runtime/Java/