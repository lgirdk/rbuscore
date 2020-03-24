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
#include "dmQueryResult.h"
#include <iterator>
#include <cstring>
#include <stdio.h>
#include <iostream>
#include <string>

dmQueryResult::dmQueryResult()
  : m_status(0)
{
}

void
dmQueryResult::clear()
{
  m_status = 0;
  m_values.clear();
  m_statusMsg.clear();
}

void
dmQueryResult::merge(dmQueryResult const& result)
{
  m_values.insert(std::end(m_values), std::begin(result.m_values), std::end(result.m_values));
  if (m_status == 0 && result.m_status != 0)
    m_status = result.m_status;
  if (!result.m_statusMsg.empty())
    setStatusMsg(result.m_statusMsg);
}

void
dmQueryResult::setStatus(int status)
{
  m_status = status;
}

void
dmQueryResult::setStatusMsg(std::string statusmsg)
{
   m_statusMsg = statusmsg;
}

void
dmQueryResult::addValue(dmNamedValue const& val, int code, char const* msg)
{
  if (m_status == 0 && code != 0)
    m_status = code;
  m_values.push_back(Param(code, msg, val));
  if (msg)
    setStatusMsg(std::string(msg));
}

dmQueryResult::Param::Param(int code, char const* msg, dmNamedValue const& val)
  : StatusCode(code)
  , Value(val)
{
  if (msg)
    StatusMessage = msg;
}
