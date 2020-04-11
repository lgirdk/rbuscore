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
#define _GNU_SOURCE 1
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>
#include <ctype.h>

#include "rbus_core.h"
#include "rbus_marshalling.h"
#include "rtLog.h"
#include "rtVector.h"

/* Begin constant definitions.*/
static const unsigned int TIMEOUT_VALUE_FIRE_AND_FORGET = 1000;
static const unsigned int MAX_SUBSCRIBER_NAME_LENGTH = MAX_OBJECT_NAME_LENGTH;
static const char * DEFAULT_EVENT = "";
#define METHOD_ADD_EVENT_SUBSCRIPTION "_subscribe"
#define METHOD_REMOVE_EVENT_SUBSCRIPTION "_unsubscribe"
/* End constant definitions.*/

/* Begin type definitions.*/

/* Begin rbus_server */

struct _server_object;
typedef struct _server_object* server_object_t;

typedef struct _server_method
{
    char name[MAX_METHOD_NAME_LENGTH+1];
    rbus_callback_t callback;
    void * data;
} *server_method_t;


typedef struct _server_event
{
    char name[MAX_EVENT_NAME_LENGTH+1];
    server_object_t object;
    rtVector listeners /*list of strings*/;
    rbus_event_subscribe_callback_t sub_callback;
    void * sub_data;
} *server_event_t;

typedef struct _server_object
{
    char name[MAX_OBJECT_NAME_LENGTH+1];
    void* data;
    rbus_callback_t callback;
    bool process_event_subscriptions;
    rtVector methods; /*list of server_method_t*/
    rtVector subscriptions; /*list of server_event_t*/
    rbus_event_subscribe_callback_t subscribe_handler_override;
    void* subscribe_handler_data;
} *server_object_t;

void server_method_create(server_method_t* meth, char const* name, rbus_callback_t callback, void* data)
{
    (*meth) = malloc(sizeof(struct _server_method));
    strcpy((*meth)->name, name);
    (*meth)->callback = callback;
    (*meth)->data = data;
}

int server_method_compare(const void* left, const void* right)
{
    return strncmp(((server_event_t)left)->name, (char*)right, MAX_METHOD_NAME_LENGTH);
}

int server_event_compare(const void* left, const void* right)
{
    return strncmp(((server_event_t)left)->name, (char*)right, MAX_EVENT_NAME_LENGTH);
}

void server_event_create(server_event_t* event, const char * event_name, server_object_t obj, rbus_event_subscribe_callback_t sub_callback, void* sub_data)
{
    (*event) = malloc(sizeof(struct _server_event));
    rtVector_Create(&(*event)->listeners);
    strcpy((*event)->name, event_name);
    (*event)->object = obj;
    (*event)->sub_callback = sub_callback;
    (*event)->sub_data = sub_data;
}

void server_event_destroy(void* p)
{
    server_event_t event = p;
    rtVector_Destroy(event->listeners, rtVector_Cleanup_Free);
    free(event);
}

void server_event_addListener(server_event_t event, char const* listener)
{
    if(!listener)
    {
        rtLog_Error("Listener is empty.");
    }
    else if(!rtVector_HasItem(event->listeners, listener, rtVector_Compare_String))
    {
        rtVector_PushBack(event->listeners, strdup(listener));

        if(event->sub_callback)
        {
            event->sub_callback(event->object->name, event->name, listener, 1, event->sub_data);
        }

        rtLog_Info("Listener %s added for event %s.", listener, event->name);
    }
    else
    {
        rtLog_Warn("Listener %s is already registered for event %s.", listener, event->name);
    }
}

void server_event_removeListener(server_event_t event, char const* listener)
{
    if(!listener)
    {
        rtLog_Error("Listener is empty.");
    }
    else if(rtVector_HasItem(event->listeners, listener, rtVector_Compare_String))
    {
        rtLog_Warn("Removing listener %s for event %s.", listener, event->name);

        rtVector_RemoveItemByCompare(event->listeners, listener, rtVector_Compare_String, rtVector_Cleanup_Free);

        if(event->sub_callback)
        {
            event->sub_callback(event->object->name, event->name, listener, 0, event->sub_data);
        }
    }
    else
    {
        rtLog_Error("Listener %s not found for event %s.", listener, event->name);
    }
}

int server_object_compare(const void* left, const void* right)
{
    return strncmp(((server_object_t)left)->name, (char*)right, MAX_OBJECT_NAME_LENGTH);
}

void server_object_create(server_object_t* obj, char const* name, rbus_callback_t callback, void* data)
{
    (*obj) = malloc(sizeof(struct _server_object));
    strcpy((*obj)->name, name);
    (*obj)->callback = callback;
    (*obj)->data = data;
    (*obj)->process_event_subscriptions = false;
    (*obj)->subscribe_handler_override = NULL;
    (*obj)->subscribe_handler_data = NULL;
    rtVector_Create(&(*obj)->methods);
    rtVector_Create(&(*obj)->subscriptions);
}

void server_object_destroy(void* p)
{
    server_object_t obj = p;
    rtVector_Destroy(obj->methods, rtVector_Cleanup_Free);
    rtVector_Destroy(obj->subscriptions, server_event_destroy);
    free(obj);
}

rbus_error_t server_object_subscription_handler(server_object_t obj, const char * event, char const* subscriber, int added)
{
    if((NULL == event) || (NULL == subscriber) ||
       (MAX_SUBSCRIBER_NAME_LENGTH <= strlen(subscriber)) || 
       (MAX_EVENT_NAME_LENGTH <= strlen(event)))
    {
        rtLog_Error("Cannot %s subscriber %s to event %s. Length exceeds limits.", added ? "add":"remove", subscriber, event);
        return RTMESSAGE_BUS_ERROR_INVALID_PARAM;
    }
    
    if(obj->subscribe_handler_override)
    {
        return (rbus_error_t)obj->subscribe_handler_override(obj->name, event, subscriber, added, obj->subscribe_handler_data);
    }

    server_event_t server_event = rtVector_Find(obj->subscriptions, event, server_event_compare);

    if(server_event)
    {
        if(added)
        {
            rtVector_PushBack(server_event->listeners, strdup(subscriber));
        }
        else
        {
            rtVector_RemoveItemByCompare(server_event->listeners, subscriber, rtVector_Compare_String, rtVector_Cleanup_Free);
        }
        return RTMESSAGE_BUS_SUCCESS;
    }
    else
    {
        rtLog_Error("Object %s doesn't support event %s. Cannot %s listener.", obj->name, event, added ? "add":"remove");
        return RTMESSAGE_BUS_ERROR_UNSUPPORTED_EVENT;
    }
}

typedef struct _queued_request
{
    rtMessageHeader hdr;
    rtMessage msg;
    server_object_t obj;
} *queued_request_t;

void queued_request_create(queued_request_t* req, rtMessageHeader hdr, rtMessage msg, server_object_t obj)
{
    (*req) = malloc(sizeof(struct _queued_request));
    (*req)->hdr = hdr;
    (*req)->msg = msg;
    (*req)->obj = obj;
}

/* End rbus_server */

/* Begin rbus_client */
typedef struct _client_event
{
    char name[MAX_EVENT_NAME_LENGTH+1];
    rbus_event_callback_t callback;
    void* data;
} *client_event_t;

