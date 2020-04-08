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
/*****************************************
Test Case : Testing Marshalling APIs
******************************************/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
extern "C" {
#include "rbus_marshalling.h"
}
#include "gtest_app.h"


#define DEFAULT_RESULT_BUFFERSIZE 128



typedef struct{
    uint32_t element1;
    char element2[100];
}testStruct_t;


static void compareMessage(rtMessage message, const char* expectedMessage)
{
    char* buff = NULL;
    uint32_t buff_length = 0;

    rtMessage_ToString(message, &buff, &buff_length);
    //printf("dumpMessage: %.*s\n", buff_length, buff);
    EXPECT_STREQ(buff, expectedMessage) << "Message comparison failed!!";
    free(buff);
}

class TestMarshallingAPIs : public ::testing::Test{

protected:

static void SetUpTestCase()
{
    printf("********************************************************************************************\n");

    printf("Set up done Successfully for TestMarshallingAPIs\n");
}

static void TearDownTestCase()
{
    printf("********************************************************************************************\n");
    printf("Clean up done Successfully for TestMarshallingAPIs\n");
}

};

TEST_F(TestMarshallingAPIs, sample_test)
{
    EXPECT_EQ(1, 1);
}

TEST_F(TestMarshallingAPIs, rbus_SetString_test1)
{
    rtMessage testMessage;
    rtError err = RT_OK;
    char tag[] = "string_field";
    char value[] = "TestString1";
    const char* resultValue = NULL;
    err = rtMessage_Create(&testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";

    rbus_SetString(testMessage, tag, value);
    err = rbus_GetString(testMessage, tag, &resultValue);
    EXPECT_EQ(err, RT_OK) << "rbus call failed";
    EXPECT_STREQ(resultValue, value) << "rbus_SetString failed";
    err = rtMessage_Release(testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";
}

TEST_F(TestMarshallingAPIs, rbus_SetString_test2)
{
    rtMessage testMessage;
    rtError err = RT_OK;
    char tag[] = "string_field";
    char value[] = "########!!!!!!TestString123456789000000000000000000000000000";
    const char* resultValue = NULL;
    err = rtMessage_Create(&testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";

    rbus_SetString(testMessage, tag, value);
    err = rbus_GetString(testMessage, tag, &resultValue);
    EXPECT_EQ(err, RT_OK) << "rbus call failed";
    EXPECT_STREQ(resultValue, value) << "rbus_SetString failed";
    err = rtMessage_Release(testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";
}

TEST_F(TestMarshallingAPIs, rbus_SetString_test3)
{
    rtMessage testMessage;
    rtError err = RT_OK;
    char tag1[] = "string_field1";
    char tag2[] = "string_field2";
    char value1[] = "########!!!!!!TestString123456789000000000000000000000000000";
    char value2[] = "TestString123456789";
    const char* resultValue = NULL;
    err = rtMessage_Create(&testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";

    rbus_SetString(testMessage, tag1, value1);
    rbus_SetString(testMessage, tag2, value2);
    err = rbus_GetString(testMessage, tag1, &resultValue);
    EXPECT_EQ(err, RT_OK) << "rbus call failed";
    EXPECT_STREQ(resultValue, value1) << "rbus_SetString failed for tag1";
    err = rbus_GetString(testMessage, tag2, &resultValue);
    EXPECT_EQ(err, RT_OK) << "rbus call failed";
    EXPECT_STREQ(resultValue, value2) << "rbus_SetString failed for tag2";
    err = rtMessage_Release(testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";
}

/*Set and  Get 100 strings to an rtMessage*/
TEST_F(TestMarshallingAPIs, rbus_SetString_test4)
{
    rtMessage testMessage;
    rtError err = RT_OK;
    char tagOriginal[50] = "string_field";
    char valueOriginal[50] = "TestString";
    char tag[50] = "";
    char value[50] = "";
    const char* resultValue = NULL;
    int i = 0;
    char count[10] = {};
    err = rtMessage_Create(&testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";

    for(i = 0; i < 100; i++)
    {
        snprintf(tag, (sizeof(tag) - 1), "%s%d", tagOriginal, i);
        snprintf(value, (sizeof(value) - 1), "%s%d", valueOriginal, i);
        rbus_SetString(testMessage, tag, value);
    }
    memset(tag, 0, 50);
    memset(value, 0, 50);
    for(i = 0; i < 100; i++)
    {
        snprintf(tag, (sizeof(tag) - 1), "%s%d", tagOriginal, i);
        snprintf(value, (sizeof(value) - 1), "%s%d", valueOriginal, i);
        err = rbus_GetString(testMessage, tag, &resultValue);
        EXPECT_EQ(err, RT_OK) << "rbus call failed";
        EXPECT_STREQ(resultValue, value) << "rbus_SetString failed for tag : " << tag ;
    }

    err = rtMessage_Release(testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";
}

/*Set and  Get 10000 strings to an rtMessage*/
TEST_F(TestMarshallingAPIs, rbus_SetString_test5)
{
    rtMessage testMessage;
    rtError err = RT_OK;
    char tagOriginal[50] = "string_field";
    char valueOriginal[50] = "TestString";
    char tag[50] = "";
    char value[50] = "";
    const char* resultValue = NULL;
    int i = 0;
    char count[10] = {};
    err = rtMessage_Create(&testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";

    for(i = 0; i < 10000; i++)
    {
        snprintf(tag, (sizeof(tag) - 1), "%s%d", tagOriginal, i);
        snprintf(value, (sizeof(value) - 1), "%s%d", valueOriginal, i);
        rbus_SetString(testMessage, tag, value);
    }
    memset(tag, 0, 50);
    memset(value, 0, 50);
    for(i = 0; i < 10000; i++)
    {
        snprintf(tag, (sizeof(tag) - 1), "%s%d", tagOriginal, i);
        snprintf(value, (sizeof(value) - 1), "%s%d", valueOriginal, i);
        err = rbus_GetString(testMessage, tag, &resultValue);
        EXPECT_EQ(err, RT_OK) << "rbus call failed";
        EXPECT_STREQ(resultValue, value) << "rbus_SetString failed for tag : " << tag ;
    }

    err = rtMessage_Release(testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";
}

/*Set and  Get 100000 strings to an rtMessage*/
TEST_F(TestMarshallingAPIs, rbus_SetString_test6)
{
    rtMessage testMessage;
    rtError err = RT_OK;
    char tagOriginal[50] = "string_field";
    char valueOriginal[50] = "TestString";
    char tag[50] = "";
    char value[50] = "";
    const char* resultValue = NULL;
    int i = 0;
    char count[10] = {};
    err = rtMessage_Create(&testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";

    for(i = 0; i < 100000; i++)
    {
        snprintf(tag, (sizeof(tag) - 1), "%s%d", tagOriginal, i);
        snprintf(value, (sizeof(value) - 1), "%s%d", valueOriginal, i);
        rbus_SetString(testMessage, tag, value);
    }
    memset(tag, 0, 50);
    memset(value, 0, 50);
    for(i = 0; i < 100000; i++)
    {
        snprintf(tag, (sizeof(tag) - 1), "%s%d", tagOriginal, i);
        snprintf(value, (sizeof(value) - 1), "%s%d", valueOriginal, i);
        err = rbus_GetString(testMessage, tag, &resultValue);
        EXPECT_EQ(err, RT_OK) << "rbus call failed";
        EXPECT_STREQ(resultValue, value) << "rbus_SetString failed for tag : " << tag ;
    }

    err = rtMessage_Release(testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";
}

TEST_F(TestMarshallingAPIs, rbus_SetString_test7)
{
    rtMessage testMessage;
    rtError err = RT_OK;
    char tag[] = "string_field";
    char value[] = "string value 1";
    const char* messageString = "\"string value 1\"";
    err = rtMessage_Create(&testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";

    rbus_SetString(testMessage, tag, value);
    compareMessage(testMessage, messageString);
    err = rtMessage_Release(testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";
}

TEST_F(TestMarshallingAPIs, rbus_GetStringValue_test1)
{
    rtMessage testMessage;
    rtError err = RT_OK;
    char tag[] = "string_field";
    char value[] = "TestString1";
    char resultValue[50] = {0};
    err = rtMessage_Create(&testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";

    rbus_SetString(testMessage, tag, value);
    err = rbus_GetStringValue(testMessage, tag, resultValue, sizeof(resultValue));
    EXPECT_EQ(err, RT_OK) << "rbus call failed";
    EXPECT_STREQ(resultValue, value) << "rbus_GetStringValue failed";
    err = rtMessage_Release(testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";
}

TEST_F(TestMarshallingAPIs, rbus_AppendString_test1)
{
    rtMessage testMessage;
    rtError err = RT_OK;
    char value[] = "TestString1";
    const char* resultValue = NULL;
    err = rtMessage_Create(&testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";

    err = rbus_AppendString(testMessage, value);
    EXPECT_EQ(err, RT_OK) << "rbus call failed";
    err = rbus_PopString(testMessage, &resultValue);
    EXPECT_EQ(err, RT_OK) << "rbus call failed";
    EXPECT_STREQ(resultValue, value) << "rbus_AppendString failed";
    err = rtMessage_Release(testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";
}

TEST_F(TestMarshallingAPIs, rbus_AppendString_test2)
{
    rtMessage testMessage;
    rtError err = RT_OK;
    char value[] = "########!!!!!!TestString123456789000000000000000000000000000";
    const char* resultValue = NULL;
    err = rtMessage_Create(&testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";

    err = rbus_AppendString(testMessage, value);
    EXPECT_EQ(err, RT_OK) << "rbus call failed";
    err = rbus_PopString(testMessage, &resultValue);
    EXPECT_EQ(err, RT_OK) << "rbus call failed";
    EXPECT_STREQ(resultValue, value) << "rbus_AppendString failed";
    err = rtMessage_Release(testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";
}

TEST_F(TestMarshallingAPIs, rbus_AppendString_test3)
{
    rtMessage testMessage;
    rtError err = RT_OK;
    char value1[] = "########!!!!!!TestString123456789000000000000000000000000000";
    char value2[] = "TestString123456789";
    const char* resultValue = NULL;
    err = rtMessage_Create(&testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";

    err = rbus_AppendString(testMessage, value1);
    EXPECT_EQ(err, RT_OK) << "rbus call failed";
    err = rbus_AppendString(testMessage, value2);
    EXPECT_EQ(err, RT_OK) << "rbus call failed";
    err = rbus_PopString(testMessage, &resultValue);
    EXPECT_EQ(err, RT_OK) << "rbus call failed";
    EXPECT_STREQ(resultValue, value1) << "rbus_AppendString failed for tag1";
    err = rbus_PopString(testMessage, &resultValue);
    EXPECT_EQ(err, RT_OK) << "rbus call failed";
    EXPECT_STREQ(resultValue, value2) << "rbus_AppendString failed for tag2";
    err = rtMessage_Release(testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";
}

/*Set and  Get 100 strings to an rtMessage*/
TEST_F(TestMarshallingAPIs, rbus_AppendString_test4)
{
    rtMessage testMessage;
    rtError err = RT_OK;
    char valueOriginal[50] = "TestString";
    char value[50] = "";
    const char* resultValue = NULL;
    int i = 0;
    char count[10] = {};
    err = rtMessage_Create(&testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";

    for(i = 0; i < 100; i++)
    {
        snprintf(value, (sizeof(value) - 1), "%s%d", valueOriginal, i);
        err = rbus_AppendString(testMessage, value);
        EXPECT_EQ(err, RT_OK) << "rbus call failed";
    }
    memset(value, 0, 50);
    for(i = 0; i < 100; i++)
    {
        snprintf(value, (sizeof(value) - 1), "%s%d", valueOriginal, i);
        err = rbus_PopString(testMessage, &resultValue);
        EXPECT_EQ(err, RT_OK) << "rbus call failed";
        EXPECT_STREQ(resultValue, value) << "rbus_AppendString failed for iteration: " << i ;
    }

    err = rtMessage_Release(testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";
}

/*Try to set empty string*/
TEST_F(TestMarshallingAPIs, rbus_AppendString_test5)
{
    rtMessage testMessage;
    rtError err = RT_OK;
    char value[] = "";
    const char* resultValue = NULL;
    err = rtMessage_Create(&testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";

    err = rbus_AppendString(testMessage, value);
    EXPECT_EQ(err, RT_OK) << "rbus call failed";
    err = rbus_PopString(testMessage, &resultValue);
    EXPECT_EQ(err, RT_OK) << "rbus call failed";
    EXPECT_STREQ(resultValue, value) << "rbus_AppendString failed";
    err = rtMessage_Release(testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";
}

/*Try to set NULL value for string*/
TEST_F(TestMarshallingAPIs, rbus_AppendString_test6)
{
    rtMessage testMessage;
    rtError err = RT_OK;
    char value[] = "";
    const char* resultValue = NULL;
    err = rtMessage_Create(&testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";

    err = rbus_AppendString(testMessage, NULL);
    EXPECT_EQ(err, RT_OK) << "rbus call failed";
    err = rbus_PopString(testMessage, &resultValue);
    EXPECT_EQ(err, RT_OK) << "rbus call failed";
    EXPECT_STREQ(resultValue, value) << "rbus_AppendString failed";
    err = rtMessage_Release(testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";
}

TEST_F(TestMarshallingAPIs, rbus_AddString_test1)
{
    rtMessage testMessage;
    rtError err = RT_OK;
    char tag[] = "string_field";
    char value[] = "string value 1";
    const char* messageString = "\"string value 1\"";
    err = rtMessage_Create(&testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";

    err = rbus_AddString(testMessage, tag, value);
    EXPECT_EQ(err, RT_OK) << "rbus call failed";
    compareMessage(testMessage, messageString);
    err = rtMessage_Release(testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";
}

TEST_F(TestMarshallingAPIs, rbus_AddString_test2)
{
    rtMessage testMessage;
    rtError err = RT_OK;
    char tag[] = "string_field";
    char value1[] = "string value 1";
    char value2[] = "string value 2";
    const char* messageString = "\"string value 1\" \"string value 2\"";
    err = rtMessage_Create(&testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";

    err = rbus_AddString(testMessage, tag, value1);
    EXPECT_EQ(err, RT_OK) << "rbus call failed";
    err = rbus_AddString(testMessage, tag, value2);
    EXPECT_EQ(err, RT_OK) << "rbus call failed";
    compareMessage(testMessage, messageString);
    err = rtMessage_Release(testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";
}

TEST_F(TestMarshallingAPIs, rbus_AddString_test3)
{
    rtMessage testMessage;
    rtError err = RT_OK;
    char tag[] = "string_field";
    char value1[] = "string value 1";
    char value2[] = "string value 2";
    char value3[] = "string value 3";
    const char* messageString="\"string value 1\" \"string value 2\" \"string value 3\"";
    err = rtMessage_Create(&testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";

    err = rbus_AddString(testMessage, tag, value1);
    EXPECT_EQ(err, RT_OK) << "rbus call failed";
    err = rbus_AddString(testMessage, tag, value2);
    EXPECT_EQ(err, RT_OK) << "rbus call failed";
    err = rbus_AddString(testMessage, tag, value3);
    EXPECT_EQ(err, RT_OK) << "rbus call failed";
    compareMessage(testMessage, messageString);
    err = rtMessage_Release(testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";
}

TEST_F(TestMarshallingAPIs, rbus_GetStringItem_test1)
{
    rtMessage testMessage;
    rtError err = RT_OK;
    char tag[] = "string_field";
    const char value[3][15] = {"string value 1", "string value 2", "string value 3"};
    char result[100] = {0};
    int i = 0;
    err = rtMessage_Create(&testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";

    for(i = 0; i < 3; i++)
    {
        err = rbus_AddString(testMessage, tag, *(value + i));
        EXPECT_EQ(err, RT_OK) << "rbus call failed";
    }

    for(i = 0; i < 3; i++)
    {
        rbus_GetStringItem(testMessage, tag, i, result, sizeof(result));
        EXPECT_STREQ(result, *(value + i)) << "rbus_GetStringItem failed for index " << i;
        memset(result, 0, sizeof(result));
    }

    err = rtMessage_Release(testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";
}

TEST_F(TestMarshallingAPIs, rbus_GetArrayLength_test1)
{
    rtMessage testMessage;
    rtError err = RT_OK;
    char tag[] = "string_field";
    char value1[] = "string value 1";
    char value2[] = "string value 2";
    char value3[] = "string value 3";
    int32_t arrayLength = 0;
    err = rtMessage_Create(&testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";

    err = rbus_AddString(testMessage, tag, value1);
    EXPECT_EQ(err, RT_OK) << "rbus call failed";
    err = rbus_AddString(testMessage, tag, value2);
    EXPECT_EQ(err, RT_OK) << "rbus call failed";
    err = rbus_AddString(testMessage, tag, value3);
    EXPECT_EQ(err, RT_OK) << "rbus call failed";
    err = rbus_GetArrayLength(testMessage, tag, &arrayLength);
    EXPECT_EQ(err, RT_OK) << "rbus to get arraylength call failed";
    EXPECT_EQ(arrayLength, 3) << "GetArrayLength failed";
    err = rtMessage_Release(testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";
}
//rbus_AddBinaryData


TEST_F(TestMarshallingAPIs, rbus_SetInt32_test1)
{
    rtMessage testMessage;
    rtError err = RT_OK;
    char tag[] = "int32_field";
    int32_t value = 2000;
    int32_t resultValue = 0;
    err = rtMessage_Create(&testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";

    err = rbus_SetInt32(testMessage, tag, value);
    EXPECT_EQ(err, RT_OK) << "rbus call failed";
    err = rbus_GetInt32(testMessage, tag, &resultValue);
    EXPECT_EQ(err, RT_OK) << "rbus call failed";
    EXPECT_EQ(resultValue, value) << "rbus_SetInt32 failed";
    err = rtMessage_Release(testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";
}

TEST_F(TestMarshallingAPIs, rbus_SetInt32_test2)
{
    rtMessage testMessage;
    rtError err = RT_OK;
    char tag[] = "int32_field";
    int32_t value = 2147483647;
    int32_t resultValue = 0;
    err = rtMessage_Create(&testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";

    err = rbus_SetInt32(testMessage, tag, value);
    EXPECT_EQ(err, RT_OK) << "rbus call failed";
    err = rbus_GetInt32(testMessage, tag, &resultValue);
    EXPECT_EQ(err, RT_OK) << "rbus call failed";
    EXPECT_EQ(resultValue, value) << "rbus_SetInt32 failed";
    err = rtMessage_Release(testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";
}

/*Set and  Get 100 integers to an rtMessage*/
TEST_F(TestMarshallingAPIs, rbus_SetInt32_test3)
{
    rtMessage testMessage;
    rtError err = RT_OK;
    char tagOriginal[50] = "int32_field";
    int32_t valueOriginal = 2000;
    char tag[50] = "";
    int32_t value = 0;
    int32_t resultValue = 0;
    int i = 0;
    char count[10] = {};
    err = rtMessage_Create(&testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";

    for(i = 0; i < 100; i++)
    {
        value = valueOriginal;
        snprintf(tag, (sizeof(tag) - 1), "%s%d", tagOriginal, i);
        value = value * i;
        err = rbus_SetInt32(testMessage, tag, value);
        EXPECT_EQ(err, RT_OK) << "rbus call failed";
    }
    memset(tag, 0, 50);
    for(i = 0; i < 100; i++)
    {
        value = valueOriginal;
        snprintf(tag, (sizeof(tag) - 1), "%s%d", tagOriginal, i);
        value = value * i;
        err = rbus_GetInt32(testMessage, tag, &resultValue);
        EXPECT_EQ(err, RT_OK) << "rbus call failed";
        EXPECT_EQ(resultValue, value) << "rbus_SetInt32 failed for tag : " << tag ;
    }
    err = rtMessage_Release(testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";
}

TEST_F(TestMarshallingAPIs, rbus_AppendInt32_test1)
{
    rtMessage testMessage;
    rtError err = RT_OK;
    int32_t value = 2000;
    int32_t resultValue = 0;
    err = rtMessage_Create(&testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";

    err = rbus_AppendInt32(testMessage, value);
    EXPECT_EQ(err, RT_OK) << "rbus call failed";
    err = rbus_PopInt32(testMessage, &resultValue);
    EXPECT_EQ(err, RT_OK) << "rbus call failed";
    EXPECT_EQ(resultValue, value) << "rbus_AppendInt32 failed";
    err = rtMessage_Release(testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";
}

TEST_F(TestMarshallingAPIs, rbus_AppendInt32_test2)
{
    rtMessage testMessage;
    rtError err = RT_OK;
    int32_t value = 2147483647;
    int32_t resultValue = 0;
    err = rtMessage_Create(&testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";

    err = rbus_AppendInt32(testMessage, value);
    EXPECT_EQ(err, RT_OK) << "rbus call failed";
    err = rbus_PopInt32(testMessage, &resultValue);
    EXPECT_EQ(err, RT_OK) << "rbus call failed";
    EXPECT_EQ(resultValue, value) << "rbus_AppendInt32 failed";
    err = rtMessage_Release(testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";
}

/*Set and  Get 100 integers to an rtMessage*/
TEST_F(TestMarshallingAPIs, rbus_AppendInt32_test3)
{
    rtMessage testMessage;
    rtError err = RT_OK;
    int32_t valueOriginal = 2000;
    int32_t value = 0;
    int32_t resultValue = 0;
    int i = 0;
    char count[10] = {};
    err = rtMessage_Create(&testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";

    for(i = 0; i < 100; i++)
    {
        value = valueOriginal;
        sprintf(count, "%d", i);
        value = value * i;
        err = rbus_AppendInt32(testMessage, value);
        EXPECT_EQ(err, RT_OK) << "rbus call failed";
    }
    for(i = 0; i < 100; i++)
    {
        value = valueOriginal;
        sprintf(count, "%d", i);
        value = value * i;
        err = rbus_PopInt32(testMessage, &resultValue);
        EXPECT_EQ(err, RT_OK) << "rbus call failed";
        EXPECT_EQ(resultValue, value) << "rbus_AppendInt32 failed for iteration: " << i;
    }
    err = rtMessage_Release(testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";
}

TEST_F(TestMarshallingAPIs, rbus_SetDouble_test1)
{
    rtMessage testMessage;
    rtError err = RT_OK;
    char tag[] = "double_field";
    double value = 999.999;
    double resultValue = 0;
    err = rtMessage_Create(&testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";

    err = rbus_SetDouble(testMessage, tag, value);
    EXPECT_EQ(err, RT_OK) << "rbus call failed";
    err = rbus_GetDouble(testMessage, tag, &resultValue);
    EXPECT_EQ(err, RT_OK) << "rbus call failed";
    EXPECT_EQ(resultValue, value) << "rbus_SetDouble failed";
    err = rtMessage_Release(testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";
}

TEST_F(TestMarshallingAPIs, rbus_SetDouble_test2)
{
    rtMessage testMessage;
    rtError err = RT_OK;
    char tag[] = "double_field";
    double value = 21474836.67;
    double resultValue = 0;
    err = rtMessage_Create(&testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";

    err = rbus_SetDouble(testMessage, tag, value);
    EXPECT_EQ(err, RT_OK) << "rbus call failed";
    err = rbus_GetDouble(testMessage, tag, &resultValue);
    EXPECT_EQ(err, RT_OK) << "rbus call failed";
    EXPECT_EQ(resultValue, value) << "rbus_SetDouble failed";

    err = rtMessage_Release(testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";
}

/*Set and  Get 100 double values to an rtMessage*/
TEST_F(TestMarshallingAPIs, rbus_SetDouble_test3)
{
    rtMessage testMessage;
    rtError err = RT_OK;
    char tagOriginal[50] = "double_field";
    double valueOriginal = 2000.0002;
    char tag[50] = "";
    double value = 0;
    double resultValue = 0;
    int i = 0;
    char count[10] = {};
    err = rtMessage_Create(&testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";

    for(i = 0; i < 100; i++)
    {
        value = valueOriginal;
        snprintf(tag, (sizeof(tag) - 1), "%s%d", tagOriginal, i);
        value = value * i;
        err = rbus_SetDouble(testMessage, tag, value);
        EXPECT_EQ(err, RT_OK) << "rbus call failed";
    }
    memset(tag, 0, 50);
    for(i = 0; i < 100; i++)
    {
        value = valueOriginal;
        snprintf(tag, (sizeof(tag) - 1), "%s%d", tagOriginal, i);
        value = value * i;
        err = rbus_GetDouble(testMessage, tag, &resultValue);
        EXPECT_EQ(err, RT_OK) << "rbus call failed";
        EXPECT_EQ(resultValue, value) << "rbus_SetDouble failed for tag : " << tag ;
    }
    err = rtMessage_Release(testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";
}

TEST_F(TestMarshallingAPIs, rbus_AppendDouble_test1)
{
    rtMessage testMessage;
    rtError err = RT_OK;
    double value = 999.999;
    double resultValue = 0;
    err = rtMessage_Create(&testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";

    err = rbus_AppendDouble(testMessage,  value);
    EXPECT_EQ(err, RT_OK) << "rbus call failed";
    err = rbus_PopDouble(testMessage, &resultValue);
    EXPECT_EQ(err, RT_OK) << "rbus call failed";
    EXPECT_EQ(resultValue, value) << "rbus_AppendDouble failed";
    err = rtMessage_Release(testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";
}

TEST_F(TestMarshallingAPIs, rbus_AppendDouble_test2)
{
    rtMessage testMessage;
    rtError err = RT_OK;
    double value = 21474836.67;
    double resultValue = 0;
    err = rtMessage_Create(&testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";

    err = rbus_AppendDouble(testMessage, value);
    EXPECT_EQ(err, RT_OK) << "rbus call failed";
    err = rbus_PopDouble(testMessage, &resultValue);
    EXPECT_EQ(err, RT_OK) << "rbus call failed";
    EXPECT_EQ(resultValue, value) << "rbus_AppendDouble failed";

    err = rtMessage_Release(testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";
}

/*Set and  Get 100 double values to an rtMessage*/
TEST_F(TestMarshallingAPIs, rbus_AppendDouble_test3)
{
    rtMessage testMessage;
    rtError err = RT_OK;
    double valueOriginal = 2000.0002;
    double value = 0;
    double resultValue = 0;
    int i = 0;
    char count[10] = {};
    err = rtMessage_Create(&testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";

    for(i = 0; i < 100; i++)
    {
        value = valueOriginal;
        sprintf(count, "%d", i);
        value = value * i;
        err = rbus_AppendDouble(testMessage, value);
        EXPECT_EQ(err, RT_OK) << "rbus call failed";
    }
    for(i = 0; i < 100; i++)
    {
        value = valueOriginal;
        sprintf(count, "%d", i);
        value = value * i;
        err = rbus_PopDouble(testMessage, &resultValue);
        EXPECT_EQ(err, RT_OK) << "rbus call failed";
        EXPECT_EQ(resultValue, value) << "rbus_AppendDouble failed for iteration: " << i ;
    }
    err = rtMessage_Release(testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";
}

TEST_F(TestMarshallingAPIs, rbus_AddBinaryData_test1)
{
    rtMessage testMessage;
    rtError err = RT_OK;
    char tag[] = "array_field";
    testStruct_t sampleStruct = {10, "String1"};
    const testStruct_t *ptr;
    unsigned int size = 0;

    err = rtMessage_Create(&testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";

    err = rbus_AddBinaryData(testMessage, tag, (void *)&sampleStruct, sizeof(sampleStruct));
    EXPECT_EQ(err, RT_OK) << "rbus call failed";
    err = rbus_GetBinaryData(testMessage, tag, (const void **)&ptr, &size);
    EXPECT_EQ(err, RT_OK) << "rbus call failed";
    EXPECT_EQ(size, sizeof(sampleStruct)) << "BinaryData size comparison failed";
    EXPECT_EQ(ptr->element1, sampleStruct.element1) << "BinaryData element1 comparison failed";
    EXPECT_STREQ(ptr->element2, sampleStruct.element2) << "BinaryData element1 comparison failed";
    err = rtMessage_Release(testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";
}
TEST_F(TestMarshallingAPIs, rbus_AppendBinaryData_test1)
{
    rtMessage testMessage;
    rtError err = RT_OK;
    testStruct_t sampleStruct = {10, "String1"};
    const testStruct_t *ptr;
    unsigned int size = 0;

    err = rtMessage_Create(&testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";

    err = rbus_AppendBinaryData(testMessage, (void *)&sampleStruct, sizeof(sampleStruct));
    EXPECT_EQ(err, RT_OK) << "rbus call failed";
    err = rbus_PopBinaryData(testMessage, (const void **)&ptr, &size);
    EXPECT_EQ(err, RT_OK) << "rbus call failed";
    EXPECT_EQ(size, sizeof(sampleStruct)) << "BinaryData size comparison failed";
    EXPECT_EQ(ptr->element1, sampleStruct.element1) << "BinaryData element1 comparison failed";
    EXPECT_STREQ(ptr->element2, sampleStruct.element2) << "BinaryData element1 comparison failed";
    err = rtMessage_Release(testMessage);
    EXPECT_EQ(err, RT_OK) << "rtMessage call failed";
}

TEST_F(TestMarshallingAPIs, rbus_SetMessage_test1)
{
    rtMessage childMessage;
    rtMessage parentMessage;
    char parentTag[] = "parent_field";
    char tag[] = "child_field";
    char value[] = "TestString1";
    const char* messageString = "\"TestString1\"";

    rtMessage_Create(&childMessage);
    rbus_SetString(childMessage, tag, value);

    rtMessage_Create(&parentMessage);
    rbus_SetMessage(parentMessage, parentTag, childMessage);

    compareMessage(parentMessage, messageString);

    rtMessage_Release(childMessage);
    rtMessage_Release(parentMessage);
}

TEST_F(TestMarshallingAPIs, rbus_GetMessage_test1)
{
    rtMessage childMessage;
    rtMessage parentMessage;
    rtMessage procuredMessage;
    char parentTag[] = "parent_field";
    char tag[] = "child_field";
    char value[] = "TestString1";
    const char* messageString = "\"TestString1\"";

    rtMessage_Create(&childMessage);
    rbus_SetString(childMessage, tag, value);

    rtMessage_Create(&parentMessage);
    rbus_SetMessage(parentMessage, parentTag, childMessage);

    rbus_GetMessage(parentMessage, parentTag, &procuredMessage);
    compareMessage(procuredMessage, messageString);

    rtMessage_Release(procuredMessage);
    rtMessage_Release(childMessage);
    rtMessage_Release(parentMessage);
}

TEST_F(TestMarshallingAPIs, rbus_AddMessage_test1)
{
    rtMessage childMessage1;
    rtMessage childMessage2;
    rtMessage parentMessage;
    char parentTag[] = "parent_field";
    char tag1[] = "child_field1";
    char tag2[] = "child_field2";
    char value1[] = "TestString1";
    char value2[] = "TestString2";
    const char* messageString = "\"TestString1\" \"TestString2\"";

    rtMessage_Create(&childMessage1);
    rbus_SetString(childMessage1, tag1, value1);
    rtMessage_Create(&childMessage2);
    rbus_SetString(childMessage2, tag2, value2);

    rtMessage_Create(&parentMessage);
    rbus_AddMessage(parentMessage, parentTag, childMessage1);
    rbus_AddMessage(parentMessage, parentTag, childMessage2);

    compareMessage(parentMessage, messageString);

    rtMessage_Release(childMessage1);
    rtMessage_Release(childMessage2);
    rtMessage_Release(parentMessage);
}

TEST_F(TestMarshallingAPIs, rbus_GetMessageItem_test1)
{
    rtMessage childMessage1;
    rtMessage childMessage2;
    rtMessage parentMessage;
    rtMessage procuredMessage;
    char parentTag[] = "parent_field";
    char tag1[] = "child_field1";
    char tag2[] = "child_field2";
    char value1[] = "TestString1";
    char value2[] = "TestString2";
    const char* messageString1 = "\"TestString1\"";
    const char* messageString2 = "\"TestString2\"";

    rtMessage_Create(&childMessage1);
    rbus_SetString(childMessage1, tag1, value1);
    rtMessage_Create(&childMessage2);
    rbus_SetString(childMessage2, tag2, value2);

    rtMessage_Create(&parentMessage);
    rbus_AddMessage(parentMessage, parentTag, childMessage1);
    rbus_AddMessage(parentMessage, parentTag, childMessage2);

    rbus_GetMessageItem(parentMessage, parentTag, 0, &procuredMessage);
    compareMessage(procuredMessage, messageString1);
    rtMessage_Release(procuredMessage);
    rbus_GetMessageItem(parentMessage, parentTag, 1, &procuredMessage);
    compareMessage(procuredMessage, messageString2);
    rtMessage_Release(procuredMessage);

    rtMessage_Release(childMessage1);
    rtMessage_Release(childMessage2);
    rtMessage_Release(parentMessage);
}
