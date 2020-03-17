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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "routing_tree.h"
#include "rtLog.h"

#define ROOT_LIST_LENGTH 100
#define TOKEN_SEPARATOR '.'
#define RTREE_QUICK_LOOKUP_CAPACITY 100
#define KEY_BUNDLE_RESIZE_INCREMENT 4

struct node_t;
typedef struct node_t
{
    struct node_t *parent;
    struct node_t **children_head;
    char * name;
    void * value;
    unsigned int name_length;
    unsigned int num_children;
} node_t;

typedef enum
{
    COMPLETE_LIST = 0,
    TRIMMED_LIST
} rtree_key_bundle_type_t;

static node_t root_list[ROOT_LIST_LENGTH];
static rtree_key_bundle_t g_quick_lookup_list;
static unsigned int g_quick_lookup_is_uptodate = 0;
static rtree_routing_strategy_t g_routing_strategy = ROUTING_STRATEGY_OPTIMIZED_V2;


int dummy_equivalence_fn(void * existing, void * incoming)
{
    if(existing == incoming)
        return 0;
    else
        return 1;
}

value_comparison_fn_t g_comparison_fn = &dummy_equivalence_fn; //g_comparison_fn must return 0 of values are equivalent.

static int g_num_dynamic_nodes;
static int g_num_root_nodes;

void rtree_set_routing_strategy(rtree_routing_strategy_t strategy)
{
    if((ROUTING_STRATEGY_NORMAL == strategy) || (ROUTING_STRATEGY_OPTIMIZED_V1 == strategy))
        g_routing_strategy = strategy;
    else
        g_routing_strategy = ROUTING_STRATEGY_OPTIMIZED_V2;
    rtLog_Info("Routing strategy set to %d", g_routing_strategy);
}

void rtree_initialize()
{
  rtree_initialize_key_bundle(&g_quick_lookup_list, RTREE_QUICK_LOOKUP_CAPACITY);
}
void rtree_get_stats()
{
    rtLog_Info("<< rtree statistics: %d root nodes, %d dynamic nodes. >>", g_num_root_nodes, g_num_dynamic_nodes);
}

static void rtree_set_value_comparator(value_comparison_fn_t fn)
{
    if(NULL == fn)
    {
        g_comparison_fn = dummy_equivalence_fn;
        rtLog_Info("Clearing value comparator. Reset to default comparator.");
    }
    else
    {
        g_comparison_fn = fn;
        rtLog_Info("New comparison function installed");
    }
}

static int add_root_node(char * key, void * optional_value, node_t ** created_node)
{
    int ret = 0;
    int i = 0;
    //rtLog_Debug("Creating root node for %s", key);
    for(i = 0; i < ROOT_LIST_LENGTH; i++)
    {
        if(NULL == root_list[i].name)
        {
            if((0 != root_list[i].num_children) || (NULL != root_list[i].children_head))
            {
                rtLog_Warn("Bad cleanup. Node %s indicates it has %d children", key, root_list[i].num_children);
                ret = -1;
                free(key);
                return ret;
            }
            root_list[i].name = key;
            root_list[i].name_length = strlen(key);
            root_list[i].value = optional_value;
            if(NULL != created_node)
                *created_node = &root_list[i];
            rtLog_Debug("Created root node at position %d for %s", i, key);
            g_num_root_nodes++;
            return ret;
        }
    }

    if(ROOT_LIST_LENGTH == i)
    {
        rtLog_Error("Cannot add key %s. Ran out of space!", key);
        ret = -1;
        free(key);
    }
    return ret;
}


static inline  node_t * search_matching_root_node(const char * key, unsigned int * bytes_consumed)
{
    for(int i = 0; i < ROOT_LIST_LENGTH; i++)
    {
        //rtLog_Debug("Check root node %s vs key %s", root_list[i].name, key);
        if((NULL != root_list[i].name) && (0 == strncmp(key, root_list[i].name, root_list[i].name_length)))
        {
            /*Extra check to make sure that it's not a case of key abcd matching against node.name abc */
            if((TOKEN_SEPARATOR == root_list[i].name[root_list[i].name_length - 1]) || ('\0' == key[root_list[i].name_length]))
            {
                rtLog_Debug("Found matching root node for %s", key);
                *bytes_consumed = root_list[i].name_length;
                return &root_list[i];
            }
        }
    }
    return NULL;
}

