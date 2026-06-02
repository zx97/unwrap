#include "unwrap_v1_state.h"
#include "diana_grammar.h"
#include <algorithm>
#include <cstring>

V1State::V1State() {}

int V1State::get_node_type(int ni) {
  if (ni == 0) return 0;
  auto it = node_tbl.find(ni);
  return it != node_tbl.end() ? it->second : 0;
}

int V1State::get_line(int ni) {
  if (ni == 0) return 0;
  auto it = line_tbl.find(ni);
  return it != line_tbl.end() ? it->second : 0;
}

int V1State::get_column(int ni) {
  if (ni == 0) return 0;
  auto it = column_tbl.find(ni);
  return it != column_tbl.end() ? it->second : 0;
}

std::string V1State::get_lexical_str(int li) {
  if (li == 0) return {};
  auto it = lexical_str_tbl.find(li);
  return it != lexical_str_tbl.end() ? it->second : "";
}

bool V1State::is_attr_in_version(int nti, int ap, int wv) {
  for (int i = 0; ; i++) {
    if (attr_vsn_tbl[i].node_type_id == 0) break;
    if (attr_vsn_tbl[i].node_type_id == nti && attr_vsn_tbl[i].attr_pos == ap)
      return attr_vsn_tbl[i].introduced <= wv;
  }
  return true;
}

int V1State::get_attr_val(int ni, int ap) {
  if (ni == 0 || ap == 0) return 0;
  int nt = get_node_type(ni);
  if (nt == 0) return 0;
  if (!is_attr_in_version(nt, ap, wrap_version)) return 0;
  auto ait = attr_ref_tbl.find(ni);
  if (ait == attr_ref_tbl.end()) return invalid_ref("node/attr", ni, ap);
  int ai = ait->second + ap - 1;
  auto att = attr_tbl.find(ai);
  if (att == attr_tbl.end()) return invalid_ref("node/attr", ni, ap);
  return att->second;
}

int V1State::get_subnode_idx(int ni, int ap) {
  int v = get_attr_val(ni, ap);
  if (v != 0 && node_tbl.find(v) == node_tbl.end()) return invalid_ref("sub-node", ni, ap, v);
  return v;
}

int V1State::get_lexical_idx(int ni, int ap) {
  int v = get_attr_val(ni, ap);
  if (v != 0 && lexical_str_tbl.find(v) == lexical_str_tbl.end()) return invalid_ref("lexical", ni, ap, v);
  return v;
}

int V1State::get_list_idx(int ni, int ap) {
  int v = get_attr_val(ni, ap);
  if (v != 0 && as_list_tbl.find(v) == as_list_tbl.end()) return invalid_ref("list", ni, ap, v);
  return v;
}

int V1State::get_list_len(int li) {
  if (li == 0) return 0;
  auto it = as_list_tbl.find(li);
  return it != as_list_tbl.end() ? it->second : 0;
}

int V1State::get_list_element(int li, int lp) {
  if (li == 0 || lp == 0) return 0;
  auto it = as_list_tbl.find(li + lp);
  if (it == as_list_tbl.end()) return invalid_ref("list element", li, lp);
  int n = it->second;
  if (node_tbl.find(n) == node_tbl.end()) return invalid_ref("list element", li, lp, n);
  return n;
}

int V1State::get_list_element(int ni, int ap, int lp) {
  return get_list_element(get_list_idx(ni, ap), lp);
}

int V1State::get_subnode_type(int ni, int ap) {
  return get_node_type(get_subnode_idx(ni, ap));
}

std::string V1State::get_lexical_str(int ni, int ap) {
  return get_lexical_str(get_lexical_idx(ni, ap));
}

int V1State::get_list_len(int ni, int ap) {
  return get_list_len(get_list_idx(ni, ap));
}

int V1State::invalid_ref(const std::string&, int, int, int) {
  invalid_ref_f = true;
  return 0;
}

void V1State::bit_check(int& v, int b, const std::string& t, int s) {
  if ((v & b) == b) { do_static(t, s); v &= ~b; }
}

void V1State::stack_push(int ni) {
  stack.push_back({ni,0,0,0});
  active_nodes[ni] = true;
}

void V1State::stack_pop() {
  if (!stack.empty()) {
    active_nodes.erase(stack.back().node_idx);
    stack.pop_back();
  }
}

void V1State::stack_set(int ap, int lp, int ll) {
  if (!stack.empty()) {
    auto& s = stack.back();
    s.attr_pos = ap; s.list_pos = lp; s.list_len = ll;
  }
}

void V1State::stack_reset() { stack.clear(); active_nodes.clear(); }

StackRec V1State::get_parent(int l) {
  if ((int)stack.size() <= l) return {};
  return stack[stack.size()-1-l];
}

int V1State::get_parent_idx(int l) {
  if ((int)stack.size() <= l) return 0;
  return stack[stack.size()-1-l].node_idx;
}

int V1State::get_parent_type(int l) {
  return get_node_type(get_parent_idx(l));
}

void V1State::output(const std::string& t) { buffer += t; }

void V1State::emit_init() {
  unwrapped.clear(); buffer.clear();
  curr_line = 1; curr_column = 1;
  emit_line = 1; emit_column = 1;
  token_cnt = 0;
  prior_buffer.clear(); next_buffer.clear();
  line_gap_limit2 = 1000000; line_soft_limit2 = 0;
  last_special_f = true; exact_text_f = false;
}

void V1State::emit_flush() {
  if (!buffer.empty()) { unwrapped += buffer; buffer.clear(); }
}

void V1State::emit_pos(int ni) {
  if (ni != 0) { emit_line = get_line(ni); emit_column = get_column(ni); }
}
