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
#include <msgpack.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "rtError.h"
#include "rtLog.h"

#if defined(__GNUC__)                                                          \
    && ((__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ > 8)))           \
    && !defined(NO_ATOMICS)
#include <stdatomic.h>
#elif defined(__GNUC__)                                                        \
    && ((__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ > 0)))           \
    && !defined(NO_ATOMICS)
#define atomic_int volatile int
#else
#define atomic_int volatile int
static pthread_mutex_t g_atomic_mutex = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;
#endif

static void rt_atomic_fetch_add(atomic_int* var, int value)
{
#if defined(__GNUC__)                                                          \
    && ((__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ > 8)))           \
    && !defined(NO_ATOMICS)
    __atomic_fetch_add(var, value, __ATOMIC_SEQ_CST);
#elif defined(__GNUC__)                                                        \
    && ((__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ > 0)))           \
    && !defined(NO_ATOMICS)

    __sync_fetch_and_add(var, value);
#else
    pthread_mutex_lock(&g_atomic_mutex);

    if(NULL != var)
        *(var) = *(var) + value;

    pthread_mutex_unlock(&g_atomic_mutex);
#endif
}

static void rt_atomic_fetch_sub(atomic_int* var, int value)
{
#if defined(__GNUC__)                                                          \
    && ((__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ > 8)))           \
    && !defined(NO_ATOMICS)
    __atomic_fetch_sub(var, value, __ATOMIC_SEQ_CST);
#elif defined(__GNUC__)                                                        \
    && ((__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ > 0)))           \
    && !defined(NO_ATOMICS)
    __sync_fetch_and_sub(var, value);

#else
    pthread_mutex_lock(&g_atomic_mutex);

    if(NULL != var)
        *(var) = *(var) - value;

    pthread_mutex_unlock(&g_atomic_mutex);
#endif
}

typedef struct 
{
    atomic_int count;
    msgpack_sbuffer sbuf;
    msgpack_packer pk;
    msgpack_unpacked upk;
    size_t read_offset;
    int meta_offset;
} _rtMessage;

typedef _rtMessage * rtMessage;



void print(char const* buf, unsigned int len)
{
    size_t i = 0;
    for(; i < len ; ++i)
        printf("<%c>-<0x%x> ", 0xff & buf[i], 0xff & buf[i]);
    printf("\n");
}
rtError rtMessage_Create(rtMessage* message)
{
    _rtMessage * ptr = (_rtMessage *)malloc(sizeof(_rtMessage));
    msgpack_sbuffer_init(&ptr->sbuf);
    msgpack_packer_init(&ptr->pk, &ptr->sbuf, msgpack_sbuffer_write);
    msgpack_unpacked_init(&ptr->upk);
    *message = ptr;
    ptr->read_offset = 0;
    ptr->meta_offset = 0;
    (*message)->count = 0;
    rt_atomic_fetch_add(&(*message)->count, 1);
    return 0;
}

rtError rtMessage_Release(rtMessage m)
{
    rt_atomic_fetch_sub(&m->count, 1);
    if (m->count == 0)
    {
        msgpack_sbuffer_destroy(&m->sbuf);
        msgpack_unpacked_destroy(&m->upk);
        free(m);
    }
    return 0;
}
rtError rtMessage_FromBytes(rtMessage* message, uint8_t const* buff, int n)
{
    _rtMessage * ptr = (_rtMessage *)malloc(sizeof(_rtMessage));
    msgpack_sbuffer_init(&ptr->sbuf);
    msgpack_unpacked_init(&ptr->upk);
    msgpack_sbuffer_write((void *)&ptr->sbuf, (const char *)buff, n);
    *message = ptr;
    ptr->read_offset = 0;
    ptr->meta_offset = 0;
    (*message)->count = 0;
    rt_atomic_fetch_add(&(*message)->count, 1);
    return 0;
}
rtError rtMessage_ToByteArray(rtMessage message, uint8_t** buff, uint32_t* n)
{
    *buff = (uint8_t *)message->sbuf.data;
    *n = message->sbuf.size;
    return 0;
}
    rtError
