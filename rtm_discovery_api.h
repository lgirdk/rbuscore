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
#ifndef __RTM_DISCOVERY_API_H__
#define __RTM_DISCOVERY_API_H__

#define RTM_DISCOVERY_DESTINATION "_RTROUTED.INBOX.QUERY"
#define RTM_DISCOVERY_ENTRIES "entries"
#define RTM_DISCOVERY_NUM_ENTRIES "num_entries"
#define RTM_DISCOVERY_RESULT "result"
#define RTM_DISCOVERY_EXPRESSION "exp"
#define RTM_DISCOVERY_RESULT_SUCCESS 0
#define RTM_DISCOVERY_RESULT_FAILURE 1

#define ELEMENT_ENUMERATION_NUM_ENTRIES "num_entries"
#define ELEMENT_ENUMERATION_ENTRIES "entries"
#define ELEMENT_ENUMERATION_EXPRESSION "exp"
#define ELEMENT_ENUMERATION_OBJECT "expression"
#define ELEMENT_ENUMERATION "_enumerate_elements"
#define TRACE_ORIGIN_OBJECT "_trace_origin_object"

#define REGISTERED_COMPONENTS "_registered_components"
#define REGISTERED_COMPONENTS_ENTRIES "entries"
#define REGISTERED_COMPONENTS_SUCCESS 0
#define REGISTERED_COMPONENTS_FAILURE 1
#define REGISTERED_COMPONENTS_SIZE "num_entries"
#endif