typedef struct _client_subscription
{
    char object[MAX_OBJECT_NAME_LENGTH+1];
    rtVector events; /*list of client_event_t*/
} *client_subscription_t;

void client_event_create(client_event_t* event, const char* name, rbus_event_callback_t callback, void* data)
{
    (*event) = malloc(sizeof(struct _client_event));
    (*event)->callback = callback;
    (*event)->data = data;
    strcpy((*event)->name, name);
}

int client_event_compare(const void* left, const void* right)
{
    return strncmp(((const client_event_t)left)->name, (char const*)right, MAX_EVENT_NAME_LENGTH);
}

int client_subscription_compare(const void* left, const void* right)
{
    return strncmp(((const client_subscription_t)left)->object, (char const*)right, MAX_OBJECT_NAME_LENGTH);
}

void client_subscription_create(client_subscription_t* sub, const char * object_name)
{
    (*sub) = malloc(sizeof(struct _client_subscription));
    strcpy((*sub)->object, object_name);
    rtVector_Create(&(*sub)->events); 
}

void client_subscription_destroy(void* p)
{
    client_subscription_t sub = p;
    rtVector_Destroy(sub->events, rtVector_Cleanup_Free);
    free(sub);
}

/* End rbus_client */

/* End type definitions.*/

/* Begin global variables*/
#define MAX_DAEMON_ADDRESS_LEN 256
static char g_daemon_address[MAX_DAEMON_ADDRESS_LEN] = "unix:///tmp/rtrouted";
static rtConnection g_connection = NULL;
static rtVector g_server_objects; /*server_object_t list*/
static pthread_mutex_t g_mutex;
static bool g_run_event_client_dispatch = false;
static rtVector g_event_subscriptions_for_client; /*client_subscription_t list. Used by the subscriber to track all active subscriptions. */
rtVector g_queued_requests; /*list of queued_request */
/* End global variables*/

static int lock()
{
	return pthread_mutex_lock(&g_mutex);
}

static int unlock()
{
	return pthread_mutex_unlock(&g_mutex);
}

static rbus_error_t send_subscription_request(const char * object_name, const char * event_name, bool activate);

static void perform_init()
{
	rtLog_Info("Performing init");

	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);

	/* Warning: using an error checking mutex but not checking for errors is a very bad approach... */

	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
	pthread_mutex_init(&g_mutex, &attr);

    rtVector_Create(&g_server_objects);
    rtVector_Create(&g_event_subscriptions_for_client);
}

static void perform_cleanup()
{
    size_t i, sz;

    rtLog_Info("Performing cleanup");

    lock();

    rtVector_Destroy(g_server_objects, server_object_destroy);

    sz = rtVector_Size(g_event_subscriptions_for_client);
    if(sz>0)
    {
        rtLog_Info("Cancelling active event subscriptions.");
        unlock();
        for(i = 0; i < sz; ++i)
        {
            size_t i2, sz2;
            client_subscription_t sub = rtVector_At(g_event_subscriptions_for_client, i);

            sz2 = rtVector_Size(sub->events);
            for(i2 = 0; i2 < sz2; i2++)
            {   
                client_event_t event = rtVector_At(sub->events, i2);
                send_subscription_request(sub->object, event->name, false);
            }
        }
        lock();
    }
    rtVector_Destroy(g_event_subscriptions_for_client, client_subscription_destroy);

    unlock();

    pthread_mutex_destroy(&g_mutex);
}

rbus_error_t set_message_method(rtMessage msg, const char *method)
{
    rtMessage_BeginMetaSectionWrite(msg);
    rtMessage_SetString(msg, MESSAGE_FIELD_METHOD, method);
    rtMessage_EndMetaSectionWrite(msg);
	return RTMESSAGE_BUS_SUCCESS;
}
#if 0 /*mrollins cannot use this after converting g_server_objects to a dynamic rtVector*/
static int dummyOnMessage(const char *, const char *, rtMessage, void *, rtMessage *)
{
	/*do nothing.*/
	return 0;
}
#endif

static server_object_t get_object(const char * object_name)
{
    return rtVector_Find(g_server_objects, object_name, server_object_compare);
}

static rbus_error_t translate_rt_error(rtError err)
{
    if(RT_OK == err)
        return RTMESSAGE_BUS_SUCCESS;
    else
        return RTMESSAGE_BUS_ERROR_GENERAL;
}

static void dispatch_method_call(rtMessage msg, const rtMessageHeader *hdr, server_object_t obj)
{
    rtError err = RT_OK;
    const char* method_name = NULL;
    rtMessage response = NULL;
    bool handler_invoked = false;
    
    rtMessage_BeginMetaSectionRead(msg);
    err = rtMessage_GetString(msg, MESSAGE_FIELD_METHOD, &method_name);
    rtMessage_EndMetaSectionRead(msg);
    
    lock();
    if( rtVector_Size(obj->methods) > 0 && RT_OK == err)
    {
        server_method_t method = rtVector_Find(obj->methods, method_name, server_method_compare);

        if(method)
        {
            unlock();
            method->callback(hdr->topic, method_name, msg, method->data, &response); //FIXME: potential for race.
            handler_invoked = true;
        }
    }
    if(false == handler_invoked)
    {
        unlock();
        obj->callback(hdr->topic, method_name, msg, obj->data, &response); //FIXME: potential for race
    }

    if(rtMessageHeader_IsRequest(hdr))
    {
        /* The origin of this message expects a response.*/
        if(NULL == response)
        {
            /* App declined to issue a response. Make one up ourselves. */
            rtMessage_Create(&response);
            rtMessage_SetInt32(response, MESSAGE_FIELD_RESULT, RTMESSAGE_BUS_ERROR_UNSUPPORTED_METHOD);
        }
        set_message_method(response, METHOD_RESPONSE);
        if((err= rtConnection_SendResponse(g_connection, hdr, response, TIMEOUT_VALUE_FIRE_AND_FORGET)) != RT_OK)
        {
            rtLog_Error("Failed to send response to incoming message. Error code: 0x%x", err);
        }
        rtMessage_Release(response);
    }
}

static void onMessage(rtMessageHeader const* hdr, rtMessage msg, void* closure)
{
    /*using namespace rbus_server;*/
    static int stack_counter = 0;
    stack_counter++;
    server_object_t obj = (server_object_t)closure;

    if(1 != stack_counter)
    {
        //We're in the midst of handling another request. Queue this one for later.
        queued_request_t req;
        queued_request_create(&req, *hdr, msg, obj);
        rtVector_PushBack(g_queued_requests, req);
    }
    else
        dispatch_method_call(msg, hdr, obj);

    if((1 == stack_counter) && rtVector_Size(g_queued_requests) > 0)
    {
        //Consume the request queue now that the earlier request has been fully handled.
        while(rtVector_Size(g_queued_requests) > 0)
        {
            queued_request_t req = rtVector_At(g_queued_requests, 0);
            dispatch_method_call(req->msg, &req->hdr, req->obj);
            rtVector_RemoveItem(g_queued_requests, req, rtVector_Cleanup_Free);
        }
    }
    stack_counter--;
    return;
}

