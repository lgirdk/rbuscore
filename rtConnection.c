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
#include "rtMessage.h"
#include "rtConnection.h"
#include "rtEncoder.h"
#include "rtError.h"
#include "rtLog.h"
#include "rtMessageHeader.h"
#include "rtSocket.h"

#if defined(__GNUC__)                                                          \
    && ((__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ > 8)))           \
    && !defined(NO_ATOMICS)
#define C11_ATOMICS_SUPPORTED 1
#include <stdatomic.h>
#else
typedef volatile int atomic_uint_least32_t;
#define _GNU_SOURCE 1
#endif

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <glib.h>
#include <semaphore.h>
#include <errno.h>
#include <time.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/time.h>

#define RTMSG_LISTENERS_MAX 64
#define RTMSG_SEND_BUFFER_SIZE (1024 * 8)
#ifndef SOL_TCP
#define SOL_TCP 6
#endif

#define DEFAULT_SEND_BUFFER_SIZE 1024
struct _rtListener
{
  int                     in_use;
  void*                   closure;
  char*                   expression;
  uint32_t                subscription_id;
  rtMessageCallback       callback;
};

struct _rtConnection
{
  int                     fd;
  struct sockaddr_storage local_endpoint;
  struct sockaddr_storage remote_endpoint;
  uint8_t*                send_buffer;
  uint8_t*                recv_buffer;
  int                     recv_buffer_capacity;
  atomic_uint_least32_t   sequence_number;
  char*                   application_name;
  rtConnectionState       state;
  char                    inbox_name[RTMSG_HEADER_MAX_TOPIC_LENGTH];
  struct _rtListener      listeners[RTMSG_LISTENERS_MAX];
  rtMessage               response;
  pthread_mutex_t         mutex;
  GList *                 pending_requests_list;
  rtMessageSimpleCallback default_callback;
  unsigned int            run_dispatch;
  pthread_t               dispatch_thread;
};

typedef struct 
{
  uint32_t sequence_number;
  uint32_t flags;
  sem_t sem;
  rtMessage response;
}pending_request;

static pid_t g_dispatch_tid;
static int g_taint_packets = 0; 
static int rtConnection_StartDispatch(rtConnection con);
static int rtConnection_StopDispatch(rtConnection con);

static void onInboxMessage(rtMessageHeader const* hdr, uint8_t const* p, uint32_t n, void* closure)
{
  struct _rtConnection* con = (struct _rtConnection *) closure;
  if (hdr->flags & rtMessageFlags_Response)
  {
    if(hdr->flags & rtMessageFlags_Undeliverable)
    {
      con->response = NULL;
    }
    else
    {
      rtMessage_FromBytes(&con->response, p, n);
    }

    pthread_mutex_lock(&con->mutex);
    GList *list;
    for(list = con->pending_requests_list; list != NULL; list = list->next)
    {
      pending_request *entry = (pending_request *)list->data;
      if(entry->sequence_number == hdr->sequence_number)
      {
        entry->response = con->response;
        entry->flags = hdr->flags;
        con->response = NULL;
        sem_post(&(entry->sem));
        break;
      }
    }
    pthread_mutex_unlock(&con->mutex);

    if(NULL != con->response)
      rtMessage_Release(con->response);
  }
  else if(NULL != con->default_callback)
  {
    rtMessage msg;
    rtMessage_FromBytes(&msg, p, n);
    con->default_callback(hdr, msg);
    rtMessage_Release(msg);
  }
}

static rtError rtConnection_SendInternal(rtConnection con, char const* topic,
  uint8_t const* buff, uint32_t n, char const* reply_topic, int flags, uint32_t sequence_number);
  

static uint32_t
rtConnection_GetNextSubscriptionId()
{
  static uint32_t next_id = 1;
  return next_id++;
}

static int
rtConnection_ShouldReregister(rtError e)
{
  if (rtErrorFromErrno(ENOTCONN) == e) return 1;
  if (rtErrorFromErrno(EPIPE) == e) return 1;
  return 0;
}

