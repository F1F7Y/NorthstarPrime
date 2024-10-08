#pragma once

#include "logging/logging.h"

inline std::mutex g_LogMutex;

#if defined(LAUNCHER) || defined(WSOCKPROXY)
void LogMsg(eLogLevel eLevel, const char* pszMessage, int nCode);
#endif

void DevMsg(eLog eContext, const char* fmt, ...);
void Warning(eLog eContext, const char* fmt, ...);
void Error(eLog eContext, int nCode, const char* fmt, ...);

void CoreMsg(eLog eContext, eLogLevel eLevel, const int iCode, const char* pszName, const char* fmt, ...);

const char* Log_GetContextString(eLog eContext);

#ifdef NORTHSTAR
void PluginMsg(eLogLevel eLevel, const char* pszName, const char* fmt, ...);
#endif