rtMessage_ToByteArrayWithSize(rtMessage message, uint8_t** buff, uint32_t suggested_size, uint32_t* n)
{
    (void) suggested_size;
    return rtMessage_ToByteArray(message, buff, n);
}
void rtMessage_SetString(rtMessage message, char const* name, char const* value)
{
    (void) name;
    int length = strlen(value) + 1;
    msgpack_pack_str(&message->pk, length);
    msgpack_pack_str_body(&message->pk, value, length);
}

rtError rtMessage_SetInt32(rtMessage message, char const* name, int32_t value)
{
    (void) name;
    if(0 == msgpack_pack_int32(&message->pk, value))
        return RT_OK;
    else
        return RT_FAIL;
}
static rtError rtMessage_UnpackNextItem(rtMessage const m)
{
    if(MSGPACK_UNPACK_SUCCESS == msgpack_unpack_next(&m->upk, m->sbuf.data, m->sbuf.size, &m->read_offset))
        return RT_OK;
    else
        return RT_FAIL;
}
rtError rtMessage_GetString(rtMessage const m, char const* name, char const** value)
{
    (void) name;
    rtError ret = RT_OK;
    ret = rtMessage_UnpackNextItem(m);
    if((0 == ret) && (MSGPACK_OBJECT_STR == m->upk.data.type))
        *value = m->upk.data.via.str.ptr;
    else
        ret = RT_FAIL;
    return ret;
}
rtError rtMessage_GetInt32(rtMessage const m, char const* name, int32_t* value)
{
    (void) name;
    rtError ret = RT_OK;
    ret = rtMessage_UnpackNextItem(m);
    if((0 == ret) && ((MSGPACK_OBJECT_POSITIVE_INTEGER == m->upk.data.type) || (MSGPACK_OBJECT_NEGATIVE_INTEGER == m->upk.data.type)))
        *value = (int32_t)m->upk.data.via.i64;
    else
        ret = RT_FAIL;
    return ret;
}

rtError rtMessage_AddMessage(rtMessage m, char const* name, rtMessage const item)
{
    (void) name;
    if (!m || !item)
        return RT_ERROR_INVALID_ARG;
    int ret = 0;
    ret |= msgpack_pack_bin(&m->pk, item->sbuf.size);
    ret |= msgpack_pack_bin_body(&m->pk, item->sbuf.data, item->sbuf.size);
    if(0 == ret)
        return RT_OK;
    else
        return RT_FAIL;
}

rtError rtMessage_ToString(rtMessage const m, char** s, uint32_t* n)
{
    const int ALLOC_INCREMENT = 2048;
    int size = ALLOC_INCREMENT;
    char * buffer = (char *)malloc(size);
    if(NULL == buffer)
        return RT_FAIL;
    *s = buffer;

    int saved_offset = m->read_offset;
    m->read_offset = 0;

    int write_offset = 0;
    while(0 == rtMessage_UnpackNextItem(m))
    {
        if((1 >= (size - write_offset)) || 
                //Special handling for text as snprintf will write past a buffer boundary if precision calls for it.
                ((MSGPACK_OBJECT_STR == m->upk.data.type) && ((int)m->upk.data.via.str.size > (size - write_offset - 3/*account for quotes + terminator in the output*/))))
        {
            //Truncated output. Indicate so.
            //First, make room for ellipsis

            if((size - write_offset) <= 4)
                write_offset = size - 4;
            buffer[write_offset++] = '.';
            buffer[write_offset++] = '.';
            buffer[write_offset++] = '.';
            buffer[write_offset++] = 0;
            break;
        }
        write_offset += msgpack_object_print_buffer(buffer + write_offset, size - write_offset, m->upk.data);
        if(1 < (size - write_offset))
        {
            buffer[write_offset++] = ' ';
            buffer[write_offset] = '\0';
        }
    }
    //Trim the last space.
    if(write_offset && (' ' == buffer[write_offset - 1]))
    {
        write_offset--;
        buffer[write_offset] = '\0';
    }

    m->read_offset = saved_offset;
    *n = write_offset;
    return RT_OK;
}

rtError rtMessage_AddString(rtMessage m, char const* name, char const* value)
{
    (void) name;
    rtMessage_SetString(m, name, value);
    return RT_OK;
}

