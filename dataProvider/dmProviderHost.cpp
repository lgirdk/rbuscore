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
#include "dmProviderOperation.h"

#include <sstream>
#include <cstring>
#include <iostream>
#include <algorithm>

#include <rtConnection.h>
#include <rtError.h>
#include <rtLog.h>

#include "dmUtility.h"

class dmProviderHostImpl : public dmProviderHost
{
public:
  dmProviderHostImpl()
  {

  }

  virtual ~dmProviderHostImpl()
  {
    if (m_con)
    {
      rtLog_Info("closing rtMessage connection");
      rtConnection_Destroy(m_con);
      m_con = nullptr;
    }
  }

private:
  virtual bool providerRegistered(std::string const& name)
  {
    std::stringstream topic;
    topic << "RDK.MODEL.";
    topic << name;
    std::string s = topic.str();

    rtError e = rtConnection_AddListener(m_con, s.c_str(), &dmProviderHostImpl::requestHandler, this);
    if (e != RT_OK)
    {
      rtLog_Warn("failed to register provider listener. %s", rtStrError(e));
      return false;
    }

    return true;
  }

  void start()
  {
    rtConnection_Create(&m_con, "USE_UNIQUE_NAME_HERE", "tcp://127.0.0.1:10001");
    std::unique_lock<std::mutex> lock(m_mutex);
    m_thread.reset(new std::thread(&dmProviderHostImpl::run, this));
  }

  void stop()
  {
    rtConnection_Destroy(m_con);
    std::unique_lock<std::mutex> lock(m_mutex);
    if (m_thread)
    {
      // TODO
    }
  }

  void run()
  {
    while (true)
    {
      rtError e = rtConnection_Dispatch(m_con);
      if (e != RT_OK)
      {
        rtLog_Warn("error during dispatch:%s", rtStrError(e));
      }
    }
  }

  static void requestHandler(rtMessageHeader const* hdr, uint8_t const* buff, uint32_t n,
    void* closure)
  {
    if (!rtMessageHeader_IsRequest(hdr))
    {
      rtLog_Error("got message that wasn't request in datamodel callback");
      return;
    }

    rtMessage req;
    rtError e = rtMessage_FromBytes(&req, buff, n);

    if (e != RT_OK)
    {
      rtLog_Warn("failed to decode datamodel request");
    }

    dmProviderHostImpl* host = reinterpret_cast<dmProviderHostImpl *>(closure);

    std::vector<dmQueryResult> results;
    dmProviderOperation op = host->decodeOperation(req);

    if (op == dmProviderOperation_Get)
    {
      std::string provider_name;
      std::vector<dmPropertyInfo> params;
      host->decodeGetRequest(req, provider_name, params);
      host->doGet(provider_name, params, results);
    }
    else if (op == dmProviderOperation_Set)
    {
      std::string provider_name;
      std::vector<dmNamedValue> params;
      host->decodeSetRequest(req, provider_name, params);
      host->doSet(provider_name, params, results);
    }

    rtMessage res;
    rtMessage_Create(&res);
    host->encodeResult(res, results);
    rtConnection_SendResponse(m_con, hdr, res, 1000);
    rtMessage_Release(res);
  }

  dmProviderOperation decodeOperation(rtMessage req)
  {
    char const* operation = nullptr;
    rtMessage_GetString(req, "method", &operation);
    if ((strcmp(operation, "set") == 0))
      return dmProviderOperation_Set;
    else
      return dmProviderOperation_Get;
  }