static void configure_router_address()
{
    FILE* fconfig = fopen("/etc/rbus_client.conf", "r");
    if(fconfig)
    {
        size_t len;
        char buff[MAX_DAEMON_ADDRESS_LEN] = {0};

        /*locate the first word(block of printable text)*/
        while(fgets(buff, MAX_DAEMON_ADDRESS_LEN, fconfig))
        {
            len = strlen(buff);
            if(len > 0)
            {
                size_t idx1 = 0;

                /*move past any leading space*/
                while(idx1 < len && isspace(buff[idx1]))
                    idx1++;

                if(idx1 < len)
                {
                    size_t idx2 = idx1+1;

                    /*move to end of word*/
                    while(idx2 < len && !isspace(buff[idx2]))
                        idx2++;

                    if(idx2-idx1 > 0)
                    {
                        buff[idx2] = 0;
                        strcpy(g_daemon_address, &buff[idx1]);
                        break;
                    }
                }
            }
        }
        fclose(fconfig);
    }
    rtLog_Info("Broker address: %s", g_daemon_address);
}

rbus_error_t rbus_openBrokerConnection2(const char * component_name, const char * broker_address)
{
	rbus_error_t ret = RTMESSAGE_BUS_SUCCESS;
	rtError result = RT_OK;
	if(NULL == component_name)
	{
		rtLog_Error("Invalid parameter.");
		return RTMESSAGE_BUS_ERROR_INVALID_PARAM;
	}
	if(NULL != g_connection)
	{
		rtLog_Error("A connection already exists. Cannot open a new one.");
		return RTMESSAGE_BUS_ERROR_INVALID_STATE;
	}
	perform_init();
	result = rtConnection_Create(&g_connection, component_name, broker_address);
	if(RT_OK != result)
	{
		rtLog_Error("Failed to create a connection. Error: 0x%x", result);
		g_connection = NULL;
		return RTMESSAGE_BUS_ERROR_GENERAL;
	}
    /*
	for(i = 0; i < MAX_REGISTERED_OBJECTS; i++)
	{
		g_server_objects[i].callback = dummyOnMessage;
	}*/
	rtLog_Info("Successfully created connection for %s.", component_name );
	return ret;
}

rbus_error_t rbus_openBrokerConnection(const char * component_name)
{
#if 0 //Development aid.
    if(std::is_move_constructible<subscription_t::entry_t>::value)
        printf("entry_t is move constructible.");
    if(std::is_move_assignable<subscription_t::entry_t>::value)
        printf("entry_t is move assignable.");
    if(std::is_nothrow_move_assignable<subscription_t::entry_t>::value)
        printf("entry_t is no throw move assignable.");
    if(std::is_nothrow_move_constructible<subscription_t::entry_t>::value)
        printf("entry_t is no throw move constructible.");
    if(std::is_move_constructible<subscription_t>::value)
        printf("subscription_t is move constructible.");
    if(std::is_move_assignable<subscription_t>::value)
        printf("subscription_t is move assignable.");
    if(std::is_nothrow_move_assignable<subscription_t>::value)
        printf("subscription_t is no throw move assignable.");
    if(std::is_nothrow_move_constructible<subscription_t>::value)
        printf("subscription_t is no throw move constructible.");
#endif
	configure_router_address();
	return rbus_openBrokerConnection2(component_name, g_daemon_address);
}

static rbus_error_t send_subscription_request(const char * object_name, const char * event_name, bool activate)
{
    /* Method definition to add new event subscription: 
     * method name: METHOD_ADD_EVENT_SUBSCRIPTION / METHOD_REMOVE_EVENT_SUBSCRIPTION.
     * argument 1: event_name, mapped to key MESSAGE_FIELD_PAYLOAD 
     * Expected resut:
     * integer, mapped to key MESSAGE_FIELD_RESULT. 0 is success. Anything else is a failure. */
    rbus_error_t ret;

    rtMessage request, response;
    rtMessage_Create(&request);

    rbus_SetString(request, MESSAGE_FIELD_EVENT_NAME, event_name);
    rbus_SetString(request, MESSAGE_FIELD_EVENT_SENDER, rtConnection_GetReturnAddress(g_connection));

    ret = rbus_invokeRemoteMethod(object_name, (activate? METHOD_ADD_EVENT_SUBSCRIPTION : METHOD_REMOVE_EVENT_SUBSCRIPTION),
            request, TIMEOUT_VALUE_FIRE_AND_FORGET, &response);
    if(RTMESSAGE_BUS_SUCCESS == ret)
    {
        rtError extract_ret;
        int result;
        extract_ret = rtMessage_GetInt32(response, MESSAGE_FIELD_RESULT, &result);
        if(RT_OK == extract_ret)
        {
            if(RTMESSAGE_BUS_SUCCESS == result)
            {
                /*Event registration was successful.*/
                rtLog_Info("Subscription for %s::%s is now %s.", object_name, event_name, (activate? "active" : "cancelled"));
                ret = RTMESSAGE_BUS_SUCCESS;
            }
            else
            {
                /*For some reason, event publisher couldnt' handle the request.*/
                //TODO: Expand to troubleshoot causes of a failed subscription.
                rtLog_Error("Error %s subscription for %s::%s. Server returned error %d.", (activate? "adding" : "removing"), object_name, event_name, result);
                ret = RTMESSAGE_BUS_ERROR_GENERAL;
            }
        }
        else
        {
            rtLog_Error("Error adding subscription for %s::%s. Received unexpected response.", object_name, event_name);
            ret = RTMESSAGE_BUS_ERROR_MALFORMED_RESPONSE;
        }
        rtMessage_Release(response);
    }
    else
    {
        rtLog_Error("Error %s subscription for %s::%s. Communication issues.", (activate? "adding" : "removing"), object_name, event_name);
        ret = RTMESSAGE_BUS_ERROR_REMOTE_END_FAILED_TO_RESPOND;
    }

    return ret;
}

rbus_error_t rbus_closeBrokerConnection()
{
    rtError err = RT_OK;
    if(NULL == g_connection)
    {
        rtLog_Info("No connection exist to close.");
        return RTMESSAGE_BUS_ERROR_INVALID_STATE;
    }
    perform_cleanup();
    err = rtConnection_Destroy(g_connection);
    if(RT_OK != err)
    {
        rtLog_Error("Could not destroy connection. Error: 0x%x.", err);
        return RTMESSAGE_BUS_ERROR_GENERAL;
    }
    g_connection = NULL;
    rtLog_Info("Destroyed connection.");
    return RTMESSAGE_BUS_SUCCESS;
}


