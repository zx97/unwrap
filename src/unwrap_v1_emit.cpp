#include "unwrap_v1_state.h"
#include <algorithm>
#include <cctype>

void V1State::emit(const std::string& text, int special, int node_idx) {
  int l_line, l_column;
  if (node_idx != 0) {
    l_line = get_line(node_idx);
    l_column = get_column(node_idx);
  } else if (special == 0 || special == 2 || special == 3) { // S_CURRENT or S_EXACT
    l_line = emit_line; l_column = emit_column;
    emit_line = 1; emit_column = 1;
  } else if (special == 5) { // S_INLINE
    l_line = 1; l_column = 1;
  } else {
    l_line = 1; l_column = 1;
  }

  if (special == 4) { // S_BEFORE_NEXT
    if (!next_buffer.empty() || (!text.empty() && text != ")" && text != ","))
      next_buffer += (text != ")" && text != "," ? " " : "") + text;
    else next_buffer += text;
    return;
  }
  if (special == 6 || special == 7) { // S_END / S_END_EXPR
    prior_buffer += next_buffer + "\n" + std::string(l_column > 0 ? l_column - 1 : 0, ' ') + text;
    next_buffer.clear();
    return;
  }

  if (l_line == 1 && l_column == 1 && token_cnt != 0) {
    if (text == ";") {
      prior_buffer += next_buffer + ";";
      next_buffer.clear();
    } else if (special == 3) { // S_EXACT
      exact_text_f = true;
      std::string t = text;
      for (auto& c : t) if (c == ' ') c = 1;
      next_buffer += (next_buffer.empty() ? (prior_buffer.empty() ? "" : " ") : " ") + t;
    } else {
      std::string pre = (text != ")" && text != "," ? " " : "");
      if (!next_buffer.empty()) next_buffer += pre + text;
      else prior_buffer += pre + text;
    }
    return;
  }

  token_cnt++;

  if (line_soft_limit2 > 0 && l_column > line_soft_limit2) {
    if (l_line > curr_line) l_column = 7;
    else l_column = curr_column;
  }

  if (!prior_buffer.empty() || !next_buffer.empty()) {
    if (prior_buffer.empty()) {
      prior_buffer = (curr_line < l_line ? std::string(std::min(l_line - curr_line, line_gap_limit2), '\n') : "");
    } else if (curr_line > l_line) {
      for (auto& c : prior_buffer) if (c == '\n') c = ' ';
      next_buffer = prior_buffer + " " + next_buffer;
      prior_buffer.clear();
    } else if (curr_line == l_line) {
      for (auto& c : prior_buffer) if (c == '\n') c = ' ';
      int needed = l_column - curr_column - (int)prior_buffer.size() - (int)next_buffer.size() - 1;
      next_buffer = prior_buffer + (needed > 0 ? std::string(needed, ' ') : " ") + next_buffer;
      prior_buffer.clear();
    } else {
      prior_buffer += '\n';
      int nl = 0; for (auto c : prior_buffer) if (c == '\n') nl++;
      int diff = std::min(l_line - curr_line - nl, line_gap_limit2);
      if (diff > 0) prior_buffer += std::string(diff, '\n');
      else if (diff < 0) {
        for (int i = 0; i < -diff; i++) {
          auto p = prior_buffer.find('\n');
          if (p != std::string::npos) {
            auto e = prior_buffer.find_first_not_of(' ', p+1);
            if (e == std::string::npos) prior_buffer.erase(p);
            else prior_buffer.replace(p, e-p, " ");
          }
        }
      }
    }

    if (!prior_buffer.empty()) { curr_line = l_line; curr_column = 1; }

    if (curr_line > l_line) {
      std::string nb;
      bool wsp = false;
      for (auto c : next_buffer) { if (c == ' ') { if (!wsp) { nb += ' '; wsp = true; } } else { nb += c; wsp = false; } }
      curr_column += (int)nb.size();
      next_buffer = nb;
    } else if (next_buffer.empty()) {
      if (l_column > curr_column) { next_buffer = std::string(l_column - curr_column, ' '); curr_column = l_column; }
    } else {
      next_buffer += ' ';
      int diff = l_column - curr_column - (int)next_buffer.size();
      if (diff > 0) { next_buffer = std::string(diff, ' ') + next_buffer; curr_column = l_column; }
      else if (diff < 0) {
        while (diff < 0 && !next_buffer.empty()) {
          auto p = next_buffer.find_last_of(' ');
          if (p == std::string::npos) break;
          next_buffer.erase(p, 1); diff++;
        }
        curr_column += (int)next_buffer.size();
      } else curr_column = l_column;
    }

    if (exact_text_f) {
      for (auto& c : prior_buffer) if (c == 1) c = ' ';
      for (auto& c : next_buffer) if (c == 1) c = ' ';
      exact_text_f = false;
    }
    output(prior_buffer + next_buffer);
    prior_buffer.clear(); next_buffer.clear();
  } else {
    std::string spacer;
    if (l_line > curr_line) {
      spacer = std::string(std::min(l_line - curr_line, line_gap_limit2), '\n') + std::string(l_column > 0 ? l_column - 1 : 0, ' ');
      curr_line = l_line; curr_column = l_column;
    } else if (l_line == curr_line && l_column > curr_column) {
      spacer = std::string(l_column - curr_column, ' ');
      curr_column = l_column;
    } else if (always_space_f) {
      if (token_cnt > 1) { spacer = ' '; curr_column++; }
    } else {
      if (!last_special_f && !text.empty()) {
        char fc = text[0];
        if (isalnum((unsigned char)fc) || fc == '_' || fc == '$' || fc == '#' || fc == '"') {
          spacer = ' '; curr_column++;
        }
      }
    }
    output(spacer);
  }

  output(text);

  if (!always_space_f && !text.empty()) {
    char lc = text.back();
    last_special_f = !(isalnum((unsigned char)lc) || lc == '_' || lc == '$' || lc == '#' || lc == '"');
  }

  auto nl = text.find('\n');
  if (nl == std::string::npos) curr_column += (int)text.size();
  else {
    int nn = 0; for (auto c : text) if (c == '\n') nn++;
    curr_line += nn;
    curr_column = (int)text.size() - (int)text.rfind('\n');
  }
}

