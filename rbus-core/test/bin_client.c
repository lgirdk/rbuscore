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
#include <string.h>
#include <unistd.h>
#include "rbus_core.h"
#include "rbus_marshalling.h"
#include "bin_header.h"
#include "rtLog.h"

static char buffer[100];


static binstruct_t mystruct;
static void fill_mystruct()
{
    mystruct.a = 3;
    mystruct.b = 4;
    mystruct.c = true;
    strncpy(mystruct.name, "client string", (sizeof(mystruct.name)-1));
    mystruct.d = 0xF0;
}



int main(int argc, char *argv[])
{
    (void) argc;
    rbus_error_t err = RTMESSAGE_BUS_SUCCESS;
    printf("syntax: sample_client <name of client instance> <destination object name>\n");
    rtLog_SetLevel(RT_LOG_INFO);
    fill_mystruct();

    if((err = rbus_openBrokerConnection(argv[1])) == RTMESSAGE_BUS_SUCCESS)
    {
        printf("Successfully connected to bus.\n");
    }

    snprintf(buffer, (sizeof(buffer) - 1), "%s", argv[1]);

    /*Pull the object from remote end.*/
    rtMessage response;
    if((err = rbus_pullObj(argv[2], 1000, &response)) == RTMESSAGE_BUS_SUCCESS)
    {
        printf("Received object %s\n", argv[2]);
        const binstruct_t * ptr;
        unsigned int size = 0;
        rtMessage_GetBinaryData(response, MESSAGE_FIELD_PAYLOAD, (const void **)&ptr, &size);
        printf("Payload: %d %d %d %s %d\n", ptr->a, ptr->b, ptr->c, ptr->name, ptr->d);
        rtMessage_Release(response);
    }
    else
    {
        printf("Could not pull object %s\n", argv[2]);
    }

    /* Push the object to remote end.*/
    rtMessage setter;
    rtMessage_Create(&setter);
    rbus_AddBinaryData(setter, MESSAGE_FIELD_PAYLOAD, (void *)&mystruct, sizeof(mystruct));
    if((err = rbus_pushObj(argv[2], setter, 1000)) == RTMESSAGE_BUS_SUCCESS)
    {
        printf("Push object %s\n", argv[2]);
    }
    else
    {
        printf("Could not push object %s. Error: 0x%x\n", argv[2], err);
    }
    rtMessage_Release(setter);

    /* Pull again to make sure that "set" worked. */
    if((err = rbus_pullObj(argv[2], 1000, &response)) == RTMESSAGE_BUS_SUCCESS)
    {
        printf("Received object %s\n", argv[2]);
        const binstruct_t * ptr;
        unsigned int size = 0;
        rbus_GetBinaryData(response, MESSAGE_FIELD_PAYLOAD, (const void **)&ptr, &size);
        printf("Payload: %d %d %d %s %d\n", ptr->a, ptr->b, ptr->c, ptr->name, ptr->d);
        rtMessage_Release(response);
    }
    else
    {
        printf("Could not pull object %s\n", argv[2]);
    }
    if((err = rbus_closeBrokerConnection()) == RTMESSAGE_BUS_SUCCESS)
    {
        printf("Successfully disconnected from bus.\n");
    }
    return 0;
}
