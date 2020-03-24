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
#include "dmProviderHost.h"
#include "dmProvider.h"
#include <unistd.h>

class MyProvider : public dmProvider
{
public:
  MyProvider()
  {

  }

private:
  dmValue getNoiseLevel()
  {
    return "10dB";
  }

  void setNoiseLevel(const std::string& name, const std::string& value)
  {
    printf("\nSet %s as %s\n", name.c_str(), value.c_str());
  }

protected:
  // override the basic get and use ladder if/else
  virtual void doGet(dmPropertyInfo const& param, dmQueryResult& result)
  {
    if (param.name() == "X_RDKCENTRAL-COM_UserName")
    {
      result.addValue(dmNamedValue(param.name(), "xcam_user"));
    }
    else if (param.name() == "X_RDKCENTRAL-COM_NoiseLevel")
    {
      result.addValue(dmNamedValue(param.name(), getNoiseLevel()));
    }
    else if (param.name() == " ")
    {
      result.addValue(dmNamedValue(" ", " "), 1, "Parameter Not found");
    }
    else
    {
      result.addValue(dmNamedValue(param.name(),"dummy"), 0, "DummyValue");
    }
  }

  virtual void doSet(dmNamedValue const& param, dmQueryResult& result)
  {
    if (param.name() == "X_RDKCENTRAL-COM_NoiseLevel")
    {
      setNoiseLevel(param.name(), param.value().toString());
      result.addValue(dmNamedValue(param.name(), param.value()));
    }
    else if(param.name() == " ")
    {
      result.addValue(dmNamedValue(" ", " "), 1, "Parameter not found");
    }
    else if(param.name() == "nonwritable")
    {
      result.addValue(dmNamedValue(" ", " "), 1, "Parameter not writable");
    }
    else
      result.addValue(dmNamedValue(param.name(), param.value()));
  }
};

int main()
{
  dmProviderHost* host = dmProviderHost::create();
  host->start();

  host->registerProvider("Device.WiFi", std::unique_ptr<dmProvider>(new MyProvider()));

  while (true)
  {
    sleep(1);
  }

  host->stop();
  return 0;
}
