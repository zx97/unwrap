#pragma once
#include <string>
#include <vector>
#include <map>
#include "diana_nodes.h"

struct StackRec {
  int node_idx = 0;
  int attr_pos = 0;
  int list_pos = 0;
  int list_len = 0;
};

class V1State {
public:
  // Tables
  std::map<int,int> lexical_tbl;
  std::map<int,int> node_tbl;
  std::map<int,int> attr_ref_tbl;
  std::map<int,int> column_tbl;
  std::map<int,int> line_tbl;
  std::map<int,int> attr_tbl;
  std::map<int,int> as_list_tbl;
  std::map<int,std::string> lexical_str_tbl;

  int wrap_version = 0, root_idx = 0;
  std::string unit_type, unit_name;
  int header_start = 0, header_end = 0;

  // Emitter state
  std::string unwrapped, buffer, prior_buffer, next_buffer;
  int curr_line = 1, curr_column = 1;
  int emit_line = 1, emit_column = 1;
  int token_cnt = 0;
  bool last_special_f = true, exact_text_f = false;
  bool always_space_f = false, runnable_f = true;
  int line_gap_limit2 = 1000000, line_soft_limit2 = 0;
  int quote_limit = 0, line_endings = 0;

  // Stack
  std::vector<StackRec> stack;
  std::map<int,bool> active_nodes;

  // Flags
  bool invalid_ref_f = false, unknown_attr_f = false;
  bool infinite_loop_f = false, meta_mismatch_f = false;

  // Grammar version tracking
  std::vector<int> definitive_grammars = {8003000,8105000,8106000,9000000,9200000};

  V1State();

  // Table access
  int get_node_type(int ni);
  int get_line(int ni);
  int get_column(int ni);
  std::string get_lexical_str(int li);
  int get_attr_val(int ni, int ap);
  int get_subnode_idx(int ni, int ap);
  int get_lexical_idx(int ni, int ap);
  int get_list_idx(int ni, int ap);
  int get_list_len(int li);
  int get_list_element(int li, int lp);
  int get_list_element(int ni, int ap, int lp);
  int get_subnode_type(int ni, int ap);
  std::string get_lexical_str(int ni, int ap);
  int get_list_len(int ni, int ap);
  int invalid_ref(const std::string& r, int i, int p, int v=0);
  bool is_attr_in_version(int nti, int ap, int wv);

  // Bit ops
  bool bit_set(int v, int b) { return (v&b)==b; }
  void bit_clear(int& v, int b) { v&=~b; }
  void bit_check(int& v, int b, const std::string& t, int s=0);

  // Stack
  void stack_push(int ni);
  void stack_pop();
  void stack_set(int ap, int lp=0, int ll=0);
  void stack_reset();
  StackRec get_parent(int l=1);
  int get_parent_idx(int l=1);
  int get_parent_type(int l=1);

  // Emitter
  void output(const std::string& t);
  void emit_init();
  void emit_flush();
  void emit_pos(int ni);
  void emit(const std::string& t, int s=0, int ni=0);
  void do_static(const std::string& t, int s=0, int ni=0, int ap=0);

  // Attribute helpers
  void do_symbol(int ni, int ap, bool qf=false);
  void do_string(int ni, int ap, bool nf=false);
  void do_lexical(int ni, int ap, const std::string& p="", const std::string& sf="");
  void do_numeric(int ni, int ap);
  void do_meta(int, int) {}
  void do_unknown(int ni, int ap=0);
  void do_as_list(int ni, int ap, const std::string& p="", const std::string& sep="", const std::string& sf="", const std::string& delim="");
  void do_subnode(int ni, int ap, const std::string& p="", const std::string& sf="");

  // Special cases + node dispatcher
  std::string get_std_name(int ni, int ap);
  bool do_special_cases(int p_ni);
  void do_node(int p_ni);

  // Parser
  void parse_tree(const std::string& src);

  // Entry point
  std::string unwrap(const std::string& src);
};