static int get_matching_root_node(const char * key, void * value, node_t ** node, unsigned int * bytes_consumed)
{
    int ret = 0;
    unsigned int i = 0;

    if((*node = search_matching_root_node(key, bytes_consumed)) != NULL)
        return ret;

    /*No matching root node exists.*/
    char * buffer = NULL;
    for(i = 0; i < strlen(key); i++)
    {
        if(TOKEN_SEPARATOR == key[i])
            break;
    }
    if(i == strlen(key))
    {
        //Couldn't find token separator anywhere. Treat the whole string as the key.
        buffer = strdup(key);
        *bytes_consumed = i;
        if(NULL == buffer)
        {
            rtLog_Error("Out of memory. Cannot copy keys");
            ret = -1;
            return ret;
        }
    }
    else
    {
        //Copy the substring only
        buffer = (char *)malloc(i + 2);
        if(NULL == buffer)
        {
            rtLog_Error("Out of memory. Cannot copy keys");
            ret = -1;
            return ret;
        }
        strncpy(buffer, key, (i + 1));
        buffer[i + 1] = '\0';
        *bytes_consumed = i + 1;
    }
    ret = add_root_node(buffer, value, node);
    return ret;
}

static node_t *get_matching_child(node_t * parent, const char *key, unsigned int *bytes_consumed)
{
    unsigned int i, j;

    for(i = 0; i < parent->num_children; i++)
    {
        node_t * child = parent->children_head[i];
        const char * left = key; 
        const char * right = child->name;
        //rtLog_Debug("Checking child #%d %s against key %s", i, right, left);
        for(j = 0; j < child->name_length; j++)
        {
            if(*left++ != *right++)
            {
                //rtLog_Debug("Child %s is not a match", child->name);
                break;
            }
        }
        if((j == child->name_length) && (('\0' == *left) || (TOKEN_SEPARATOR == *(left - 1))))
        {
            /* At this point, the first j bytes of node's name are identical to first j bytes of
             * key. Additionally, the left-most token of the incoming key is not longer than
             * node's name. This means the token is a match for this node.*/
            *bytes_consumed = child->name_length;
            return child;
        }
    }
    return NULL;
}

static int attach_child(node_t * parent, node_t *child)
{
    int ret = 0;
    parent->num_children += 1;
    node_t ** new_head = (node_t **)realloc(parent->children_head, (parent->num_children * sizeof(node_t *)));
    if(NULL == new_head)
    {
        rtLog_Error("Couldn't not allocate memory for child list");
        return -1;
    }
    parent->children_head = new_head;
    parent->children_head[parent->num_children - 1] = child;
    child->parent = parent;

    return ret;
}

static inline void revise_upstream_optimization(node_t *node)
{
    /* When a child has been removed from a node that didn't allow optimized look-up in its previous state, it's possible 
     * the remaining children possess a common "value". If that's the case, we can optimize this node for fuzzy look-up. 
     * If the current node gets a non-NULL "value" as a result, this opens up an opportunity to revise the upstream nodes 
     * as well. So the action needs to be repeated as far upstream as possible till you come across a parent node that 
     * cannot be optimized.*/
    do
    {
        if((NULL == node->value) && (0 != node->num_children))
        {
            void * reference = node->children_head[0]->value;
            if(NULL == reference)
                return; /* If reference is NULL, at least one child node is unoptimized. That means none of the
                           upstream nodes or this parent can be optimized. */
            for(unsigned int i = 1; i < node->num_children; i++)
            {
                if(0 != g_comparison_fn(reference, node->children_head[i]->value))
                    return;
            }
            node->value = reference;
            rtLog_Debug("Node %s re-optimized for look-up efficiency.", node->name);
            node = node->parent;
        }
        else
            break;
    }
    while(NULL != node);
}

