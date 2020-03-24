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
#include "rtConnection.h"
#include "rtLog.h"
#include "rtMessage.h"
#include "rtrouter_diag.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


int main(int argc, char * argv[])
{
    printf("%s",
            "----------\nHelp:\n"
            "Syntax: rtm_diag_probe <command>\n"
            "Following commands are supported:\n"
            "dumpRoutingStats - dump the number of nodes and root nodes in routing table.\n"
            "dumpRoutingTable - dump the routing table/tree itself.\n"
            "dumpQuickMatchExpressions - dump the list of quick match expressions used by optimization-1 level look-up.\n"
            "rStrategyNormal - Set routing strategy to normal (slowest).\n"
            "rStrategyOptimization1 - Set routing strategy to optimization 1 (faster).\n"
            "rStrategyOptimization2 - Set routing strategy to optimization 2 (fastest and default).\n"
            "enableVerboseLogs - Enable debug level logs in router.\n"
            "disableVerboseLogs - Disable debug level logs in router.\n"
            "enableTrafficMonitor - Dump all* bus traffic to rtrouted logs.\n"
            "disableTrafficMonitor - Disable dumping of bus traffic.\n"
            "dumpBenchmarkData - Dump raw benchmark data to rtrouted logs.\n"
            "resetBenchmarkData - Reset data collected so far for benchmarking.\n"
            "----------\n"

          );
  if(1 != argc)
  {
      rtConnection con;

      rtLog_SetLevel(RT_LOG_INFO);
      rtConnection_Create(&con, "APP2", "unix:///tmp/rtrouted");
      rtMessage out;
      rtMessage_Create(&out);
      rtMessage_SetString(out, RTROUTER_DIAG_CMD_KEY, argv[1]);
      rtConnection_SendMessage(con, out, "_RTROUTED.INBOX.DIAG");
      sleep(1);
      rtConnection_Destroy(con);
  }
  return 0;
}
