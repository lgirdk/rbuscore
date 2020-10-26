#############################################################################
# If not stated otherwise in this file or this component's Licenses.txt file
# the following copyright and licenses apply:
#
# Copyright 2019 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#############################################################################

find_package(PkgConfig)

find_library(RTMESSAGE_LIBRARIES NAMES rtMessage)
find_path(RTMESSAGE_INCLUDE_DIRS NAMES rtMessage.h PATH_SUFFIXES rtmessage rtMessage)

set(RTMESSAGE_LIBRARIES ${RTMESSAGE_LIBRARIES} CACHE PATH "Path to rtMessage library")
set(RTMESSAGE_INCLUDE_DIRS ${RTMESSAGE_INCLUDE_DIRS} )
set(RTMESSAGE_INCLUDE_DIRS ${RTMESSAGE_INCLUDE_DIRS} CACHE PATH "Path to rtMessage include")

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(RTMESSAGE DEFAULT_MSG RTMESSAGE_INCLUDE_DIRS RTMESSAGE_LIBRARIES)

mark_as_advanced(
    RTMESSAGE_FOUND
    RTMESSAGE_INCLUDE_DIRS
    RTMESSAGE_LIBRARIES
    RTMESSAGE_LIBRARY_DIRS
    RTMESSAGE_FLAGS)