void V1State::do_static(const std::string& text, int special, int node_idx, int attr_pos) {
  if (text.empty()) return;
  if (attr_pos != 0) emit(text, special, get_subnode_idx(node_idx, attr_pos));
  else emit(text, special, node_idx);
}

void V1State::do_symbol(int ni, int ap, bool qf) {
  int li = get_lexical_idx(ni, ap);
  if (li == 0) return;
  auto sym = get_lexical_str(li);
  if (sym.empty()) return;
  if (!qf && sym[0] == ' ') { emit("\"" + sym + "\"", 3); }
  else if (qf) { emit("\"" + sym + "\"", 3); }
  else if (sym == "INTERVAL DAY TO SECOND" || sym == "INTERVAL YEAR TO MONTH") {
    emit("INTERVAL", 4); emit(sym.substr(10));
  } else emit(sym);
}

void V1State::do_string(int ni, int ap, bool nf) {
  int li = get_lexical_idx(ni, ap);
  if (li == 0) return;
  auto str = get_lexical_str(li);
  int qc = 0; for (auto c : str) if (c == '\'') qc++;
  std::string r;
  if (quote_limit > 0 && qc > quote_limit && str.find("]'") == std::string::npos) {
    r = (nf ? "n" : "") + std::string("q'[") + str + "]'";
  } else {
    std::string esc;
    for (auto c : str) { if (c == '\'') esc += "''"; else esc += c; }
    r = (nf ? "N" : "") + std::string("'") + esc + "'";
  }
  emit(r, 3);
}

void V1State::do_lexical(int ni, int ap, const std::string& p, const std::string& sf) {
  int li = get_lexical_idx(ni, ap);
  if (li == 0) return;
  emit(p + get_lexical_str(li) + sf, 3);
}

void V1State::do_numeric(int ni, int ap) { do_lexical(ni, ap); }

void V1State::do_unknown(int ni, int ap) {
  if (ap == 0) { unknown_attr_f = true; emit(" {{ unknown node }} "); }
  else {
    int v = get_attr_val(ni, ap);
    if (v != 0) { unknown_attr_f = true; emit(" {{ unknown attr }} "); }
  }
}

void V1State::do_as_list(int ni, int ap, const std::string& p, const std::string& sep, const std::string& sf, const std::string& delim) {
  int li = get_list_idx(ni, ap);
  if (li == 0) return;
  int ll = get_list_len(li);
  bool first = true;
  for (int pos = 1; pos <= ll; pos++) {
    stack_set(ap, pos, ll);
    int child = get_list_element(li, pos);
    if (child == 0) continue;
    if (first) { do_static(p); first = false; }
    else do_static(!delim.empty() ? delim : (sep.empty() ? "" : sep));
    do_node(child);
    if (!delim.empty()) do_static(delim);
  }
  if (!first) do_static(sf);
}

void V1State::do_subnode(int ni, int ap, const std::string& p, const std::string& sf) {
  int child = get_subnode_idx(ni, ap);
  if (child == 0) return;
  stack_set(ap);
  if (!p.empty()) do_static(p, 4);
  do_node(child);
  if (!sf.empty()) do_static(sf);
}