   void decodeGetRequest(rtMessage req, std::string& name, std::vector<dmPropertyInfo>& params)
  {
    char const* provider_name = nullptr;
    dmPropertyInfo propertyInfo;

    rtMessage_GetString(req, "provider", &provider_name);
    if (provider_name)
      name = provider_name;

    rtMessage item;
    rtMessage_GetMessage(req, "params", &item);

    char const* property_name;
    rtMessage_GetString(item, "name", &property_name);

    std::vector<char const*> param_container = db->getParameters(provider_name);

    if (dmUtility::has_suffix(property_name, "."))
    {
      for (auto itr : param_container)
      {
        std::string param(itr);
        std::string fullparam = property_name + param;
        char* parameter = new char[256];
        dmUtility::splitQuery(fullparam.c_str(), parameter);
        propertyInfo.setName(parameter);
        params.push_back(propertyInfo);
        delete [] parameter;
      }
    }
    else
    {
      char* param = new char[256];
      dmUtility::splitQuery(property_name, param);
      if (std::find(param_container.begin(), param_container.end(), std::string(param)) != param_container.end())
      {
        propertyInfo.setName(param);
      }
      else
      {
        propertyInfo.setName(" ");
      }
      params.push_back(propertyInfo);
      delete [] param;
    }
    rtMessage_Release(item);
  }

  void decodeSetRequest(rtMessage req, std::string& name, std::vector<dmNamedValue>& params)
  {
    char const* provider_name = nullptr;
    rtMessage_GetString(req, "provider", &provider_name);
    if (provider_name)
      name = provider_name;

    std::vector<char const*> param_container = db->getParameters(provider_name);

    rtMessage item;
    rtMessage_GetMessage(req, "params", &item);

    char const* property_name;
    rtMessage_GetString(item, "name", &property_name);

    char* param = new char[256];
    dmUtility::splitQuery(property_name, param);

    char const* property_value;
    rtMessage_GetString(item, "value", &property_value);

    if (std::find(param_container.begin(), param_container.end(), std::string(param)) != param_container.end())
    {
      if (db->isWritable(param, provider_name))
      {
        dmNamedValue namedValue(param, property_value);
        params.push_back(namedValue);
      }
      else
      {
        dmNamedValue namedValue("nonwritable", " ");
        params.push_back(namedValue);
      }
    }
    else
    {
      dmNamedValue namedValue(" ", " ");
      params.push_back(namedValue);
    }

    rtMessage_Release(item);
    delete [] param;
  }

  void encodeResult(rtMessage& res, std::vector<dmQueryResult> const& resultSet)
  {
    for (auto paramresult : resultSet)
    {
      rtMessage msg;
      rtMessage_Create(&msg);
      int status_code = paramresult.status();
      std::string status_msg = paramresult.statusMsg();
      for (auto const& result : paramresult.values())
      {
        if (result.StatusCode != 0 && status_code == 0)
          status_code = result.StatusCode;

        if(!result.StatusMessage.empty())
          status_msg = result.StatusMessage;

        dmNamedValue const& val = result.Value;
        rtMessage_SetString(msg, "name", val.name().c_str());
        rtMessage_SetString(msg, "value", val.value().toString().c_str());
      }
      rtMessage_SetInt32(msg, "status", status_code);
      rtMessage_SetString(msg, "status_msg", status_msg.c_str());
      rtMessage_AddMessage(res, "result", msg);
      rtMessage_Release(msg);
    }
  }

private:
  std::unique_ptr<std::thread> m_thread;
  std::mutex m_mutex;
  static rtConnection m_con;
};

rtConnection dmProviderHostImpl::m_con = nullptr;

dmProviderHost*
dmProviderHost::create()
{
  rtLog_SetLevel(RT_LOG_DEBUG);
  return new dmProviderHostImpl();
}

bool
dmProviderHost::registerProvider(char const* object, std::unique_ptr<dmProvider> provider)
{
  m_providername = db->getProviderFromObject(object);
  bool b = providerRegistered(m_providername);
  if (b)
    m_providers.insert(std::make_pair(m_providername, std::move(provider)));
  return b;
}

void
dmProviderHost::doGet(std::string const& providerName, std::vector<dmPropertyInfo> const& params,
    std::vector<dmQueryResult>& result)
{
  auto itr = m_providers.find(providerName);
  if (itr != m_providers.end())
  {
    itr->second->doGet(params, result);
  }
}

void
dmProviderHost::doSet(std::string const& providerName, std::vector<dmNamedValue> const& params,
    std::vector<dmQueryResult>& result)
{
  auto itr = m_providers.find(providerName);
  if (itr != m_providers.end())
  {
    itr->second->doSet(params, result);
  }
}
