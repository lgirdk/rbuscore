/*****************************************
Test server for unit test client testing
******************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include "rbus_core.h"
#include "rbus_marshalling.h"
#include "rbus_test_util.h"

static char buffer[100];

static char data1[100] = "alpha init init init";



int main(int argc, char *argv[])
{
    (void) argc;
    rbus_error_t err = RTMESSAGE_BUS_SUCCESS;
    printf("syntax: sample_server <server object name>\n");

    reset_stored_data();
    if((err = rbus_openBrokerConnection(argv[1])) == RTMESSAGE_BUS_SUCCESS)
    {
        printf("Successfully connected to bus.\n");
    }

    /*Creating Object 1*/

    snprintf(buffer, (sizeof(buffer) - 1), "%s.obj1", argv[1]);
    printf("Registering object %s\n", buffer);

    if((err = rbus_registerObj(buffer, callback, NULL)) == RTMESSAGE_BUS_SUCCESS)
    {
        printf("Successfully registered object %s \n", buffer);
    }

    rbus_method_table_entry_t table[2] = {{METHOD_SETPARAMETERVALUES, (void *)data1, handle_set2}, {METHOD_GETPARAMETERVALUES, (void *)data1, handle_get2}};

    /* registered the Methods */
    rbus_registerMethodTable(buffer, table, 2); 
   
    /* addelement to the object1 */
    rbus_addElement(buffer, "alpha_alias");

    /*registered the Events with name events */
    char data1[] = "data1";
    rbus_registerEvent(buffer,"event_1",sub1_callback, data1);

    rtMessage msg1;
    rtMessage_Create(&msg1);

    rbus_SetString(msg1, "foo", "bar");

    rbus_publishEvent(buffer, "event_1", msg1);

    rtMessage_Release(msg1);

  //  sleep(4);

    pause();

    if((err = rbus_closeBrokerConnection()) == RTMESSAGE_BUS_SUCCESS)
    {
        printf("Successfully disconnected from bus.\n");
    }
    return 0;
}
