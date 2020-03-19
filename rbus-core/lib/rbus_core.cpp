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
#include <string>
#include <vector>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>
#include <algorithm>
#include <fstream>
#include <string>
#include <list>
//#include <type_traits>

#include "rbus_core.h"
#include "rbus_marshalling.h"
#include "rtLog.h"

/* Begin constant definitions.*/
static const unsigned int TIMEOUT_VALUE_FIRE_AND_FORGET = 1000;
static const unsigned int MAX_SUBSCRIBER_NAME_LENGTH = MAX_OBJECT_NAME_LENGTH;
static const char * DEFAULT_EVENT = "";
static std::string g_daemon_address;
#define METHOD_ADD_EVENT_SUBSCRIPTION "_subscribe"
#define METHOD_REMOVE_EVENT_SUBSCRIPTION "_unsubscribe"
/* End constant definitions.*/

/* Begin type definitions.*/
namespace rbus_server
{
    struct method_table_entry_t
    {
        std::string method;
        rbus_callback_t callback;
        void * data;
    };

    struct rbus_object;

    struct event_entry_t 
    {
        std::string event_name;
        rbus_object* object;
        std::vector <std::string> listeners;
        rbus_event_subscribe_callback_t sub_callback;
        void * sub_data;

        event_entry_t(const char * event, rbus_object* obj, rbus_event_subscribe_callback_t cb, void * data);
        void addListener(const char * listener);
        void removeListener(const char * listener);
    };

    struct rbus_object
    {
        char name[MAX_OBJECT_NAME_LENGTH];
        void * data;
        rbus_callback_t callback;
        int num_registered_methods;
        bool process_event_subscriptions;
        method_table_entry_t method_callbacks[MAX_SUPPORTED_METHODS];
        std::vector <event_entry_t> subscription_table;

        rbus_error_t addSubscriber(const char * event, const char * subscriber)
        {
            if((NULL == event) || (NULL == subscriber) ||
                    (MAX_SUBSCRIBER_NAME_LENGTH <= strlen(subscriber)) || (MAX_EVENT_NAME_LENGTH <= strlen(event)))
            {
                rtLog_Error("Cannot add subscriber %s to event %s. Length exceeds limits.", subscriber, event);//TODO: bad idea tp try to print a bad input. Clean up.
                return RTMESSAGE_BUS_ERROR_INVALID_PARAM;
            }
            
            const auto iter = std::find_if(subscription_table.begin(), subscription_table.end(), 
                    [&event](const event_entry_t &entry) -> bool { return (entry.event_name == event); });

            if(subscription_table.end() != iter)
            {
                iter->addListener(subscriber);
                return RTMESSAGE_BUS_SUCCESS;
            }
            else
            {
                rtLog_Error("Object %s doesn't support event %s. Cannot register listener.", name, event);
                return RTMESSAGE_BUS_ERROR_UNSUPPORTED_EVENT;
            }
        }

        rbus_error_t removeSubscriber(const char * event, const char * subscriber)
        {
            if((NULL == event) || (NULL == subscriber) ||
                    (MAX_SUBSCRIBER_NAME_LENGTH <= strlen(subscriber)) || (MAX_EVENT_NAME_LENGTH <= strlen(event)))
            {
                rtLog_Error("Cannot remove subscriber %s to event %s. Length exceeds limits.", subscriber, event); //TODO: bad idea tp try to print a bad input. Clean up.
                return RTMESSAGE_BUS_ERROR_INVALID_PARAM;
            }

            const auto iter = std::find_if(subscription_table.begin(), subscription_table.end(), 
                    [&event](const event_entry_t &entry) -> bool { return (entry.event_name == event); });

            if(subscription_table.end() != iter)
            {
                iter->removeListener(subscriber);
                return RTMESSAGE_BUS_SUCCESS;
            }
            else
            {
                rtLog_Error("Object %s doesn't support event %s.", name, event);
                return RTMESSAGE_BUS_ERROR_INVALID_PARAM;
            }
        }
    };
    
