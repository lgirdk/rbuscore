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
cmake_policy(SET CMP0048 NEW)

project(rtmessage
	VERSION 0.1.0)
include(ExternalProject)
cmake_policy(SET CMP0015 NEW)
set(CMAKE_BUILD_TYPE Debug)
set(RDKLOGGER_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../../rdklogger/)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB2 REQUIRED glib-2.0)

set(CMAKE_C_FLAGS_DEBUG "-DENABLE_ROUTER_BENCHMARKING -Wall -Wextra -g -O0 -fno-inline")
set(CMAKE_C_FLAGS_RELEASE "-DENABLE_ROUTER_BENCHMARKING -Werror -Wall -Wextra")
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

# Below option downloads and builds Messagepack library, builds low level rtmessage libraries, its daemon and sample app, plus intermediate level bus-core API libraries and its sample apps.
if (BUILD_FOR_DESKTOP)
	# Make sure clang-tidy is installed. "make run-clang-tidy" will generate the clang-tidy-output
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

	option(BUILD_RTMESSAGE_LIB "BUILD_RTMESSAGE_LIB" ON)
	option(BUILD_RTMESSAGE_SAMPLE_APP "BUILD_RTMESSAGE_SAMPLE_APP" ON)
	option(BUILD_RTMESSAGE_ROUTED "BUILD_RTMESSAGE_ROUTED" ON)
	option(BUILD_RBUS "BUILD_RBUS" ON)
	option(BUILD_RBUS_SAMPLE_APPS "BUILD_RBUS_SAMPLE_APPS" ON)
	option(BUILD_RBUS_UNIT_TEST "BUILD_RBUS_UNIT_TEST" ON)
	option(BUILD_RTMESSAGE_ONLY "BUILD_RTMESSAGE_ONLY" ON)
endif (BUILD_FOR_DESKTOP)

#The below option builds only low level rtMessage APIs, plus the daemon and sample apps for those APIs.
if (BUILD_RTMESSAGE_ONLY)
	option(BUILD_RTMESSAGE_LIB "BUILD_RTMESSAGE_LIB" ON)
	option(BUILD_RTMESSAGE_SAMPLE_APP "BUILD_RTMESSAGE_SAMPLE_APP" ON)
	option(BUILD_RTMESSAGE_ROUTED "BUILD_RTMESSAGE_ROUTED" ON)
endif (BUILD_RTMESSAGE_ONLY)

# Builds intermdiate level bus-core APIs and its sample apps. Low level rtMessage library is also built in order to fulfil an internal dependency. There is a way to avoid that with a more complicated CMakeLists.txt, but keeping it simple for now. 
if (BUILD_RBUS)
	option(BUILD_RTMESSAGE_LIB "BUILD_RTMESSAGE_LIB" ON)
	option(BUILD_RBUS_SAMPLE_APPS "BUILD_RBUS_SAMPLE_APPS" OFF)
	option(BUILD_RBUS_UNIT_TEST "BUILD_RBUS_UNIT_TEST" OFF)
	option(BUILD_RBUS_BENCHMARK_TEST "BUILD_RBUS_BENCHMARK_TEST" OFF)
endif (BUILD_RBUS)

if (BUILD_FOR_DESKTOP)
	ExternalProject_Add(
		msgpack
		GIT_REPOSITORY https://github.com/msgpack/msgpack-c.git 
		GIT_TAG 2.1.4
		BUILD_IN_SOURCE 0
		UPDATE_DISCONNECTED 1
		PREFIX ${CMAKE_SOURCE_DIR}/msgpack
		BINARY_DIR ${CMAKE_SOURCE_DIR}/msgpack/build
		CMAKE_ARGS "-DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}"
		BUILD_COMMAND make)
endif (BUILD_FOR_DESKTOP)