static void detach_child(node_t * parent, node_t * child)
{
    /*Locate the child in the array and get it out.*/
    for(unsigned int i = 0; i < parent->num_children; i++)
    {
        if(child == parent->children_head[i])
        {
            //rtLog_Debug("Located child %s in parent %s. It will now be removed. Parent has %d children.", child->name, parent->name, parent->num_children);
            child->parent = NULL;
            parent->num_children--;
            for(unsigned int j = i; j < parent->num_children; j++)
                parent->children_head[j] = parent->children_head[j + 1];
        }
    }

    if(0 == parent->num_children)
    {
        //rtLog_Debug("Node %s has no more children.", parent->name);
        free(parent->children_head);
        parent->children_head = NULL;
    }
    else
    {
        node_t ** new_head = (node_t **)realloc(parent->children_head, (parent->num_children * sizeof(node_t *)));
        if(NULL == new_head)
            rtLog_Error("Couldn't not re-allocate memory when removing child.");
        else
            parent->children_head = new_head;
    }
}

static inline void initialize_dynamic_node(node_t *node, char * key, unsigned int key_length, void * value)
{
    node->name = key;
    node->name_length = key_length;
    node->value = value;
    node->num_children = 0;
    node->children_head = NULL;
}

static int generate_sub_tree(node_t * parent, const char * key, void * value)
{
    int ret = 0;
    unsigned int i = 0;

    while('\0' != *key)
    {
        unsigned int key_length = strlen(key);

        node_t * fresh = (node_t *)malloc(sizeof(node_t));
        if(NULL == fresh)
        {
            rtLog_Error("Cannot generate sub tree. Out of memory");
            return  -1;
        }

        char * buffer = NULL;
        for(i = 0; i < key_length; i++)
            if(TOKEN_SEPARATOR == key[i])
                break;

        if(i == key_length)
        {
            //Couldn't find token separator. This tree ends here. 
            buffer = strdup(key);
            if(NULL == buffer)
            {
                rtLog_Error("Out of memory. Cannot generate leaf key");
                free(fresh);
                return -1;;
            }
            rtLog_Debug("Created final node %s", buffer);
            initialize_dynamic_node(fresh, buffer, key_length, value);
            ret = attach_child(parent, fresh);
            if(0 != ret)
            {
                free(fresh);
            }
            g_num_dynamic_nodes++;
            return ret;
        }
        else
        {
            //Copy the substring only
            buffer = (char *)malloc(i + 2);
            if(NULL == buffer)
            {
                rtLog_Error("Out of memory. Cannot copy keys");
                free(fresh);
                return -1;
            }
            strncpy(buffer, key, (i + 1));
            buffer[i + 1] = '\0';
            rtLog_Debug("Created intermediate node %s for key %s", buffer, key);
            initialize_dynamic_node(fresh, buffer, (i + 1), value);
            ret = attach_child(parent, fresh);
            if(0 != ret)
            {
                free(fresh);
                break;
            }

            //Prepare for next iteration.
            parent = fresh;
            key += i + 1;
            g_num_dynamic_nodes++;
        }
    }
    return ret;
}

/* Optimization philosophy:
 * For every node you traverse in setting a value,
 * if the node's value is equivalent to incoming value, do nothing.
 * If the node's value is not equivalent to incoming value, NULL that node's value.
 *
 * Because the tree is always traversed beginning at the root and propagating outward, if you come across a node with equivalent value,
 * ALL nodes downstream of it are guaranteed to possess the same value.*/
static inline void optimize_route(node_t * node, void * value, unsigned int * current_state) //Note: current_state must be initialed to 0 before every traversal.
{
    if((0 == *current_state) && (NULL != node->value)) // 0 is unoptimized.
    {
        if(0 == g_comparison_fn(node->value, value))
        {
            rtLog_Debug("Value for key %s is equivalent. Optimizing downstream tree.", node->name);
            *current_state = 1;
        }
        else
        {
            rtLog_Debug("Value for key %s is not equivalent. Cancelling optimzation.", node->name);
            node->value = NULL;
        }
    }
}


