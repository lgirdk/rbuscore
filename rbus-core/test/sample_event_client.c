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
#include "rbus_core.h"
#include "rbus_marshalling.h"
#include "rtLog.h"

#define OBJ1_NAME "foo"
#define OBJ2_NAME "bar"

static void dumpMessage(rtMessage message)
{
    char* buff = NULL;
    uint32_t buff_length = 0;

    rtMessage_ToString(message, &buff, &buff_length);
    printf("dumpMessage: %.*s\n", buff_length, buff);
    free(buff);
}

static int event_callback(const char * object_name,  const char * event_name, rtMessage message, void * user_data)
{
    (void) user_data;
    printf("In event callback for object %s, event %s.\n", object_name, event_name);
    dumpMessage(message);
    return 0;
}
int main(int argc, char *argv[])
{
    (void) argc;
    rbus_error_t err = RTMESSAGE_BUS_SUCCESS;
    printf("syntax: sample_client <name of client instance> <destination object name>\n");
    rtLog_SetLevel(RT_LOG_INFO);

    if((err = rbus_openBrokerConnection(argv[1])) == RTMESSAGE_BUS_SUCCESS)
    {
        printf("Successfully connected to bus.\n");
    }

    printf("Registering event callback.\n");

    rbus_subscribeToEvent("non_object", "event_2", &event_callback, NULL, NULL); //Negative test case.
    rbus_subscribeToEvent("non_object", NULL, &event_callback, NULL, NULL); //Negative test case.
    rbus_subscribeToEvent(OBJ1_NAME, "event3", &event_callback, NULL, NULL); //Negative test case.
    rbus_subscribeToEvent(OBJ1_NAME, "event1", &event_callback, NULL, NULL);
    rbus_subscribeToEvent(OBJ1_NAME, "event1", &event_callback, NULL, NULL); //Negative test case.
    rbus_subscribeToEvent(OBJ1_NAME, "event2", &event_callback, NULL, NULL);
    rbus_subscribeToEvent(OBJ2_NAME, NULL, &event_callback, NULL, NULL);

    //support rbus events being elements
    rbus_subscribeToEvent(NULL, "event4", &event_callback, NULL, NULL);

    sleep(10);
    rbus_unsubscribeFromEvent(OBJ1_NAME, "event2");
    sleep(1);
#if 1
    /*Pull the object from remote end.*/
    rtMessage response;
    if((err = rbus_pullObj(OBJ1_NAME, 1000, &response)) == RTMESSAGE_BUS_SUCCESS)
    {
        const char* buff = NULL;
        printf("Received object %s\n", OBJ1_NAME);
        rbus_GetString(response, MESSAGE_FIELD_PAYLOAD, &buff);
        printf("Payload: %s\n", buff);
        rtMessage_Release(response);
    }
    else
    {
        printf("Could not pull object %s\n", OBJ1_NAME);
    }

    //Check whether aliases work.

    if((err = rbus_pullObj("obj1_alias", 1000, &response)) == RTMESSAGE_BUS_SUCCESS)
    {
        const char* buff = NULL;
        printf("Received object %s\n", "obj1_alias");
        rbus_GetString(response, MESSAGE_FIELD_PAYLOAD, &buff);
        printf("Payload: %s\n", buff);
        rtMessage_Release(response);
    }
    else
    {
        printf("Could not pull object %s\n", OBJ1_NAME);
    }
    if((err = rbus_pullObj("obj1_alias2", 1000, &response)) == RTMESSAGE_BUS_SUCCESS)
    {
        const char* buff = NULL;
        printf("Received object %s\n", "obj1_alias2");
        rbus_GetString(response, MESSAGE_FIELD_PAYLOAD, &buff);
        printf("Payload: %s\n", buff);
        rtMessage_Release(response);
    }
    else
    {
        printf("Could not pull object %s\n", OBJ1_NAME);
    }
    
    if((err = rbus_pullObj(OBJ2_NAME, 1000, &response)) == RTMESSAGE_BUS_SUCCESS)
    {
        const char* buff = NULL;
        printf("Received object %s\n", OBJ2_NAME);
        rbus_GetString(response, MESSAGE_FIELD_PAYLOAD, &buff);
        printf("Payload: %s\n", buff);
        rtMessage_Release(response);
    }
    else
    {
        printf("Could not pull object %s\n", OBJ1_NAME);
    }

    /* Push the object to remote end.*/
    rtMessage setter;
    rtMessage_Create(&setter);
    rbus_SetString(setter, MESSAGE_FIELD_PAYLOAD, "foobar");
    if((err = rbus_pushObj(OBJ1_NAME, setter, 1000)) == RTMESSAGE_BUS_SUCCESS)
    {
        printf("Push object %s\n", OBJ1_NAME);
    }
    else
    {
        printf("Could not push object %s. Error: 0x%x\n", OBJ1_NAME, err);
    }

    /* Pull again to make sure that "set" worked. */
    if((err = rbus_pullObj(OBJ1_NAME, 1000, &response)) == RTMESSAGE_BUS_SUCCESS)
    {
        const char* buff = NULL;
        printf("Received object %s\n", OBJ1_NAME);
        rbus_GetString(response, MESSAGE_FIELD_PAYLOAD, &buff);
        printf("Payload: %s\n", buff);
        rtMessage_Release(response);
    }
    else
    {
        printf("Could not pull object %s\n", OBJ1_NAME);
    }
#endif
    if((err = rbus_closeBrokerConnection()) == RTMESSAGE_BUS_SUCCESS)
    {
        printf("Successfully disconnected from bus.\n");
    }
    return 0;
}
