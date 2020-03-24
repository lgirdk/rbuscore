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
#ifndef __DM_PROVIDER_H__
#define __DM_PROVIDER_H__

#include "dmPropertyInfo.h"
#include "dmQueryResult.h"

#include <map>
#include <vector>

class dmProvider
{
public:
  dmProvider();
  virtual ~dmProvider();

  inline std::string const& name() const
    { return m_name; }

  virtual void doGet(std::vector<dmPropertyInfo> const& params, std::vector<dmQueryResult>& result);
  virtual void doSet(std::vector<dmNamedValue> const& params, std::vector<dmQueryResult>& result);

protected:
  virtual void doGet(dmPropertyInfo const& param, dmQueryResult& result);
  virtual void doSet(dmNamedValue const& param, dmQueryResult& result);

  using getter_function = std::function<dmValue (void)>;
  using setter_function = std::function<void (dmNamedValue const& value, dmQueryResult& result)>;

  void onGet(std::string const& propertyName, getter_function func);
  void onSet(std::string const& propertyName, setter_function func);

protected:
  std::string m_name;

private:
  struct provider_functions
  {
    getter_function getter;
    setter_function setter;
  };

  std::map< std::string, provider_functions > m_provider_functions;
};

#endif