rbus_error_t rbus_registerObj(const char * object_name, rbus_callback_t handler, void * user_data)
{
    rtError err = RT_OK;
    server_object_t obj = NULL;

    if(NULL == g_connection)
    {
        rtLog_Error("Not connected. Cannot register objects yet.");
        return RTMESSAGE_BUS_ERROR_INVALID_STATE;
    }

    if(NULL == object_name)
    {
        rtLog_Error("Object name is NULL");
        return RTMESSAGE_BUS_ERROR_INVALID_PARAM;
    }

    int object_name_len = strlen(object_name);
    if((MAX_OBJECT_NAME_LENGTH <= object_name_len) || (0 == object_name_len))
    {
        rtLog_Error("object_name name is too long/short.");
        return RTMESSAGE_BUS_ERROR_INVALID_PARAM;
    }

    lock();
    obj = rtVector_Find(g_server_objects, object_name, server_object_compare);
    unlock();
    if(obj)
    {
        rtLog_Error("%s is already registered. Rejecting duplicate registration.", object_name);
        return RTMESSAGE_BUS_ERROR_INVALID_PARAM;
    }

    server_object_create(&obj, object_name, handler, user_data);

    //TODO: callback signature translation. rtMessage uses a significantly wider signature for callbacks. Translate to something simpler.
    err = rtConnection_AddListener(g_connection, object_name, onMessage, obj);

    if(RT_OK == err)
    {
        size_t sz;

        lock();
        rtVector_PushBack(g_server_objects, obj);
        sz = rtVector_Size(g_server_objects);
        unlock();
        rtLog_Info("Registered object %s", object_name);
        if(sz >= MAX_REGISTERED_OBJECTS)
        {
            rtLog_Warn("Number of registered objects is %lu", sz);
        }
        return RTMESSAGE_BUS_SUCCESS;
    }
    else
    {
        rtLog_Error("Failed to register object. Error: 0x%x", err);
        server_object_destroy(obj);
        return RTMESSAGE_BUS_ERROR_GENERAL;
    }
}

rbus_error_t rbus_registerMethod(const char * object_name, const char *method_name, rbus_callback_t handler, void * user_data)
{
    /*using namespace rbus_server;*/
    rbus_error_t ret = RTMESSAGE_BUS_SUCCESS;
    if(MAX_METHOD_NAME_LENGTH <= strlen(method_name))
        return RTMESSAGE_BUS_ERROR_INVALID_PARAM;

    lock();
    //TODO: Check that method name length is within limits and search for duplicates
    server_object_t obj = get_object(object_name);
    if(obj)
    {
        if(MAX_SUPPORTED_METHODS <= rtVector_Size(obj->methods))
        {
            rtLog_Error("Too many methods registered with object %s. Cannot register more.", object_name);
            ret = RTMESSAGE_BUS_ERROR_OUT_OF_RESOURCES;
        }
        else
        {
            server_method_t method = rtVector_Find(obj->methods, method_name, server_method_compare);

            if(method)
            {
               unlock();
               rtLog_Error("Method %s is already registered,Rejecting duplicate registration.", method_name);
               return RTMESSAGE_BUS_ERROR_INVALID_PARAM;
            }
            else
            {
                server_method_create(&method, method_name, handler, user_data);
                rtVector_PushBack(obj->methods, method);
                rtLog_Info("Successfully registered method %s with object %s", method_name, object_name);
            }
        }
    }
    else
    {
        rtLog_Error("Couldn't locate object %s.", object_name);
        ret = RTMESSAGE_BUS_ERROR_INVALID_PARAM;
    }
    unlock();
    return ret;
}


rbus_error_t rbus_unregisterMethod(const char * object_name, const char *method_name)
{
    /*using namespace rbus_server;*/
	rbus_error_t ret = RTMESSAGE_BUS_SUCCESS;
	lock();

    server_object_t obj = get_object(object_name);
    if(obj)
    {
        server_method_t method = rtVector_Find(obj->methods, method_name, server_method_compare);
        if(method)
        {
            rtVector_RemoveItem(obj->methods, method, rtVector_Cleanup_Free);
            rtLog_Info("Successfully unregistered method %s from object %s", method_name, object_name);
        }
        else
        {
            rtLog_Error("Couldn't find a method %s registered with object %s.", method_name, object_name);
            ret = RTMESSAGE_BUS_ERROR_GENERAL;
        }
    }
    else	
    {
        rtLog_Error("Couldn't locate object %s.", object_name);
        ret = RTMESSAGE_BUS_ERROR_INVALID_PARAM;
    }
    unlock();
    return ret;
}

rbus_error_t rbus_addElementEvent(const char * object_name, const char* event)
{
    return rbus_addElement(object_name, event);
}

rbus_error_t rbus_registerMethodTable(const char * object_name, rbus_method_table_entry_t *table, unsigned int num_entries)
{
    rbus_error_t ret= RTMESSAGE_BUS_SUCCESS;
    rtLog_Info("Registering method table for object %s", object_name);
    for(unsigned int i = 0; i < num_entries; i++)
    {
        if((ret = rbus_registerMethod(object_name, table[i].method, table[i].callback, table[i].user_data)) != RTMESSAGE_BUS_SUCCESS)
        {
            rtLog_Error("Failed to register table with object %s. Method: %s. Aborting remaining method registrations.", object_name, table[i].method);
            break;
        }
    }
    return ret;
}

rbus_error_t rbus_unregisterMethodTable(const char * object_name, rbus_method_table_entry_t *table, unsigned int num_entries)
{
    rbus_error_t ret = RTMESSAGE_BUS_SUCCESS;
    rtLog_Info("Unregistering method table for object %s", object_name);
    for(unsigned int i = 0; i < num_entries; i++)
    {
        if((ret = rbus_unregisterMethod(object_name, table[i].method)) != RTMESSAGE_BUS_SUCCESS)
        {
            rtLog_Error("Failed to unregister table with object %s. Method: %s. Aborting remaining method unregistrations.", object_name, table[i].method);
            break;
        }
    }
    return ret;
}

rbus_error_t rbus_unregisterObj(const char * object_name)
{
    /*using namespace rbus_server;*/
    rtError err = RT_OK;
    rbus_error_t ret = RTMESSAGE_BUS_SUCCESS;
    if((NULL == object_name) || ('\0' == object_name[0]) || (MAX_OBJECT_NAME_LENGTH <= strlen(object_name)))
    {
        rtLog_Error("object_name is invalid.");
        return RTMESSAGE_BUS_ERROR_INVALID_PARAM;
    }

    err = rtConnection_RemoveListener(g_connection, object_name);
    if(RT_OK != err)
        return RTMESSAGE_BUS_ERROR_GENERAL;

    lock();
    server_object_t obj = get_object(object_name);
    if(NULL != obj)
    {
        rtVector_RemoveItem(g_server_objects, obj, server_object_destroy);
        rtLog_Info("Unregistered object %s.", object_name);
    }
    else
    {
        rtLog_Error("No matching entry for object %s.", object_name);
        ret = RTMESSAGE_BUS_ERROR_GENERAL;
    }
    unlock();

    return ret;
}

rbus_error_t rbus_addElement(const char * object_name, const char * element)
{
    rtError err = RT_OK;

    if(NULL == g_connection)
    {
        rtLog_Error("Not connected.");
        return RTMESSAGE_BUS_ERROR_INVALID_STATE;
    }

    if((NULL == object_name) || (NULL == element))
    {
        rtLog_Error("Object/element name is NULL");
        return RTMESSAGE_BUS_ERROR_INVALID_PARAM;
    }

    int object_name_len = strlen(object_name);
    int element_name_len = strlen(element);
    if((MAX_OBJECT_NAME_LENGTH <= object_name_len) || (0 == object_name_len) ||
            (MAX_OBJECT_NAME_LENGTH <= element_name_len) || (0 == element_name_len))
    {
        rtLog_Error("object/element name is too long/short.");
        return RTMESSAGE_BUS_ERROR_INVALID_PARAM;
    }

    err = rtConnection_AddAlias(g_connection, object_name, element);
    if(RT_OK != err)
    {
        rtLog_Error("Failed to add element. Error: 0x%x", err);
        return RTMESSAGE_BUS_ERROR_GENERAL;
    }

    rtLog_Debug("Added alias %s for object %s.", element, object_name);
    return RTMESSAGE_BUS_SUCCESS;
}

