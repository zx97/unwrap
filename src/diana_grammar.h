#pragma once
#include "diana_nodes.h"

struct AttrTypeInfo { int id; const char* name; const char* base_type; const char* ref_type; };
extern const AttrTypeInfo attr_type_tbl[191];

struct AttrVersionEntry { int node_type_id; int attr_pos; int introduced; };
extern const AttrVersionEntry attr_vsn_tbl[];

// Returns pointer to start of attr list for given node type, and sets count
const int* get_node_attr_list(int node_type, int& count);