static rtError
rtConnection_ConnectAndRegister(rtConnection con)
{
  int i;
  int ret;
  socklen_t socket_length;

  i = 1;
  ret = 0;
  rtSocketStorage_GetLength(&con->remote_endpoint, &socket_length);

  if (con->fd != -1)
    close(con->fd);

  rtLog_Info("connecting to router");
  con->fd = socket(con->remote_endpoint.ss_family, SOCK_STREAM, 0);
  if (con->fd == -1)
    return rtErrorFromErrno(errno);
  rtLog_Info("router connection up");

  fcntl(con->fd, F_SETFD, fcntl(con->fd, F_GETFD) | FD_CLOEXEC);
  setsockopt(con->fd, SOL_TCP, TCP_NODELAY, &i, sizeof(i));

  int retry = 0;
  while (retry <= 3)
  {
    ret = connect(con->fd, (struct sockaddr *)&con->remote_endpoint, socket_length);
    if (ret == -1)
    {
      if (ret == ECONNREFUSED)
      {
        sleep(1);
        retry++;
      }
    }
    else
    {
      break;
    }
  }

  rtSocket_GetLocalEndpoint(con->fd, &con->local_endpoint);

  {
    uint16_t local_port;
    uint16_t remote_port;
    char local_addr[64];
    char remote_addr[64];

    rtSocketStorage_ToString(&con->local_endpoint, local_addr, sizeof(local_addr), &local_port);
    rtSocketStorage_ToString(&con->remote_endpoint, remote_addr, sizeof(remote_addr), &remote_port);
    rtLog_Info("connect %s:%d -> %s:%d", local_addr, local_port, remote_addr, remote_port);
  }

  for (i = 0; i < RTMSG_LISTENERS_MAX; ++i)
  {
    if (con->listeners[i].in_use)
    {
      rtMessage m;
      rtMessage_Create(&m);
      rtMessage_SetString(m, "topic", con->listeners[i].expression);
      rtMessage_SetInt32(m, "route_id", con->listeners[i].subscription_id);
      rtConnection_SendMessage(con, m, "_RTROUTED.INBOX.SUBSCRIBE");
      rtMessage_Release(m);
    }
  }

  return RT_OK;
}

static rtError
rtConnection_EnsureRoutingDaemon()
{
  int ret = system("rtrouted 2> /dev/null");

  // 127 is return from sh -c (@see system manpage) when command is not found in $PATH
  if (WEXITSTATUS(ret) == 127)
    ret = system("./rtrouted 2> /dev/null");

  // exit(12) from rtrouted means another instance is already running
  if (WEXITSTATUS(ret) == 12)
    return RT_OK;

  if (ret != 0)
    rtLog_Error("Cannot run rtrouted. Code:%d", ret);

  return RT_OK;
}

static rtError
rtConnection_ReadUntil(rtConnection con, uint8_t* buff, int count, int32_t timeout)
{
  ssize_t bytes_read = 0;
  ssize_t bytes_to_read = count;

  (void) timeout;

  while (bytes_read < bytes_to_read)
  {
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(con->fd, &read_fds);

    if ((0 < timeout) && (timeout != INT32_MAX))
    {
      // TODO: include suseconds_t tv_usecs;
      time_t seconds = (timeout / 1000);
      struct timeval tv = { seconds, 0 };
      select(con->fd + 1, &read_fds, NULL, NULL, &tv);
      if (!FD_ISSET(con->fd, &read_fds))
        return RT_ERROR_TIMEOUT;
    }

    ssize_t n = recv(con->fd, buff + bytes_read, (bytes_to_read - bytes_read), MSG_NOSIGNAL);
    if (n == 0)
    {
      if(0 != con->run_dispatch)
        rtLog_Error("Failed to read error : %s", rtStrError(rtErrorFromErrno(ENOTCONN)));
      return rtErrorFromErrno(ENOTCONN);
    }

    if (n == -1)
    {
      if (errno == EINTR)
        continue;
      rtError e = rtErrorFromErrno(errno);
      rtLog_Error("failed to read from fd %d. %s", con->fd, rtStrError(e));
      return e;
    }
    bytes_read += n;
  }
  return RT_OK;
}