rbus_error_t rbus_removeElement(const char * object, const char * element)
{
    if((NULL == object) || (NULL == element))
    {
        rtLog_Error("Object/element name is NULL");
        return RTMESSAGE_BUS_ERROR_INVALID_PARAM;
    }

    int object_name_len = strlen(object);
    int element_name_len = strlen(element);
    if((MAX_OBJECT_NAME_LENGTH <= object_name_len) || (0 == object_name_len) ||
            (MAX_OBJECT_NAME_LENGTH <= element_name_len) || (0 == element_name_len))
    {
        rtLog_Error("object/element name is too long/short.");
        return RTMESSAGE_BUS_ERROR_INVALID_PARAM;
    }
    rtError err = rtConnection_RemoveAlias(g_connection, object, element);
    if(RT_OK != err)
        return RTMESSAGE_BUS_ERROR_GENERAL;
    return RTMESSAGE_BUS_SUCCESS;
}

rbus_error_t rbus_pushObj(const char * object_name, rtMessage message, int timeout_millisecs)
{
    rtError err = RT_OK;
    rbus_error_t ret = RTMESSAGE_BUS_SUCCESS;
    rtMessage response = NULL;
    if((ret = rbus_invokeRemoteMethod(object_name, METHOD_SETPARAMETERVALUES, message, timeout_millisecs, &response)) != RTMESSAGE_BUS_SUCCESS)
    {
        rtLog_Error("Failed to send message. Error code: 0x%x", err);
        return ret;
    }
    else
    {
        int result = RTMESSAGE_BUS_SUCCESS;
        if((err = rtMessage_GetInt32(response, MESSAGE_FIELD_RESULT, &result) == RT_OK))
        {
            ret = (rbus_error_t)result;
        }
        else
        {
            rtLog_Error("%s.", stringify(RTMESSAGE_BUS_ERROR_MALFORMED_RESPONSE));
            ret = RTMESSAGE_BUS_ERROR_MALFORMED_RESPONSE;
        }
        rtMessage_Release(response);
    }
    return ret;
}


rbus_error_t rbus_invokeRemoteMethod(const char * object_name, const char *method, rtMessage out, int timeout_millisecs, rtMessage *in)
{
    rtError err = RT_OK;
    rbus_error_t ret = RTMESSAGE_BUS_SUCCESS;
    if(MAX_OBJECT_NAME_LENGTH <= strnlen(object_name, MAX_OBJECT_NAME_LENGTH))
    {
        rtLog_Error("Object name is too long.");
        return RTMESSAGE_BUS_ERROR_INVALID_PARAM;
    }

    *in = NULL;
    if(NULL == out)
        rtMessage_Create(&out);

    set_message_method(out, method);
    err = rtConnection_SendRequest(g_connection, out, object_name, in, timeout_millisecs);
    if(RT_OK != err)
    {
        if(RT_OBJECT_NO_LONGER_AVAILABLE == err)
        {
            rtLog_Error("Cannot reach object %s.", object_name);
            ret = RTMESSAGE_BUS_ERROR_DESTINATION_UNREACHABLE;
        }
        else if(RT_ERROR_TIMEOUT == err)
        {
            rtLog_Error("Request timed out. Error code: 0x%x", err);
            ret = RTMESSAGE_BUS_ERROR_REMOTE_TIMED_OUT;
        }
        else
        {
            rtLog_Error("Failed to send message. Error code: 0x%x", err);
            ret = RTMESSAGE_BUS_ERROR_GENERAL;
        }
    }
    else
    {

        method = NULL;
        rtMessage_BeginMetaSectionRead(*in);
		rtMessage_GetString(*in, MESSAGE_FIELD_METHOD, &method);
        rtMessage_EndMetaSectionRead(*in);
        if(NULL != method)
        {
            if(0 != strncmp(METHOD_RESPONSE, method, MAX_METHOD_NAME_LENGTH))
            {
                rtLog_Error("%s.", stringify(RTMESSAGE_BUS_ERROR_MALFORMED_RESPONSE));
                ret = RTMESSAGE_BUS_ERROR_MALFORMED_RESPONSE;
            }
        }
        else
        {
            rtLog_Error("%s.", stringify(RTMESSAGE_BUS_ERROR_MALFORMED_RESPONSE));
            ret = RTMESSAGE_BUS_ERROR_MALFORMED_RESPONSE;
        }
    }

    rtMessage_Release(out);
    if((RTMESSAGE_BUS_SUCCESS != ret) && (NULL != *in))
    {
        rtMessage_Release(*in);
        *in = NULL;
    }
    return ret;
}


/*TODO: make this really fire and forget.*/
rbus_error_t rbus_pushObjNoAck(const char * object_name, rtMessage message)
{
	return rbus_pushObj(object_name, message, TIMEOUT_VALUE_FIRE_AND_FORGET);
}

rbus_error_t rbus_pullObj(const char * object_name, int timeout_millisecs, rtMessage *response)
{
    rbus_error_t ret = RTMESSAGE_BUS_SUCCESS;
    rtError err = RT_OK;
    if((ret = rbus_invokeRemoteMethod(object_name, METHOD_GETPARAMETERVALUES, NULL, timeout_millisecs, response)) != RTMESSAGE_BUS_SUCCESS)
    {
        rtLog_Error("Failed to send message. Error code: 0x%x", ret);
    }
    else
    {
        int result = RTMESSAGE_BUS_SUCCESS;
        if((err = rtMessage_GetInt32(*response, MESSAGE_FIELD_RESULT, &result) == RT_OK))
        {
            ret = (rbus_error_t)result;
        }
        else
        {
            rtLog_Error("%s.", stringify(RTMESSAGE_BUS_ERROR_MALFORMED_RESPONSE));
            ret = RTMESSAGE_BUS_ERROR_MALFORMED_RESPONSE;
        }
        if(RTMESSAGE_BUS_SUCCESS != ret) 
        {
            rtMessage_Release(*response);
            *response = NULL;
        }
    }
    return ret;
}

rbus_error_t rbus_sendMessage(rtMessage message, const char * destination, const char * sender)
{
    rtError ret = rtConnection_SendMessageWithSenderInfo(g_connection, message, destination, sender);
    return translate_rt_error(ret);
}

void ack();