static int rtree_set_value_in_tree(const char * key, void * value)
{
    int ret = 0;
    unsigned int bytes_consumed = 0;
    unsigned int optimization_state = 0;

    /*Locate root node to start tree search.*/
    node_t * parent = NULL;
    node_t* child = NULL;
    if((ret = get_matching_root_node(key, value, &parent, &bytes_consumed)) == 0)
    {
        optimize_route(parent, value, &optimization_state);
        /*Start at second token (first is already matched). Recursively go through the tree to find matching node.*/
        const char * sub_key = key + bytes_consumed;
        while((child = get_matching_child(parent, sub_key, &bytes_consumed)) != NULL)
        {
            //rtLog_Debug("Traversing node %s", child->name);
            optimize_route(child, value, &optimization_state);
            sub_key += bytes_consumed;
            parent = child;
        }
        if('\0' == *sub_key)
        {
            //rtLog_Debug("Reached leaf node %s for key %s", parent->name, key);
            parent->value = value;
        }
        else
        {
            //There are parts of the key that aren't represented in the tree yet. Create them.
            ret = generate_sub_tree(parent, sub_key, value);
        }
    }
    return ret;
}

int rtree_set_value(const char * key, void * value)
{
    int ret = 0;
    //TODO: Find a more efficient solution to avoiding duplicate entries.
    void * duplicate_value;
    if(0 == rtree_get_value(key, &duplicate_value))
    {
        rtLog_Error("Rejecting new route because a duplicate one already exists.");
        return -1;
    }
    rtLog_Debug("Setting value for key %s", key);
    unsigned int current_num_root_nodes = g_num_root_nodes;
    unsigned int current_num_dynamic_nodes = g_num_dynamic_nodes;

    if(MAX_KEY_LENGTH <= strlen(key))
    {
        rtLog_Error("Key %s is too long. Won't add.", key);
        return -1;
    }

    /*Handle special cases first.*/
    if('_' == key[0])
    {
        int i = 0;
        /*Internal route. Wildcard capability not required. Check if we already have this key. Replace value if so. Else, create a new node and add it.*/
        for(i = 0; i < ROOT_LIST_LENGTH; i++)
        {
            if((NULL != root_list[i].name) && (0 == strncmp(key, root_list[i].name, root_list[i].name_length)))
            {
                rtLog_Warn("Found existing key for %s. Setting new value", key);
                root_list[i].value = value;
                break;
            }
        }

        if(ROOT_LIST_LENGTH == i)
        {
            /*Node for this key doesn't exist yet. Create it.*/
            ret = add_root_node(strdup(key), value, NULL);
        }
    }
    else
    {
        /*We're in wildcard territory. Tokenize and process.*/
        ret = rtree_set_value_in_tree(key, value);

    }
    rtLog_Debug("Operation added %d root nodes, %d dynamic nodes", (g_num_root_nodes - current_num_root_nodes), (g_num_dynamic_nodes - current_num_dynamic_nodes));
    if(0 == ret)
        g_quick_lookup_is_uptodate = 0;
    return ret;
}

int rtree_get_value(const char * key, void **value)
{
    int ret = 0;
    unsigned int bytes_consumed = 0;

    node_t *parent = NULL;
    rtLog_Debug("Getting value for key %s", key);
    if((parent = search_matching_root_node(key, &bytes_consumed)) != NULL)
    {
        key += bytes_consumed;
        if('\0' == *key)
        {
            rtLog_Debug("Found complete match in root node %s", parent->name);
            if(NULL == parent->value)
            {
                rtLog_Debug("Key %s is not resolvable.", key);
                ret = -1;
            }
            else
                *value = parent->value;
        }
        else
        {
            node_t * child = NULL;
            while((child = get_matching_child(parent, key, &bytes_consumed)) != NULL)
            {
                //rtLog_Debug("Traversing node %s", child->name);
                key += bytes_consumed;
                parent = child;
            }
            if('\0' == *key)
            {
                //rtLog_Debug("Reached leaf node %s", parent->name);
                if(NULL == parent->value)
                {
                    rtLog_Debug("Key %s is not resolvable.", key);
                    ret = -1;
                }
                else
                    *value = parent->value;
            }
            else
            {
                rtLog_Debug("Cannot find path to key");
                ret = -1;
            }
        }

    }
    else
    {
        rtLog_Debug("Couldn't locate matching root node for key %s", key);
        ret = -1;
    }
    return ret;
}