rtError
rtConnection_Create(rtConnection* con, char const* application_name, char const* router_config)
{
  int i;
  rtError err;

  i = 0;
  err = RT_OK;

  rtConnection c = (rtConnection) malloc(sizeof(struct _rtConnection));
  if (!c)
    return rtErrorFromErrno(ENOMEM);

  pthread_mutexattr_t mutex_attribute;
  pthread_mutexattr_init(&mutex_attribute);
  pthread_mutexattr_settype(&mutex_attribute, PTHREAD_MUTEX_ERRORCHECK);
  if( 0 != pthread_mutex_init(&c->mutex, &mutex_attribute))
  {
    rtLog_Error("Could not initialize mutex. Cannot create connection.\n");
    free(c);
    return RT_ERROR;
  }
  for (i = 0; i < RTMSG_LISTENERS_MAX; ++i)
  {
    c->listeners[i].in_use = 0;
    c->listeners[i].closure = NULL;
    c->listeners[i].callback = NULL;
    c->listeners[i].subscription_id = 0;
  }

  c->response = NULL;
  c->send_buffer = (uint8_t *) malloc(RTMSG_SEND_BUFFER_SIZE);
  c->recv_buffer = (uint8_t *) malloc(RTMSG_SEND_BUFFER_SIZE);
  c->recv_buffer_capacity = RTMSG_SEND_BUFFER_SIZE;
  c->sequence_number = 1;
#ifdef C11_ATOMICS_SUPPORTED
  atomic_init(&(c->sequence_number), 1);
#endif
  c->application_name = strdup(application_name);
  c->fd = -1;
  c->pending_requests_list = NULL;
  c->default_callback = NULL;
  c->run_dispatch = 0;
  memset(c->inbox_name, 0, RTMSG_HEADER_MAX_TOPIC_LENGTH);
  memset(&c->local_endpoint, 0, sizeof(struct sockaddr_storage));
  memset(&c->remote_endpoint, 0, sizeof(struct sockaddr_storage));
  memset(c->send_buffer, 0, RTMSG_SEND_BUFFER_SIZE);
  memset(c->recv_buffer, 0, RTMSG_SEND_BUFFER_SIZE);
  snprintf(c->inbox_name, RTMSG_HEADER_MAX_TOPIC_LENGTH, "_%s.INBOX.%d", c->application_name, (int) getpid());

  err = rtSocketStorage_FromString(&c->remote_endpoint, router_config);
  if (err != RT_OK)
  {
    rtLog_Warn("failed to parse:%s. %s", router_config, rtStrError(err));
    free(c);
    return err;
  }

  err = rtConnection_ConnectAndRegister(c);
  if (err != RT_OK)
  {
  }

  if (err == RT_OK)
  {
    rtConnection_AddListener(c, c->inbox_name, onInboxMessage, c);
    *con = c;
  }
  rtConnection_StartDispatch(c);
  return err;
}

rtError
rtConnection_Destroy(rtConnection con)
{
  if (con)
  {
    pthread_mutex_lock(&con->mutex);
    con->run_dispatch = 0;
    pthread_mutex_unlock(&con->mutex);
    
    if (con->fd != -1)
      shutdown(con->fd, SHUT_RDWR);
    
    rtConnection_StopDispatch(con);
    
    if (con->fd != -1)
      close(con->fd);
    if (con->send_buffer)
      free(con->send_buffer);
    if (con->recv_buffer)
      free(con->recv_buffer);
    if (con->application_name)
      free(con->application_name);

    for (unsigned int i = 0; i < RTMSG_LISTENERS_MAX; ++i)
    {
      if (con->listeners[i].in_use)
        free(con->listeners[i].expression);
    }
    /*Unblock all threads waiting for RPC responses.*/
    pthread_mutex_lock(&con->mutex);
    int found_pending_requests = 0;
    GList *list;
    for(list = con->pending_requests_list; list != NULL; list = list->next)
    {
      found_pending_requests = 1;
      sem_post(&((pending_request *)(list->data))->sem);
    }
    pthread_mutex_unlock(&con->mutex);
    if(0 != found_pending_requests)
    {
      rtLog_Error("Warning! Found pending requests while destroying connection.");
      sleep(1); /* ugly hack to allow all sendRequest() calls to return and stop using con->* data members. Hopefully, this will never be 
      executed in practice. Revisit if necessary. */
    }

    pthread_mutex_destroy(&con->mutex);
    free(con);
  }
  return 0;
}

