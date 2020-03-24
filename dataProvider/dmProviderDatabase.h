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
#ifndef __DM_PROVIDER_DATABASE_H__
#define __DM_PROVIDER_DATABASE_H__

#include "dmProviderInfo.h"
#include "dmQuery.h"
#include "dmProviderOperation.h"
#include "dmPropertyInfo.h"

#include <map>
#include <string>
#include <vector>

class dmProviderDatabase
{
public:
  dmProviderDatabase(std::string const& dir);

  dmQuery* createQuery() const;
  dmQuery* createQuery(dmProviderOperation op, char const* s) const;
  char const* getProviderFromObject(char const* object) const;
  std::vector<char const*> getParameters(char const* provider) const;
  int isWritable(char const* param, char const* provider);
private:
  char const* getProvider(char const* query) const;
  void loadFromDir(std::string const& dir);
  void loadFile(std::string const& dir, char const* fname);
private:
  std::string m_modelDirectory;
};

#endif