int rtree_get_value_o1(const char * key, void **value)
{
    int ret = 0;
    unsigned int bytes_consumed = 0;

    node_t *parent = NULL;
    rtLog_Debug("Getting value (O1) for key %s", key);
    if((parent = search_matching_root_node(key, &bytes_consumed)) != NULL)
    {
        key += bytes_consumed;
        if('\0' == *key)
        {
            rtLog_Debug("Found complete match in root node %s", parent->name);
            if(NULL == parent->value)
            {
                rtLog_Error("Key \"%s\" is not resolvable.", key);
                ret = -1;
            }
            else
                *value = parent->value;
        }
        else if(NULL != parent->value)
        {
            //rtLog_Debug("Found early match in root node %s", parent->name);
            *value = parent->value;
        }
        else
        {
            node_t * child = NULL;
            while((child = get_matching_child(parent, key, &bytes_consumed)) != NULL)
            {
                //rtLog_Debug("Traversing node %s", child->name);
                if(NULL != child->value)
                {
                    //rtLog_Debug("Found early match in node %s", child->name);
                    *value = child->value;
                    return ret;
                }
                key += bytes_consumed;
                parent = child;
            }
            if('\0' == *key)
            {
                //rtLog_Debug("Reached leaf node %s", parent->name);
                if(NULL == parent->value)
                {
                    rtLog_Error("Key \"%s\" is not resolvable.", key);
                    ret = -1;
                }
                else
                    *value = parent->value;
            }
            else
            {
                rtLog_Warn("Cannot find path to key");
                ret = -1;
            }
        }

    }
    else
    {
        rtLog_Error("Couldn't locate matching root node for key %s", key);
        ret = -1;
    }
    return ret;
}

static int comparator(const void * left, const void * right)
{
    const char * l = ((rtree_key_map_entry_t *)left)->key;
    const char * r = ((rtree_key_map_entry_t *)right)->key;
    return (-1 * strcmp(l, r)); //descending order preferred.
}

static void get_complete_path_to_node(node_t * node, char * buffer)
{
    int length = 0;

    //Find out how long the complete key is.
    node_t * reverse_iter = node;;
    while(NULL != reverse_iter)
    {
        length += reverse_iter->name_length;
        reverse_iter = reverse_iter->parent;
    }

    //Assemble the string.
    if(0 < length)
    {
        buffer[length] = '\0';
        reverse_iter = node;;
        while(NULL != reverse_iter)
        {
            length -= reverse_iter->name_length;
            char * w_ptr = buffer + length ;
            strncpy(w_ptr, reverse_iter->name, reverse_iter->name_length);
            reverse_iter = reverse_iter->parent;
        }
    }
    else
        rtLog_Error("Couldn't generate path for node.");

}

static int rtree_add_node_to_bundle(rtree_key_bundle_t * bundle, node_t * node)
{
    /*Bundle memory management*/
    if(bundle->num_entries == bundle->capacity)
    {
        rtLog_Debug("Resizing bundle.");
        bundle->capacity += KEY_BUNDLE_RESIZE_INCREMENT;
        rtree_key_map_entry_t * temp = (rtree_key_map_entry_t *)realloc(bundle->entries, (bundle->capacity * sizeof(rtree_key_map_entry_t)));
        if(NULL == temp)
        {
            rtLog_Warn("Memory reallocation for key bundle failed. Cannot generate quick-match expressions.");
            return -1;
        }
        bundle->entries = temp;
    }
    rtree_key_map_entry_t * entry = &bundle->entries[bundle->num_entries];
    entry->value = node->value;
    get_complete_path_to_node(node, entry->key);
    entry->length = strlen(entry->key);
    bundle->num_entries++;
    return 0; 
}