rtError
rtConnection_SendErrorMessageToCaller(int clnt_fd ,rtMessageHeader const* request_header)
{
    rtConnection t_con = (rtConnection) malloc(sizeof(struct _rtConnection));
    memset(t_con,0,sizeof(struct _rtConnection));
 
    t_con->fd = clnt_fd;
    t_con->send_buffer = (uint8_t *) malloc(RTMSG_SEND_BUFFER_SIZE);
    t_con->recv_buffer = (uint8_t *) malloc(RTMSG_SEND_BUFFER_SIZE);
    memset(t_con->send_buffer, 0, RTMSG_SEND_BUFFER_SIZE);
    memset(t_con->recv_buffer, 0, RTMSG_SEND_BUFFER_SIZE);
    //Adding topic in request header
    rtMessageHeader new_header;
    rtMessageHeader_Init(&new_header);
    strcpy(new_header.topic, "NO.ROUTE.RESPONSE");
    strcpy(new_header.reply_topic, request_header->reply_topic);

    //Create Response
    rtMessage res;
    rtMessage_Create(&res);
    rtMessage msg;
    rtMessage_Create(&msg);
    rtMessage_SetString(msg, "name", "Error Message");
    rtMessage_SetString(msg, "value", " ");
    rtMessage_SetString(msg, "status_msg", "No Route found for this Parameter");
    rtMessage_SetInt32(msg, "status", 1);
    rtMessage_AddMessage(res, "result", msg);
    
    //Send response
    rtConnection_SendResponse(t_con, &new_header, res, 1000);
    return RT_OK;
}

rtError
rtConnection_SendMessage(rtConnection con, rtMessage msg, char const* topic)
{
  uint8_t* p;
  uint32_t n;
  rtError err;
  rtMessage_ToByteArrayWithSize(msg, &p, DEFAULT_SEND_BUFFER_SIZE, &n);
  pthread_mutex_lock(&con->mutex);
  err = rtConnection_SendBinary(con, topic, p, n);
  pthread_mutex_unlock(&con->mutex);
  return err;
}

rtError
rtConnection_SendMessageWithSenderInfo(rtConnection con, rtMessage msg, char const* topic, char const* sender)
{
  uint8_t* p;
  uint32_t n;
  rtError err;
  uint32_t sequence_number;
  rtMessage_ToByteArrayWithSize(msg, &p, DEFAULT_SEND_BUFFER_SIZE, &n);
  pthread_mutex_lock(&con->mutex);
#ifdef C11_ATOMICS_SUPPORTED
  sequence_number = atomic_fetch_add_explicit(&con->sequence_number, 1, memory_order_relaxed);
#else
  sequence_number = __sync_fetch_and_add(&con->sequence_number, 1);
#endif
  err = rtConnection_SendInternal(con, topic, p, n, sender, 0, sequence_number);
  pthread_mutex_unlock(&con->mutex);
  return err;
}

rtError
rtConnection_SendResponse(rtConnection con, rtMessageHeader const* request_hdr, rtMessage const res, int32_t timeout)
{
  uint8_t* p;
  uint32_t n;
  rtError err;

  rtMessage_ToByteArrayWithSize(res, &p, DEFAULT_SEND_BUFFER_SIZE, &n);
  err = rtConnection_SendInternal(con, request_hdr->reply_topic, p, n, request_hdr->topic, rtMessageFlags_Response,
          request_hdr->sequence_number);
  (void) timeout;

  return err;
}

rtError
rtConnection_SendBinary(rtConnection con, char const* topic, uint8_t const* p, uint32_t n)
{
  uint32_t sequence_number;
#ifdef C11_ATOMICS_SUPPORTED
  sequence_number = atomic_fetch_add_explicit(&con->sequence_number, 1, memory_order_relaxed);
#else
  sequence_number = __sync_fetch_and_add(&con->sequence_number, 1);
#endif
  return rtConnection_SendInternal(con, topic, p, n, NULL, 0, sequence_number);
}

