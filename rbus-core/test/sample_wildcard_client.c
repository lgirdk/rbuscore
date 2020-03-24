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


static void dumpMessage(rtMessage message)
{
    char* buff = NULL;
    uint32_t buff_length = 0;

    rtMessage_ToString(message, &buff, &buff_length);
    printf("dumpMessage: %.*s\n", buff_length, buff);
    free(buff);
}

static void pullAndDumpObject(const char * object)
{
    rbus_error_t err = RTMESSAGE_BUS_SUCCESS;
    rtMessage response;
    if((err = rbus_pullObj(object, 1000, &response)) == RTMESSAGE_BUS_SUCCESS)
    {
        printf("Received object %s\n", object);
        dumpMessage(response);
        rtMessage_Release(response);
    }
    else
        printf("Could not pull object %s\n", object);
}


static void queryWildcardExpression(const char * expression)
{
    rtMessage response;
    int num_entries = 0;
    const char *entry;
    
    if(RTMESSAGE_BUS_SUCCESS == rbus_resolveWildcardDestination(expression, &num_entries, &response))
    {
        printf("Query for expression %s was successful. See result below:\n", expression);
        for(int i = 0; i < num_entries; i++)
        {
            rbus_PopString(response, &entry);
            printf("Destination %d is %s\n", i, entry);
        }
        rtMessage_Release(response);
    }
    else
        printf("Query for expression %s was not successful.\n", expression);
}

int main(int argc, char *argv[])
{
    (void) argc;
    (void) argv;
    rbus_error_t err = RTMESSAGE_BUS_SUCCESS;
    rtLog_SetLevel(RT_LOG_INFO);

    if((err = rbus_openBrokerConnection("wildcard_client")) == RTMESSAGE_BUS_SUCCESS)
    {
        printf("Successfully connected to bus.\n");
    }


    /*Pull the object from remote end.*/
    pullAndDumpObject("noobject");
    pullAndDumpObject("obj1");
    pullAndDumpObject("device.wifi.x.y");
    pullAndDumpObject("device.wifi.");
    pullAndDumpObject("device.wifi.a");
    pullAndDumpObject("device.wifi.aa");
    pullAndDumpObject("device.wifi.ab");
    pullAndDumpObject("device.wifi.abbb");
    pullAndDumpObject("device.");
    pullAndDumpObject("device.tr69.x.y");
    pullAndDumpObject("device.wifii.a");
    pullAndDumpObject("device.tr69.");
    
    queryWildcardExpression("obj1");
    queryWildcardExpression("device.wifi.x.");
    queryWildcardExpression("device.wifi.");
    queryWildcardExpression("device.");
    queryWildcardExpression("device.tr69.x.y");
    queryWildcardExpression("device.tr69.");
    queryWildcardExpression("noobject");

    if((err = rbus_closeBrokerConnection()) == RTMESSAGE_BUS_SUCCESS)
    {
        printf("Successfully disconnected from bus.\n");
    }
    return 0;
}
