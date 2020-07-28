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
#ifndef __RBUS_TYPES_H__
#define __RBUS_TYPES_H__ 
#include <stdio.h> //remove when RTW_LOG no longer has a printf dependency
#ifdef __cplusplus
extern "C" {
#endif
typedef enum
{
	RTMESSAGE_BUS_SUCCESS = 0,
	RTMESSAGE_BUS_ERROR_GENERAL,
	RTMESSAGE_BUS_ERROR_INVALID_PARAM,
	RTMESSAGE_BUS_ERROR_INSUFFICIENT_MEMORY,
	RTMESSAGE_BUS_ERROR_INVALID_STATE,
	RTMESSAGE_BUS_ERROR_REMOTE_END_DECLINED_TO_RESPOND,
	RTMESSAGE_BUS_ERROR_REMOTE_END_FAILED_TO_RESPOND,
	RTMESSAGE_BUS_ERROR_REMOTE_TIMED_OUT,
	RTMESSAGE_BUS_ERROR_MALFORMED_RESPONSE,
	RTMESSAGE_BUS_ERROR_UNSUPPORTED_METHOD,
	RTMESSAGE_BUS_ERROR_UNSUPPORTED_EVENT,
	RTMESSAGE_BUS_ERROR_OUT_OF_RESOURCES,
	RTMESSAGE_BUS_ERROR_DESTINATION_UNREACHABLE,
	RTMESSAGE_BUS_SUCCESS_ASYNC
} rbus_error_t;

typedef enum
{
	MESSAGE_CLASS_MESSAGE_v0 = 0,
	MESSAGE_CLASS_EVENT_v0,
	MESSAGE_CLASS_MAX
} rbus_message_class_t;


typedef enum _rbuscore_bus_status
{
    RBUSCORE_ENABLED = 0,
    RBUSCORE_ENABLED_PENDING,
    RBUSCORE_DISABLED_PENDING,
    RBUSCORE_DISABLED
} rbuscore_bus_status_t;

#define METHOD_INVALID "METHOD_INVALID"
#define METHOD_SETPARAMETERVALUES "METHOD_SETPARAMETERVALUES"
#define METHOD_GETPARAMETERVALUES "METHOD_GETPARAMETERVALUES"
#define METHOD_GETPARAMETERNAMES "METHOD_GETPARAMETERNAMES"
#define METHOD_SETPARAMETERATTRIBUTES "METHOD_SETPARAMETERATTRIBUTES"
#define METHOD_GETPARAMETERATTRIBUTES "METHOD_GETPARAMETERATTRIBUTES"
#define METHOD_COMMIT "METHOD_COMMIT"
#define METHOD_ADDTBLROW "METHOD_ADDTBLROW"
#define METHOD_DELETETBLROW "METHOD_DELETETBLROW"
#define METHOD_RPC "METHOD_RPC"
#define METHOD_RESPONSE "METHOD_RESPONSE"
#define METHOD_MAX "METHOD_MAX"

/*Message fields defined by rtwrapper*/
#define MESSAGE_FIELD_RESULT "_rtw_result"
#define MESSAGE_FIELD_PAYLOAD "_payload"
#define MESSAGE_FIELD_METHOD "_method"
#define MESSAGE_FIELD_EVENT_NAME "_event"
#define MESSAGE_FIELD_EVENT_SENDER "_esender"
#define MESSAGE_FIELD_EVENT_HAS_FILTER "_ehasfilter"
#define MESSAGE_FIELD_EVENT_FILTER "_efilter"
/*End message fields. */

#define stringify(s) _stringify(s)
#define _stringify(s) #s

#ifdef __cplusplus
}
#endif
#endif