rtError
rtConnection_SendRequest(rtConnection con, rtMessage const req, char const* topic,
  rtMessage* res, int32_t timeout)
{
  rtError ret = RT_OK;
  uint8_t* p;
  uint32_t n;
  rtError err;
  struct timespec until;
  int wait_result;
  uint32_t sequence_number;
  
  pid_t tid = syscall(__NR_gettid);
  rtMessage_ToByteArrayWithSize(req, &p, DEFAULT_SEND_BUFFER_SIZE, &n);
  pthread_mutex_lock(&con->mutex);
#ifdef C11_ATOMICS_SUPPORTED
  sequence_number = atomic_fetch_add_explicit(&con->sequence_number, 1, memory_order_relaxed);
#else
  sequence_number = __sync_fetch_and_add(&con->sequence_number, 1);
#endif
 
  /*Populate the pending request and enqueue it.*/
  pending_request queue_entry; 
  queue_entry.sequence_number = sequence_number;
  queue_entry.flags = 0;
  sem_init(&queue_entry.sem, 0, 0);
  queue_entry.response = NULL;

  con->pending_requests_list = g_list_prepend(con->pending_requests_list, (gpointer)&queue_entry);
  
  err = rtConnection_SendInternal(con, topic, p, n, con->inbox_name, rtMessageFlags_Request, sequence_number);
  if (err != RT_OK)
  {
    ret = err;
    goto dequeue_and_return;
  }
  pthread_mutex_unlock(&con->mutex);

  if(tid != g_dispatch_tid)
  {

    clock_gettime(CLOCK_REALTIME, &until);
    until.tv_sec += timeout / 1000;
    until.tv_nsec += ((long)timeout % 1000L) * 1000000L;
    if(1000000000L < until.tv_nsec)
    {
      until.tv_sec += 1;
      until.tv_nsec -= 1000000000L;
    }

    wait_result = sem_timedwait(&queue_entry.sem, &until); //TODO: handle wake triggered by signals
  }
  else
  {
    //Handle nested dispatching.
    struct timeval start_time, end_time, diff;
    gettimeofday(&start_time, NULL);
    do
    {
      if((err = rtConnection_TimedDispatch(con, timeout)) == RT_OK)
      {
        int sem_value = 0;
        sem_getvalue(&queue_entry.sem, &sem_value);
        if(0 < sem_value)
        {
          wait_result = 0;
          break;
        }
        else
        {
          //It's a response to a different message. Adjust the timeout value and try again.
          gettimeofday(&end_time, NULL);
          timersub(&end_time, &start_time, &diff);
          long long diff_ms = (diff.tv_sec * 1000ll + (long long)(diff.tv_usec / 1000ll));
          if((long long)timeout <= diff_ms)
          {
            wait_result = 1;
            errno = ETIMEDOUT;
            break;
          }
          else
          {
            timeout -= (int32_t)diff_ms;
            //rtLog_Info("Retry nested call with timeout of %d ms", timeout);
          }
        }
      }
      else
      {
        rtLog_Error("Nested read failed.");
        wait_result = 1;
        break;
      }
    } while(RT_OK == err);
    
  }
  if(0 == wait_result)
  {
    /*Sem posted*/
    pthread_mutex_lock(&con->mutex);
    if (queue_entry.response != NULL)
    {
      // TODO: add ref counting to rtMessage
      *res = queue_entry.response; 
    }
    else if(queue_entry.flags & rtMessageFlags_Undeliverable)
      ret = RT_OBJECT_NO_LONGER_AVAILABLE;
    else
      /*For some reason, we unblocked, but there's no data.*/
      ret = RT_ERROR;
  }
  else
  {
    /*Wait failed. Was this a timeout?*/
    if(ETIMEDOUT == errno)
      ret = RT_ERROR_TIMEOUT;
    else
      ret = RT_ERROR;
  }

dequeue_and_return:
  con->pending_requests_list = g_list_remove(con->pending_requests_list, (gpointer)&queue_entry);
  pthread_mutex_unlock(&con->mutex);
  sem_destroy(&queue_entry.sem);
  return ret;
}