if (BUILD_FOR_DESKTOP)
	message("Building for desktop")
	include_directories(
		${CMAKE_CURRENT_SOURCE_DIR}
		${CMAKE_CURRENT_SOURCE_DIR}/msgpack/src/msgpack/include
		${GLIB2_INCLUDE_DIRS})
	link_directories(
		${CMAKE_CURRENT_SOURCE_DIR}
		${CMAKE_CURRENT_SOURCE_DIR}/msgpack/build
		${CMAKE_BINARY_DIR}
		${LIBRARY_DIR}
		${GLIB2_LIBRARY_DIRS})
else()
	if (ENABLE_RDKLOGGER)
		include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${INCLUDE_DIR} ${INCLUDE_DIR}/messagepack ${RDKLOGGER_PATH} ${RDKLOGGER_PATH}/include ${GLIB2_INCLUDE_DIRS})
		link_directories(${CMAKE_CURRENT_SOURCE_DIR} ${LIBRARY_DIR} ${RDKLOGGER_PATH}/src/.libs/)
		add_definitions(-DENABLE_RDKLOGGER)
	else ()
		include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${INCLUDE_DIR} ${INCLUDE_DIR}/messagepack ${GLIB2_INCLUDE_DIRS})
		link_directories(${CMAKE_CURRENT_SOURCE_DIR} ${LIBRARY_DIR})
	endif (ENABLE_RDKLOGGER)
endif (BUILD_FOR_DESKTOP)

set(RTMESSAGE_LINK_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR} ${LIBRARY_DIR} ${RDKLOGGER_PATH}/src/.libs/)

if (ENABLE_RTMESSAGE_PROFILE)
	message("Enabling rtMessage profile")
	set(CMAKE_CFLAGS "${CMAKE_C_FLAGS} -pg")
endif (ENABLE_RTMESSAGE_PROFILE)

if (BUILD_RTMESSAGE_LIB)
	message("Building rtMessage lib")
	add_library(
		rtMessage 
		SHARED
		rtConnection.c
		rtLog.c
		rtError.c
		rtMessageHeader.c
		rtEncoder.c
		rtMessage_mpack.c
		rtSocket.c
		rtVector.c
		base64.c
		routing_tree.c
		local_benchmarking.c)
	if (BUILD_FOR_DESKTOP)
		add_dependencies(rtMessage msgpack)
	else ()
		if (ENABLE_RDKLOGGER)
			target_link_libraries(rtMessage -llog4c -lrdkloggers)
		endif (ENABLE_RDKLOGGER)
	endif(BUILD_FOR_DESKTOP)
	target_link_libraries(rtMessage ${LIBRARY_LINKER_OPTIONS} -pthread -lmsgpackc -lglib-2.0)
	set_target_properties(rtMessage
		PROPERTIES SOVERSION "0"
		VERSION "${PROJECT_VERSION}")
endif (BUILD_RTMESSAGE_LIB)

if (BUILD_RBUS)
	add_subdirectory(rbus-core/lib)
	add_subdirectory(rbus-core/test)
	add_subdirectory(rbus-core/session_manager)
endif (BUILD_RBUS)

if (BUILD_DATAPROVIDER_LIB)
	add_library(
		dataProvider
		SHARED
		dataProvider/dmProviderDatabase.cpp
		dataProvider/dmProviderHost.cpp
		dataProvider/dmPropertyInfo.cpp
		dataProvider/dmProviderInfo.cpp
		dataProvider/dmQueryResult.cpp
		dataProvider/dmProvider.cpp
		dataProvider/dmError.cpp
		dataProvider/dmValue.cpp)
	add_dependencies(
		dataProvider
		rtMessage)
	target_link_libraries(
		dataProvider
		rtMessage)
endif (BUILD_DATAPROVIDER_LIB)

if (BUILD_RTMESSAGE_ROUTED)
	message ("Building rtrouted")
	set(CMAKE_CFLAGS " ${CMAKE_C_FLAGS}")
	add_executable(rtrouted rtrouted.c)
	if (BUILD_FOR_DESKTOP)
		add_dependencies(rtrouted msgpack)
	endif(BUILD_FOR_DESKTOP)
	target_link_libraries(rtrouted ${LIBRARY_LINKER_OPTIONS} rtMessage)