static int subscription_handler(const char *not_used, const char * method_name, rtMessage in, void * user_data, rtMessage *out)
{
    /*using namespace rbus_server;*/
    const char * sender = NULL;
    const char * event_name = NULL;
    server_object_t obj = (server_object_t)user_data;
    (void)not_used;

    if(RT_OK != rtMessage_Create(out))
    {
        rtLog_Error("Unable to create response message.");
        return -1;
    }

    if((RT_OK == rtMessage_GetString(in, MESSAGE_FIELD_EVENT_NAME, &event_name)) &&
        (RT_OK == rtMessage_GetString(in, MESSAGE_FIELD_EVENT_SENDER, &sender))) 
    {
        /*Extract arguments*/
        if((NULL == sender) || (NULL == event_name))
        {
            rtLog_Error("Malformed subscription request. Sender: %s. Event: %s.", sender, event_name);
            rtMessage_SetInt32(*out, MESSAGE_FIELD_RESULT, RTMESSAGE_BUS_ERROR_INVALID_PARAM);
        }
        else
        {
            int added = strncmp(method_name, METHOD_ADD_EVENT_SUBSCRIPTION, MAX_METHOD_NAME_LENGTH) == 0 ? 1 : 0;
            rbus_error_t ret = server_object_subscription_handler(obj, event_name, sender, added);
            rtMessage_SetInt32(*out, MESSAGE_FIELD_RESULT, ret);
        }
    }
    
    return 0;
}


static rbus_error_t install_subscription_handlers(server_object_t object)
{
    rbus_error_t ret = RTMESSAGE_BUS_SUCCESS; 

    server_method_t method = rtVector_Find(object->methods, METHOD_ADD_EVENT_SUBSCRIPTION, server_method_compare);

    if(method)
    {
        rtLog_Info("Object already accepts subscription requests.");
        return ret;
    }

    /*No subscription handlers present. Add them.*/
    rtLog_Info("Adding handler for subscription requests for %s.", object->name);
    if((ret = rbus_registerMethod(object->name, METHOD_ADD_EVENT_SUBSCRIPTION, subscription_handler, object)) != RTMESSAGE_BUS_SUCCESS)
    {
        rtLog_Error("Could not register add_subscription_handler.");
    }
    else
    {
        if((ret = rbus_registerMethod(object->name, METHOD_REMOVE_EVENT_SUBSCRIPTION, subscription_handler, object)) != RTMESSAGE_BUS_SUCCESS)
        {
            rtLog_Error("Could not register remove_subscription_handler.");
        }
        else
        {
            rtLog_Info("Successfully registered subscription handlers for %s.", object->name);
            object->process_event_subscriptions = true;
        }
    }
    return ret;
}

rbus_error_t rbus_registerEvent(const char* object_name, const char * event_name, rbus_event_subscribe_callback_t callback, void * user_data)
{
    /*using namespace rbus_server;*/
    rbus_error_t ret = RTMESSAGE_BUS_SUCCESS;
    server_object_t obj;

    if(NULL == event_name)
        event_name = DEFAULT_EVENT;
    if(NULL == object_name)
    {
        rtLog_Error("Invalid parameter(s)");
        return RTMESSAGE_BUS_ERROR_INVALID_PARAM;
    }
    if(MAX_EVENT_NAME_LENGTH <= strnlen(event_name, MAX_EVENT_NAME_LENGTH))
    {
        rtLog_Error("Event name is too long.");
        return RTMESSAGE_BUS_ERROR_INVALID_PARAM;
    }

    lock();
    obj = get_object(object_name);
    if(obj)
    {
        server_event_t evt = rtVector_Find(obj->subscriptions, event_name, server_event_compare);

        if(evt)
        {
            rtLog_Info("Event %s already exists in subscription table.", event_name);
        }
        else
        {
            server_event_create(&evt, event_name, obj, callback, user_data);
            rtVector_PushBack(obj->subscriptions, evt);
            rtLog_Info("Registered event %s::%s.", object_name, event_name);
        }
        if(!obj->process_event_subscriptions)
            ret = install_subscription_handlers(obj);
    }
    else
    {
        rtLog_Error("Could not find object %s", object_name);
        ret = RTMESSAGE_BUS_ERROR_INVALID_PARAM;
    }
    unlock();
    return ret;
}

rbus_error_t rbus_unregisterEvent(const char* object_name, const char * event_name)
{
    /*using namespace rbus_server;*/
    rbus_error_t ret = RTMESSAGE_BUS_SUCCESS;
    if(NULL == event_name)
        event_name = DEFAULT_EVENT;

    lock();

    server_object_t obj = get_object(object_name);
    if(obj)
    {
        server_event_t evt = rtVector_Find(obj->subscriptions, event_name, server_event_compare);

        if(evt)
        {
            rtVector_RemoveItem(obj->subscriptions, evt, server_event_destroy);
            rtLog_Info("Event %s::%s has been unregistered.", object_name, event_name);
            /* If we've removed all events and RPC registrations, delete the object itself.*/
        }
        else
        {
            rtLog_Info("Event %s could not be found in subscription table of object %s.", event_name, object_name);
            ret = RTMESSAGE_BUS_ERROR_INVALID_PARAM;
        }
    }
    else
    {
    
        rtLog_Error("Could not find object %s", object_name);
        ret = RTMESSAGE_BUS_ERROR_INVALID_PARAM;
    }
    unlock();
    return ret;
}
static void master_event_callback(const rtMessageHeader* hdr, rtMessage msg, void* closure)
{
    /*using namespace rbus_client;*/
    const char * sender = hdr->reply_topic;
    const char * event_name = NULL;
    rtError err;
    size_t subs_len;
    size_t i;
    (void)closure;
    
    /*Sanitize the incoming data.*/
    if(MAX_OBJECT_NAME_LENGTH <= strlen(sender))
    {
        rtLog_Error("Object name length exceeds limits.");
        return;
    }
    rtMessage_BeginMetaSectionRead(msg);
    err = rtMessage_GetString(msg, MESSAGE_FIELD_EVENT_NAME, &event_name);
    rtMessage_EndMetaSectionRead(msg);
    if(RT_OK != err)
    {
        rtLog_Error("Event message doesn't contain an event name.");
        return;
    }
    
    lock();
    subs_len = rtVector_Size(g_event_subscriptions_for_client);
    for(i = 0; i < subs_len; ++i)
    {
        client_subscription_t sub = rtVector_At(g_event_subscriptions_for_client, i);

        if( strncmp(sub->object, sender, MAX_OBJECT_NAME_LENGTH) == 0 ||
            strncmp(sub->object, event_name, MAX_OBJECT_NAME_LENGTH) == 0 ) /* support rbus events being elements : the object name will be the event name */
        {
            client_event_t evt = rtVector_Find(sub->events, event_name, client_event_compare);

            if(evt)
            {
                unlock();
                evt->callback(sender, event_name, msg, evt->data);
                return;
            }
            /* support rbus events being elements : keep searching */
        }
    }
    /* If no matching objects exist in records. Create a new entry.*/
    unlock();
    rtLog_Warn("Received event %s::%s for which no subscription exists.", sender, event_name);
    return;
}


static rbus_error_t remove_subscription_callback(const char * object_name,  const char * event_name)
{
    /*using namespace rbus_client;*/
    client_subscription_t sub;
    rbus_error_t ret = RTMESSAGE_BUS_ERROR_INVALID_PARAM;

    lock();
    sub = rtVector_Find(g_event_subscriptions_for_client, object_name, client_subscription_compare);
    if(sub)
    {
        client_event_t evt = rtVector_Find(sub->events, event_name, client_event_compare);
        if(evt)
        {
            rtVector_RemoveItem(sub->events, evt, rtVector_Cleanup_Free);
            rtLog_Info("Subscription removed for event %s::%s.", object_name, event_name);
            ret = RTMESSAGE_BUS_SUCCESS;

            if(rtVector_Size(sub->events) == 0)
            {
                rtLog_Info("Zero event subscriptions remaining for object %s. Cleaning up.", object_name);
                rtVector_RemoveItem(g_event_subscriptions_for_client, sub, client_subscription_destroy);
            }
        }
        else
        {
            rtLog_Warn("Subscription for event %s::%s not found.", object_name, event_name);
        }
    }
    unlock();
    return ret;
}