rtError
rtConnection_SendInternal(rtConnection con, char const* topic, uint8_t const* buff,
  uint32_t n, char const* reply_topic, int flags, uint32_t sequence_number)
{
  rtError err;
  int num_attempts;
  int max_attempts;
  ssize_t bytes_sent;
  rtMessageHeader header;

  max_attempts = 2;
  num_attempts = 0;

  rtMessageHeader_Init(&header);
  header.payload_length = n;

  strcpy(header.topic, topic);
  header.topic_length = strlen(header.topic);
  if (reply_topic)
  {
    strcpy(header.reply_topic, reply_topic);
    header.reply_topic_length = strlen(reply_topic);
  }
  else
  {
    header.reply_topic[0] = '\0';
    header.reply_topic_length = 0;
  }
  header.sequence_number = sequence_number; 
  header.flags = flags;
#ifdef ENABLE_ROUTER_BENCHMARKING
  if(1 == g_taint_packets)
    header.flags |= rtMessageFlags_Tainted;
#endif

  err = rtMessageHeader_Encode(&header, con->send_buffer);
  if (err != RT_OK)
    return err;

  struct iovec send_vec[] = {{con->send_buffer, header.header_length}, {(void *)buff, header.payload_length}};
  struct msghdr send_hdr = {NULL, 0, send_vec, 2, NULL, 0, 0};
  do
  {
    bytes_sent = sendmsg(con->fd, &send_hdr, MSG_NOSIGNAL);
    if (bytes_sent != (header.header_length + header.payload_length))
    {
      if (bytes_sent == -1)
        err = rtErrorFromErrno(errno);
      else
        err = RT_FAIL;
    }

    if (err != RT_OK && rtConnection_ShouldReregister(err))
    {
        err = rtConnection_ConnectAndRegister(con);
    }
  }
  while ((err != RT_OK) && (num_attempts++ < max_attempts));

  return err;
}

rtError
rtConnection_AddListener(rtConnection con, char const* expression, rtMessageCallback callback, void* closure)
{
  int i;

  pthread_mutex_lock(&con->mutex);
  for (i = 0; i < RTMSG_LISTENERS_MAX; ++i)
  {
    if (!con->listeners[i].in_use)
      break;
  }

  if (i >= RTMSG_LISTENERS_MAX)
  {
    pthread_mutex_unlock(&con->mutex);
    return rtErrorFromErrno(ENOMEM);
  }

  con->listeners[i].in_use = 1;
  con->listeners[i].subscription_id = rtConnection_GetNextSubscriptionId();
  con->listeners[i].closure = closure;
  con->listeners[i].callback = callback;
  con->listeners[i].expression = strdup(expression);
  pthread_mutex_unlock(&con->mutex);
  
  rtMessage m;
  rtMessage_Create(&m);
  rtMessage_SetInt32(m, "add", 1);
  rtMessage_SetString(m, "topic", expression);
  rtMessage_SetInt32(m, "route_id", con->listeners[i].subscription_id); 
  rtConnection_SendMessage(con, m, "_RTROUTED.INBOX.SUBSCRIBE");
  rtMessage_Release(m);

  return 0;
}

rtError
rtConnection_RemoveListener(rtConnection con, char const* expression)
{
  int i;
  int route_id = 0;
  pthread_mutex_lock(&con->mutex);
  for (i = 0; i < RTMSG_LISTENERS_MAX; ++i)
  {
    if ((con->listeners[i].in_use) && (0 == strcmp(expression, con->listeners[i].expression)))
    {
        con->listeners[i].in_use = 0;
        route_id = con->listeners[i].subscription_id;
        con->listeners[i].subscription_id = 0;
        con->listeners[i].closure = NULL;
        con->listeners[i].callback = NULL;
        free(con->listeners[i].expression);
        con->listeners[i].expression = NULL;
        break;
    }
  }
  pthread_mutex_unlock(&con->mutex);

  if (i >= RTMSG_LISTENERS_MAX)
    return RT_ERROR_INVALID_ARG; 

  rtMessage m;
  rtMessage_Create(&m);
  rtMessage_SetInt32(m, "add", 0);
  rtMessage_SetString(m, "topic", expression);
  rtMessage_SetInt32(m, "route_id", route_id); 
  rtConnection_SendMessage(con, m, "_RTROUTED.INBOX.SUBSCRIBE");
  rtMessage_Release(m);
  return 0;
}

