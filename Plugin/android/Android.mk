LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)


#GSTREAMER_ROOT_ANDROID    :=  $(abspath $(LOCAL_PATH)/../arm64)
#SYSROOT  :=  $(abspath $(LOCAL_PATH))

LOCAL_MODULE    := UnityGStreamerPlugin
LOCAL_SRC_FILES := ../src/DebugLog.cpp ../src/RenderingPlugin.cpp ../src/GstDataPipeline.cpp ../src/GstBasePipeline.cpp ../src/GstAVPipeline.cpp ../src/GstAVPipelineOpenGLES.cpp ../src/GstMicPipeline.cpp

LOCAL_SHARED_LIBRARIES := gstreamer_android
LOCAL_LDLIBS := -llog -landroid -lEGL -lGLESv2
include $(BUILD_SHARED_LIBRARY)

ifndef GSTREAMER_ROOT_ANDROID
$(error GSTREAMER_ROOT_ANDROID is not defined!)
endif

ifeq ($(TARGET_ARCH_ABI),armeabi)
GSTREAMER_ROOT        := $(GSTREAMER_ROOT_ANDROID)/arm
else ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
GSTREAMER_ROOT        := $(GSTREAMER_ROOT_ANDROID)/armv7
else ifeq ($(TARGET_ARCH_ABI),arm64-v8a)
GSTREAMER_ROOT        := $(GSTREAMER_ROOT_ANDROID)/arm64
else ifeq ($(TARGET_ARCH_ABI),x86)
GSTREAMER_ROOT        := $(GSTREAMER_ROOT_ANDROID)/x86
else ifeq ($(TARGET_ARCH_ABI),x86_64)
GSTREAMER_ROOT        := $(GSTREAMER_ROOT_ANDROID)/x86_64
else
$(error Target arch ABI not supported: $(TARGET_ARCH_ABI))
endif

GSTREAMER_NDK_BUILD_PATH  := $(GSTREAMER_ROOT)/share/gst-android/ndk-build/

include $(GSTREAMER_NDK_BUILD_PATH)/plugins.mk

GSTREAMER_PLUGINS_CORE_CUSTOM := coreelements app audioconvert audiorate audioresample videorate videoconvertscale autodetect
GSTREAMER_PLUGINS_CODECS_CUSTOM := videoparsersbad vpx opus audioparsers opusparse androidmedia
GSTREAMER_PLUGINS_NET_CUSTOM := tcp rtsp rtp rtpmanager udp srtp webrtc dtls nice rswebrtc rsrtp sctp
GSTREAMER_PLUGINS_PLAYBACK := playback
GSTREAMER_PLUGINS_EFFECTS_CUSTOM := webrtcdsp
GSTREAMER_PLUGINS         := $(GSTREAMER_PLUGINS_CORE_CUSTOM) $(GSTREAMER_PLUGINS_CODECS_CUSTOM) $(GSTREAMER_PLUGINS_NET_CUSTOM) \
                             $(GSTREAMER_PLUGINS_ENCODING) \
                             $(GSTREAMER_PLUGINS_SYS) \
                             $(GSTREAMER_PLUGINS_PLAYBACK) \
                             $(GSTREAMER_PLUGINS_EFFECTS_CUSTOM)

GSTREAMER_EXTRA_DEPS      := gstreamer-video-1.0 glib-2.0 gstreamer-sdp-1.0 gstreamer-webrtc-nice-1.0 gstreamer-app-1.0

G_IO_MODULES = openssl

include $(GSTREAMER_NDK_BUILD_PATH)/gstreamer-1.0.mk