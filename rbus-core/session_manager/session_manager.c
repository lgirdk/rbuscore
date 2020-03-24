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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "rbus_core.h"
#include "rbus_marshalling.h"
#include "rtLog.h"
#include "rbus_session_mgr.h"

static int g_counter;
static int g_current_session_id;

static int request_session_id(const char * destination, const char * method, rtMessage request, void * user_data, rtMessage *response)
{
    (void) request;
    (void) user_data;
    (void) destination;
    (void) method;
    rtMessage_Create(response);
    if(0 == g_current_session_id)
    {
        g_current_session_id = ++g_counter;
        printf("Creating new session %d\n", g_current_session_id);
        rbus_SetInt32(*response, MESSAGE_FIELD_RESULT, RTMESSAGE_BUS_SUCCESS);
        rbus_SetInt32(*response, MESSAGE_FIELD_PAYLOAD, g_current_session_id);
    }
    else
    {
        printf("Cannot create new session when session %d is active.\n", g_current_session_id);
        rbus_SetInt32(*response, MESSAGE_FIELD_RESULT, RTMESSAGE_BUS_ERROR_INVALID_STATE);
    }
    return 0;
}

static int get_session_id(const char * destination, const char * method, rtMessage request, void * user_data, rtMessage *response)
{
    (void) request;
    (void) user_data;
    (void) destination;
    (void) method;
    rtMessage_Create(response);
    printf("Current session id is %d\n", g_current_session_id);
    rbus_SetInt32(*response, MESSAGE_FIELD_RESULT, RTMESSAGE_BUS_SUCCESS);
    rbus_SetInt32(*response, MESSAGE_FIELD_PAYLOAD, g_current_session_id);
    return 0;
}

static int end_session(const char * destination, const char * method, rtMessage request, void * user_data, rtMessage *response)
{
    (void) user_data;
    (void) destination;
    (void) method;
    rtMessage_Create(response);
    int sessionid = 0;
    rbus_error_t result = RTMESSAGE_BUS_SUCCESS;

    if(RT_OK == rbus_GetInt32(request, MESSAGE_FIELD_PAYLOAD, &sessionid))
    {
        if(sessionid == g_current_session_id)
        {
            printf("End of session %d\n", g_current_session_id);
            g_current_session_id = 0;
            //Cue event announcing end of session.
        }
        else
        {
            printf("Cannot end session %d. It doesn't match active session, which is %d.\n", sessionid, g_current_session_id);
            result = RTMESSAGE_BUS_ERROR_INVALID_STATE;
        }
    }
    else
    {
        printf("Session id not found. Cannot process end of session.\n");
        result = RTMESSAGE_BUS_ERROR_INVALID_PARAM;
    }
    rbus_SetInt32(*response, MESSAGE_FIELD_RESULT, result);
    return 0;
}


static int callback(const char * destination, const char * method, rtMessage message, void * user_data, rtMessage *response)
{
    (void) user_data;
    (void) response;
    (void) destination;
    (void) method;
    printf("Received message in base callback.\n");
    char* buff = NULL;
    uint32_t buff_length = 0;

    rtMessage_ToString(message, &buff, &buff_length);
    printf("%s\n", buff);
    free(buff);

    return 0;
}

int main(int argc, char *argv[])
{
    (void) argc;
    (void) argv;
    rbus_error_t err = RTMESSAGE_BUS_SUCCESS;
    printf("rbus session manager launching.\n");
    rtLog_SetLevel(RT_LOG_INFO);

    if((err = rbus_openBrokerConnection(RBUS_SMGR_DESTINATION_NAME)) == RTMESSAGE_BUS_SUCCESS)
    {
        printf("Successfully connected to bus.\n");
    }

    if((err = rbus_registerObj(RBUS_SMGR_DESTINATION_NAME, callback, NULL)) == RTMESSAGE_BUS_SUCCESS)
    {
        printf("Successfully registered object.\n");
    }

    rbus_method_table_entry_t table[3] = {
        {RBUS_SMGR_METHOD_GET_CURRENT_SESSION_ID, NULL, get_session_id}, 
        {RBUS_SMGR_METHOD_REQUEST_SESSION_ID, NULL, request_session_id}, 
        {RBUS_SMGR_METHOD_END_SESSION, NULL, end_session}};
    rbus_registerMethodTable(RBUS_SMGR_DESTINATION_NAME, table, 3); 
    pause();

    if((err = rbus_closeBrokerConnection()) == RTMESSAGE_BUS_SUCCESS)
    {
        printf("Successfully disconnected from bus.\n");
    }
    printf("rbus session manager exiting.\n");
    return 0;
}
