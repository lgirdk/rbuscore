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
/*****************************************
Test server for unit test client testing
******************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include "rbus_core.h"
#include "rbus_marshalling.h"
#include "rbus_test_util.h"

static char data[100] = "init init init";

static test_struct_t test_struct1 = {};
static test_struct_t test_struct2 = {} ;
static test_array_data_t test_array1 = {};

student_details_t student_data[100] = {};
int count = 0;

void reset_stored_data()
{
    memset(data, 0, sizeof(data));
    strncpy(data, "init init init", sizeof(data));

    memset(&test_struct1, 0, sizeof(test_struct_t));
    memset(&test_struct2, 0, sizeof(test_struct_t));

    for(int i = 0; i < count; i++)
        memset((student_data + i), 0, sizeof(student_details_t));
    count = 0;
}

int handle_get1(const char * destination, const char * method, rtMessage request, void * user_data, rtMessage *response)
{
    (void) request;
    (void) user_data;
    (void) destination;
    (void) method;
    rtMessage_Create(response);
    rbus_SetString(*response, MESSAGE_FIELD_PAYLOAD, data);
    rbus_SetInt32(*response, MESSAGE_FIELD_RESULT, RTMESSAGE_BUS_SUCCESS);
    return 0;
}

int handle_set1(const char * destination, const char * method, rtMessage request, void * user_data, rtMessage *response)
{
    (void) user_data;
    (void) response;
    (void) destination;
    (void) method;
    rtError err = RT_OK;
    const char * payload = NULL;
    if((err = rbus_GetString(request, MESSAGE_FIELD_PAYLOAD, &payload) == RT_OK)) //TODO: Should payload be freed?
    {
        strncpy(data, payload, sizeof(data));
    }
    rtMessage_Create(response);
    rbus_SetInt32(*response, MESSAGE_FIELD_RESULT, RTMESSAGE_BUS_SUCCESS);
    return 0;
}
int handle_get2(const char * destination, const char * method, rtMessage request, void * user_data, rtMessage *response)
{
    (void) request;
    (void) user_data;
    (void) destination;
    (void) method;
    rtMessage_Create(response);
    rbus_SetString(*response, MESSAGE_FIELD_PAYLOAD, data);
    rbus_SetInt32(*response, MESSAGE_FIELD_RESULT, RTMESSAGE_BUS_SUCCESS);
    return 0;
}

int handle_set2(const char * destination, const char * method, rtMessage request, void * user_data, rtMessage *response)
{
    (void) user_data;
    (void) response;
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

int handle_getStudentInfo(const char * destination, const char * method, rtMessage request, void * user_data, rtMessage *response)
{
    (void) request;
    (void) user_data;
    (void) destination;
    (void) method;
    int i;
    rtMessage_Create(response);
    for(i = 0; i <count; i++)
    {
       if(0 == strncmp(student_data[i].object_name, (char *)user_data, 50))
          rbus_SetString(*response, MESSAGE_FIELD_PAYLOAD, student_data[i].student_name);
    }
    rbus_SetInt32(*response, MESSAGE_FIELD_RESULT, RTMESSAGE_BUS_SUCCESS);

    return 0;
}

int handle_setStudentInfo(const char * destination, const char * method, rtMessage request, void * user_data, rtMessage *response)
{
    (void) user_data;
    (void) response;
    (void) destination;
    (void) method;
    rtError err = RT_OK;
    const char * payload = NULL;

    if(NULL != user_data)
    {
        strncpy(student_data[count].object_name, (char *)user_data,sizeof(student_data[count].object_name));
    }
    if((err = rbus_GetString(request, MESSAGE_FIELD_PAYLOAD, &payload) == RT_OK))
    {
        strncpy(student_data[count].student_name, payload, 50);
    }
    count++;
    rtMessage_Create(response);
    rbus_SetInt32(*response, MESSAGE_FIELD_RESULT, RTMESSAGE_BUS_SUCCESS);
    return 0;
}

int handle_getBinaryData(const char * destination, const char * method, rtMessage request, void * user_data, rtMessage *response)
{
    (void) request;
    (void) user_data;
    (void) destination;
    (void) method;
    rtMessage_Create(response);
    rbus_AddBinaryData(*response, MESSAGE_FIELD_PAYLOAD, (void *)&test_array1, sizeof(test_array1));
    rbus_SetInt32(*response, MESSAGE_FIELD_RESULT, RTMESSAGE_BUS_SUCCESS);
    return 0;
}

int handle_setBinaryData(const char * destination, const char * method, rtMessage request, void * user_data, rtMessage *response)
{
    (void) user_data;
    (void) destination;
    (void) method;
    rtError err = RT_OK;
    const test_array_data_t *payload;
    unsigned int size = 0;

    if((err = rbus_GetBinaryData(request, MESSAGE_FIELD_PAYLOAD, (const void**)&payload, &size) == RT_OK))
    {
        test_array1 = *payload;
    }
    rtMessage_Create(response);
    rtMessage_SetInt32(*response, MESSAGE_FIELD_RESULT, RTMESSAGE_BUS_SUCCESS);
    return 0;
}

int handle_getAttributes1(const char * destination, const char * method, rtMessage request, void * user_data, rtMessage *response)
{
    (void) request;
    (void) user_data;
    (void) destination;
    (void) method;
    rtMessage_Create(response);
    rbus_SetString(*response, MESSAGE_FIELD_PAYLOAD, data);
    rbus_SetString(*response, "name", test_struct1.name);
    rbus_SetInt32(*response, "age", test_struct1.age);
    rbus_SetInt32(*response, MESSAGE_FIELD_RESULT, RTMESSAGE_BUS_SUCCESS);
    return 0;
}
int handle_setAttributes1(const char * destination, const char * method, rtMessage request, void * user_data, rtMessage *response)
{
    (void) user_data;
    (void) destination;
    (void) method;
    rtMessage_Create(response);
    rbus_SetString(*response, MESSAGE_FIELD_PAYLOAD, data);
    rtError err = RT_OK;
    const char * name = NULL;
    if((err = rbus_GetString(request, "name", &name) == RT_OK)) //TODO: Should payload be freed?
    {
        strncpy(test_struct1.name, name, sizeof(test_struct1.name));
        //printf("Value set to name: %s \n", name);
    }
    rbus_GetInt32(request, "age", &test_struct1.age);
    rbus_SetInt32(*response, MESSAGE_FIELD_RESULT, RTMESSAGE_BUS_SUCCESS);
    return 0;
}

int handle_getAttributes2(const char * destination, const char * method, rtMessage request, void * user_data, rtMessage *response)
{
    (void) request;
    (void) user_data;
    (void) destination;
    (void) method;
    rtMessage_Create(response);
    rbus_SetString(*response, MESSAGE_FIELD_PAYLOAD, data);
    rbus_SetString(*response, "name", test_struct2.name);
    rbus_SetInt32(*response, "age", test_struct2.age);
    rbus_SetInt32(*response, MESSAGE_FIELD_RESULT, RTMESSAGE_BUS_SUCCESS);
    return 0;
}
int handle_setAttributes2(const char * destination, const char * method, rtMessage request, void * user_data, rtMessage *response)
{
    (void) user_data;
    (void) destination;
    (void) method;
    rtMessage_Create(response);
    rbus_SetString(*response, MESSAGE_FIELD_PAYLOAD, data);
    rtError err = RT_OK;
    const char * name = NULL;
    if((err = rbus_GetString(request, "name", &name) == RT_OK)) //TODO: Should payload be freed?
    {
        strncpy(test_struct2.name, name, sizeof(test_struct2.name));
        //printf("Value set to name: %s \n", name);
    }
    rbus_GetInt32(request, "age", &test_struct2.age);
    rbus_SetInt32(*response, MESSAGE_FIELD_RESULT, RTMESSAGE_BUS_SUCCESS);
    return 0;
}

void handle_unknown(const char * destination, const char * method, rtMessage request, rtMessage *response)
{
    (void) request;
    (void) destination;
    (void) method;
    rtMessage_Create(response);
    rbus_SetInt32(*response, MESSAGE_FIELD_RESULT, RTMESSAGE_BUS_ERROR_UNSUPPORTED_METHOD);
}

int callback(const char * destination, const char * method, rtMessage message, void * user_data, rtMessage *response)
{
    (void) user_data;
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
