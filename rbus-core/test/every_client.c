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
int main(int argc, char *argv[])
{
    (void) argc;
    rbus_error_t err = RTMESSAGE_BUS_SUCCESS;
    printf("syntax: sample_client <object to gett>\n");
    rtLog_SetLevel(RT_LOG_INFO);

    if((err = rbus_openBrokerConnection("everyclient")) == RTMESSAGE_BUS_SUCCESS)
    {
        printf("Successfully connected to bus.\n");
    }

    rtMessage response;
    if(RTMESSAGE_BUS_SUCCESS == rbus_invokeRemoteMethod(argv[1], METHOD_GETPARAMETERVALUES, NULL, 10000, &response))
    {
        dumpMessage(response);
        rtMessage_Release(response);
    }
    else
        printf("RPC failed.\n");
    if((err = rbus_closeBrokerConnection()) == RTMESSAGE_BUS_SUCCESS)
    {
        printf("Successfully disconnected from bus.\n");
    }
    return 0;
}
