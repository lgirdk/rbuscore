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
#ifndef __ROUTING_TREE_H__
#define __ROUTING_TREE_H__

#define MAX_KEY_LENGTH 512
typedef struct
{
    unsigned int length;
    char key[MAX_KEY_LENGTH];
    void * value;
} rtree_key_map_entry_t;

typedef struct 
{
    rtree_key_map_entry_t * entries;
    unsigned int capacity; //Number of entries, not actual size.
    unsigned int num_entries;
} rtree_key_bundle_t;


typedef enum
{
    ROUTING_STRATEGY_NORMAL = 0,
    ROUTING_STRATEGY_OPTIMIZED_V1,
    ROUTING_STRATEGY_OPTIMIZED_V2
} rtree_routing_strategy_t;

typedef int (*value_comparison_fn_t)(void * existing, void * incoming);
void rtree_initialize();
int rtree_set_value(const char * key, void * value);
int rtree_get_value(const char * key, void **value);
int rtree_remove_value(const char * key);
int rtree_get_value_o1(const char * key, void **value);
int rtree_get_value_o2(const char * key, void **value);
#ifdef ENABLE_ROUTER_BENCHMARKING
int rtree_lookup_value(const char * key, void **value);
#else
#define rtree_lookup_value rtree_get_value_o2
#endif
void rtree_get_stats();
void rtree_traverse_and_log();
void rtree_generate_quick_match_expressions(rtree_key_bundle_t * bundle);
int rtree_remove_nodes_matching_value(void * data);
int rtree_get_uniquely_resolvable_endpoints_for_expression(const char *expression, rtree_key_bundle_t * bundle);
int rtree_initialize_key_bundle(rtree_key_bundle_t * bundle, unsigned int capacity);
void rtree_set_routing_strategy(rtree_routing_strategy_t);
void rtree_dump_quick_match_expressions();
int rtree_get_all_nodes_matching_value(rtree_key_bundle_t * bundle, void * value);
#endif