static inline void sort_bundle(rtree_key_bundle_t * bundle)
{
    qsort(bundle->entries, bundle->num_entries, sizeof(rtree_key_map_entry_t), &comparator);
}

int rtree_get_value_o2(const char * key, void **value)
{
    int ret = -1;

    rtLog_Debug("Getting value (O2) for key %s", key);
    
    if(0 == g_quick_lookup_is_uptodate)
    {
        g_quick_lookup_list.num_entries = 0;
        rtree_generate_quick_match_expressions(&g_quick_lookup_list);
        sort_bundle(&g_quick_lookup_list);
        g_quick_lookup_is_uptodate = 1;
    }
    //TODO: Avoid linear search.
    for(unsigned int i = 0; i < g_quick_lookup_list.num_entries; i++)
    {
        rtree_key_map_entry_t * entry = &g_quick_lookup_list.entries[i];
        if(0 == strncmp(key, entry->key, entry->length))
        {
            *value = entry->value;
            ret = 0;
            break;
        }
    }
    return ret;
}

void rtree_dump_quick_match_expressions()
{
    rtLog_Info("Begin quick match expressions:");
    
    if(0 == g_quick_lookup_is_uptodate)
    {
        g_quick_lookup_list.num_entries = 0;
        rtree_generate_quick_match_expressions(&g_quick_lookup_list);
        sort_bundle(&g_quick_lookup_list);
        g_quick_lookup_is_uptodate = 1;
    }
    for(unsigned int i = 0; i < g_quick_lookup_list.num_entries; i++)
    {
        rtree_key_map_entry_t * entry = &g_quick_lookup_list.entries[i];
        rtLog_Info("%d. %s", i, entry->key);
    }
    rtLog_Info("End quick match expressions:");
}
#ifdef ENABLE_ROUTER_BENCHMARKING
int rtree_lookup_value(const char * key, void **value)
{
    if(ROUTING_STRATEGY_OPTIMIZED_V2 == g_routing_strategy)
        return rtree_get_value_o2(key, value);
    else if(ROUTING_STRATEGY_OPTIMIZED_V1 == g_routing_strategy)
        return rtree_get_value_o1(key, value);
    else
        return rtree_get_value(key, value);
}
#endif

void remove_root_node(node_t * node)
{
    /*This node shouldn't have any children at this point.*/
    if(0 != node->num_children)
        rtLog_Warn("Attempting to remove root node %s with %d children.", node->name, node->num_children);
    rtLog_Debug("Root node %s is being decommissioned", node->name);
    node->name_length = 0;
    free(node->name);
    node->name = NULL;
    node->value = NULL;
    g_num_root_nodes--;
}

void free_dynamic_node(node_t * node)
{
    if(0 != node->num_children)
        rtLog_Warn("Attempting to remove dynamic node %s with %d children.", node->name, node->num_children);
    rtLog_Debug("Dynamic node %s is being decommissioned", node->name);
    free(node->name);
    free(node);
    g_num_dynamic_nodes--;
}

void remove_node(node_t * node)
{
    rtLog_Debug("Request to remove node %s", node->name);
    /*Iteratively remove all intermediate nodes that lead nowhere but to this node.*/
    node_t * parent;
    do
    {
        parent = node->parent;
        if(NULL == parent)
        {
            remove_root_node(node);
            break;
        }
        detach_child(parent, node);
        free_dynamic_node(node);
        revise_upstream_optimization(parent);
        node = parent;
    }
    while(0 == parent->num_children);
}

int rtree_remove_value(const char * key)
{
    int ret = 0;
    unsigned int bytes_consumed = 0;

    node_t *node = NULL;
    rtLog_Debug("Remove value for key %s", key);
    if((node = search_matching_root_node(key, &bytes_consumed)) != NULL)
    {
        key += bytes_consumed;
        if('\0' == *key)
        {
            rtLog_Debug("Found complete match in root node %s", node->name);
            remove_node(node);
        }
        else
        {
            node_t * child = NULL;
            while((child = get_matching_child(node, key, &bytes_consumed)) != NULL)
            {
                rtLog_Debug("Traversing node %s", child->name);
                key += bytes_consumed;
                node = child;
            }
            if('\0' == *key)
            {
                rtLog_Debug("Reached leaf node %s", node->name);
                remove_node(node);
            }
            else
            {
                rtLog_Error("Cannot find path to key");
                ret = -1;
            }
        }

        /*Now that we've located the node to remove, handle it.*/

    }
    else
    {
        rtLog_Error("Couldn't locate matching root node for key %s", key);
        ret = -1;
    }
    return ret;
}

