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
#include <cstring>

#include "dmProvider.h"

dmProvider::dmProvider()
{
}

dmProvider::~dmProvider()
{
}

void
dmProvider::doGet(std::vector<dmPropertyInfo> const& params, std::vector<dmQueryResult>& result)
{
  dmQueryResult temp;
  for (auto const& propInfo : params)
  {
    auto itr = m_provider_functions.find(propInfo.name());
    if ((itr != m_provider_functions.end()) && (itr->second.getter != nullptr))
    {
      dmValue val = itr->second.getter();
      temp.addValue(dmNamedValue(propInfo.name(), val));
    }
    else
    {
      doGet(propInfo, temp);
    }
    result.push_back(temp);
    temp.clear();
  }
}

void
dmProvider::doGet(dmPropertyInfo const& /*param*/, dmQueryResult& /*result*/)
{
  // EMPTY: overridden by the user 
}

void
dmProvider::doSet(std::vector<dmNamedValue> const& params, std::vector<dmQueryResult>& result)
{
  dmQueryResult temp;
  for (auto const& value : params)
  {
    auto itr = m_provider_functions.find(value.name());
    if ((itr != m_provider_functions.end()) && (itr->second.setter != nullptr))
    {
      itr->second.setter(value, temp);
    }
    else
    {
      doSet(value, temp);
    }
    result.push_back(temp);
    temp.clear();
  }
}

void
dmProvider::doSet(dmNamedValue const& /*param*/, dmQueryResult& /*result*/)
{
  // EMPTY: overridden by the user 
}


// inserts function callback
void
dmProvider::onGet(std::string const& propertyName, getter_function func)
{
  auto itr = m_provider_functions.find(propertyName);
  if (itr != m_provider_functions.end())
  {
    itr->second.getter = func;
  }
  else
  {
    provider_functions funcs;
    funcs.setter = nullptr;
    funcs.getter = func;
    m_provider_functions.insert(std::make_pair(propertyName, funcs));
  }
}

// inserts function callback
void
dmProvider::onSet(std::string const& propertyName, setter_function func)
{
  auto itr = m_provider_functions.find(propertyName);
  if (itr != m_provider_functions.end())
  {
    itr->second.setter = func;
  }
  else
  {
    provider_functions funcs;
    funcs.setter = func;
    funcs.getter = nullptr;
    m_provider_functions.insert(std::make_pair(propertyName, funcs));
  }
}
