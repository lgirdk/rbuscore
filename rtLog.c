/*
  * If not stated otherwise in this file or this component's Licenses.txt file
  * the following copyright and licenses apply:
  *
  * Copyright 2019 RDK Management
  *
  * Licensed under the Apache License, Version 2.0 (the "License");
  * you may not use this file except in compliance with the License.
  * You may obtain a copy of the License at
  *
  * http://www.apache.org/licenses/LICENSE-2.0
  *
  * Unless required by applicable law or agreed to in writing, software
  * distributed under the License is distributed on an "AS IS" BASIS,
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
*/  

/* 
 * This file contains code from pxCore which is Copyright 2005-2017 John Robinson
 * Licensed under the Apache 2.0 license.
*/

// rtLog.cpp

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "rtLog.h"

#ifndef WIN32
#include <unistd.h>
#include <sys/syscall.h>
#include <pthread.h>
#else
#include <Windows.h>
#define strcasecmp _stricmp
#endif

#ifndef RT_LOGPREFIX
#define RT_LOGPREFIX "rt:"
#endif

#ifdef ENABLE_RDKLOGGER
#include "rdk_debug.h"
#endif


#include <inttypes.h>

#ifdef __cplusplus
static void setLogLevelFromEnvironment()
#else
static void setLogLevelFromEnvironment() __attribute__((constructor));

void setLogLevelFromEnvironment()
#endif
{
  const char* s = getenv("RBUSCORE_LOG_LEVEL");
  if (s && strlen(s))
  {
    rtLogLevel level = RT_LOG_ERROR;
    if      (strcasecmp(s, "debug") == 0) level = RT_LOG_DEBUG;
    else if (strcasecmp(s, "info") == 0)  level = RT_LOG_INFO;
    else if (strcasecmp(s, "warn") == 0)  level = RT_LOG_WARN;
    else if (strcasecmp(s, "error") == 0) level = RT_LOG_ERROR;
    else if (strcasecmp(s, "fatal") == 0) level = RT_LOG_FATAL;
    else
    {
      fprintf(stderr, "invalid RBUSCORE_LOG_LEVEL set: %s", s);
      abort();
    }
    rtLog_SetLevel(level);
  }
}

#ifdef __cplusplus
struct LogLevelSetter
{
  LogLevelSetter()
  {
    setLogLevelFromEnvironment();
  }
};

static LogLevelSetter __logLevelSetter; // force RBUSCORE_LOG_LEVEL to be read from env
#endif

static const char* rtLogLevelStrings[] =
{
  "DEBUG",
  "INFO",
  "WARN",
  "ERROR",
  "FATAL"
};

static rtLogHandler sLogHandler = NULL;
static rtLogLevel sLevel = RT_LOG_FATAL;
static rtLoggerSelection sOption = RT_USE_RTLOGGER;
static const uint32_t numRTLogLevels = sizeof(rtLogLevelStrings)/sizeof(rtLogLevelStrings[0]);

rtThreadId rtThreadGetCurrentId();

#ifdef ENABLE_RDKLOGGER
rdk_LogLevel rdkLogLevelFromrtLogLevel(rtLogLevel level);
#endif

const char* rtLogLevelToString(rtLogLevel l)
{
  const char* s = "OUT-OF-BOUNDS";
  if (l < numRTLogLevels)
    s = rtLogLevelStrings[l];
  return s;
}

rtLogLevel rtLogLevelFromString(char const* s)
{
  rtLogLevel level = RT_LOG_INFO;
  if (s)
  {
    if (strcasecmp(s, "debug") == 0)
      level = RT_LOG_DEBUG;
    else if (strcasecmp(s, "info") == 0)
      level = RT_LOG_INFO;
    else if (strcasecmp(s, "warn") == 0)
      level = RT_LOG_WARN;
    else if (strcasecmp(s, "error") == 0)
      level = RT_LOG_ERROR;
    else if (strcasecmp(s, "fatal") == 0)
      level = RT_LOG_FATAL;
  }
  return level;
}

#ifdef ENABLE_RDKLOGGER
rdk_LogLevel rdkLogLevelFromrtLogLevel(rtLogLevel level)
{
  rdk_LogLevel rdklevel = RDK_LOG_INFO;
  switch (level)
  {
    case RT_LOG_FATAL:
      rdklevel = RDK_LOG_FATAL;
      break;
    case RT_LOG_ERROR:
      rdklevel = RDK_LOG_ERROR;
      break;
    case RT_LOG_WARN:
      rdklevel = RDK_LOG_WARN;
      break;
    case RT_LOG_INFO:
      rdklevel = RDK_LOG_INFO;
      break;
    case RT_LOG_DEBUG:
      rdklevel = RDK_LOG_DEBUG;
      break;
  }
  return rdklevel;
}
#endif

static const char* rtTrimPath(const char* s)
{
  if (!s)
    return s;

  const char* t = strrchr(s, (int) '/');
  if (t) t++;
  if (!t) t = s;

  return t;
}


void rtLogSetLogHandler(rtLogHandler logHandler)
{
  sLogHandler = logHandler;
}

void rtLog_SetLevel(rtLogLevel level)
{
  sLevel = level;
}

void rtLog_SetOption(rtLoggerSelection option)
{
  sOption = option;
}

#define RT_LOG_BUFFER_SIZE    1024
void rtLogPrintf(rtLogLevel level, const char* file, int line, const char* format, ...)
{
  size_t n = 0;
  char buff[RT_LOG_BUFFER_SIZE] = {0};

  const char* logLevel = rtLogLevelToString(level);
  const char* path = rtTrimPath(file);

  rtThreadId threadId = rtThreadGetCurrentId();

  va_list args;

  va_start(args, format);
  n = vsnprintf(buff, (RT_LOG_BUFFER_SIZE - 1), format, args);
  va_end(args);

  if (n > (RT_LOG_BUFFER_SIZE - 1))
  {
    buff[RT_LOG_BUFFER_SIZE - 4] = '.';
    buff[RT_LOG_BUFFER_SIZE - 3] = '.';
    buff[RT_LOG_BUFFER_SIZE - 2] = '.';
  }
  buff[RT_LOG_BUFFER_SIZE - 1] = '\0';

  if (NULL != sLogHandler)
  {
    if (level < sLevel)
      return;

    sLogHandler(level, path, line, threadId, buff);
  }
#ifdef ENABLE_RDKLOGGER
  else if (RT_USE_RDKLOGGER == sOption)
  {
    rdk_LogLevel rdklevel = rdkLogLevelFromrtLogLevel(level);
    RDK_LOG(rdklevel, "LOG.RDK.RTMESSAGE", "%s(%d) :%s\n", path, line, buff);
  }
#endif
  else
  {
    if (level < sLevel)
      return;

    printf(RT_LOGPREFIX "%5s %s:%d -- Thread-%" RT_THREADID_FMT ": %s \n", logLevel, path, line, threadId, buff);
  }
}

rtThreadId rtThreadGetCurrentId()
{
#ifdef __APPLE__
  uint64_t threadId = 0;
  pthread_threadid_np(NULL, &threadId);
  return threadId;
#elif WIN32
  return GetCurrentThreadId();
#else
  return syscall(__NR_gettid);
#endif
}
