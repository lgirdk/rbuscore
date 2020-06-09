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
#include "rtSocket.h"
#include "rtError.h"
#include "rtLog.h"

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/un.h>

rtError
rtSocketStorage_ParsePort(char const* s, uint16_t* port)
{
  char const* p = strrchr(s, ':');
  if (!p)
  {
    rtLog_Error("failed to parse port from address:%s", s);
    return RT_ERROR_INVALID_ARG;
  }

  *port = (uint16_t) strtol(p + 1, NULL, 10);
  return RT_OK;
}

rtError
rtSocketStorage_GetLength(struct sockaddr_storage* endpoint, socklen_t* len)
{
  switch (endpoint->ss_family)
  {
    case AF_INET:
      *len = sizeof(struct sockaddr_in);
      break;
    case AF_INET6:
      *len = sizeof(struct sockaddr_in6);
      break;
    case AF_UNIX:
      *len = sizeof(struct sockaddr_un);
      break;
    default:
      abort();
  }
  return RT_OK;
}

rtError
rtSocketStorage_ToString(struct sockaddr_storage* ss, char* buff, int n, uint16_t* port)
{
  if (ss->ss_family == AF_INET)
  {
    struct sockaddr_in* v4 = (struct sockaddr_in *) ss;
    void* addr = &v4->sin_addr;
    int family = AF_INET;

    if (port)
      *port = ntohs(v4->sin_port);
    family = AF_INET;

    if (addr)
      inet_ntop(family, addr, buff, n);
  }
  else if (ss->ss_family == AF_UNIX)
  {
    struct sockaddr_un* un = (struct sockaddr_un *) ss;
    if (port)
      *port = 0;
    snprintf(buff, (sizeof(n) - 1), "%s%s", "unix://", un->sun_path);
  }

  return RT_OK;
}

rtError
rtSocket_GetLocalEndpoint(int fd, struct sockaddr_storage* endpoint)
{
  int         ret;
  socklen_t   len;

  len = sizeof(struct sockaddr_in);
  memset(endpoint, 0, sizeof(struct sockaddr_storage));

  if ((ret = getsockname(fd, (struct sockaddr *)endpoint, &len)) == -1)
    rtLog_Error("getsockname:%s", rtStrError(errno));

  return RT_OK;
}

#define UNIX_PATH_MAX 256

rtError
rtSocketStorage_FromString(struct sockaddr_storage* ss, char const* addr)
{
  int ret = 0;
  uint16_t port = 0;
  char ip[128];
  rtError err;
  struct sockaddr_in* v4;
  struct sockaddr_in6* v6;

  memset(ip, 0, sizeof(ip));

  if (strncmp(addr, "unix://", 7) == 0)
  {
    struct sockaddr_un* un = (struct sockaddr_un*) ss;
    un->sun_family = AF_UNIX;
    strncpy(un->sun_path, addr + 7, (sizeof(un->sun_path)-1));
    return RT_OK;
  }

  if (strncmp(addr, "tcp://", 6) != 0)
    return RT_ERROR_INVALID_ARG;

  err = rtSocketStorage_ParsePort(addr, &port);
  if (err != RT_OK)
    return err;

  char const* p = strrchr(addr + 6, ':');
  if (!p)
  {
    rtLog_Warn("invalid address string:%s", (addr+6));
    return RT_ERROR_INVALID_ARG;
  }

  strncpy(ip, addr+6, (p-addr-6));
  rtLog_Info("parsing ip address:%s", ip);

  v4 = (struct sockaddr_in *) ss;
  ret = inet_pton(AF_INET, ip, &v4->sin_addr);

  if (ret == 1)
  {
    #ifndef __linux__
    v4->sin_len = sizeof(struct sockaddr_in);
    #endif

    v4->sin_family = AF_INET;
    v4->sin_port = htons(port);
    ss->ss_family = AF_INET;

    return RT_OK;
  }
  else if (ret == 0)
  {
    v6 = (struct sockaddr_in6 *) ss;
    ret = inet_pton(AF_INET6, ip, &v6->sin6_addr);
    if (ret == 1)
    {
      // it's a numeric address
      v6->sin6_family = AF_INET6;
      v6->sin6_port = htons(port);
    }
  }
  else
  {
    return rtErrorFromErrno(errno);
  }
  return RT_OK;
}

rtError
rtSocketAddrStorage_GetLength(struct sockaddr_storage* ss, socklen_t* len)
{
  if (!len)
    return RT_ERROR_INVALID_ARG;

  if (ss->ss_family == AF_INET)
    *len = sizeof(struct sockaddr_in);
  else if (ss->ss_family == AF_INET6)
    *len = sizeof(struct sockaddr_in6);
  else if (ss->ss_family == AF_UNIX)
    *len = sizeof(struct sockaddr_un);
  else
    *len = sizeof(struct sockaddr_storage);

  return RT_OK;
}