    event_entry_t::event_entry_t(const char * event, rbus_object* obj, rbus_event_subscribe_callback_t cb, void * data) : 
        event_name(event), object(obj), sub_callback(cb), sub_data(data) {}

    void event_entry_t::addListener(const char * listener)
    {
        if(NULL == listener)
        {
            rtLog_Error("Listener is empty.");
            return;
        }
        /*Avoid doing this if a duplicate entry exists.*/
        const auto iter = std::find_if(listeners.begin(), listeners.end(), 
                [&listener](const std::string &existing) -> bool { return (existing == listener); });

        if(listeners.end() != iter)
        {
            rtLog_Warn("Listener %s is already registered for event %s.", listener, event_name.c_str());
            return;
        }
        listeners.emplace_back(listener);

        if(sub_callback)
        {
            sub_callback(object->name, event_name.c_str(), listener, 1, sub_data);
        }

        rtLog_Info("Listener %s added for event %s.", listener, event_name.c_str());
    }

    void event_entry_t::removeListener(const char * listener)
    {
        if(NULL == listener)
        {
            rtLog_Error("Listener is empty.");
            return;
        }
        const auto iter = std::find_if(listeners.begin(), listeners.end(), 
                [&listener](const std::string &existing) -> bool { return (existing == listener); });

        if(listeners.end() != iter)
        {
            rtLog_Warn("Removing listener %s for event %s.", listener, event_name.c_str());
            listeners.erase(iter);

            if(sub_callback)
            {
                sub_callback(object->name, event_name.c_str(), listener, 0, sub_data);
            }
        }
        else
        {
            rtLog_Error("Listener %s not found for event %s.", listener, event_name.c_str());
        }
        return;
    }

    struct queued_request
    {
        rtMessageHeader hdr;
        rtMessage msg;
        rbus_server::rbus_object * obj;
    };
}

namespace rbus_client
{
    struct subscription_t
    {
        struct event_entry_t
        {
            std::string event_name;
            rbus_event_callback_t callback;
            void * user_data;
            event_entry_t(const char * event, rbus_event_callback_t cb, void * data) : event_name(event), callback(cb), user_data(data) {}
        };

        std::string object;
        std::vector <event_entry_t> event_list;

        subscription_t(const char * object_name, const char * event_name, rbus_event_callback_t cb, void * user_data) : object(object_name)
        {
            rtLog_Info("%s: Adding subscription for %s::%s.", __FUNCTION__, object_name, event_name);
            event_list.emplace_back(event_name, cb, user_data);
        }
    };
}


/* End type definitions.*/

/* Begin global variables*/
static const char * default_daemon_address = "unix:///tmp/rtrouted"; 
static rtConnection g_connection = NULL;
static rbus_server::rbus_object g_object_table[MAX_REGISTERED_OBJECTS];
static pthread_mutex_t g_mutex = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;
static bool g_run_event_client_dispatch = false;
static std::vector <rbus_client::subscription_t> g_event_subscriptions_for_client; //Used by the subscriber to track all active subscriptions.
/* End global variables*/


static int lock()
{
	return pthread_mutex_lock(&g_mutex);
}

static int unlock()
{
	return pthread_mutex_unlock(&g_mutex);
}

rbus_error_t set_message_method(rtMessage msg, const char *method)
{
    rtMessage_BeginMetaSectionWrite(msg);
    rtMessage_SetString(msg, MESSAGE_FIELD_METHOD, method);
    rtMessage_EndMetaSectionWrite(msg);
	return RTMESSAGE_BUS_SUCCESS;
}

static int dummyOnMessage(const char *, const char *, rtMessage, void *, rtMessage *)
{
	/*do nothing.*/
	return 0;
}

static rbus_server::rbus_object * get_object(const char * object_name)
{
    for(int i = 0; i < MAX_REGISTERED_OBJECTS; i++)
    {
        if(0 == strncmp(g_object_table[i].name, object_name, MAX_OBJECT_NAME_LENGTH))
            return &g_object_table[i];
    }
    return NULL;
}

