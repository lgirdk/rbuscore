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

// rtLog.h

#ifndef RT_LOG_H_
#define RT_LOG_H_
#include <stdint.h>

#ifdef __APPLE__
typedef uint64_t rtThreadId;
#define RT_THREADID_FMT PRIu64
#elif defined WIN32
typedef unsigned long rtThreadId;
#define RT_THREADID_FMT "l"
#else
typedef int32_t rtThreadId;
#define RT_THREADID_FMT "d"
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
  RT_LOG_DEBUG = 0,
  RT_LOG_INFO  = 1,
  RT_LOG_WARN  = 2,
  RT_LOG_ERROR = 3,
  RT_LOG_FATAL = 4
} rtLogLevel;

typedef enum
{
  RT_USE_RTLOGGER,
  RT_USE_RDKLOGGER
}rtLoggerSelection;

typedef void (*rtLogHandler)(rtLogLevel level, const char* file, int line, int threadId, char* message);

void rtLog_SetLevel(rtLogLevel l);
void rtLogSetLogHandler(rtLogHandler logHandler);


const char* rtLogLevelToString(rtLogLevel level);
rtLogLevel  rtLogLevelFromString(const char* s);

void rtLog_SetOption(rtLoggerSelection option);

#ifdef __GNUC__
#define RT_PRINTF_FORMAT(IDX, FIRST) __attribute__ ((format (printf, IDX, FIRST)))
#else
#define RT_PRINTF_FORMAT(IDX, FIRST)
#endif

void rtLogPrintf(rtLogLevel level, const char* file, int line, const char* format, ...) RT_PRINTF_FORMAT(4, 5);

#define rtLog_Debug(FORMAT...) rtLogPrintf(RT_LOG_DEBUG, __FILE__, __LINE__, FORMAT)
#define rtLog_Info(FORMAT...)  rtLogPrintf(RT_LOG_INFO,  __FILE__, __LINE__, FORMAT)
#define rtLog_Warn(FORMAT...)  rtLogPrintf(RT_LOG_WARN,  __FILE__, __LINE__, FORMAT)
#define rtLog_Error(FORMAT...) rtLogPrintf(RT_LOG_ERROR, __FILE__, __LINE__, FORMAT)
#define rtLog_Fatal(FORMAT...) rtLogPrintf(RT_LOG_FATAL, __FILE__, __LINE__, FORMAT)

#ifdef __cplusplus
}
#endif
#endif 

