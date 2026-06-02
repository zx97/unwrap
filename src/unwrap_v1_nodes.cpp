#include "unwrap_v1_state.h"
#include "diana_grammar.h"
#include <cctype>

std::string V1State::get_std_name(int ni, int ap) {
  int idx = get_subnode_idx(ni, ap);
  if (get_node_type(idx) == DI_U_NAM) {
    if (!bit_set(get_attr_val(idx, 4), 1))
      return get_lexical_str(idx, 1);
  }
  return {};
}

bool V1State::do_special_cases(int p_ni) {
  int nt = get_node_type(p_ni);
  int pt = get_parent_type();

  // Simple leaf nodes
  if (nt == D_ALL) { do_static("*"); return true; }
  if (nt == D_AND_TH) { do_static("AND", 5); return true; }

  // D_AGGREG - parenthesized or not
  if (nt == D_AGGREG) {
    if (pt == Q_EXEC_IMMEDIATE || pt == Q_FETCH_)
      do_as_list(p_ni, 1, "", ",", "");
    else if (pt == Q_INSERT && get_list_len(p_ni, 1) == 1)
      do_as_list(p_ni, 1, "", ",", "");
    else
      do_as_list(p_ni, 1, "(", ",", ")");
    do_unknown(p_ni, 2); do_unknown(p_ni, 3); do_unknown(p_ni, 4);
    return true;
  }

  // D_ALTERN - WHEN / ELSE clause
  if (nt == D_ALTERN) {
    if (get_subnode_idx(p_ni, 1) == 0) do_static("ELSE", 1, p_ni, 2);
    else {
      do_static("WHEN");
      do_subnode(p_ni, 1);
      do_static("THEN", 1, p_ni, 2);
    }
    do_subnode(p_ni, 2);
    do_unknown(p_ni, 3); do_unknown(p_ni, 4); do_meta(p_ni, 5);
    return true;
  }

  // D_APPLY
  if (nt == D_APPLY) { do_subnode(p_ni, 1); do_subnode(p_ni, 2); return true; }

  // D_ARRAY
  if (nt == D_ARRAY) {
    int fl = get_attr_val(p_ni, 5);
    if (fl == 0) { do_static("TABLE OF"); do_subnode(p_ni, 2); do_subnode(p_ni, 1); }
    else if (fl == 1) { do_static("TABLE OF"); do_subnode(p_ni, 2); }
    else if (fl == 2) {
      do_static("VARRAY"); do_static("(", 1, p_ni, 1);
      do_subnode(p_ni, 1); do_static(") OF"); do_subnode(p_ni, 2);
    } else do_unknown(p_ni, 5);
    do_unknown(p_ni, 3); do_unknown(p_ni, 4); do_unknown(p_ni, 6);
    return true;
  }

  // D_ASSIGN
  if (nt == D_ASSIGN) {
    do_subnode(p_ni, 1); do_static(":=", 4); do_subnode(p_ni, 2);
    do_unknown(p_ni, 3); do_meta(p_ni, 4);
    return true;
  }

  // D_ASSOC
  if (nt == D_ASSOC) { do_subnode(p_ni, 1); do_static("=>", 4); do_subnode(p_ni, 2); return true; }

  // D_ATTRIB - % attribute
  if (nt == D_ATTRIB) {
    do_subnode(p_ni, 1); do_static("%"); do_subnode(p_ni, 2);
    do_unknown(p_ni, 3); do_unknown(p_ni, 4); do_unknown(p_ni, 5);
    return true;
  }

  // D_BINARY
  if (nt == D_BINARY) {
    do_subnode(p_ni, 1);
    int op = get_subnode_idx(p_ni, 2);
    if (get_node_type(op) == DI_U_NAM) {
      auto s = get_lexical_str(op, 1);
      if (s == "NOT_LIKE") s = "NOT LIKE";
      do_static(s, 1, p_ni, 2);
    }
    do_subnode(p_ni, 3);
    do_unknown(p_ni, 4); do_unknown(p_ni, 5);
    return true;
  }

  // D_BLOCK
  if (nt == D_BLOCK) {
    do_static("DECLARE"); do_as_list(p_ni, 1, "", "", ";", ";");
    do_static("BEGIN"); do_as_list(p_ni, 2); do_subnode(p_ni, 3);
    do_unknown(p_ni, 4); do_unknown(p_ni, 5); do_unknown(p_ni, 6);
    do_unknown(p_ni, 7); do_unknown(p_ni, 8); do_unknown(p_ni, 9);
    do_meta(p_ni, 10); do_unknown(p_ni, 11); do_unknown(p_ni, 12);
    do_unknown(p_ni, 13); do_unknown(p_ni, 14); do_unknown(p_ni, 15);
    do_unknown(p_ni, 16);
    return true;
  }

  // D_CASE
  if (nt == D_CASE) {
    do_static("CASE"); do_subnode(p_ni, 1); do_as_list(p_ni, 2);
    do_static("END", 6, p_ni, 2);
    do_unknown(p_ni, 3); do_meta(p_ni, 4); do_unknown(p_ni, 5);
    do_unknown(p_ni, 6); do_unknown(p_ni, 7);
    return true;
  }

  // D_C_ATTR
  if (nt == D_C_ATTR) { do_subnode(p_ni, 1); do_static(":=", 4); do_subnode(p_ni, 2); do_unknown(p_ni, 3); do_unknown(p_ni, 4); return true; }

  // D_COMP_R - component with range
  if (nt == D_COMP_R) { do_subnode(p_ni, 1); do_static("(", 1, p_ni, 2); do_subnode(p_ni, 2); do_subnode(p_ni, 3); do_static(")"); return true; }

  // D_COMP_U - root compilation unit
  if (nt == D_COMP_U) {
    do_meta(p_ni, 1); do_meta(p_ni, 2); do_meta(p_ni, 3);
    do_meta(p_ni, 4); do_meta(p_ni, 5); do_meta(p_ni, 6);
    do_meta(p_ni, 7); do_meta(p_ni, 8); do_meta(p_ni, 9);
    return true;
  }

  // D_COMPIL
  if (nt == D_COMPIL) { do_as_list(p_ni, 1); do_meta(p_ni, 2); return true; }

  // D_COND_C
  if (nt == D_COND_C) { do_subnode(p_ni, 1); do_as_list(p_ni, 2); do_unknown(p_ni, 3); do_meta(p_ni, 4); return true; }

  // D_CONSTA
  if (nt == D_CONSTA) { do_as_list(p_ni, 1); do_subnode(p_ni, 2); do_subnode(p_ni, 3); do_meta(p_ni, 4); return true; }

  // D_CONSTR - type constraint (handles special timestamp/interval cases)
  if (nt == D_CONSTR) {
    auto nm = get_std_name(p_ni, 1);
    if (!nm.empty()) {
      if (nm == "TIMESTAMP WITH TIME ZONE") {
        do_static("TIMESTAMP"); do_subnode(p_ni, 2); do_static("WITH TIME ZONE");
        do_unknown(p_ni, 3); do_unknown(p_ni, 4); do_unknown(p_ni, 5);
        do_unknown(p_ni, 6); do_unknown(p_ni, 7); do_unknown(p_ni, 8); do_unknown(p_ni, 9);
        return true;
      }
      if (nm == "TIMESTAMP WITH LOCAL TIME ZONE") {
        do_static("TIMESTAMP"); do_subnode(p_ni, 2); do_static("WITH LOCAL TIME ZONE");
        do_unknown(p_ni, 3); do_unknown(p_ni, 4); do_unknown(p_ni, 5);
        do_unknown(p_ni, 6); do_unknown(p_ni, 7); do_unknown(p_ni, 8); do_unknown(p_ni, 9);
        return true;
      }
      if (nm == "INTERVAL YEAR TO MONTH" && get_subnode_type(p_ni, 2) == D_RANGE) {
        int ci = get_subnode_idx(p_ni, 2);
        do_static("INTERVAL", 4); do_static("YEAR"); do_subnode(ci, 1, "(", ")"); do_static("TO MONTH");
        do_unknown(p_ni, 3); do_unknown(p_ni, 4); do_unknown(p_ni, 5);
        do_unknown(p_ni, 6); do_unknown(p_ni, 7); do_unknown(p_ni, 8); do_unknown(p_ni, 9);
        return true;
      }
      if (nm == "INTERVAL DAY TO SECOND" && get_subnode_type(p_ni, 2) == D_RANGE) {
        int ci = get_subnode_idx(p_ni, 2);
        do_static("INTERVAL", 4); do_static("DAY"); do_subnode(ci, 1, "(", ")"); do_static("TO SECOND"); do_subnode(ci, 2, "(", ")");
        do_unknown(p_ni, 3); do_unknown(p_ni, 4); do_unknown(p_ni, 5);
        do_unknown(p_ni, 6); do_unknown(p_ni, 7); do_unknown(p_ni, 8); do_unknown(p_ni, 9);
        return true;
      }
    }
    int ci = get_subnode_idx(p_ni, 2);
    if (ci != 0) { do_subnode(p_ni, 1); do_static("("); do_subnode(p_ni, 2); do_static(")"); }
    else do_subnode(p_ni, 1);
    do_unknown(p_ni, 3); do_unknown(p_ni, 4); do_unknown(p_ni, 5);
    do_unknown(p_ni, 6); do_unknown(p_ni, 7); do_unknown(p_ni, 8); do_unknown(p_ni, 9);
    return true;
  }

  // D_CONTEX
  if (nt == D_CONTEX) { do_as_list(p_ni, 1); return true; }

  // D_CONVER
  if (nt == D_CONVER) { do_subnode(p_ni, 1); do_static("("); do_subnode(p_ni, 2); do_static(")"); do_unknown(p_ni, 3); do_unknown(p_ni, 4); return true; }

  // D_DECL
  if (nt == D_DECL) {
    do_static("DECLARE"); do_as_list(p_ni, 1, "", "", ";", ";");
    do_static("BEGIN"); do_as_list(p_ni, 2); do_subnode(p_ni, 3);
    do_unknown(p_ni, 4); do_unknown(p_ni, 5); do_unknown(p_ni, 6);
    do_unknown(p_ni, 7); do_unknown(p_ni, 8); do_unknown(p_ni, 9); do_meta(p_ni, 10);
    return true;
  }

  // D_DEFERR - deferred declaration
  if (nt == D_DEFERR) { do_as_list(p_ni, 1); do_subnode(p_ni, 2); return true; }

  // D_ENTRY
  if (nt == D_ENTRY) { do_meta(p_ni, 1); do_subnode(p_ni, 2); return true; }

  // D_ENTRY_
  if (nt == D_ENTRY_) { do_subnode(p_ni, 1); do_as_list(p_ni, 2); do_unknown(p_ni, 3); do_meta(p_ni, 4); return true; }

  // D_EXCEPT - exception handler section
  if (nt == D_EXCEPT) {
    do_static("EXCEPTION"); do_as_list(p_ni, 1); do_subnode(p_ni, 2); do_meta(p_ni, 3);
    return true;
  }

  // D_EXIT
  if (nt == D_EXIT) {
    do_static("EXIT"); do_subnode(p_ni, 1); do_subnode(p_ni, 2);
    do_unknown(p_ni, 3); do_unknown(p_ni, 4); do_unknown(p_ni, 5); do_meta(p_ni, 6);
    return true;
  }

  // D_F_ - function specification
  if (nt == D_F_) { do_as_list(p_ni, 1); do_subnode(p_ni, 2); do_meta(p_ni, 3); return true; }

  // D_F_BODY - function body
  if (nt == D_F_BODY) {
    do_static("FUNCTION"); do_subnode(p_ni, 1); do_static("RETURN"); do_subnode(p_ni, 2);
    do_static("IS"); do_subnode(p_ni, 3); do_static("BEGIN"); do_as_list(p_ni, 4);
    do_static("END");
    do_meta(p_ni, 5);
    return true;
  }

  // D_F_CALL - function/operator call
  if (nt == D_F_CALL) {
    // Check for operator calls
    if (get_subnode_type(p_ni, 1) == D_USED_O && get_subnode_type(p_ni, 2) == DS_PARAM) {
      auto opname = get_lexical_str(get_subnode_idx(p_ni, 1), 1);
      if (opname == "NOT_LIKE") opname = "NOT LIKE";
      int pi = get_list_idx(get_subnode_idx(p_ni, 2), 1);
      int pc = get_list_len(pi);
      if (pc == 1) {
        if (opname == "(+)") { do_node(get_list_element(pi, 1)); do_static(opname, 1, p_ni, 1); }
        else { do_static(opname, 1, p_ni, 1); do_node(get_list_element(pi, 1)); }
      } else if (pc == 2) {
        do_node(get_list_element(pi, 1));
        do_static(opname, 1, p_ni, 1);
        do_node(get_list_element(pi, 2));
      } else if (pc == 3 && (opname == "LIKE" || opname == "NOT LIKE")) {
        do_node(get_list_element(pi, 1));
        do_static(opname, 1, p_ni, 1);
        do_node(get_list_element(pi, 2));
        do_static("ESCAPE");
        do_node(get_list_element(pi, 3));
      } else return false;
      return true;
    }
    do_subnode(p_ni, 1); do_static("("); do_subnode(p_ni, 2); do_static(")");
    do_unknown(p_ni, 3); do_unknown(p_ni, 4); do_unknown(p_ni, 5);
    return true;
  }

  // D_F_DECL
  if (nt == D_F_DECL) { do_static("FUNCTION"); do_subnode(p_ni, 1); do_static("RETURN"); do_subnode(p_ni, 2); do_subnode(p_ni, 3); do_meta(p_ni, 4); do_unknown(p_ni, 5); do_unknown(p_ni, 6); return true; }

  // D_F_SPEC
  if (nt == D_F_SPEC) { do_as_list(p_ni, 1); do_as_list(p_ni, 2); do_meta(p_ni, 3); return true; }

  // D_FOR
  if (nt == D_FOR) { do_subnode(p_ni, 1); do_static("IN"); do_subnode(p_ni, 2); return true; }

  // D_FORM
  if (nt == D_FORM) { do_as_list(p_ni, 1); do_unknown(p_ni, 2); do_meta(p_ni, 3); return true; }

  // D_FORM_C
  if (nt == D_FORM_C) { do_subnode(p_ni, 1); do_static("("); do_subnode(p_ni, 2); do_static(")"); do_unknown(p_ni, 3); do_unknown(p_ni, 4); do_meta(p_ni, 5); return true; }

  // D_GOTO
  if (nt == D_GOTO) { do_static("GOTO"); do_subnode(p_ni, 1); do_unknown(p_ni, 2); do_unknown(p_ni, 3); do_unknown(p_ni, 4); do_unknown(p_ni, 5); do_meta(p_ni, 6); return true; }

  // D_IF
  if (nt == D_IF) {
    do_static("IF"); do_as_list(p_ni, 1);
    do_static("END IF", 6, p_ni, 2);
    do_unknown(p_ni, 2); do_meta(p_ni, 3); do_unknown(p_ni, 4); do_unknown(p_ni, 5);
    return true;
  }

  // D_IN
  if (nt == D_IN) { do_as_list(p_ni, 1); do_subnode(p_ni, 2); do_static("IN"); do_unknown(p_ni, 4); do_unknown(p_ni, 5); return true; }

  // D_IN_OUT
  if (nt == D_IN_OUT) { do_as_list(p_ni, 1); do_subnode(p_ni, 2); do_static("IN OUT"); do_unknown(p_ni, 4); do_unknown(p_ni, 5); return true; }

  // D_INDEX
  if (nt == D_INDEX) {
    auto s = get_lexical_str(p_ni, 1);
    do_lexical(p_ni, 1);
    return true;
  }

  // D_INDEXE
  if (nt == D_INDEXE) { do_subnode(p_ni, 1); do_static("("); do_as_list(p_ni, 2); do_static(")"); do_unknown(p_ni, 3); return true; }

  // D_INNER_
  if (nt == D_INNER_) { do_as_list(p_ni, 1); do_meta(p_ni, 2); return true; }

  // D_INTEGE
  if (nt == D_INTEGE) { do_subnode(p_ni, 1); do_unknown(p_ni, 2); do_unknown(p_ni, 3); do_unknown(p_ni, 4); return true; }

  // D_L_PRIV
  if (nt == D_L_PRIV) { do_unknown(p_ni, 1); return true; }

  // D_LABELE
  if (nt == D_LABELE) { do_static("<<"); do_as_list(p_ni, 1); do_static(">>"); do_subnode(p_ni, 2); do_meta(p_ni, 3); return true; }

  // D_LOOP
  if (nt == D_LOOP) {
    do_static("LOOP"); do_as_list(p_ni, 2);
    do_static("END LOOP", 6, p_ni, 1);
    do_unknown(p_ni, 3); do_unknown(p_ni, 4); do_unknown(p_ni, 5);
    do_unknown(p_ni, 6); do_meta(p_ni, 7); do_unknown(p_ni, 8); do_unknown(p_ni, 9);
    return true;
  }

  // D_MEMBER - .member access
  if (nt == D_MEMBER) { do_subnode(p_ni, 1); do_static("."); do_subnode(p_ni, 2); do_subnode(p_ni, 3); return true; }

  // D_NAMED
  if (nt == D_NAMED) { do_as_list(p_ni, 1); do_subnode(p_ni, 2); return true; }

  // D_NAMED_
  if (nt == D_NAMED_) { do_subnode(p_ni, 1); do_subnode(p_ni, 2); do_meta(p_ni, 3); return true; }

  // D_NULL_A
  if (nt == D_NULL_A) { do_static("NULL", 1, p_ni, 1); return true; }

  // D_NULL_S
  if (nt == D_NULL_S) { do_static("NULL"); do_unknown(p_ni, 1); do_meta(p_ni, 2); return true; }

  // D_NUMBER
  if (nt == D_NUMBER) { do_as_list(p_ni, 1); do_subnode(p_ni, 2); return true; }

  // D_NUMERI
  if (nt == D_NUMERI) { do_numeric(p_ni, 1); do_unknown(p_ni, 2); do_unknown(p_ni, 3); return true; }

  // D_OUT
  if (nt == D_OUT) { do_as_list(p_ni, 1); do_subnode(p_ni, 2); do_static("OUT"); do_unknown(p_ni, 4); do_unknown(p_ni, 5); return true; }

  // D_P_
  if (nt == D_P_) { do_as_list(p_ni, 1); do_unknown(p_ni, 2); do_unknown(p_ni, 3); do_meta(p_ni, 4); return true; }

  // D_P_BODY - procedure body
  if (nt == D_P_BODY) {
    do_static("PROCEDURE"); do_subnode(p_ni, 1); do_static("IS");
    do_subnode(p_ni, 2); do_static("BEGIN"); do_as_list(p_ni, 3);
    do_static("END");
    do_meta(p_ni, 4);
    return true;
  }

  // D_P_CALL
  if (nt == D_P_CALL) { do_subnode(p_ni, 1); do_static("("); do_subnode(p_ni, 2); do_static(")"); do_unknown(p_ni, 3); do_unknown(p_ni, 4); do_meta(p_ni, 5); return true; }

  // D_P_DECL
  if (nt == D_P_DECL) { do_static("PROCEDURE"); do_subnode(p_ni, 1); do_static("IS"); do_subnode(p_ni, 2); do_meta(p_ni, 3); do_unknown(p_ni, 4); do_unknown(p_ni, 5); return true; }

  // D_P_SPEC
  if (nt == D_P_SPEC) { do_as_list(p_ni, 1); do_as_list(p_ni, 2); do_meta(p_ni, 3); return true; }

  // D_PARENT
  if (nt == D_PARENT) { do_static("("); do_subnode(p_ni, 1); do_static(")"); do_unknown(p_ni, 2); do_unknown(p_ni, 3); return true; }

  // D_PARM_C
  if (nt == D_PARM_C) { do_subnode(p_ni, 1); do_static("("); do_subnode(p_ni, 2); do_static(")"); do_unknown(p_ni, 3); do_unknown(p_ni, 4); do_unknown(p_ni, 5); return true; }

  // D_PARM_F
  if (nt == D_PARM_F) { do_lexical(p_ni, 1); do_subnode(p_ni, 2); do_subnode(p_ni, 3); return true; }

  // D_PRAGMA
  if (nt == D_PRAGMA) { do_subnode(p_ni, 1); do_static("("); do_subnode(p_ni, 2); do_static(")"); do_meta(p_ni, 3); return true; }

  // D_PRIVAT
  if (nt == D_PRIVAT) { do_unknown(p_ni, 1); return true; }

  // D_QUALIF - qualified name
  if (nt == D_QUALIF) { do_subnode(p_ni, 1); do_static("."); do_subnode(p_ni, 2); do_unknown(p_ni, 3); do_unknown(p_ni, 4); return true; }

  // D_R_ - record type
  if (nt == D_R_) {
    do_static("RECORD"); do_static("(");
    do_as_list(p_ni, 1, "", ",", ")");
    do_unknown(p_ni, 2); do_unknown(p_ni, 3); do_unknown(p_ni, 4); do_unknown(p_ni, 5);
    do_unknown(p_ni, 6); do_meta(p_ni, 7); do_unknown(p_ni, 8); do_unknown(p_ni, 9);
    do_unknown(p_ni, 10); do_unknown(p_ni, 11); do_unknown(p_ni, 12); do_unknown(p_ni, 13);
    do_unknown(p_ni, 14); do_unknown(p_ni, 15); do_unknown(p_ni, 16);
    return true;
  }

  // D_R_REP
  if (nt == D_R_REP) { do_subnode(p_ni, 1); do_as_list(p_ni, 3); return true; }

  // D_RAISE
  if (nt == D_RAISE) { do_static("RAISE"); do_subnode(p_ni, 1); do_unknown(p_ni, 2); do_meta(p_ni, 3); return true; }

  // D_RANGE
  if (nt == D_RANGE) {
    do_subnode(p_ni, 1); do_static(".."); do_subnode(p_ni, 2);
    do_unknown(p_ni, 3); do_unknown(p_ni, 4); do_unknown(p_ni, 5); do_unknown(p_ni, 6);
    return true;
  }

  // D_RENAME
  if (nt == D_RENAME) { do_static("RENAME"); do_subnode(p_ni, 1); do_meta(p_ni, 2); return true; }

  // D_RETURN
  if (nt == D_RETURN) { do_static("RETURN"); do_subnode(p_ni, 1); do_unknown(p_ni, 2); do_unknown(p_ni, 3); do_meta(p_ni, 4); return true; }

  // D_REVERS
  if (nt == D_REVERS) { do_static("REVERSE"); do_subnode(p_ni, 1); do_subnode(p_ni, 2); return true; }

  // D_S_BODY - subtype body
  if (nt == D_S_BODY) { do_subnode(p_ni, 1); do_static("IS"); do_subnode(p_ni, 2); do_subnode(p_ni, 3); do_meta(p_ni, 4); do_unknown(p_ni, 5); do_unknown(p_ni, 6); do_unknown(p_ni, 7); do_unknown(p_ni, 8); return true; }

  // D_S_DECL - subtype declaration
  if (nt == D_S_DECL) { do_static("SUBTYPE"); do_subnode(p_ni, 1); do_static("IS"); do_subnode(p_ni, 2); do_subnode(p_ni, 3); do_meta(p_ni, 4); return true; }

  // D_S_ED - dotted name component
  if (nt == D_S_ED) { do_subnode(p_ni, 1); do_static("."); do_subnode(p_ni, 2); do_unknown(p_ni, 3); return true; }

  // D_SLICE
  if (nt == D_SLICE) { do_subnode(p_ni, 1); do_static("("); do_subnode(p_ni, 2); do_static(")"); do_unknown(p_ni, 3); do_unknown(p_ni, 4); return true; }

  // D_STRING
  if (nt == D_STRING) { do_string(p_ni, 1); do_unknown(p_ni, 2); do_unknown(p_ni, 3); do_unknown(p_ni, 4); do_unknown(p_ni, 5); return true; }

  // D_SUBTYP
  if (nt == D_SUBTYP) { do_static("SUBTYPE"); do_subnode(p_ni, 1); do_static("IS"); do_subnode(p_ni, 2); do_meta(p_ni, 3); return true; }

  // D_SUBUNI
  if (nt == D_SUBUNI) { do_subnode(p_ni, 1); do_static("."); do_subnode(p_ni, 2); do_meta(p_ni, 3); return true; }

  // D_TYPE
  if (nt == D_TYPE) { do_static("TYPE"); do_subnode(p_ni, 1); do_static("IS"); do_as_list(p_ni, 2); do_subnode(p_ni, 3); do_meta(p_ni, 4); return true; }

  // D_USE
  if (nt == D_USE) { do_as_list(p_ni, 1); return true; }

  // D_USED_B, D_USED_C, D_USED_O
  if (nt == D_USED_B) { do_lexical(p_ni, 1); do_unknown(p_ni, 2); do_unknown(p_ni, 3); do_unknown(p_ni, 4); return true; }
  if (nt == D_USED_C) { do_lexical(p_ni, 1); do_unknown(p_ni, 2); do_unknown(p_ni, 3); do_unknown(p_ni, 4); return true; }
  if (nt == D_USED_O) { do_lexical(p_ni, 1); do_unknown(p_ni, 2); do_unknown(p_ni, 3); return true; }

  // D_VAR
  if (nt == D_VAR) { do_as_list(p_ni, 1); do_subnode(p_ni, 2); do_subnode(p_ni, 3); do_meta(p_ni, 4); do_unknown(p_ni, 5); return true; }

  // D_WHILE
  if (nt == D_WHILE) { do_static("WHILE"); do_subnode(p_ni, 1); do_meta(p_ni, 2); return true; }

  // D_WITH
  if (nt == D_WITH) { do_as_list(p_ni, 1); return true; }

  // DI_* info nodes
  if (nt == DI_ARGUM) { do_lexical(p_ni, 1); return true; }
  if (nt == DI_ATTR_) { do_lexical(p_ni, 1); return true; }
  if (nt == DI_COMP_) { do_lexical(p_ni, 1); do_unknown(p_ni, 2); do_unknown(p_ni, 3); do_unknown(p_ni, 4); return true; }
  if (nt == DI_CONST) { do_lexical(p_ni, 1); do_unknown(p_ni, 2); do_unknown(p_ni, 3); do_unknown(p_ni, 4); do_unknown(p_ni, 5); do_unknown(p_ni, 6); do_unknown(p_ni, 7); return true; }
  if (nt == DI_DSCRM) { do_lexical(p_ni, 1); do_unknown(p_ni, 2); do_unknown(p_ni, 3); do_unknown(p_ni, 4); do_unknown(p_ni, 5); return true; }
  if (nt == DI_ENUM) { do_lexical(p_ni, 1); do_unknown(p_ni, 2); do_unknown(p_ni, 3); do_unknown(p_ni, 4); return true; }
  if (nt == DI_EXCEP) { do_lexical(p_ni, 1); do_unknown(p_ni, 2); do_unknown(p_ni, 3); do_unknown(p_ni, 4); do_unknown(p_ni, 5); do_unknown(p_ni, 6); do_unknown(p_ni, 7); return true; }
  if (nt == DI_FORM) { do_lexical(p_ni, 1); do_unknown(p_ni, 2); do_unknown(p_ni, 3); do_unknown(p_ni, 4); do_unknown(p_ni, 5); do_unknown(p_ni, 6); do_unknown(p_ni, 7); do_unknown(p_ni, 8); do_unknown(p_ni, 9); do_unknown(p_ni, 10); do_unknown(p_ni, 11); do_unknown(p_ni, 12); return true; }
  if (nt == DI_FUNCT) { do_lexical(p_ni, 1); /* meta attrs */ do_meta(p_ni, 13); do_unknown(p_ni, 14); do_unknown(p_ni, 15); do_unknown(p_ni, 16); do_unknown(p_ni, 17); do_unknown(p_ni, 18); do_unknown(p_ni, 19); do_unknown(p_ni, 20); return true; }
  if (nt == DI_IN) { do_lexical(p_ni, 1); do_unknown(p_ni, 2); do_unknown(p_ni, 3); do_unknown(p_ni, 4); do_unknown(p_ni, 5); do_unknown(p_ni, 6); do_unknown(p_ni, 7); do_unknown(p_ni, 8); do_meta(p_ni, 9); do_unknown(p_ni, 10); return true; }
  if (nt == DI_IN_OU) { do_lexical(p_ni, 1); do_unknown(p_ni, 2); do_unknown(p_ni, 3); do_unknown(p_ni, 4); do_unknown(p_ni, 5); do_unknown(p_ni, 6); do_unknown(p_ni, 7); do_meta(p_ni, 8); return true; }
  if (nt == DI_ITERA) { do_lexical(p_ni, 1); do_unknown(p_ni, 2); do_unknown(p_ni, 3); do_unknown(p_ni, 4); return true; }
  if (nt == DI_L_PRI) { do_lexical(p_ni, 1); do_unknown(p_ni, 2); return true; }
  if (nt == DI_LABEL) { do_lexical(p_ni, 1); do_unknown(p_ni, 2); do_unknown(p_ni, 3); do_unknown(p_ni, 4); do_unknown(p_ni, 5); do_unknown(p_ni, 6); do_meta(p_ni, 7); do_unknown(p_ni, 8); return true; }
  if (nt == DI_NAMED) { do_lexical(p_ni, 1); do_unknown(p_ni, 2); do_meta(p_ni, 3); do_unknown(p_ni, 4); return true; }
  if (nt == DI_NUMBE) { do_lexical(p_ni, 1); do_unknown(p_ni, 2); do_unknown(p_ni, 3); return true; }
  if (nt == DI_OUT) { do_lexical(p_ni, 1); do_unknown(p_ni, 2); do_unknown(p_ni, 3); do_unknown(p_ni, 4); do_unknown(p_ni, 5); do_unknown(p_ni, 6); do_unknown(p_ni, 7); do_meta(p_ni, 8); return true; }
  if (nt == DI_PACKA) { do_lexical(p_ni, 1); do_unknown(p_ni, 2); do_unknown(p_ni, 3); do_unknown(p_ni, 4); do_unknown(p_ni, 5); do_unknown(p_ni, 6); do_unknown(p_ni, 7); do_unknown(p_ni, 8); do_unknown(p_ni, 9); return true; }
  if (nt == DI_PRAGM) { do_as_list(p_ni, 1); do_lexical(p_ni, 2); return true; }
  if (nt == DI_PRIVA) { do_lexical(p_ni, 1); do_unknown(p_ni, 2); return true; }
  if (nt == DI_PROC) { do_lexical(p_ni, 1); do_meta(p_ni, 12); do_unknown(p_ni, 13); do_unknown(p_ni, 14); do_unknown(p_ni, 15); do_unknown(p_ni, 16); do_unknown(p_ni, 17); do_unknown(p_ni, 18); do_unknown(p_ni, 19); do_unknown(p_ni, 20); return true; }
  if (nt == DI_SUBTY) { do_lexical(p_ni, 1); do_unknown(p_ni, 2); do_unknown(p_ni, 3); return true; }
  if (nt == DI_TYPE) { do_lexical(p_ni, 1); do_unknown(p_ni, 2); do_unknown(p_ni, 3); do_unknown(p_ni, 4); do_unknown(p_ni, 5); do_unknown(p_ni, 6); do_unknown(p_ni, 7); return true; }
  if (nt == DI_U_ALY) { do_lexical(p_ni, 1); do_unknown(p_ni, 2); return true; }
  if (nt == DI_U_BLT) { do_lexical(p_ni, 1); do_unknown(p_ni, 2); do_unknown(p_ni, 3); return true; }
  if (nt == DI_U_OBJ) { do_lexical(p_ni, 1); do_unknown(p_ni, 2); do_unknown(p_ni, 3); do_unknown(p_ni, 4); return true; }
  if (nt == DI_USER) { do_lexical(p_ni, 1); do_unknown(p_ni, 2); return true; }
  if (nt == DI_VAR) { do_lexical(p_ni, 1); do_unknown(p_ni, 2); do_unknown(p_ni, 3); do_unknown(p_ni, 4); do_unknown(p_ni, 5); do_unknown(p_ni, 6); do_unknown(p_ni, 7); return true; }

  // DS_* list nodes
  if (nt == DS_ALTER) { do_as_list(p_ni, 1); do_unknown(p_ni, 2); do_unknown(p_ni, 3); do_meta(p_ni, 4); return true; }
  if (nt == DS_APPLY) { do_as_list(p_ni, 1); return true; }
  if (nt == DS_CHOIC) { do_as_list(p_ni, 1); return true; }
  if (nt == DS_COMP_) { do_as_list(p_ni, 1); return true; }
  if (nt == DS_D_RAN) { do_as_list(p_ni, 1); return true; }
  if (nt == DS_DECL) { do_as_list(p_ni, 1); do_meta(p_ni, 2); return true; }
  if (nt == DS_ENUM_) { do_as_list(p_ni, 1); do_unknown(p_ni, 2); return true; }
  if (nt == DS_EXP) { do_as_list(p_ni, 1); return true; }
  if (nt == DS_ID) { do_as_list(p_ni, 1); return true; }
  if (nt == DS_ITEM) { do_as_list(p_ni, 1); do_meta(p_ni, 2); return true; }
  if (nt == DS_NAME) { do_as_list(p_ni, 1); return true; }
  if (nt == DS_P_ASS) { do_as_list(p_ni, 1); return true; }
  if (nt == DS_PARAM) { do_as_list(p_ni, 1); return true; }
  if (nt == DS_PRAGM) { do_as_list(p_ni, 1); do_meta(p_ni, 2); return true; }
  if (nt == DS_STM) { do_as_list(p_ni, 1); do_meta(p_ni, 2); return true; }
  if (nt == DS_UPDNW) { do_as_list(p_ni, 1); do_meta(p_ni, 2); return true; }

  // Q_* SQL nodes
  if (nt == Q_ALIAS_) { do_subnode(p_ni, 1); do_subnode(p_ni, 2); return true; }
  if (nt == Q_BINARY) { do_subnode(p_ni, 1); do_unknown(p_ni, 2); do_subnode(p_ni, 3); return true; }
  if (nt == Q_BIND) { do_lexical(p_ni, 1); do_unknown(p_ni, 2); do_unknown(p_ni, 3); do_unknown(p_ni, 4); do_unknown(p_ni, 5); do_unknown(p_ni, 6); do_unknown(p_ni, 7); return true; }
  if (nt == Q_C_BODY) { do_static("CURSOR"); do_subnode(p_ni, 1); do_static("IS"); do_subnode(p_ni, 2); do_unknown(p_ni, 4); do_meta(p_ni, 5); do_unknown(p_ni, 6); do_unknown(p_ni, 7); return true; }
  if (nt == Q_C_CALL) { do_subnode(p_ni, 1); do_static("("); do_subnode(p_ni, 2); do_static(")"); do_unknown(p_ni, 3); do_unknown(p_ni, 4); do_unknown(p_ni, 5); do_meta(p_ni, 6); return true; }
  if (nt == Q_C_DECL) { do_static("CURSOR"); do_subnode(p_ni, 1); do_static("IS"); do_subnode(p_ni, 2); do_meta(p_ni, 3); return true; }
  if (nt == Q_CHAR) { do_static("CHAR"); do_subnode(p_ni, 1); return true; }
  if (nt == Q_CLOSE_) { do_static("CLOSE"); do_subnode(p_ni, 1); do_meta(p_ni, 2); return true; }
  if (nt == Q_COMMIT) { do_static("COMMIT"); do_meta(p_ni, 1); do_meta(p_ni, 2); return true; }
  if (nt == Q_COMMNT) { do_lexical(p_ni, 1); return true; }
  if (nt == Q_CONNEC) { do_subnode(p_ni, 1); do_static("CONNECT BY"); do_subnode(p_ni, 2); do_meta(p_ni, 3); return true; }
  if (nt == Q_CREATE) { do_subnode(p_ni, 1); do_subnode(p_ni, 2); return true; }
  if (nt == Q_CURREN) { do_lexical(p_ni, 1); return true; }
  if (nt == Q_CURSOR) { do_static("CURSOR"); do_as_list(p_ni, 1); do_subnode(p_ni, 2); do_unknown(p_ni, 3); do_meta(p_ni, 4); return true; }
  if (nt == Q_DATABA) { do_subnode(p_ni, 1); do_subnode(p_ni, 2); do_meta(p_ni, 3); return true; }
  if (nt == Q_DELETE) { do_static("DELETE FROM"); do_subnode(p_ni, 1); do_subnode(p_ni, 2); do_unknown(p_ni, 3); do_meta(p_ni, 4); do_unknown(p_ni, 5); return true; }
  if (nt == Q_DICTIO) { do_lexical(p_ni, 1); return true; }
  if (nt == Q_DROP_S) { do_lexical(p_ni, 1); return true; }
  if (nt == Q_EXP) { do_unknown(p_ni, 1); do_as_list(p_ni, 2); do_subnode(p_ni, 3); do_unknown(p_ni, 4); return true; }
  if (nt == Q_F_CALL) { do_subnode(p_ni, 1); do_unknown(p_ni, 2); do_subnode(p_ni, 3); do_unknown(p_ni, 4); return true; }
  if (nt == Q_FETCH_) { do_static("FETCH"); do_subnode(p_ni, 1); do_static("INTO"); do_subnode(p_ni, 2); do_unknown(p_ni, 3); do_unknown(p_ni, 4); do_unknown(p_ni, 5); return true; }
  if (nt == Q_FRCTRN) { do_as_list(p_ni, 1); return true; }
  if (nt == Q_GENSQL) { do_unknown(p_ni, 1); do_meta(p_ni, 2); return true; }
  if (nt == Q_INSERT) { do_static("INSERT INTO"); do_subnode(p_ni, 1); do_subnode(p_ni, 2); do_subnode(p_ni, 3); do_unknown(p_ni, 4); do_unknown(p_ni, 5); do_unknown(p_ni, 6); do_unknown(p_ni, 7); do_unknown(p_ni, 8); return true; }
  if (nt == Q_LINK) { do_subnode(p_ni, 1); do_static("@"); do_subnode(p_ni, 2); return true; }
  if (nt == Q_LOCK_T) { do_static("LOCK TABLE"); do_as_list(p_ni, 1); do_unknown(p_ni, 2); return true; }
  if (nt == Q_NUMBER) { do_static("NUMBER"); do_subnode(p_ni, 1); return true; }
  if (nt == Q_OPEN_S) { do_static("OPEN"); do_subnode(p_ni, 1); do_static("("); do_subnode(p_ni, 2); do_static(")"); do_unknown(p_ni, 3); do_meta(p_ni, 4); return true; }
  if (nt == Q_ORDER_) { do_static("ORDER BY"); do_unknown(p_ni, 1); do_subnode(p_ni, 2); return true; }
  if (nt == Q_RLLBCK) { do_static("ROLLBACK"); do_meta(p_ni, 1); do_meta(p_ni, 2); return true; }
  if (nt == Q_ROLLBA) { do_lexical(p_ni, 1); return true; }
  if (nt == Q_SAVEPO) { do_static("SAVEPOINT"); do_lexical(p_ni, 1); return true; }
  if (nt == Q_SCHEMA) { do_subnode(p_ni, 1); do_subnode(p_ni, 2); do_meta(p_ni, 3); return true; }
  if (nt == Q_SELECT) { do_static("SELECT"); do_subnode(p_ni, 1); do_subnode(p_ni, 2); do_subnode(p_ni, 3); do_unknown(p_ni, 4); do_subnode(p_ni, 5); do_unknown(p_ni, 6); return true; }
  if (nt == Q_SEQUE) { do_subnode(p_ni, 1); do_unknown(p_ni, 2); do_subnode(p_ni, 3); return true; }
  if (nt == Q_SET_CL) { do_subnode(p_ni, 1); do_static("="); do_subnode(p_ni, 2); return true; }
  if (nt == Q_SQL_ST) { do_subnode(p_ni, 1); do_subnode(p_ni, 2); do_unknown(p_ni, 3); do_unknown(p_ni, 4); do_meta(p_ni, 5); return true; }
  if (nt == Q_STATEM) { do_meta(p_ni, 1); return true; }
  if (nt == Q_SUBQUE) { do_static("("); do_subnode(p_ni, 1); do_static(")"); do_unknown(p_ni, 2); do_unknown(p_ni, 3); do_unknown(p_ni, 4); return true; }
  if (nt == Q_SYNON) { do_subnode(p_ni, 1); do_unknown(p_ni, 2); do_unknown(p_ni, 3); return true; }
  if (nt == Q_TABLE) { do_subnode(p_ni, 1); do_unknown(p_ni, 2); do_unknown(p_ni, 3); do_unknown(p_ni, 4); do_unknown(p_ni, 5); do_unknown(p_ni, 6); do_unknown(p_ni, 7); do_meta(p_ni, 8); do_unknown(p_ni, 9); do_unknown(p_ni, 10); do_unknown(p_ni, 11); return true; }
  if (nt == Q_TBL_EX) { do_subnode(p_ni, 1); do_subnode(p_ni, 2); do_subnode(p_ni, 3); do_subnode(p_ni, 4); do_subnode(p_ni, 5); do_unknown(p_ni, 6); do_unknown(p_ni, 7); return true; }
  if (nt == Q_UPDATE) { do_static("UPDATE"); do_subnode(p_ni, 1); do_static("SET"); do_subnode(p_ni, 2); do_subnode(p_ni, 3); do_unknown(p_ni, 4); do_meta(p_ni, 5); do_unknown(p_ni, 6); return true; }
  if (nt == Q_VIEW) { do_as_list(p_ni, 1); do_subnode(p_ni, 2); do_unknown(p_ni, 3); do_unknown(p_ni, 4); do_meta(p_ni, 5); return true; }

  // QI_* nodes
  if (nt == QI_BIND_) { do_lexical(p_ni, 1); do_unknown(p_ni, 2); do_unknown(p_ni, 3); do_unknown(p_ni, 4); do_unknown(p_ni, 5); do_unknown(p_ni, 6); do_unknown(p_ni, 7); return true; }
  if (nt == QI_CURSO) { do_lexical(p_ni, 1); do_unknown(p_ni, 2); do_unknown(p_ni, 3); do_unknown(p_ni, 4); do_unknown(p_ni, 5); do_unknown(p_ni, 6); do_unknown(p_ni, 7); do_unknown(p_ni, 8); do_unknown(p_ni, 9); do_unknown(p_ni, 10); do_unknown(p_ni, 11); do_unknown(p_ni, 12); do_meta(p_ni, 13); do_unknown(p_ni, 14); do_unknown(p_ni, 15); do_unknown(p_ni, 16); do_unknown(p_ni, 17); return true; }
  if (nt == QI_DATAB) { do_lexical(p_ni, 1); do_unknown(p_ni, 2); do_unknown(p_ni, 3); do_unknown(p_ni, 4); do_unknown(p_ni, 5); do_unknown(p_ni, 6); do_unknown(p_ni, 7); do_unknown(p_ni, 8); do_unknown(p_ni, 9); return true; }
  if (nt == QI_SCHEM) { do_lexical(p_ni, 1); do_unknown(p_ni, 2); do_unknown(p_ni, 3); do_unknown(p_ni, 4); do_unknown(p_ni, 5); do_unknown(p_ni, 6); do_unknown(p_ni, 7); do_unknown(p_ni, 8); do_unknown(p_ni, 9); return true; }
  if (nt == QI_TABLE) { do_lexical(p_ni, 1); return true; }

  // More complex nodes
  if (nt == QS_SET_C) { do_as_list(p_ni, 1); return true; }
  if (nt == Q_RTNING) { do_static("RETURNING"); do_as_list(p_ni, 1); do_unknown(p_ni, 2); do_unknown(p_ni, 3); return true; }
  if (nt == D_FORALL) { do_static("FORALL"); do_subnode(p_ni, 1); do_static("IN"); do_subnode(p_ni, 2); do_unknown(p_ni, 3); return true; }
  if (nt == D_IN_BIND) { do_static("IN"); do_subnode(p_ni, 1); return true; }
  if (nt == D_IN_OUT_BIND) { do_static("IN OUT"); do_subnode(p_ni, 1); return true; }
  if (nt == D_OUT_BIND) { do_static("OUT"); do_subnode(p_ni, 1); return true; }
  if (nt == D_S_OPER) { do_static("STATIC"); do_subnode(p_ni, 1); do_subnode(p_ni, 2); do_subnode(p_ni, 3); do_meta(p_ni, 4); do_unknown(p_ni, 5); return true; }
  if (nt == D_X_NAMED_RESULT || nt == D_X_NAMED_TYPE) { do_unknown(p_ni, 1); do_unknown(p_ni, 2); do_subnode(p_ni, 3); return true; }
  if (nt == Q_EXEC_IMMEDIATE) { do_static("EXECUTE IMMEDIATE"); do_subnode(p_ni, 1); do_subnode(p_ni, 2); do_subnode(p_ni, 3); do_subnode(p_ni, 4); do_unknown(p_ni, 5); return true; }
  if (nt == D_PERCENT) { do_static("%"); do_subnode(p_ni, 2); return true; }
  if (nt == D_SAMPLE) { do_static("SAMPLE"); do_subnode(p_ni, 1); do_subnode(p_ni, 2); return true; }
  if (nt == D_CASE_EXP) { do_static("CASE"); do_subnode(p_ni, 1); do_as_list(p_ni, 2); do_unknown(p_ni, 3); do_unknown(p_ni, 4); do_static("END", 7, p_ni, 2); return true; }
  if (nt == D_COALESCE) { do_static("COALESCE"); do_as_list(p_ni, 1, "(", ",", ")"); do_unknown(p_ni, 2); return true; }
  if (nt == D_NULLIF) { do_static("NULLIF"); do_static("("); do_subnode(p_ni, 1); do_static(","); do_subnode(p_ni, 2); do_static(")"); do_unknown(p_ni, 3); return true; }
  if (nt == D_PIPE) { do_static("PIPE ROW"); do_static("("); do_subnode(p_ni, 1); do_static(")"); do_unknown(p_ni, 2); do_unknown(p_ni, 3); do_meta(p_ni, 4); return true; }
  if (nt == D_ELAB) { do_unknown(p_ni, 1); do_unknown(p_ni, 2); do_as_list(p_ni, 3); do_meta(p_ni, 4); return true; }
  if (nt == D_IMPL_BODY) { do_subnode(p_ni, 1); do_meta(p_ni, 2); return true; }
  if (nt == D_ALT_TYPE) { do_static("ALTER TYPE"); do_as_list(p_ni, 1); do_unknown(p_ni, 2); return true; }
  if (nt == D_ALTERN_EXP) { do_as_list(p_ni, 1); do_subnode(p_ni, 2); return true; }
  if (nt == D_AN_ALTER) { do_as_list(p_ni, 1); return true; }
  if (nt == D_ADT_BODY) { do_lexical(p_ni, 1); do_unknown(p_ni, 2); do_unknown(p_ni, 3); do_unknown(p_ni, 4); do_unknown(p_ni, 5); do_unknown(p_ni, 6); do_unknown(p_ni, 7); do_unknown(p_ni, 8); do_meta(p_ni, 9); return true; }
  if (nt == D_ADT_SPEC) { do_as_list(p_ni, 1); do_unknown(p_ni, 2); do_unknown(p_ni, 3); do_unknown(p_ni, 4); do_unknown(p_ni, 5); do_unknown(p_ni, 6); do_meta(p_ni, 7); do_unknown(p_ni, 8); return true; }
  if (nt == D_CHARSET_SPEC) { do_unknown(p_ni, 1); do_unknown(p_ni, 2); do_unknown(p_ni, 3); do_unknown(p_ni, 4); return true; }
  if (nt == D_EXT_TYPE) { do_lexical(p_ni, 1); do_unknown(p_ni, 2); do_meta(p_ni, 3); return true; }
  if (nt == D_EXTERNAL) { do_static("EXTERNAL"); do_subnode(p_ni, 1); do_unknown(p_ni, 2); do_unknown(p_ni, 3); do_unknown(p_ni, 4); do_unknown(p_ni, 5); do_unknown(p_ni, 6); do_unknown(p_ni, 7); do_meta(p_ni, 8); do_unknown(p_ni, 9); do_unknown(p_ni, 10); do_unknown(p_ni, 11); do_unknown(p_ni, 12); do_unknown(p_ni, 13); return true; }
  if (nt == D_LIBRARY) { do_static("LIBRARY"); do_subnode(p_ni, 1); do_static("IS"); do_subnode(p_ni, 2); do_unknown(p_ni, 3); do_unknown(p_ni, 4); return true; }
  if (nt == D_S_PT) { do_subnode(p_ni, 1); do_subnode(p_ni, 2); do_unknown(p_ni, 3); do_unknown(p_ni, 4); return true; }
  if (nt == D_T_PTR || nt == D_T_REF) { do_static("REF"); do_subnode(p_ni, 1); do_meta(p_ni, 2); return true; }
  if (nt == D_X_CODE || nt == D_X_CTX || nt == D_X_NAME || nt == D_X_RETN || nt == D_X_STAT) { do_unknown(p_ni, 1); do_unknown(p_ni, 2); return true; }
  if (nt == D_X_FRML) { do_lexical(p_ni, 1); do_unknown(p_ni, 2); do_unknown(p_ni, 3); do_unknown(p_ni, 4); return true; }
  if (nt == DI_LIBRARY) { do_lexical(p_ni, 1); do_unknown(p_ni, 2); return true; }
  if (nt == DS_X_PARM) { do_as_list(p_ni, 1); return true; }
  if (nt == DI_BULK_ITER) { do_lexical(p_ni, 1); do_unknown(p_ni, 2); do_unknown(p_ni, 3); do_unknown(p_ni, 4); return true; }
  if (nt == DI_OPSP) { do_lexical(p_ni, 1); do_unknown(p_ni, 2); do_unknown(p_ni, 3); do_unknown(p_ni, 4); do_unknown(p_ni, 5); do_unknown(p_ni, 6); do_unknown(p_ni, 7); do_unknown(p_ni, 8); return true; }
  if (nt == DS_USING_BIND) { do_as_list(p_ni, 1); return true; }
  if (nt == Q_BULK) { do_subnode(p_ni, 1); do_unknown(p_ni, 2); return true; }
  if (nt == Q_DOPEN_STM) { do_static("OPEN"); do_subnode(p_ni, 1); do_static("FOR"); do_subnode(p_ni, 2); do_subnode(p_ni, 3); return true; }
  if (nt == Q_DSQL_ST) { do_subnode(p_ni, 1); do_unknown(p_ni, 2); do_unknown(p_ni, 3); do_meta(p_ni, 4); return true; }
  if (nt == D_SQL_STMT) { do_unknown(p_ni, 1); do_unknown(p_ni, 2); do_unknown(p_ni, 3); do_unknown(p_ni, 4); do_unknown(p_ni, 5); do_unknown(p_ni, 6); do_unknown(p_ni, 7); do_unknown(p_ni, 8); do_unknown(p_ni, 9); do_unknown(p_ni, 10); do_unknown(p_ni, 11); do_meta(p_ni, 12); return true; }
  if (nt == D_SUBPROG_PROP) { do_unknown(p_ni, 1); do_unknown(p_ni, 2); do_unknown(p_ni, 3); do_unknown(p_ni, 4); do_meta(p_ni, 5); return true; }
  if (nt == VTABLE_ENTRY) { do_unknown(p_ni, 1); do_unknown(p_ni, 2); do_unknown(p_ni, 3); do_unknown(p_ni, 4); return true; }
  if (nt == D_ELLIPSIS) { do_lexical(p_ni, 1); return true; }
  if (nt == D_VALIST) { do_subnode(p_ni, 1); do_as_list(p_ni, 2); return true; }

  return false;
}