static void node_trace(node_t * node, unsigned int *level)
{
    *level += 1;
    printf("%*s", *level * 4, "");
    printf("<%s> children:%d optimized? %s\n", node->name, node->num_children, (NULL == node->value ? "N" : "Y"));

    for(unsigned int i = 0; i < node->num_children; i++)
        node_trace(node->children_head[i], level);
    *level -= 1;
}

void rtree_traverse_and_log()
{
    printf("Begin routing table trace.\n");
    unsigned int level = 0;
    for(int i = 0; i < ROOT_LIST_LENGTH; i++)
    {
        if(NULL != root_list[i].name)
            node_trace(&root_list[i], &level);
    }
    printf("End routing table trace.\n");
}


static void rtree_generate_quick_match_expressions_for_node(node_t * node, rtree_key_bundle_t * bundle, rtree_key_bundle_type_t type)
{
    if(NULL != node->value)
    {
        if(TRIMMED_LIST == type)
        {
            for(unsigned int i = 0; i < bundle->num_entries; i++)
                if(node->value == bundle->entries[i].value)
                    return;
        }
        rtree_add_node_to_bundle(bundle, node);
        rtLog_Debug("Generated quick match expression #%d %s", bundle->num_entries, bundle->entries[bundle->num_entries - 1].key);
    }
    else
    {
        for(unsigned int i = 0; i < node->num_children; i++)
            rtree_generate_quick_match_expressions_for_node(node->children_head[i], bundle, type);
    }
}

void rtree_generate_quick_match_expressions(rtree_key_bundle_t * bundle)
{
    for(int i = 0; i < ROOT_LIST_LENGTH; i++)
    {
        if(NULL != root_list[i].name)
            rtree_generate_quick_match_expressions_for_node(&root_list[i], bundle, COMPLETE_LIST);
    }
}

static int rtree_get_dynamic_nodes_matching_value(node_t *node, rtree_key_bundle_t * bundle, void * value)
{
    int ret = 0;
    if(NULL != node->value)
    {
        if(0 != g_comparison_fn(node->value, value))
            return ret;
        else if(0 == node->num_children)
            rtree_add_node_to_bundle(bundle, node);
        else
        {
            for(unsigned int i = 0; i < node->num_children; i++)
                rtree_get_dynamic_nodes_matching_value(node->children_head[i], bundle, value); //A comparison of 'value' is redudant here, but going with this to reuse code for now.
        }
    }
    else
    {
        for(unsigned int i = 0; i < node->num_children; i++)
            rtree_get_dynamic_nodes_matching_value(node->children_head[i], bundle, value);
    }
    return ret;
}


int rtree_get_all_nodes_matching_value(rtree_key_bundle_t * bundle, void * value)
{
    int ret = 0;
    int node_removed = 0;
    for(unsigned int i = 0; i < ROOT_LIST_LENGTH; i++)
    {
        if(NULL != root_list[i].name)
        {
            if(NULL == root_list[i].value)
            {
                for(unsigned int j = 0; j < root_list[i].num_children; j++)
                    rtree_get_dynamic_nodes_matching_value(root_list[i].children_head[j], bundle, value);
            }
            else if(0 == g_comparison_fn(value, root_list[i].value))
                rtree_add_node_to_bundle(bundle, &root_list[i]);
        }
    }
    return ret;
}

int remove_subtree(node_t * node)
{
    rtLog_Debug("Remove sub-tree beginning at %s", node->name);

    for(unsigned int i = 0; i < node->num_children; i++)
    {
        remove_subtree(node->children_head[i]);
        i--; //Because we removed a child in the above step, the list children_head has now changed and shrunk by one position.
    }

    detach_child(node->parent, node);
    free_dynamic_node(node);
    return 0;
}