rbus_error_t rbus_subscribeToEvent(const char * object_name,  const char * event_name, rbus_event_callback_t callback, void * user_data)
{
    /*using namespace rbus_client;*/
    rbus_error_t ret = RTMESSAGE_BUS_SUCCESS;
    client_subscription_t sub;
    client_event_t evt;

    /* support rbus events being elements : use the event_name as the object_name because event_name is alias to object */
    if(object_name == NULL && event_name != NULL) 
        object_name = event_name;

    if((NULL == object_name) || (NULL == callback))
    {
        rtLog_Error("Invalid parameter(s)");
        return RTMESSAGE_BUS_ERROR_INVALID_PARAM;
    }
    if(MAX_OBJECT_NAME_LENGTH <= strnlen(object_name, MAX_OBJECT_NAME_LENGTH))
    {
        rtLog_Error("Object name is too long.");
        return RTMESSAGE_BUS_ERROR_INVALID_PARAM;
    }
    if(MAX_EVENT_NAME_LENGTH <= strnlen(event_name, MAX_EVENT_NAME_LENGTH))
    {
        rtLog_Error("Event name is too long.");
        return RTMESSAGE_BUS_ERROR_INVALID_PARAM;
    }

    lock();
   
    if(NULL == event_name)
        event_name = DEFAULT_EVENT;

    if(false == g_run_event_client_dispatch)
    {
        rtLog_Debug("Starting event dispatching.");
        rtConnection_AddDefaultListener(g_connection, &master_event_callback, NULL);
        g_run_event_client_dispatch = true;
    }

    sub = rtVector_Find(g_event_subscriptions_for_client, object_name, client_subscription_compare);
    if(sub)
    {
        if(rtVector_Find(sub->events, event_name, client_event_compare))
        {
            /*sub already exist and event already registered so do nothing*/
            rtLog_Warn("Subscription exists for event %s::%s.", object_name, event_name);
            unlock();
            return RTMESSAGE_BUS_SUCCESS;
        }
    }
    else
    {
        /*sub didn't exist so create it*/
        client_subscription_create(&sub, object_name);
        rtVector_PushBack(g_event_subscriptions_for_client, sub);
    }

    /*create event and add to sub*/
    client_event_create(&evt, event_name, callback, user_data);
    rtVector_PushBack(sub->events, evt);
    rtLog_Info("Added subscription for event %s::%s.", object_name, event_name);

    unlock();

    if((ret = send_subscription_request(object_name, event_name, true)) != RTMESSAGE_BUS_SUCCESS)
    {
        /*Something went wrong in the RPC. Undo what we did so far and report error.*/
        lock();
        remove_subscription_callback(object_name, event_name);
        unlock();
    }
    return ret;
}


rbus_error_t rbus_unsubscribeFromEvent(const char * object_name,  const char * event_name)
{
    rbus_error_t ret = RTMESSAGE_BUS_ERROR_INVALID_PARAM;

    /* support rbus events being elements */
    if(object_name == NULL && event_name != NULL) 
        object_name = event_name;

    if(NULL == object_name)
    {
        rtLog_Error("Invalid parameter(s)");
        return ret;
    }
    if(MAX_OBJECT_NAME_LENGTH <= strnlen(object_name, MAX_OBJECT_NAME_LENGTH))
    {
        rtLog_Error("Object name is too long.");
        return RTMESSAGE_BUS_ERROR_INVALID_PARAM;
    }
    if(NULL == event_name)
        event_name = DEFAULT_EVENT;

    remove_subscription_callback(object_name, event_name);
    ret = send_subscription_request(object_name, event_name, false);
    return ret;
}

rbus_error_t rbus_publishEvent(const char* object_name,  const char * event_name, rtMessage out)
{
    /*using namespace rbus_server;*/
    rbus_error_t ret = RTMESSAGE_BUS_SUCCESS;
    if(NULL == event_name)
        event_name = DEFAULT_EVENT;
    if(MAX_OBJECT_NAME_LENGTH <= strnlen(object_name, MAX_OBJECT_NAME_LENGTH))
    {
        rtLog_Error("Object name is too long.");
        return RTMESSAGE_BUS_ERROR_INVALID_PARAM;
    }
    rtMessage_BeginMetaSectionWrite(out);
    rbus_SetString(out, MESSAGE_FIELD_EVENT_NAME, event_name);
    rbus_SetString(out, MESSAGE_FIELD_EVENT_SENDER, object_name); 
    rtMessage_EndMetaSectionWrite(out);

    lock();
    server_object_t obj = get_object(object_name);
    if(obj)
    {
        server_event_t evt = rtVector_Find(obj->subscriptions, event_name, server_event_compare);

        if(evt)
        {
            size_t nlistener, i;

            nlistener = rtVector_Size(evt->listeners);
            rtLog_Debug("Event %s exists in subscription table. Dispatching to %lu subscribers.", event_name, nlistener);
            for(i=0; i < nlistener; ++i)
            {
                char const* listener = (char const*)rtVector_At(evt->listeners, i);
                if(RTMESSAGE_BUS_SUCCESS != rbus_sendMessage(out, listener, object_name))
                {
                    rtLog_Error("Couldn't send event %s::%s to %s.", object_name, event_name, listener);
                }
            }
        }
        else
        {
            rtLog_Error("Could not find event %s", event_name);
            ret = RTMESSAGE_BUS_ERROR_INVALID_PARAM;
        }
    }
    else 
    {
        /*Object not present yet. Register it now.*/
        rtLog_Error("Could not find object %s", object_name);
        ret = RTMESSAGE_BUS_ERROR_INVALID_PARAM;
    }
    unlock();

    return ret;
}

rbus_error_t rbus_registerSubscribeHandler(const char* object_name, rbus_event_subscribe_callback_t callback, void * user_data)
{
    /*using namespace rbus_server;*/
    rbus_error_t ret = RTMESSAGE_BUS_SUCCESS;

    if((NULL == object_name) || (NULL == callback))
    {
        rtLog_Error("Invalid parameter(s)");
        return RTMESSAGE_BUS_ERROR_INVALID_PARAM;
    }

    lock();
    server_object_t obj = get_object(object_name);
    if(obj)
    {
        obj->subscribe_handler_override = callback;
        obj->subscribe_handler_data = user_data;
        if(!obj->process_event_subscriptions)
            ret = install_subscription_handlers(obj);
    }
    else
    {
        rtLog_Error("Could not find object %s", object_name);
        ret = RTMESSAGE_BUS_ERROR_INVALID_PARAM;
    }
    unlock();
    return ret;
}

