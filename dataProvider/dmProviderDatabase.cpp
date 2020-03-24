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

#include <iostream>
#include <dirent.h>
#include <cstddef>
#include <string>
#include <cstring>
#include <map>
#include <iterator>
#include <fstream>
#include <iomanip>


#include "rtConnection.h"
#include "rtMessage.h"
#include "rtError.h"
#include "rtLog.h"

#include <cJSON.h>

#include "dmUtility.h"

namespace
{
  std::map<std::string, cJSON*> modelInfoDB;

  bool matches_object(char const* query, char const* obj)
  {
    char const* p = strrchr(query, '.');
    int n = static_cast<int>((intptr_t) p - (intptr_t) query);
    return strncmp(query, obj, n) == 0;
  }
}

class dmQueryImpl : public dmQuery
{
public:
  dmQueryImpl()
    : m_provider_name(nullptr)
  {
    rtConnection_Create(&m_con, "DMCLI", "tcp://127.0.0.1:10001");

    #ifdef DEFAULT_DATAMODELDIR
    std::string datamodel_dir = DEFAULT_DATAMODELDIR;
    #else
    std::string datamode_dir;
    #endif

    db = new dmProviderDatabase(datamodel_dir);
  }

  ~dmQueryImpl()
  {
    rtConnection_Destroy(m_con);
    if (db)
    {
      delete db;
      db = nullptr;
    }
  }

  void dmRequestResponse(std::string& topic, std::string& parameter)
  {
    rtMessage req;
    rtMessage_Create(&req);
    rtMessage_SetString(req, "method", m_operation.c_str());
    rtMessage_SetString(req, "provider", m_provider_name);
    rtMessage item;
    rtMessage_Create(&item);
    rtMessage_SetString(item, "name", parameter.c_str());
    if (strcmp(m_operation.c_str(), "set") == 0)
      rtMessage_SetString(item, "value", m_value.c_str());
    rtMessage_SetMessage(req, "params", item);
    rtMessage res;
    rtError err = rtConnection_SendRequest(m_con, req, topic.c_str(), &res, 2000);
    if (err == RT_OK)
    {
      int32_t len;
      rtMessage_GetArrayLength(res, "result", &len);

      for (int32_t i = 0; i < len; i++)
      {
      rtMessage item;
      rtMessage_GetMessageItem(res, "result", i, &item);
      int status;
      char const* param = nullptr;
      char const* value = nullptr;
      char const* status_msg = nullptr;

      if (rtMessage_GetString(item, "name", &param) != RT_OK)
        rtLog_Error("failed to get 'name' from paramter");
      if (rtMessage_GetString(item, "value", &value) != RT_OK)
        rtLog_Error("failed to get 'value' from parameter");
      if (rtMessage_GetInt32(item, "status", &status) != RT_OK)
        rtLog_Error("failed to get 'status' from response");
      if (rtMessage_GetString(item, "status_msg", &status_msg) != RT_OK)
        rtLog_Error("failed to get 'status message' from response");

      if (param != nullptr && value != nullptr)
        m_results.addValue(dmNamedValue(param, dmValue(value)), status, status_msg);
      }

      rtMessage_Release(res);
      rtMessage_Release(req);
    }
  }

  virtual bool exec()
  {
    if (!m_provider_name)
    {
      rtLog_Warn("Trying to execute a query without a provider");
      return false;
    }

    std::string topic("RDK.MODEL.");
    topic += m_provider_name;

    rtLog_Debug("sending dm query : %s on topic :%s", m_query.c_str(), topic.c_str());

    dmRequestResponse(topic, m_query);

    reset();
    return true;
  }

  virtual void reset()
  {
    m_operation.clear();
    m_query.clear();
    m_provider_name = nullptr;
    m_value.clear();
    // m_count = 0;
  }

  virtual bool setQueryString(dmProviderOperation op, char const* s)
  {
    switch (op)
    {
      case dmProviderOperation_Get:
      {
        m_operation = "get";
        m_query = s;
      }
      break;
      case dmProviderOperation_Set:
      {
        m_operation = "set";
        std::string data(s);
        if (data.find("=") != std::string::npos)
        {
          std::size_t position = data.find("=");
          m_query = data.substr(0, position);
          m_value = data.substr(position+1);
        }
        else
        {
          rtLog_Error("SET operation without value");
          return false;
        }
        break;
      }
    }
    return true;
  }

  virtual dmQueryResult const& results()
  {
    return m_results;
  }

  void setProviderName(char const* provider_name)
  {
    m_provider_name = provider_name;
  }

private:
  rtConnection m_con;
  dmQueryResult m_results;
  std::string m_query;
  std::string m_value;
  std::string m_operation;
  char const* m_provider_name;
  dmProviderDatabase *db;

  // TODO: int m_count;
  // should use static counter or other way to inject json-rpc request/id
};

dmProviderDatabase::dmProviderDatabase(std::string const& dir)
  : m_modelDirectory(dir)
{
  loadFromDir(dir);
}

