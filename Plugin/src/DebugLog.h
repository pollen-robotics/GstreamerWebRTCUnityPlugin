#pragma once
#include<stdio.h>
#include <string>
#include <stdio.h>
#include <sstream>

#define DLLExport __declspec(dllexport)

extern "C"
{
    //Create a callback delegate
    typedef void(*FuncCallBack)(const char* message, int Level, int size);
    static FuncCallBack callbackInstance = nullptr;
    DLLExport void RegisterDebugCallback(FuncCallBack cb);
}

//Level Enum
enum class Level { Info, Warning, Error};

class  Debug
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