rbus_error_t rbus_publishSubscriberEvent(const char* object_name,  const char * event_name, const char* listener, rtMessage out)
{
    /*using namespace rbus_server;*/
    rbus_error_t ret = RTMESSAGE_BUS_SUCCESS;
    if(NULL == event_name)
        event_name = DEFAULT_EVENT;
    if(MAX_OBJECT_NAME_LENGTH <= strnlen(object_name, MAX_OBJECT_NAME_LENGTH))
    {
        rtLog_Error("Object name is too long.");
        return RTMESSAGE_BUS_ERROR_INVALID_PARAM;
    }
    rtMessage_BeginMetaSectionWrite(out);
    rbus_SetString(out, MESSAGE_FIELD_EVENT_NAME, event_name);
    rbus_SetString(out, MESSAGE_FIELD_EVENT_SENDER, object_name); 
    rtMessage_EndMetaSectionWrite(out);
    lock();
    server_object_t obj = get_object(object_name);
    if(NULL == obj)
    {
        /*Object not present yet. Register it now.*/
        rtLog_Error("Could not find object %s", object_name);
        ret = RTMESSAGE_BUS_ERROR_INVALID_PARAM;
    }
    if(rbus_sendMessage(out, listener, object_name) != RTMESSAGE_BUS_SUCCESS)
    {
       rtLog_Error("Couldn't send event %s::%s to %s.", object_name, event_name, listener);
    }
    unlock();
    return ret;
}

rbus_error_t rbus_registeredComponents(rtMessage *in)
{
    rbus_error_t ret = RTMESSAGE_BUS_SUCCESS;
    rtError err = RT_OK;
    rtMessage out;
    rtMessage_Create(&out);
    rtMessage_SetInt32(out, "dummy", 0); //Necessary because zero length payloads trip up rtMessage.
    err = rtConnection_SendRequest(g_connection, out, REGISTERED_COMPONENTS, in, TIMEOUT_VALUE_FIRE_AND_FORGET);
    if(RT_OK == err)
        ret = RTMESSAGE_BUS_SUCCESS;
    else
    {
        rtLog_Error("Failed with error code %d", err);
        ret = RTMESSAGE_BUS_ERROR_GENERAL;
    }
    rtMessage_Release(out);
    return ret;
}

rbus_error_t rbus_GetElementsAddedByObject(const char * expression, rtMessage *in)
{
    rtError err = RT_OK;
    rbus_error_t ret = RTMESSAGE_BUS_SUCCESS;

    rtMessage out;
    rtMessage_Create(&out);
    rtMessage_SetString(out, ELEMENT_ENUMERATION_OBJECT, expression);

    err = rtConnection_SendRequest(g_connection, out, ELEMENT_ENUMERATION, in, TIMEOUT_VALUE_FIRE_AND_FORGET);
    if(RT_OK == err)
        ret = RTMESSAGE_BUS_SUCCESS;
    else
        ret = RTMESSAGE_BUS_ERROR_GENERAL;

    rtMessage_Release(out);

    return ret;
}

rbus_error_t rbus_resolveWildcardDestination(const char * expression, int * num_entries, rtMessage *in)
{
    rbus_error_t ret = RTMESSAGE_BUS_SUCCESS;
    rtError err = RT_OK;
    rtMessage out;
    rtMessage_Create(&out);
    rtMessage_SetString(out, RTM_DISCOVERY_EXPRESSION, expression);
    err = rtConnection_SendRequest(g_connection, out, RTM_DISCOVERY_DESTINATION, in, TIMEOUT_VALUE_FIRE_AND_FORGET);
    if(RT_OK == err)
    {
        int result;
        if((RT_OK == rbus_GetInt32(*in, RTM_DISCOVERY_RESULT, &result)) && (RTM_DISCOVERY_RESULT_SUCCESS == result))
        {
            rtMessage_GetInt32(*in, RTM_DISCOVERY_NUM_ENTRIES, num_entries);
            ret = RTMESSAGE_BUS_SUCCESS;
        }
        else
        {
            ret = RTMESSAGE_BUS_ERROR_GENERAL;
            rtMessage_Release(*in);
        }
    }
    else
        ret = RTMESSAGE_BUS_ERROR_MALFORMED_RESPONSE;
    rtMessage_Release(out);
    return ret;
}

rbus_error_t rbus_findMatchingObjects(const char* elements[], const int len, char *** objects)
{
    rbus_error_t ret = RTMESSAGE_BUS_SUCCESS;
    rtError err = RT_OK;
    rtMessage out, in;

    if(1 > len)
    {
        rtLog_Error("List is empty.");
        return RTMESSAGE_BUS_ERROR_INVALID_PARAM;
    }

    rtMessage_Create(&out);
    rbus_AppendInt32(out, len);
    for(int i = 0; i < len; i++)
    {
        if(NULL != elements[i])
            rbus_AppendString(out, elements[i]);
        else
        {
            rtLog_Error("Null entries in element list.");
            rtMessage_Release(out);
            return RTMESSAGE_BUS_ERROR_INVALID_PARAM;
        }    
    }
    err = rtConnection_SendRequest(g_connection, out, TRACE_ORIGIN_OBJECT, &in, TIMEOUT_VALUE_FIRE_AND_FORGET);

    if(RT_OK == err)
    {
        int result;
        const char * value = NULL;
        if((RT_OK == rbus_PopInt32(in, &result)) && (RTM_DISCOVERY_RESULT_SUCCESS == result))
        {
            char **array_ptr = (char **)malloc(len * sizeof(char *));
            if (NULL != array_ptr)
            {
                *objects = array_ptr;
                memset(array_ptr, 0, (len * sizeof(char *)));
                for (int i = 0; i < len; i++)
                {
                    if ((RT_OK != rbus_PopString(in, &value)) || (NULL == (array_ptr[i] = strndup(value, MAX_OBJECT_NAME_LENGTH))))
                    {
                        for (int j = 0; j < i; j++)
                            free(array_ptr[j]);
                        free(array_ptr);
                        rtLog_Error("Read/Memory allocation failure");
                        ret = RTMESSAGE_BUS_ERROR_GENERAL;
                        break;
                    }
                }
            }
            else
            {
                rtLog_Error("Memory allocation failure");
                ret = RTMESSAGE_BUS_ERROR_INSUFFICIENT_MEMORY;
            }
        }
        else
            ret = RTMESSAGE_BUS_ERROR_GENERAL;
        rtMessage_Release(in);
    }
    else
        ret = RTMESSAGE_BUS_ERROR_MALFORMED_RESPONSE;
    rtMessage_Release(out);
    return ret;    
}
rbus_error_t subscribeOnevent(const char * path, rbus_callback_t callback);
rbus_error_t unsubscribeOnevent(const char * path);

rbuscore_bus_status_t rbuscore_checkBusStatus(void)
{
    if(access("/nvram/rbus_support", F_OK) == 0)
    {
        rtLog_Info ("Currently RBus Enabled");
        return RBUSCORE_ENABLED;
    }
    else if(access("/nvram/rbus_support_on_pending", F_OK) == 0)
    {
        rtLog_Info ("Currently RBus is Disabled; Next boot - RBus Enabled");
        return RBUSCORE_ENABLED_PENDING;
    }
    else if(access("/nvram/rbus_support_off_pending", F_OK) == 0)
    {
        rtLog_Info ("Currently RBus is Enabled; Next boot - RBus Disabled");
        return RBUSCORE_DISABLED_PENDING;
    }
    else
    {
        rtLog_Info ("Currently RBus Disabled");
        return RBUSCORE_DISABLED;
    }
}
