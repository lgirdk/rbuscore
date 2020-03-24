##########################################################################
# If not stated otherwise in this file or this component's Licenses.txt
# file the following copyright and licenses apply:
#
# Copyright 2019 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
##########################################################################
include_directories(
	../../..
    ../../include)

if (BUILD_RBUS_UNIT_TEST)
find_package(GTest QUIET)
#Certain builds/branches don't come with GTest support. Do not attempt to build unit-test binaries if that's the case. 
if (NOT GTEST_INCLUDE_DIR)
    message("Warning! GTest wasn't found. Unit test capabilities won't be built.")
else ()
    add_executable(rbus_test_server rbus_test_util.cpp rbus_test_server.cpp)
    add_dependencies(rbus_test_server rbus-core)
    target_link_libraries(rbus_test_server rbus-core)

    enable_testing()
    include_directories(${GTEST_INCLUDE_DIR})

    add_executable(rbus_unit_test_app
                   rbus_unit_test_marshalling.cpp
                   rbus_test_util.cpp
                   rbus_unit_test_client.cpp
                   rbus_unit_stresstest_server.cpp
                   rbus_unit_test_server.cpp
                   rbus_unit_test_multiple_objects.cpp
                   rbus_unit_test_multiple_servers.cpp
                   rbus_unit_test_nested_servers.cpp
                   rbus_unit_test_app.cpp)
    add_dependencies(rbus_unit_test_app rbus-core)
    target_link_libraries(rbus_unit_test_app rbus-core
                                           gtest)

    add_test(
        NAME rbus_unit_test_app
        COMMAND rbus_unit_test_app
    )

    install (TARGETS rbus_test_server rbus_unit_test_app
            RUNTIME DESTINATION bin)
endif ()
endif (BUILD_RBUS_UNIT_TEST)
