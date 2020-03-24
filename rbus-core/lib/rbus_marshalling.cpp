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
#include "rbus_marshalling.h"
#include "msgpack.h"
/**
 * Add string field to the message
 * @param message to be modified
 * @param name of the field to be added
 * @param value of the field to be added
 * @return void
 **/
void rbus_SetString(rtMessage message, char const* name, char const* value)
{
    if(!value)
        rtMessage_SetString(message, name, "");
    else
        rtMessage_SetString(message, name, value);
}
/**
 * Add string field to array in message
 * @param message to be modified
 * @param name of the field to be added
 * @param value of the field to be added
 * @return rtError
 **/
rtError rbus_AddString(rtMessage message, char const* name, char const* value)
{
    return rtMessage_AddString(message, name, value);
}

rtError rbus_AppendString(rtMessage message, char const* value)
{
    return rtMessage_AddString(message, nullptr, (nullptr != value ? value : ""));
}
/**
 * Add binary data to message
 * @param message to be modified
 * @param name of the field to be added
 * @param ptr pointer to the data buffer (ptr may be freed after this call)
 * @param size of the buffer
 * @return rtError
 **/
rtError rbus_AddBinaryData(rtMessage message, char const* name, void const * ptr, const uint32_t size)
{
    return rtMessage_AddBinaryData(message, name, ptr, size);
}

rtError rbus_AppendBinaryData(rtMessage message, void const * ptr, const uint32_t size)
{
    return rtMessage_AddBinaryData(message, nullptr, ptr, size);
}
/**
 * Add message field to array in message
 * @param message to be modified
 * @param name of the field to be added
 * @param message to be added
 * @return rtError
 **/
rtError rbus_AddMessage(rtMessage m, char const* name, rtMessage const item)
{
    return rtMessage_AddMessage(m, name, item);
}

/**
 * Get length of array from message
 * @param message to get array length from
 * @param name of the array
 * @param fill length of array
 * @return rtError
 **/
rtError rbus_GetArrayLength(rtMessage const m, char const* name, int32_t* length)
{
    return rtMessage_GetArrayLength(m, name, length);
}

/**
 * Get string item from array in message
 * @param message to get string item from
 * @param name of the string item
 * @param index of array
 * @param value obtained
 * @param length of string item
 * @return rtError
 **/
rtError rbus_GetStringItem(rtMessage const m, char const* name, int32_t idx, char* value, int len)
{
    return rtMessage_GetStringItem(m, name, idx, value, len);
}
/**
 * Get message item from array in parent message
 * @param message to get message item from
 * @param name of message item
 * @param index of array
 * @param message obtained
 * @return rtError
 **/
rtError rbus_GetMessageItem(rtMessage const m, char const* name, int32_t idx, rtMessage* msg)
{
    return rtMessage_GetMessageItem(m, name, idx, msg);
}
/**
 * Add integer field to the message
 * @param message to be modified
 * @param name of the field to be added
 * @param integer value of the field to be added
 * @return void
 **/
rtError rbus_SetInt32(rtMessage message, char const* name, int32_t value)
{
    return rtMessage_SetInt32(message, name, value);
}
rtError rbus_AppendInt32(rtMessage message,int32_t value)
{
    return rtMessage_SetInt32(message, nullptr, value);
}
/**
 * Add double field to the message
 * @param message to be modified
 * @param name of the field to be added
 * @param double value of the field to be added
 * @return rtError
 **/
rtError rbus_SetDouble(rtMessage message, char const* name, double value)
{
    return rtMessage_SetDouble(message, name, value);
}

rtError rbus_AppendDouble(rtMessage message,double value)
{
    return rtMessage_SetDouble(message, nullptr, value);
}
/**
 * Add sub message field to the message
 * @param message to be modified
 * @param name of the field to be added
 * @param new message item to be added
 * @return rtError
 **/
rtError rbus_SetMessage(rtMessage message, char const* name, rtMessage item)
{
    return rtMessage_SetMessage(message, name, item);
}

/**
 * Get field value of type string using field name.
 * @param message to get field
 * @param name of the field
 * @param pointer to string value obtained.
 * @return rtError
 **/
rtError rbus_GetString(rtMessage const m, char const* name, char const** value)
{
    return rtMessage_GetString(m, name, value);
}
rtError rbus_PopString(rtMessage const m,char const** value)
{
    return rtMessage_GetString(m, nullptr, value);
}
/**
 * Get binary data from message
 * @param message to be read from
 * @param name of the field to get
 * @param ptr pointer to data buffer (buffer is automatically freed when message is released.) 
 * @param size of the data buffer
 * @return rtError
 **/
rtError rbus_GetBinaryData(rtMessage message, char const* name, const void ** ptr, uint32_t *size)
{
    return rtMessage_GetBinaryData(message, name, ptr, size);
}
rtError rbus_PopBinaryData(rtMessage message, const void ** ptr, uint32_t *size)
{
    return rtMessage_GetBinaryData(message, nullptr, ptr, size);
}

/**
 * Get field value of type string using field name.
 * @param message to get field
 * @param name of the field
 * @param pointer to string value obtained.
 * @param size of value obtained
 * @return rtError
 **/
rtError rbus_GetStringValue(rtMessage const m, char const* name, char* value, int n)
{
    return rtMessage_GetStringValue(m, name, value, n);
}

/**
 * Get field value of type integer using field name.
 * @param message to get field
 * @param name of the field
 * @param pointer to integer value obtained.
 * @return rtError
 **/
rtError rbus_GetInt32(rtMessage const m, char const* name, int32_t* value)
{
    return rtMessage_GetInt32(m, name, value);
}
rtError rbus_PopInt32(rtMessage const m, int32_t* value)
{
    return rtMessage_GetInt32(m, nullptr, value);
}

/**
 * Get field value of type double using field name.
 * @param message to get field
 * @param name of the field
 * @param pointer to double value obtained.
 * @return rtError
 **/
rtError rbus_GetDouble(rtMessage const m, char const* name, double* value)
{
    return rtMessage_GetDouble(m, name, value);
}
rtError rbus_PopDouble(rtMessage const m, double* value)
{
    return rtMessage_GetDouble(m, nullptr, value);
}

/**
 * Get field value of type message using name
 * @param message to get field
 * @param name of the field
 * @param message obtained
 * @return rtError
 **/
rtError rbus_GetMessage(rtMessage const m, char const* name, rtMessage* item)
{
    return rtMessage_GetMessage(m, name, item);
}
