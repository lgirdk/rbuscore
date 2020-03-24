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

#ifndef __DM_VALUE_H__
#define __DM_VALUE_H__

#include <stdint.h>
#include <memory>
#include <string>

enum dmValueType
{
  dmValueType_Int8,
  dmValueType_Int16,
  dmValueType_Int32,
  dmValueType_Int64,
  dmValueType_UInt8,
  dmValueType_UInt16,
  dmValueType_UInt32,
  dmValueType_UInt64,
  dmValueType_String,
  dmValueType_Single,
  dmValueType_Double,
  dmValueType_Unknown
};

dmValueType dmValueType_fromString(char const* s);
char const* dmValueType_toString(dmValueType t);

class dmValue
{
public:
  dmValue(std::string const& s);
  dmValue(char const* s);
  dmValue(int8_t n);
  dmValue(int16_t n);
  dmValue(int32_t n);
  dmValue(int64_t n);
  dmValue(uint8_t n);
  dmValue(uint16_t n);
  dmValue(uint32_t n);
  dmValue(uint64_t n);
  dmValue(float f);
  dmValue(double d);

  std::string toString() const;

  inline dmValueType type() const
    { return m_type; }

private:
  union value {
    value() { }
    value(int8_t n) : int8Value(n) { }
    value(int16_t n) : int16Value(n) { }
    value(int32_t n) : int32Value(n) { }
    value(int64_t n) : int64Value(n) { }
    value(uint8_t n) : uint64Value(n) { }
    value(uint16_t n) : uint64Value(n) { }
    value(uint32_t n) : uint64Value(n) { }
    value(uint64_t n) : uint64Value(n) { }
    value(float f) : singleValue(f) { }
    value(double d) : doubleValue(d) { }
    uint8_t     uint8Value;
    uint16_t    uint16Value;
    uint32_t    uint32Value;
    uint64_t    uint64Value;
    int8_t      int8Value;
    int16_t     int16Value;
    int32_t     int32Value;
    int64_t     int64Value;
    float       singleValue;
    double      doubleValue;
  };

  dmValueType   m_type;
  std::string   m_string;
  value         m_value;
};

class dmNamedValue
{
public:
  dmNamedValue(std::string const& name, dmValue const& value)
    : m_name(name)
    , m_value(value) 
  {
  }

  inline std::string const& name() const
    { return m_name; }

  inline dmValue const& value() const
    { return m_value; }

private:
  std::string m_name;
  dmValue m_value;
};

#endif