endif (BUILD_RTMESSAGE_ROUTED)

if (BUILD_DMCLI)
	add_executable(dmcli dataProvider/dmcli.cpp)
	if (BUILD_FOR_DESKTOP)
		add_dependencies(dmcli msgpack rtMessage dataProvider)
		add_definitions("-DDEFAULT_DATAMODELDIR=\"${CMAKE_CURRENT_SOURCE_DIR}/dataProvider/data\"")
	else()
		add_definitions("-DDEFAULT_DATAMODELDIR=\"${RDK_FSROOT_PATH}/etc/model\"")
	endif (BUILD_FOR_DESKTOP)
	target_link_libraries(dmcli dataProvider rtMessage)
endif (BUILD_DMCLI)

if (BUILD_DMCLI_SAMPLE_APP)
	# sample provider for general
	add_executable(sample_provider_gen dataProvider/sample_provider_gen.cpp)
	if (BUILD_FOR_DESKTOP)
		add_dependencies(sample_provider_gen dataProvider)
	endif (BUILD_FOR_DESKTOP)
	target_link_libraries(sample_provider_gen ${LIBRARY_LINKER_OPTIONS} rtMessage dataProvider)
	# sample provider for wifi
	add_executable(sample_provider_wifi dataProvider/sample_provider_wifi.cpp)
	if (BUILD_FOR_DESKTOP)
		add_dependencies(sample_provider_wifi dataProvider)
	endif (BUILD_FOR_DESKTOP)
	target_link_libraries(sample_provider_wifi ${LIBRARY_LINKER_OPTIONS} rtMessage dataProvider)
endif (BUILD_DMCLI_SAMPLE_APP)

if (BUILD_RTMESSAGE_SAMPLE_APP)
	# sample_send
	add_executable(sample_send sample_send.c)
	if (BUILD_FOR_DESKTOP)
		add_dependencies(sample_send msgpack)
	endif (BUILD_FOR_DESKTOP)
	add_dependencies(sample_send rtMessage)
	target_link_libraries(sample_send ${LIBRARY_LINKER_OPTIONS} rtMessage)

	# sample_recv
	add_executable(sample_recv sample_recv.c)
	if (BUILD_FOR_DESKTOP)
		add_dependencies(sample_recv msgpack)
	endif (BUILD_FOR_DESKTOP)
	add_dependencies(sample_send rtMessage)
	target_link_libraries(sample_recv ${LIBRARY_LINKER_OPTIONS} rtMessage)

	# sample_req
	add_executable(sample_req sample_req.c)
	if (BUILD_FOR_DESKTOP)
		add_dependencies(sample_req msgpack)
	endif (BUILD_FOR_DESKTOP)
	add_dependencies(sample_send rtMessage)
	target_link_libraries(sample_req ${LIBRARY_LINKER_OPTIONS} rtMessage)

	# sample_res
	add_executable(sample_res sample_res.c)
	if (BUILD_FOR_DESKTOP)
		add_dependencies(sample_res msgpack)
	endif (BUILD_FOR_DESKTOP)
	add_dependencies(sample_send rtMessage)
	target_link_libraries(sample_res ${LIBRARY_LINKER_OPTIONS} rtMessage)
	
    # diag_probe
	add_executable(rtm_diag_probe diag_probe.c)
	add_dependencies(rtm_diag_probe rtMessage)
	target_link_libraries(rtm_diag_probe ${LIBRARY_LINKER_OPTIONS} rtMessage)
endif (BUILD_RTMESSAGE_SAMPLE_APP)

if (BUILD_RTMESSAGE_ONLY)
	install (TARGETS rtrouted rtMessage rtm_diag_probe
		RUNTIME DESTINATION bin
		LIBRARY DESTINATION lib)
	install(FILES rtMessage.h rtMessageHeader.h rtError.h rtConnection.h rtVector.h rtm_discovery_api.h DESTINATION "include/rbus-core")
endif (BUILD_RTMESSAGE_ONLY)
