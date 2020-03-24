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
#ifndef __DM_UTILITY_H__
#define __DM_UTILITY_H__

#include <string>

class dmUtility
{
public:
  static void splitQuery(char const* query, char* parameter)
  {
    std::string str(query);
    std::size_t position = str.find_last_of(".\\");

    parameter[0]= '\0';
    std::strcat(parameter, str.substr(position + 1).c_str());
    str.clear();
  }

  static bool has_suffix(const std::string &str, const std::string &suffix)
  {
    return str.size() >= suffix.size() && str.find(suffix, str.size() - suffix.size()) != str.npos;
  }
};

#endif
