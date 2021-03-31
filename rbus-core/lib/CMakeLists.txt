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
cmake_policy(SET CMP0048 NEW)

project(rbus-core
    VERSION 0.0.0)

include_directories(
    ../..
    ../include)

add_library(
    rbus-core
    SHARED
    rbus_core.c
    rbus_message.c)

include_directories(${RTMESSAGE_INCLUDE_DIRS})

if (BUILD_FOR_DESKTOP)
    add_dependencies(rbus-core msgpack)
endif (BUILD_FOR_DESKTOP)

target_link_libraries(
    rbus-core
    ${RTMESSAGE_LIBRARIES}
    ${RDKLOGGER_LIBRARIES}
    ${MSGPACK_LIBRARIES}
    -lpthread)

set_target_properties(rbus-core
    PROPERTIES SOVERSION "0"
    VERSION "${PROJECT_VERSION}")

install (TARGETS rbus-core 
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib)

install (DIRECTORY ../include/ 
    DESTINATION "include/rbus-core")