void
dmProviderDatabase::loadFromDir(std::string const& dirname)
{
  DIR* dir;
  struct dirent *ent;

  rtLog_Debug("loading database from directory:%s", dirname.c_str());
  if ((dir = opendir(dirname.c_str())) != NULL)
  {
    while ((ent = readdir(dir)) != NULL)
    {
      if (strcmp(ent->d_name,".") != 0 && strcmp(ent->d_name,"..") != 0)
        loadFile(dirname.c_str(), ent->d_name);
    }
    closedir(dir);
  } 
  else
  {
    rtLog_Warn("failed to open directory:%s. %s", dirname.c_str(),
      strerror(errno));
  }
}

void
dmProviderDatabase::loadFile(std::string const& dir, char const* fname)
{
  std::string path = dir;
  path += "/";
  path += fname;

  rtLog_Debug("load file:%s", path.c_str());

  std::ifstream file;
  file.open(path.c_str(), std::ifstream::in);
  if (!file.is_open())
  {
    rtLog_Warn("failed to open fie. %s. %s", fname, strerror(errno));
    return;
  }

  file.seekg(0, file.end);
  int length = file.tellg();
  file.seekg(0, file.beg);

  std::vector<char> buff(length + 1, '\0');
  file.read(buff.data(), length);

  cJSON* json = cJSON_Parse(buff.data());
  if (!json)
  {
    rtLog_Error("Failed to parse json from:%s. %s", path.c_str(), cJSON_GetErrorPtr());
  }
  else
  {
    cJSON* name = cJSON_GetObjectItem(json, "name");
    modelInfoDB.insert(std::make_pair(name->valuestring, json));
  }
}

int
dmProviderDatabase::isWritable(char const* param, char const* provider)
{
  for (auto iter : modelInfoDB)
  {
    cJSON* objProv = cJSON_GetObjectItem(iter.second, "provider");
    std::string objectProvider = objProv->valuestring;
    if (strcmp(objectProvider.c_str(), provider) == 0)
    {
      cJSON *properties = cJSON_GetObjectItem(iter.second, "properties");
      for (int i = 0 ; i < cJSON_GetArraySize(properties); i++)
      {
        cJSON *subitem = cJSON_GetArrayItem(properties, i);
        cJSON* obj = cJSON_GetObjectItem(subitem, "name");
        std::string objName = obj->valuestring;
        if (strcmp(objName.c_str(), param) == 0)
        {
          cJSON* objWrite = cJSON_GetObjectItem(subitem, "writable");
          return objWrite->valueint;
        }
      }
    }
  }
  return 1;
}

char const*
dmProviderDatabase::getProvider(char const* query) const
{
  for (auto iter : modelInfoDB)
  {
    if (matches_object(query, iter.first.c_str()))
    {
      cJSON* obj = cJSON_GetObjectItem(iter.second, "provider");
      return obj->valuestring;
    }
  }
  return nullptr;
}

char const*
dmProviderDatabase::getProviderFromObject(char const* object) const
{
  for (auto itr : modelInfoDB)
  {
    if (strcmp(itr.first.c_str(), object) == 0)
    {
      cJSON* obj = cJSON_GetObjectItem(itr.second, "provider");
      return obj->valuestring;
    }
  }
  return nullptr;
}

std::vector<char const*>
dmProviderDatabase::getParameters(char const* provider) const
{
  std::vector<char const*> parameters;
  for (auto itr : modelInfoDB)
  {
    cJSON* objProv = cJSON_GetObjectItem(itr.second, "provider");
    std::string objectProvider = objProv->valuestring;
    if (strcmp(objectProvider.c_str(), provider) == 0)
    {
      cJSON *properties = cJSON_GetObjectItem(itr.second, "properties");
      for (int i = 0 ; i < cJSON_GetArraySize(properties); i++)
      {
        cJSON *subitem = cJSON_GetArrayItem(properties, i);
        cJSON* obj = cJSON_GetObjectItem(subitem, "name");
        parameters.push_back(obj->valuestring);
      }
    }
  }
  return parameters;
}

dmQuery*
dmProviderDatabase::createQuery() const
{
  return new dmQueryImpl();
}

dmQuery* 
dmProviderDatabase::createQuery(dmProviderOperation op, char const* query_string) const
{
  std::string querystr(query_string);
  char const* provider_name = nullptr;

  if (dmUtility::has_suffix(querystr, "."))
  {
    querystr.erase(querystr.size() - 1);
    provider_name = getProviderFromObject(querystr.c_str());
  }
  else
  {
    provider_name = getProvider(query_string);
  }
  if (!provider_name)
  {
    rtLog_Warn("failed to find provider for query string:%s", query_string);
    return nullptr;
  }

  dmQueryImpl* query = new dmQueryImpl();
  bool status = query->setQueryString(op, query_string);
  if (status)
    query->setProviderName(provider_name);
  return query;
}
