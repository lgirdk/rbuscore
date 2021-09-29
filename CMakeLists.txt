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
cmake_minimum_required (VERSION 2.8.7)

if (POLICY CMP0048)
  cmake_policy(SET CMP0048 NEW)
  project(rbuscore VERSION 0.1.0)
else () 
  project(rbuscore)
  set(PROJECT_VERSION "0.1.0")
endif (POLICY CMP0048)

include(ExternalProject)
cmake_policy(SET CMP0015 NEW)
set(CMAKE_BUILD_TYPE Debug)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/")
find_package(PkgConfig REQUIRED)
find_package(rtMessage REQUIRED)

include_directories(${RTMESSAGE_INCLUDE_DIRS})
link_directories(${RTMESSAGE_LIBRARIES})
include_directories(${CJSON_INCLUDE_DIRS})
#link_directories(${CJSON_LIBRARIES})

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS} -DENABLE_ROUTER_BENCHMARKING -Wall -Wextra -g -O0 -fno-inline")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS} -DENABLE_ROUTER_BENCHMARKING -Werror -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-DENABLE_ROUTER_BENCHMARKING -Wall -Wextra -g -O0 -fno-inline")
set(CMAKE_CXX_FLAGS_RELEASE "-DENABLE_ROUTER_BENCHMARKING -Wall -Wextra") # -Werror")

if (CMAKE_VERSION VERSION_LESS "3.1")
    message("CMAKE ${CMAKE_CXX_COMPILER_ID}")
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set (CMAKE_CXX_FLAGS "-std=gnu++11 ${CMAKE_CXX_FLAGS}")
        set (CMAKE_C_FLAGS "-std=c11 ${CMAKE_C_FLAGS}")
    endif ()
else ()
    set (CMAKE_CXX_STANDARD 11)
    set (CMAKE_C_STANDARD 11)
endif ()

option(ENABLE_RDKLOGGER "ENABLE_RDKLOGGER" OFF)
option(BUILD_RBUS_SAMPLE_APPS "BUILD_RBUS_SAMPLE_APPS" OFF)
option(BUILD_RBUS_UNIT_TEST "BUILD_RBUS_UNIT_TEST" OFF)
option(BUILD_RBUS_BENCHMARK_TEST "BUILD_RBUS_BENCHMARK_TEST" OFF)

if (ENABLE_RDKLOGGER)
    find_package(rdklogger REQUIRED)
    add_definitions(-DENABLE_RDKLOGGER)
endif (ENABLE_RDKLOGGER)

if (BUILD_FOR_DESKTOP)
    if (ENABLE_CLANG_TIDY)
        find_program(GENERATOR run-clang-tidy)
        if(NOT GENERATOR)
            message("clang-tidy not found!")
        else ()
            set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
            set(CLANG_TIDY_EXE "/usr/bin/run-clang-tidy")
            add_custom_target(run-clang-tidy ALL
                WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                COMMENT "Creating clang output"
                COMMAND ${CLANG_TIDY_EXE} -checks=-*,clang-analyzer-* 2>&1 > ${CMAKE_CURRENT_BINARY_DIR}/clang-tidy-output.txt
                DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/compile_commands.json)
        endif()
    endif (ENABLE_CLANG_TIDY)

    option(BUILD_RBUS_SAMPLE_APPS "BUILD_RBUS_SAMPLE_APPS" ON)
    option(BUILD_RBUS_UNIT_TEST "BUILD_RBUS_UNIT_TEST" ON)
    add_definitions(-DRBUS_ALWAYS_ON)
else ()
  find_package(msgpack REQUIRED)
endif (BUILD_FOR_DESKTOP)

if (BUILD_FOR_DESKTOP)
    message("Building for desktop")

    ExternalProject_Add(
        msgpack
        GIT_REPOSITORY https://github.com/msgpack/msgpack-c.git
        GIT_TAG 2.1.4
        BUILD_IN_SOURCE 0
        UPDATE_DISCONNECTED 1
        PREFIX ${CMAKE_CURRENT_BINARY_DIR}/msgpack
        BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/msgpack/build
        CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
                   -DMSGPACK_BUILD_EXAMPLES=OFF
                   -DMSGPACK_BUILD_TESTS=OFF
        BUILD_COMMAND make)

    set(MSGPACK_LIBRARIES "-lmsgpackc")

    include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}/msgpack/src/msgpack/include)
    link_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}/msgpack/build
        ${CMAKE_BINARY_DIR}
        ${LIBRARY_DIR})
else()
    include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${INCLUDE_DIR})
    include_directories(${RTMESSAGE_INCLUDE_DIRS} ${MSGPACK_INCLUDE_DIRS} ${RDKLOGGER_INCLUDE_DIRS})
    link_directories(${CMAKE_CURRENT_SOURCE_DIR} ${LIBRARY_DIR})
endif (BUILD_FOR_DESKTOP)

    add_subdirectory(rbus-core/lib)
    add_subdirectory(rbus-core/session_manager)
    add_subdirectory(rbus-core/test)