rtError
rtConnection_AddAlias(rtConnection con, char const* existing, const char *alias)
{
  int i;

  for (i = 0; i < RTMSG_LISTENERS_MAX; ++i)
  {
    if (1 == con->listeners[i].in_use)
    {
      if(0 == strncmp(con->listeners[i].expression, existing, (strlen(con->listeners[i].expression) + 1)))
      {
        rtMessage m;
        rtMessage_Create(&m);
        rtMessage_SetInt32(m, "add", 1);
        rtMessage_SetString(m, "topic", alias);
        rtMessage_SetInt32(m, "route_id", con->listeners[i].subscription_id); 
        rtConnection_SendMessage(con, m, "_RTROUTED.INBOX.SUBSCRIBE");
        rtMessage_Release(m);
        break;
      }
    }

  }

  if (i >= RTMSG_LISTENERS_MAX)
    return rtErrorFromErrno(ENOMEM);

  return 0;
}
rtError
rtConnection_RemoveAlias(rtConnection con, char const* existing, const char *alias)
{
  int i;

  for (i = 0; i < RTMSG_LISTENERS_MAX; ++i)
  {
    if (1 == con->listeners[i].in_use)
    {
      if(0 == strncmp(con->listeners[i].expression, existing, (strlen(con->listeners[i].expression) + 1)))
      {
        rtMessage m;
        rtMessage_Create(&m);
        rtMessage_SetInt32(m, "add", 0);
        rtMessage_SetString(m, "topic", alias);
        rtMessage_SetInt32(m, "route_id", con->listeners[i].subscription_id); 
        rtConnection_SendMessage(con, m, "_RTROUTED.INBOX.SUBSCRIBE");
        rtMessage_Release(m);
        break;
      }
    }

  }

  if (i >= RTMSG_LISTENERS_MAX)
    return rtErrorFromErrno(ENOMEM);

  return 0;
}
rtError
rtConnection_AddDefaultListener(rtConnection con,
  rtMessageSimpleCallback callback)
{
  con->default_callback = callback;
  return 0;
}

rtError
rtConnection_Dispatch(rtConnection con)
{
  return rtConnection_TimedDispatch(con, -1);
}

rtError
_rtConnection_ReadAndDropBytes(int fd, unsigned int bytes_to_read)
{
  uint8_t buff[512];

  while (0 < bytes_to_read)
  {
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);
    
    ssize_t n = recv(fd, buff, (sizeof(buff) > bytes_to_read ? bytes_to_read : sizeof(buff)), MSG_NOSIGNAL);
    if (n == 0)
    {
      rtLog_Error("Failed to read error : %s", rtStrError(rtErrorFromErrno(ENOTCONN)));
      return rtErrorFromErrno(ENOTCONN);
    }

    if (n == -1)
    {
      if (errno == EINTR)
        continue;
      rtError e = rtErrorFromErrno(errno);
      rtLog_Error("failed to read from fd %d. %s", fd, rtStrError(e));
      return e;
    }
    bytes_to_read -= n;
  }
  return RT_OK;
}

