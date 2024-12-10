/* Copyright(c) Pollen Robotics, all rights reserved.
 This source code is licensed under the license found in the
 LICENSE file in the root directory of this source tree. */

#pragma once
#include "Unity/IUnityInterface.h"
#include <sstream>
#include <stdio.h>
#include <string>

extern "C"
{
    // Create a callback delegate
    typedef void (*FuncCallBack)(const char* message, int Level, int size);
    static FuncCallBack callbackInstance = nullptr;
    void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API RegisterDebugCallback(FuncCallBack cb);
}

// Level Enum
enum class Level
{
    Info,
    Warning,
    Error
};

class Debug
{
public:
    static void Log(const char* message, Level Level = Level::Info);
    static void Log(const std::string message, Level Level = Level::Info);
    static void Log(const int message, Level Level = Level::Info);
    static void Log(const char message, Level Level = Level::Info);
    static void Log(const float message, Level Level = Level::Info);
    static void Log(const double message, Level Level = Level::Info);
    static void Log(const bool message, Level Level = Level::Info);

private:
    static void send_log(const std::stringstream& ss, const Level& Level);
};