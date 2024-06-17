#include "DebugLog.h"

#include <sstream>
#include <stdio.h>
#include <string.h>

//-------------------------------------------------------------------
void Debug::Log(const char* message, Level Level)
{
    if (callbackInstance != nullptr)
        callbackInstance(message, (int)Level, (int)strlen(message));
}

void Debug::Log(const std::string message, Level Level)
{
    const char* tmsg = message.c_str();
    if (callbackInstance != nullptr)
        callbackInstance(tmsg, (int)Level, (int)strlen(tmsg));
}

void Debug::Log(const int message, Level Level)
{
    std::stringstream ss;
    ss << message;
    send_log(ss, Level);
}

void Debug::Log(const char message, Level Level)
{
    std::stringstream ss;
    ss << message;
    send_log(ss, Level);
}

void Debug::Log(const float message, Level Level)
{
    std::stringstream ss;
    ss << message;
    send_log(ss, Level);
}

void Debug::Log(const double message, Level Level)
{
    std::stringstream ss;
    ss << message;
    send_log(ss, Level);
}

void Debug::Log(const bool message, Level Level)
{
    std::stringstream ss;
    if (message)
        ss << "true";
    else
        ss << "false";

    send_log(ss, Level);
}

void Debug::send_log(const std::stringstream& ss, const Level& Level)
{
    const std::string tmp = ss.str();
    const char* tmsg = tmp.c_str();
    if (callbackInstance != nullptr)
        callbackInstance(tmsg, (int)Level, (int)strlen(tmsg));
}
//-------------------------------------------------------------------

// Create a callback delegate
void RegisterDebugCallback(FuncCallBack cb) { callbackInstance = cb; }