rtError
rtConnection_TimedDispatch(rtConnection con, int32_t timeout)
{
  int i;
  int num_attempts;
  int max_attempts;
  uint8_t const*  itr;
  rtMessageHeader hdr;
  rtError err;

  i = 0;
  num_attempts = 0;
  max_attempts = 4;

  rtMessageHeader_Init(&hdr);

  // TODO: no error handling right now, all synch I/O

  do
  {
    con->state = rtConnectionState_ReadHeaderPreamble;
    err = rtConnection_ReadUntil(con, con->recv_buffer, RTMESSAGEHEADER_PREAMBLE_LENGTH, timeout);

    if (err == RT_ERROR_TIMEOUT)
      return err;

    if (err == RT_OK)
    {
      itr = &con->recv_buffer[RTMESSAGEHEADER_HDR_LENGTH_OFFSET];
      rtEncoder_DecodeUInt16(&itr, &hdr.header_length);
      err = rtConnection_ReadUntil(con, con->recv_buffer + RTMESSAGEHEADER_PREAMBLE_LENGTH,
          (hdr.header_length-RTMESSAGEHEADER_PREAMBLE_LENGTH), timeout);
    }
    else
    {
      /* Read failed. If this is due to a connection termination initiated by us, break and return. Retry if anything else.*/
      pthread_mutex_lock(&con->mutex);
      if(0 == con->run_dispatch)
      {
        pthread_mutex_unlock(&con->mutex); //This is a controlled exit. Break the loop.
        break;
      }
      else
        pthread_mutex_unlock(&con->mutex);
    }

    if (err == RT_OK)
    {
      #if 0
      int i;
      for (i = 0; i < hdr.header_length; ++i)
      {
        if (i %16 == 0)
          printf("\n");
        printf("0x%02x ", con->recv_buffer[i]);
      }
      printf("\n\n\n");
      #endif
      err = rtMessageHeader_Decode(&hdr, con->recv_buffer);
    }

    if (err == RT_OK)
    {
      int incoming_data_size = hdr.header_length + hdr.payload_length;
      if(con->recv_buffer_capacity < incoming_data_size)
      {
        uint8_t * ptr = (uint8_t *)realloc(con->recv_buffer, incoming_data_size + 1 /*extra byte for the string terminator that'll be added further below*/);
        if(NULL != ptr)
        {
          con->recv_buffer = ptr;
          con->recv_buffer_capacity = incoming_data_size;
          rtLog_Info("Reallocated recv buffer to %d bytes to accommodate traffic.", incoming_data_size);
        }
        else
        {
          rtLog_Info("Couldn't not reallocate recv buffer to accommodate %d bytes. Message will be dropped.", incoming_data_size);
          return _rtConnection_ReadAndDropBytes(con->fd, hdr.payload_length);
        }
      }
      err = rtConnection_ReadUntil(con, con->recv_buffer + hdr.header_length, hdr.payload_length, timeout);
      if (err == RT_OK)
      {
        // help out json parsers and other string parses
        con->recv_buffer[hdr.header_length + hdr.payload_length] = '\0';
      }
    }

    if (err != RT_OK && rtConnection_ShouldReregister(err))
    {
        err = rtConnection_ConnectAndRegister(con);
    }
  }
  while ((err != RT_OK) && (num_attempts++ < max_attempts));

  if (err == RT_OK)
  {
    {
    for (i = 0; i < RTMSG_LISTENERS_MAX; ++i)
    {
      if (con->listeners[i].in_use && (con->listeners[i].subscription_id == hdr.control_data))
      {
        rtLog_Debug("found subscription match:%d", i);
        break;
      }
    }
    }
    if (i < RTMSG_LISTENERS_MAX)
    {
      con->listeners[i].callback(&hdr, con->recv_buffer + hdr.header_length, hdr.payload_length,
        con->listeners[i].closure);
    }
  }

  if(RTMSG_SEND_BUFFER_SIZE != con->recv_buffer_capacity)
  {
    free(con->recv_buffer);
    con->recv_buffer = (uint8_t *)malloc(RTMSG_SEND_BUFFER_SIZE);
    if(NULL == con->recv_buffer)
      rtLog_Fatal("Out of memory to create recv buffer.");
    con->recv_buffer_capacity = RTMSG_SEND_BUFFER_SIZE;
  }
  return RT_OK;
}

static void * rtConnection_DispatchThread(void *data)
{
  rtError err = RT_OK;
  rtConnection con = (rtConnection)data;
  g_dispatch_tid = syscall(__NR_gettid);
  rtLog_Info("Dispatch thread starts. tid: %d\n", (int)g_dispatch_tid);
  while(1 == con->run_dispatch)
  {
    if((err = rtConnection_Dispatch(con)) != RT_OK)
    {

      pthread_mutex_lock(&con->mutex);
      if(0 == con->run_dispatch)
      {
        pthread_mutex_unlock(&con->mutex); //This is a controlled exit. Break the loop.
        break;
      }
      else
        pthread_mutex_unlock(&con->mutex);
      rtLog_Error("Dispatch failed with error 0x%x.\n", err);
      sleep(5); //Avoid tight loops if we have an unrecoverable situation.
    }
  }

  rtLog_Info("Exiting dispatch thread.\n");
  return NULL;
}

static int rtConnection_StartDispatch(rtConnection con)
{
  int ret = 0;
  if(0 == con->run_dispatch)
  {
    con->run_dispatch = 1;
    if(0 != pthread_create(&con->dispatch_thread, NULL, rtConnection_DispatchThread, (void *)con))
    {
      rtLog_Error("Unable to launch dispatch thread.\n");
      ret = RT_ERROR;
    }
    else
    {
      rtLog_Info("Launched dispatch thread.\n");
    }
  }
  else
  {
    /* Do nothing. Dispatch thread is already running.*/
  }
  return ret;
}

static int rtConnection_StopDispatch(rtConnection con)
{
  rtLog_Info("Stopping dispatch.\n");
  con->run_dispatch = 0;
  pthread_join(con->dispatch_thread, NULL);
  return 0;
}


const char *
rtConnection_GetReturnAddress(rtConnection con)
{
  return con->inbox_name;
}

void
_rtConnection_TaintMessages(int i)
{
  g_taint_packets = i;
}