rtError rtMessage_SetMessage(rtMessage m, char const* name, rtMessage item)
{
    (void) name;
    if (!m|| !item)
        return RT_ERROR_INVALID_ARG;
    int ret = 0;
    ret |= msgpack_pack_bin(&m->pk, item->sbuf.size);
    ret |= msgpack_pack_bin_body(&m->pk, item->sbuf.data, item->sbuf.size);
    if(0 == ret)
        return RT_OK;
    else
        return RT_FAIL;
}

rtError rtMessage_GetMessage(rtMessage const message, char const* name, rtMessage* clone)
{
    (void) name;
    rtMessage newmsg = NULL;
    rtError ret = RT_OK;
    ret = rtMessage_UnpackNextItem(message);
    if((0 == ret) && (MSGPACK_OBJECT_BIN == message->upk.data.type))
    {
        rtMessage_FromBytes(&newmsg, (uint8_t *)message->upk.data.via.bin.ptr, message->upk.data.via.bin.size);
        *clone = newmsg;
    }
    else
        ret = RT_FAIL;
    return ret;
}

rtError rtMessage_AddBinaryData(rtMessage message, char const* name, void const * ptr, const uint32_t size)
{
    (void) name;
    int ret = 0;
    ret |= msgpack_pack_bin(&message->pk, size);
    ret |= msgpack_pack_bin_body(&message->pk, ptr, size);
    if(0 == ret)
        return RT_OK;
    else
        return RT_FAIL;
}

rtError rtMessage_GetStringValue(rtMessage const m, char const* name, char* fieldvalue, int n)
{
    (void) name;
    rtError ret = rtMessage_UnpackNextItem(m);
    if((0 == ret) && (MSGPACK_OBJECT_STR == m->upk.data.type))
    {
        strncpy(fieldvalue, m->upk.data.via.str.ptr, n);
    }
    else
        ret = RT_FAIL;
    return ret;
}

rtError rtMessage_GetArrayLength(rtMessage const m, char const* name, int32_t* length)
{
    (void) name;
    (void) m;
    (void) length;
    *length = 0;
    return RT_FAIL; //Unimplemented
}

rtError rtMessage_SetDouble(rtMessage message, char const* name, double value)
{
    (void) name;
    if(0 == msgpack_pack_double(&message->pk, value))
        return RT_OK;
    else
        return RT_FAIL;
}

rtError rtMessage_GetBinaryData(rtMessage m, char const* name, const void ** ptr, uint32_t *size)
{
    (void) name;
    rtError ret = rtMessage_UnpackNextItem(m);
    if((0 == ret) && (MSGPACK_OBJECT_BIN == m->upk.data.type))
    {
        *size = m->upk.data.via.bin.size;
        *ptr = (const void *)m->upk.data.via.bin.ptr; 
    }
    else
        ret = RT_FAIL;
    return ret;
}

rtError rtMessage_GetMessageItem(rtMessage const m, char const* name, int32_t idx, rtMessage* msg)
{
    (void) name;
    (void) idx;
    return rtMessage_GetMessage(m, name, msg);
}

rtError rtMessage_GetStringItem(rtMessage const m, char const* name, int32_t idx, char* value, int len)
{
    (void) name;
    (void) idx;
    return rtMessage_GetStringValue(m, name, value, len);
}

rtError rtMessage_GetDouble(rtMessage const  message, char const* name, double* value)
{
    (void) name;
    rtError ret = rtMessage_UnpackNextItem(message);
    if((0 == ret) && (MSGPACK_OBJECT_FLOAT == message->upk.data.type))
    {
        *value = message->upk.data.via.f64;
    }
    return ret;
}

void rtMessage_BeginMetaSectionWrite(rtMessage message)
{
    message->meta_offset = message->sbuf.size;
}

void rtMessage_EndMetaSectionWrite(rtMessage message)
{
    msgpack_pack_int32(&message->pk, message->meta_offset | 0x80000000);
    message->sbuf.data[message->sbuf.size - 4] &= 0x7F; //Clear the effects of mask, now that offset is stored as a 4-byte integer.
}

void rtMessage_BeginMetaSectionRead(rtMessage message)
{
    int section_offset = 0;
    message->meta_offset = message->read_offset; //For safekeeping.
    message->read_offset = message->sbuf.size - 5;
    (void) rtMessage_GetInt32(message, "offset", &section_offset);
    message->read_offset = section_offset;
}

void rtMessage_EndMetaSectionRead(rtMessage message)
{
    message->read_offset = message->meta_offset;
}

