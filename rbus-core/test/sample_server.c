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

static char buffer[100];

static char data[100] = "init init init";

static int handle_get(const char * destination, const char * method, rtMessage message, void * user_data, rtMessage *response)
{
    (void) user_data;
    (void) message;
    (void) destination;
    (void) method;
    rtMessage_Create(response);
    rbus_SetInt32(*response, MESSAGE_FIELD_RESULT, RTMESSAGE_BUS_SUCCESS);
    rbus_SetString(*response, MESSAGE_FIELD_PAYLOAD, data);
    return 0;
}

static int handle_set(const char * destination, const char * method, rtMessage request, void * user_data, rtMessage *response)
{
    (void) user_data;
    (void) destination;
    (void) method;
    rtError err = RT_OK;
    const char * payload = NULL;
    if((err = rbus_GetString(request, MESSAGE_FIELD_PAYLOAD, &payload) == RT_OK)) 
    {
        strncpy(data, payload, sizeof(data));
    }
    rtMessage_Create(response);
    rbus_SetInt32(*response, MESSAGE_FIELD_RESULT, RTMESSAGE_BUS_SUCCESS);
    return 0;
}

static void handle_unknown(const char * destination, const char * method, rtMessage message, rtMessage *response)
{
    (void) message;
    (void) destination;
    (void) method;
    rtMessage_Create(response);
    rbus_SetInt32(*response, MESSAGE_FIELD_RESULT, RTMESSAGE_BUS_ERROR_UNSUPPORTED_METHOD);
}

static int callback(const char * destination, const char * method, rtMessage message, void * user_data, rtMessage *response)
{
    (void) user_data;
    (void) destination;
    (void) method;
    printf("Received message in base callback.\n");
    char* buff = NULL;
    uint32_t buff_length = 0;

    rtMessage_ToString(message, &buff, &buff_length);
    printf("%s\n", buff);
    free(buff);

    /* Craft response message.*/
    handle_unknown(destination, method, message, response);
    return 0;
}

int main(int argc, char *argv[])
{
    (void) argc;
    rbus_error_t err = RTMESSAGE_BUS_SUCCESS;
    printf("syntax: sample_server <server object name>\n");
    rtLog_SetLevel(RT_LOG_INFO);

    if((err = rbus_openBrokerConnection(argv[1])) == RTMESSAGE_BUS_SUCCESS)
    {
        printf("Successfully connected to bus.\n");
    }

    snprintf(buffer, (sizeof(buffer) - 1), "%s.obj1", argv[1]);
    printf("Registering object %s\n", buffer);

    if((err = rbus_registerObj(buffer, callback, NULL)) == RTMESSAGE_BUS_SUCCESS)
    {
        printf("Successfully registered object.\n");
    }

    rbus_method_table_entry_t table[2] = {{METHOD_SETPARAMETERVALUES, NULL, handle_set}, {METHOD_GETPARAMETERVALUES, NULL, handle_get}};
    rbus_registerMethodTable(buffer, table, 2); 
    buffer[0] = 1;
    if((err = rbus_registerObj(buffer, callback, NULL)) == RTMESSAGE_BUS_SUCCESS) //Negative test case
    {
        printf("Successfully registered object.\n");
    }
    pause();

    if((err = rbus_closeBrokerConnection()) == RTMESSAGE_BUS_SUCCESS)
    {
        printf("Successfully disconnected from bus.\n");
    }
    return 0;
}
