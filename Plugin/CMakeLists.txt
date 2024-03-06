cmake_minimum_required(VERSION 3.10)
project(UnityGStreamerPlugin)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)

pkg_check_modules(GST REQUIRED gstreamer-1.0)

include_directories(${GST_INCLUDE_DIRS})

add_definitions(${GST_CFLAGS_OTHER})

set(SOURCE_FILES
    src/RenderingPlugin.cpp
	src/RenderAPI.cpp
	src/RenderAPI_D3D11.cpp
	src/RenderAPI_D3D12.cpp
	src/RenderAPI_Vulkan.cpp
	src/RenderAPI_Metal.mm
	src/RenderAPI_OpenGLCoreES.cpp
	src/RenderAPI.h
	src/PlatformBase.h
	src/DebugLog.cpp
	src/DebugLog.h
	src/GstAVPipeline.cpp
	src/GstAVPipeline.h
	
	src/Unity/IUnityGraphics.h
	src/Unity/IUnityGraphicsD3D9.h
	src/Unity/IUnityGraphicsD3D11.h
	src/Unity/IUnityGraphicsD3D12.h
	src/Unity/IUnityGraphicsMetal.h
	src/Unity/IUnityGraphicsVulkan.h
	src/Unity/IUnityInterface.h
	
	src/gl3w/gl3w.c
	src/gl3w/gl3w.h
	src/gl3w/glcorearb.h
)

link_directories(${GST_LIBRARY_DIRS})

add_library(UnityGStreamerPlugin SHARED ${SOURCE_FILES})

target_link_libraries(UnityGStreamerPlugin ${GST_LIBRARIES} opengl32)

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