void V1State::do_node(int p_ni) {
  if (active_nodes[p_ni]) { infinite_loop_f = true; return; }
  stack_push(p_ni);
  emit_pos(p_ni);

  int nt = get_node_type(p_ni);

  // DI_U_NAM - most common node type
  if (nt == DI_U_NAM) {
    int flags = get_attr_val(p_ni, 4);
    bit_clear(flags, 2 + 4 + 64 + 128);
    if (get_parent_type() == D_ATTRIB && get_parent().attr_pos == 2) bit_clear(flags, 1);
    bit_check(flags, 4096, "NEW");
    if (flags != 0 && flags != 1 && flags != 1024 && flags != 1025) do_unknown(p_ni, 4);
    if (bit_set(flags, 1024)) do_static("SELF AS RESULT");
    else if (flags == 0 && get_parent_type() == D_F_ && get_lexical_str(p_ni, 1) == "SELF") do_static("SELF AS RESULT");
    else do_symbol(p_ni, 1, flags == 1);
    do_meta(p_ni, 2);
    do_unknown(p_ni, 3);
    stack_pop();
    return;
  }

  // Handle all other nodes through do_special_cases
  if (!do_special_cases(p_ni)) {
    // Unknown node type - generic fallback
    do_unknown(p_ni);
    // Fall through and process attributes generically
    int cnt;
    const int* attrs = get_node_attr_list(nt, cnt);
    for (int i = 0; i < cnt; i++) {
      int ap = i + 1;
      int child = get_subnode_idx(p_ni, ap);
      if (child != 0) {
        stack_set(ap);
        do_node(child);
        continue;
      }
      int li = get_list_idx(p_ni, ap);
      if (li != 0) {
        do_as_list(p_ni, ap);
        continue;
      }
      int lex = get_lexical_idx(p_ni, ap);
      if (lex != 0) {
        do_lexical(p_ni, ap);
        continue;
      }
    }
  }

  stack_pop();
}
