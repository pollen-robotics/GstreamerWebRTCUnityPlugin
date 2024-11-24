cmake_minimum_required(VERSION 3.10)
project(UnityGStreamerPlugin)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)

pkg_check_modules(GST REQUIRED gstreamer-1.0)

set(GST_ARCH_INCLUDE_DIR ${GST_LIBRARY_DIRS}/gstreamer-1.0/include)
include_directories(${GST_INCLUDE_DIRS} ${GST_ARCH_INCLUDE_DIR})

add_definitions(${GST_CFLAGS_OTHER})
add_definitions(-DGST_USE_UNSTABLE_API)

set(SOURCE_FILES
    src/RenderingPlugin.cpp
	src/DebugLog.cpp
	src/DebugLog.h
	src/GstBasePipeline.h
	src/GstBasePipeline.cpp
	src/GstAVPipeline.cpp
	src/GstAVPipeline.h
	src/GstMicPipeline.cpp
	src/GstMicPipeline.h
	src/GstDataPipeline.cpp
	src/GstDataPipeline.h
	
	src/Unity/IUnityGraphics.h
	src/Unity/IUnityGraphicsD3D11.h
	src/Unity/IUnityInterface.h
)

link_directories(${GST_LIBRARY_DIRS})

add_library(UnityGStreamerPlugin SHARED ${SOURCE_FILES})

target_link_libraries(UnityGStreamerPlugin ${GST_LIBRARIES} gstd3d11-1.0 gstapp-1.0 gstvideo-1.0 gstwebrtc-1.0 gstsdp-1.0)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(TARGET_ARCH "x86_64")
else()
    set(TARGET_ARCH "x86")
endif()

add_custom_command(TARGET UnityGStreamerPlugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:UnityGStreamerPlugin>
            ${CMAKE_SOURCE_DIR}/../UnityProject/Packages/com.pollenrobotics.gstreamerwebrtc/Runtime/Plugins/${TARGET_ARCH}
    COMMENT "Copying UnityGStreamerPlugin.dll to destination directory")