static rbus_error_t translate_rt_error(rtError err)
{
    if(RT_OK == err)
        return RTMESSAGE_BUS_SUCCESS;
    else
        return RTMESSAGE_BUS_ERROR_GENERAL;
}

static std::list<rbus_server::queued_request *> request_queue;

static void dispatch_method_call(rtMessage msg, const rtMessageHeader *hdr, rbus_server::rbus_object *ptr)
{
    rtError err = RT_OK;
    const char* method = NULL;
    rtMessage response = NULL;
    bool handler_invoked = false;
    
    rtMessage_BeginMetaSectionRead(msg);
    err = rtMessage_GetString(msg, MESSAGE_FIELD_METHOD, &method);
    rtMessage_EndMetaSectionRead(msg);
    
    lock();
    if((0 != ptr->num_registered_methods) && (RT_OK == err))
    {
        /*If there are registered callbacks for this particular kind of message, dispatch them.*/
        for(int i = 0; i < MAX_SUPPORTED_METHODS; i++)
        {
            if(false == ptr->method_callbacks[i].method.empty())
            {
                if(0 == strncmp(method, ptr->method_callbacks[i].method.c_str(), MAX_METHOD_NAME_LENGTH))
                {
                    unlock();
                    ptr->method_callbacks[i].callback(hdr->topic, method, msg, ptr->method_callbacks[i].data, &response); //FIXME: potential for race.
                    handler_invoked = true;
                    break;
                }
            }
        }
    }
    if(false == handler_invoked)
    {
        unlock();
        ptr->callback(hdr->topic, method, msg, ptr->data, &response); //FIXME: potential for race
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
    using namespace rbus_server;
    static int stack_counter = 0;
    stack_counter++;
    rtError err = RT_OK;
    rbus_object * ptr = (rbus_object *)closure;

    if(1 != stack_counter)
    {
        //We're in the midst of handling another request. Queue this one for later.
        queued_request * req = new queued_request;
        req->hdr = *hdr;
        req->msg = msg;
        req->obj = ptr;
        request_queue.push_back(req);
    }
    else
        dispatch_method_call(msg, hdr, ptr);

    if((1 == stack_counter) && !request_queue.empty())
    {
        //Consume the request queue now that the earlier request has been fully handled.
        while(!request_queue.empty())
        {
            queued_request *ptr = request_queue.front();
            request_queue.pop_front();
            dispatch_method_call(ptr->msg, &ptr->hdr, ptr->obj);
            delete ptr;
        }
    }
    stack_counter--;
    return;
}

static void configure_router_address()
{
    std::ifstream infile;
    infile.open("/etc/rbus_client.conf");
    if(infile.is_open())
        infile >> g_daemon_address;
    if(g_daemon_address.empty()) //TODO: Sanitize address.
        g_daemon_address = default_daemon_address;
    rtLog_Info("Broker address: %s", g_daemon_address.c_str());
}

rbus_error_t rbus_openBrokerConnection2(const char * component_name, const char * broker_address)
{
	rbus_error_t ret = RTMESSAGE_BUS_SUCCESS;
	rtError result = RT_OK;
	int i = 0;
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
	result = rtConnection_Create(&g_connection, component_name, broker_address);
	if(RT_OK != result)
	{
		rtLog_Error("Failed to create a connection. Error: 0x%x", result);
		g_connection = NULL;
		return RTMESSAGE_BUS_ERROR_GENERAL;
	}

	for(i = 0; i < MAX_REGISTERED_OBJECTS; i++)
	{
		g_object_table[i].callback = dummyOnMessage;
	}
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
	return rbus_openBrokerConnection2(component_name, g_daemon_address.c_str());
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

static void perform_clean_up()
{
    lock();
    {
        using namespace rbus_server;
        rtLog_Info("Cleaning up server data if any.");
        for(int i = 0; i < MAX_REGISTERED_OBJECTS; i++)
        {
            rbus_object *obj = &g_object_table[i];
            obj->callback = dummyOnMessage;
            obj->data = NULL;
            obj->name[0] = '\0';
            obj->process_event_subscriptions = false;
            obj->num_registered_methods = 0;
            obj->subscription_table.clear();
            for(int j = 0; j < MAX_SUPPORTED_METHODS; j++)
                obj->method_callbacks[j].method.clear();
        }
    }
    {
        using namespace rbus_client;
        if(0 != g_event_subscriptions_for_client.size())
        {
            rtLog_Info("Cancelling active event subscriptions.");
            unlock();
            for(const auto &subscription : g_event_subscriptions_for_client)
            {
                for(const auto &event : subscription.event_list)
                    send_subscription_request(subscription.object.c_str(), event.event_name.c_str(), false);
            }
            lock();
            g_event_subscriptions_for_client.clear();
        }
    }
    unlock();
}

rbus_error_t rbus_closeBrokerConnection()
{
    rtError err = RT_OK;
    perform_clean_up();
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
    int i = 0;
    int insert_candidate_offset = MAX_REGISTERED_OBJECTS;

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
    for(i = 0; i < MAX_REGISTERED_OBJECTS; i++)
    {
        if('\0' == g_object_table[i].name[0])
        {
            if(MAX_REGISTERED_OBJECTS == insert_candidate_offset)
                insert_candidate_offset = i;
        }
        else if(0 == strncmp(object_name, g_object_table[i].name, MAX_OBJECT_NAME_LENGTH))
        {
            unlock();
            rtLog_Error("%s is already registered. Rejecting duplicate registration.", object_name);
            return RTMESSAGE_BUS_ERROR_INVALID_PARAM;
        }
    }

    if(MAX_REGISTERED_OBJECTS != insert_candidate_offset)
    {
        g_object_table[insert_candidate_offset].callback = handler;
        g_object_table[insert_candidate_offset].data = user_data;
        strncpy(g_object_table[insert_candidate_offset].name, object_name, (MAX_OBJECT_NAME_LENGTH - 1));
        g_object_table[insert_candidate_offset].num_registered_methods = 0;
        unlock();
    }
    else
    {
        unlock();
        rtLog_Error("No free slots in object table.");
        return RTMESSAGE_BUS_ERROR_GENERAL;
    }

    //TODO: callback signature translation. rtMessage uses a significantly wider signature for callbacks. Translate to something simpler.
    err = rtConnection_AddListener(g_connection, object_name, onMessage, &g_object_table[insert_candidate_offset]);

    if(RT_OK != err)
    {
        rtLog_Error("Failed to register object. Error: 0x%x", err);
        lock();
        g_object_table[insert_candidate_offset].callback = dummyOnMessage;
        g_object_table[insert_candidate_offset].name[0] = '\0';
        unlock();
        return RTMESSAGE_BUS_ERROR_GENERAL;
    }

    rtLog_Info("Registered object %s", object_name);
    return RTMESSAGE_BUS_SUCCESS;
}

rbus_error_t rbus_registerMethod(const char * object_name, const char *method, rbus_callback_t handler, void * user_data)
{
    using namespace rbus_server;
    rbus_error_t ret = RTMESSAGE_BUS_SUCCESS;
    if(MAX_METHOD_NAME_LENGTH <= strlen(method))
        return RTMESSAGE_BUS_ERROR_INVALID_PARAM;

    lock();
    //TODO: Check that method name length is within limits and search for duplicates
    rbus_object * obj = get_object(object_name);
    if(NULL != obj)
    {
        if(MAX_SUPPORTED_METHODS <= obj->num_registered_methods)
        {
            rtLog_Error("Too many methods registered with object %s. Cannot register more.", object_name);
            ret = RTMESSAGE_BUS_ERROR_OUT_OF_RESOURCES;
        }
        else
        {
            int j;
            for(j = 0; j < MAX_SUPPORTED_METHODS; j++)
            {
                if(0 == strncmp(method, obj->method_callbacks[j].method.c_str(), MAX_METHOD_NAME_LENGTH))
                {
                   unlock();
                   rtLog_Error("Method %s is already registered,Rejecting duplicate registration.", method);
                   return RTMESSAGE_BUS_ERROR_INVALID_PARAM;
                }
                if(true == obj->method_callbacks[j].method.empty())
                {
                    obj->method_callbacks[j].method = method;
                    obj->method_callbacks[j].callback = handler;
                    obj->method_callbacks[j].data = user_data;
                    obj->num_registered_methods++;
                    rtLog_Info("Successfully registered method %s with object %s", method, object_name);
                    break;
                }
            }
            if(MAX_SUPPORTED_METHODS == j)
            {
                rtLog_Error("Method table for %s has no free entries. Bad clean-up?", object_name);
                ret = RTMESSAGE_BUS_ERROR_OUT_OF_RESOURCES;
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


rbus_error_t rbus_unregisterMethod(const char * object_name, const char *method)
{
    using namespace rbus_server;
	rbus_error_t ret = RTMESSAGE_BUS_SUCCESS;
	lock();

    rbus_object *obj = get_object(object_name);
    if(NULL != obj)
    {
        int j;
        for(j = 0; j < MAX_SUPPORTED_METHODS; j++)
        {
            if(0 == strncmp(method, obj->method_callbacks[j].method.c_str(), MAX_METHOD_NAME_LENGTH))
            {
                obj->method_callbacks[j].method.clear();
                rtLog_Info("Successfully unregistered method %s from object %s", method, object_name);
                obj->num_registered_methods--;
                break;
            }
        }
        if(MAX_SUPPORTED_METHODS == j)
        {
            rtLog_Error("Couldn't find a method %s registered with object %s.", method, object_name);
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
    using namespace rbus_server;
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
    rbus_object *obj = get_object(object_name);
    if(NULL != obj)
    {
        obj->callback = dummyOnMessage;
        obj->data = NULL;
        obj->name[0] = '\0';
        obj->process_event_subscriptions = false;
        obj->num_registered_methods = 0;
        obj->subscription_table.clear();
        for(int j = 0; j < MAX_SUPPORTED_METHODS; j++)
            obj->method_callbacks[j].method.clear();
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

    rtLog_Info("Added alias %s for object %s.", element, object_name);
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

static int subscription_handler(const char *, const char * method_name, rtMessage in, void * user_data, rtMessage *out)
{
    using namespace rbus_server;
    const char * sender = NULL;
    const char * event_name = NULL;
    rbus_object *obj = (rbus_object *)user_data;
    
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
            rbus_error_t ret;
            if(0 == strncmp(method_name, METHOD_ADD_EVENT_SUBSCRIPTION, MAX_METHOD_NAME_LENGTH))
            {
                ret = obj->addSubscriber(event_name, sender);
            }
            else
            {
                ret = obj->removeSubscriber(event_name, sender);
            }
            rtMessage_SetInt32(*out, MESSAGE_FIELD_RESULT, ret);
        }
    }
    
    return 0;
}


static rbus_error_t install_subscription_handlers(rbus_server::rbus_object &object)
{
    rbus_error_t ret = RTMESSAGE_BUS_SUCCESS; 
    int j;
    for(j = 0; j < MAX_SUPPORTED_METHODS; j++)
    {
        if(object.method_callbacks[j].method == METHOD_ADD_EVENT_SUBSCRIPTION)
        {
            rtLog_Info("Object already accepts subscription requests.");
            return ret;
        }
    }

    /*No subscription handlers present. Add them.*/
    rtLog_Info("Adding handler for subscription requests for %s.", object.name);
    if((ret = rbus_registerMethod(object.name, METHOD_ADD_EVENT_SUBSCRIPTION, subscription_handler, &object)) != RTMESSAGE_BUS_SUCCESS)
    {
        rtLog_Error("Could not register add_subscription_handler.");
    }
    else
    {
        if((ret = rbus_registerMethod(object.name, METHOD_REMOVE_EVENT_SUBSCRIPTION, subscription_handler, &object)) != RTMESSAGE_BUS_SUCCESS)
        {
            rtLog_Error("Could not register remove_subscription_handler.");
        }
        else
        {
            rtLog_Info("Successfully registered subscription handlers for %s.", object.name);
            object.process_event_subscriptions = true;
        }
    }
    return ret;
}

rbus_error_t rbus_registerEvent(const char* object_name, const char * event, rbus_event_subscribe_callback_t callback, void * user_data)
{
    using namespace rbus_server;
    rbus_error_t ret = RTMESSAGE_BUS_SUCCESS;

    if(NULL == event)
        event = DEFAULT_EVENT;
    if((NULL == object_name) || (NULL == callback))
    {
        rtLog_Error("Invalid parameter(s)");
        return RTMESSAGE_BUS_ERROR_INVALID_PARAM;
    }

    lock();
    rbus_object *obj = get_object(object_name);
    if(NULL != obj)
    {
        const auto iter = std::find_if(obj->subscription_table.begin(), obj->subscription_table.end(), 
                [&event](const event_entry_t &entry) -> bool { return (entry.event_name == event); });

        if(obj->subscription_table.end() != iter)
        {
            rtLog_Info("Event %s already exists in subscription table.", event);
        }
        else
        {
            obj->subscription_table.emplace_back(event, obj, callback, user_data);
            rtLog_Info("Registered event %s::%s.", object_name, event);
        }
        if(false == obj->process_event_subscriptions)
            ret = install_subscription_handlers(*obj);
    }
    else
    {
        rtLog_Error("Could not find object %s", object_name);
        ret = RTMESSAGE_BUS_ERROR_INVALID_PARAM;
    }
    unlock();
    return ret;
}

rbus_error_t rbus_unregisterEvent(const char* object_name, const char * event)
{
    using namespace rbus_server;
    rbus_error_t ret = RTMESSAGE_BUS_SUCCESS;
    if(NULL == event)
        event = DEFAULT_EVENT;

    lock();

    rbus_object *obj = get_object(object_name);
    if(NULL != obj)
    {
        const auto iter = std::find_if(obj->subscription_table.begin(), obj->subscription_table.end(), 
                [&event](const event_entry_t &entry) -> bool { return (entry.event_name == event); });

        if(obj->subscription_table.end() != iter)
        {
            obj->subscription_table.erase(iter);
            rtLog_Info("Event %s::%s has been unregistered.", object_name, event);
            /* If we've removed all events and RPC registrations, delete the object itself.*/
        }
        else
        {
            rtLog_Info("Event %s could not be found in subscription table of object %s.", event, object_name);
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
    using namespace rbus_client;
    const char * sender = hdr->reply_topic;
    const char * event_name = NULL;
    rtError err;
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
    for(auto &entry : g_event_subscriptions_for_client)
    {
        if(entry.object == sender
        || entry.object == event_name /* support rbus events being elements : the object name will be the event name */
        )
        {
            const auto iter = std::find_if(entry.event_list.begin(), entry.event_list.end(), 
                    [&event_name](const subscription_t::event_entry_t &event) -> bool { return (event.event_name == event_name); });

            if(entry.event_list.end() != iter)
            {
                unlock();
                iter->callback(sender, event_name, msg, iter->user_data);
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
    using namespace rbus_client;
    rbus_error_t ret = RTMESSAGE_BUS_ERROR_INVALID_PARAM;
    lock();
    
    for(auto entry = g_event_subscriptions_for_client.begin(); entry != g_event_subscriptions_for_client.end(); entry++)
    {
        /*First, search for existing entries for the same object.*/
        if(entry->object == object_name)
        {
            /*Check whether a matching event subscription exists for this object.*/
            const auto iter = std::find_if(entry->event_list.begin(), entry->event_list.end(), 
                    [&event_name](const subscription_t::event_entry_t &event) -> bool { return (event.event_name == event_name); });

            if(entry->event_list.end() != iter)
            {
                entry->event_list.erase(iter);
                rtLog_Info("Subscription removed for event %s::%s.", object_name, event_name);
                ret = RTMESSAGE_BUS_SUCCESS;

                if(true == entry->event_list.empty())
                {
                    /*No more registrations exist for this object. Remove it altogether.*/
                    rtLog_Info("Zero event subscriptions remaining for object %s. Cleaning up.", object_name);
                    g_event_subscriptions_for_client.erase(entry);
                }
            }
            else
            {
                rtLog_Warn("Subscription for event %s::%s not found.", object_name, event_name);
            }
            break;
        }
    }
    unlock();
    return ret;
}

rbus_error_t rbus_subscribeToEvent(const char * object_name,  const char * event_name, rbus_event_callback_t callback, void * user_data)
{
    using namespace rbus_client;
    rbus_error_t ret = RTMESSAGE_BUS_SUCCESS;

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
    lock();
   
    if(NULL == event_name)
        event_name = DEFAULT_EVENT;

    if(false == g_run_event_client_dispatch)
    {
        rtLog_Info("Starting event dispatching.");
        rtConnection_AddDefaultListener(g_connection, &master_event_callback, NULL);
        g_run_event_client_dispatch = true;
    }

    bool subscription_added = false;
    for(auto &entry : g_event_subscriptions_for_client)
    {
        /*First, search for existing entries for the same object.*/
        if(entry.object == object_name)
        {
            const auto iter = std::find_if(entry.event_list.begin(), entry.event_list.end(), 
                    [&event_name](const subscription_t::event_entry_t &event) -> bool { return (event.event_name == event_name); });

            if(entry.event_list.end() != iter)
            {
                rtLog_Warn("Subscription exists for event %s::%s.", object_name, event_name);
                unlock();
                return RTMESSAGE_BUS_SUCCESS;
            }
            else
            {
                entry.event_list.emplace_back(event_name, callback, user_data);
                rtLog_Info("Added subscription for event %s::%s.", object_name, event_name);
            }
            subscription_added = true;
            break;
        }

    }
    /* If no matching objects exist in records. Create a new entry.*/
    if(false == subscription_added)
        g_event_subscriptions_for_client.emplace_back(object_name, event_name, callback, user_data);

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
    using namespace rbus_server;
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
    rbus_object *obj = get_object(object_name);
    if(NULL != obj)
    {
        const auto iter = std::find_if(obj->subscription_table.begin(), obj->subscription_table.end(), 
                [&event_name](const event_entry_t &entry) -> bool { return (entry.event_name == event_name); });

        if(obj->subscription_table.end() != iter)
        {
            rtLog_Debug("Event %s exists in subscription table. Dispatching to %lu subscribers.", event_name, iter->listeners.size());
            for(const auto & listener : iter->listeners)
            {
               if(RTMESSAGE_BUS_SUCCESS != rbus_sendMessage(out, listener.c_str(), object_name))
               {
                   rtLog_Error("Couldn't send event %s::%s to %s.", object_name, event_name, listener.c_str());
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
        if(nullptr != elements[i])
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
        const char * value = nullptr;
        if((RT_OK == rbus_PopInt32(in, &result)) && (RTM_DISCOVERY_RESULT_SUCCESS == result))
        {
            char **array_ptr = (char **)malloc(len * sizeof(char *));
            if (nullptr != array_ptr)
            {
                *objects = array_ptr;
                memset(array_ptr, 0, (len * sizeof(char *)));
                for (int i = 0; i < len; i++)
                {
                    if ((RT_OK != rbus_PopString(in, &value)) || (nullptr == (array_ptr[i] = strndup(value, MAX_OBJECT_NAME_LENGTH))))
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
