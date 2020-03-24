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
#include "dmProviderDatabase.h"
#include "dmQuery.h"

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <algorithm>
#include <iostream>
#include <string>
#include <cstring>

#include <rtLog.h>

void print_help()
{
  printf("dmcli [OPTIONS] [COMMAND] [PARAMS]\n");
  printf("\t-d  --datamodel   Set datamodel directory\n");
  printf("\t-h  --help        Print this help and exit\n");
  printf("\t-v  --verbose     Print verbose output\n");
  printf("\t-g  --get         Gets a list of parameters\n");
  printf("\t-s  --set         Sets a list of parameters\n");
  printf("\n");
  printf("Examples:\n");
  printf("\t1.) dmcli -g Device.DeviceInfo.ModelName,Device.DeviceInfo.SerialNumber\n");
  printf("\t2.) dmcli -s Services.Service.Foo.Bar=1,Services.Sevice.Foo.Baz=2\n");
  printf("\n");
}


int main(int argc, char *argv[])
{
  int exit_code = 0;
  bool verbose = false;
  std::string param_list;

  #ifdef DEFAULT_DATAMODELDIR
  std::string datamodel_dir = DEFAULT_DATAMODELDIR;
  #else
  std::string datamode_dir;
  #endif

  dmProviderOperation op = dmProviderOperation_Get;

  if (argc == 1)
  {
    print_help();
    return 0;
  }

  while (1)
  {
    int option_index;
    static struct option long_options[] = 
    {
      { "get", required_argument, 0, 'g' },
      { "set", required_argument, 0, 's' },
      { "help", no_argument, 0, 'h' },
      { "datamode-dir", required_argument, 0, 'd' },
      { "verbose", no_argument, 0, 'v' },
      { 0, 0, 0, 0 }
    };

    int c = getopt_long(argc, argv, "d:g:hs:v", long_options, &option_index);
    if (c == -1)
      break;

    switch (c)
    {
      case 'd':
        datamodel_dir = optarg;
        break;

      case 'g':
        param_list = optarg;
        op = dmProviderOperation_Get;
        break;

      case 's':
        param_list = optarg;
        op = dmProviderOperation_Set;
        break;

      case 'h':
        print_help();
        exit(0);
        break;

      case 'v':
        verbose = true;
        break;

      case '?':
        break;
    }
  }

  if (param_list.empty())
  {
    printf("\nNo parameter list found\n");
    print_help();
    exit(1);
  }

  if (verbose)
    rtLog_SetLevel(RT_LOG_DEBUG);

  dmProviderDatabase db(datamodel_dir);

  std::string delimiter = ",";
  std::string paramlist(param_list);
  std::string token;

  size_t begin = 0;
  size_t end = 0;

  while (begin != std::string::npos)
  {
    // split token on ',' and remove whitespace
    end = param_list.find(',', begin);
    std::string token = param_list.substr(begin, (end - begin));
    token.erase(std::remove_if(token.begin(), token.end(), [](char c)
    {
      return ::isspace(c);
    }), token.end());

    std::unique_ptr<dmQuery> query(db.createQuery(op, token.c_str()));

    if (!query || !query->exec())
    {
      exit_code = -1;
    }
    else
    {
      // TODO: format results nicely
      dmQueryResult const& results = query->results();
      std::cout << "result_code:" << results.status() << std::endl;
      if (!results.status())
      {
       for (auto const& param: results.values())
       {
         std::cout << param.Value.name();
         std::cout << " = ";
         std::cout << param.Value.value().toString();
         std::cout << std::endl;
       }
       std::cout << std::endl;
      }
      else
        std::cout << "Error: " << results.statusMsg() << std::endl;
    }

    begin = end == std::string::npos ? std::string::npos : end + 1;
  }

  return exit_code;
}