//return 1 if node is removed.
int remove_matching_dynamic_subtree(node_t * node, void * data)
{
    int ret = 0;
    if(NULL != node->value)
    {
        if(0 == g_comparison_fn(node->value, data))
        {
            rtLog_Debug("The dynamic subtree starting at %s matches. Will be removed.", node->name);
            node_t *parent = node->parent;
            remove_subtree(node);
            revise_upstream_optimization(parent);
            ret = 1;
        }
    }
    else
    {
        int node_removed = 0;
        for(unsigned int i = 0; i < node->num_children; i++)
        {
            node_removed = remove_matching_dynamic_subtree(node->children_head[i], data);
            if(1 == node_removed)
                i--;
        }
    }
    return ret;
}

int rtree_remove_nodes_matching_value(void * data)
{
    int ret = 0;
    int node_removed = 0;
    for(unsigned int i = 0; i < ROOT_LIST_LENGTH; i++)
    {
        if(NULL != root_list[i].name)
        {
            /*Check whether we need to go deeper to get a decision*/
            if(NULL == root_list[i].value)
            {
                //rtLog_Debug("Need to go deeper than key <%s>. One or more sub-trees might match.", root_list[i].name);
                for(unsigned int j = 0; j < root_list[i].num_children; j++)
                {
                    node_removed = remove_matching_dynamic_subtree(root_list[i].children_head[j], data);
                    if(1 == node_removed)
                        j--;//Because we removed a child in the above step, the list children_head has now changed and shrunk by one position.
                }
            }
            else if(0 == g_comparison_fn(data, root_list[i].value))
            {
                rtLog_Debug("Root node %s is a match. Wipe out all children.", root_list[i].name);
                while(0 != root_list[i].num_children)
                    remove_subtree(root_list[i].children_head[0]);
                remove_root_node(&root_list[i]);
            }
            //else
                //rtLog_Debug("Root node %s is not a match. Move on.", root_list[i].name);

        }
    }
    g_quick_lookup_is_uptodate = 0;
    return ret;
}

int rtree_get_uniquely_resolvable_endpoints_for_expression(const char *expression, rtree_key_bundle_t * bundle)
{
    int ret = 0;
    unsigned int bytes_consumed = 0;

    node_t *parent = NULL;
    rtLog_Debug("Getting uniquely resolvable subkeys for %s", expression);
    if((parent = search_matching_root_node(expression, &bytes_consumed)) != NULL)
    {
        expression += bytes_consumed;
        if('\0' == *expression)
        {
            rtLog_Debug("Found complete match in root node %s", parent->name);
            rtree_generate_quick_match_expressions_for_node(parent, bundle, TRIMMED_LIST);
        }
        else
        {
            node_t * child = NULL;
            while((child = get_matching_child(parent, expression, &bytes_consumed)) != NULL)
            {
                //rtLog_Debug("Traversing node %s", child->name);
                expression += bytes_consumed;
                parent = child;
            }
            if('\0' == *expression)
            {
                //rtLog_Debug("Reached leaf node %s", parent->name);
                rtree_generate_quick_match_expressions_for_node(parent, bundle, TRIMMED_LIST);
            }
            else
            {
                rtLog_Warn("Cannot find path to key");
                ret = -1;
            }
        }

    }
    else
    {
        rtLog_Error("Couldn't locate matching root node for key %s", expression);
        ret = -1;
    }
    return ret;
}

int rtree_initialize_key_bundle(rtree_key_bundle_t * bundle, unsigned int capacity)
{
    int ret = 0;
    bundle->capacity = capacity;
    bundle->entries = (rtree_key_map_entry_t *)malloc(bundle->capacity * sizeof(rtree_key_map_entry_t));
    bundle->num_entries = 0;
    if(NULL == bundle->entries)
    {
        rtLog_Error("Memory reallocation for key bundle failed.");
        ret = -1;
    }
    return ret;
}
