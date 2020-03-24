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

static char data1[100] = "wifi init init init";
static char data2[100] = "tr69 init init init";

static int handle_get(const char * destination, const char * method, rtMessage request, void * user_data, rtMessage *response)
{
    (void) destination;
    (void) method;
    (void) request;
    rtMessage_Create(response);
    printf("calling get %s, ptr %p\n", (const char *)user_data, user_data);
    rbus_AppendInt32(*response,  RTMESSAGE_BUS_SUCCESS);
    rbus_AppendString(*response,  (const char *)user_data);
    return 0;
}

static int handle_set(const char * destination, const char * method, rtMessage request, void * user_data, rtMessage *response)
{
    (void) destination;
    (void) method;
    rtError err = RT_OK;
    const char * payload = NULL;
    printf("calling set %s\n", (const char *)user_data);
    if((err = rbus_PopString(request,  &payload) == RT_OK)) //TODO: Should payload be freed?
    {
        strncpy((char *)user_data, payload, sizeof(data1));
    }
    rtMessage_Create(response);
    rbus_AppendInt32(*response,  RTMESSAGE_BUS_SUCCESS);
    return 0;
}

static int callback(const char * destination, const char * method, rtMessage message, void * user_data, rtMessage *response)
{
    (void) destination;
    (void) method;
    (void) user_data;
    (void) response;
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
    rtLog_SetLevel(RT_LOG_INFO);

    if((err = rbus_openBrokerConnection("wildcard_server")) == RTMESSAGE_BUS_SUCCESS)
    {
        printf("Successfully connected to bus.\n");
    }


    if((err = rbus_registerObj("foo", callback, NULL)) == RTMESSAGE_BUS_SUCCESS)
    {
        printf("Successfully registered object.\n");
    }
    if((err = rbus_registerObj("bar", callback, NULL)) == RTMESSAGE_BUS_SUCCESS)
    {
        printf("Successfully registered object.\n");
    }

    rbus_method_table_entry_t table[2] = {{METHOD_SETPARAMETERVALUES, (void *)data1, handle_set}, {METHOD_GETPARAMETERVALUES, (void *)data1, handle_get}};
    rbus_registerMethodTable("foo", table, 2);
    rbus_registerMethodTable("bar", table, 2);
    rbus_addElement("foo", "footree.1");
    rbus_addElement("foo", "footree.2");
    rbus_addElement("foo", "footree.3");
    rbus_addElement("foo", "footree.4");
    rbus_addElement("foo", "footree.5");

    rbus_addElement("bar", "bartree.1");
    rbus_addElement("bar", "bartree.2");
    rbus_addElement("bar", "bartree.3");
    rbus_addElement("bar", "bartree.4");
    rbus_addElement("bar", "bartree.5");
    
    printf("Press enter to remove object foo.\n");
    getchar();
    rbus_unregisterObj("foo");
    
    printf("Press enter to remove some elements of bar.\n");
    getchar();
    rbus_removeElement("bar", "bartree.2");
    rbus_removeElement("bar", "bartree.3");
    
    printf("Press enter to test some negative cases.\n");
    getchar();
    rbus_removeElement("bar", "bartree.3");
    rbus_unregisterObj("foo");
    
    printf("Press enter to remove bar as well.\n");
    getchar();
    rbus_unregisterObj("bar");
    
    printf("Press enter to disconnect and exit.\n");
    getchar();
    if((err = rbus_closeBrokerConnection()) == RTMESSAGE_BUS_SUCCESS)
    {
        printf("Successfully disconnected from bus.\n");
    }
    return 0;
}
