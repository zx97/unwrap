#include "unwrap_v1_state.h"
#include "diana_grammar.h"
#include <cstdlib>
#include <cctype>
#include <stdexcept>
#include <functional>

static std::string trim(const std::string& s) {
  size_t a = 0, b = s.size();
  while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r')) a++;
  while (b > a && (s[b-1] == ' ' || s[b-1] == '\t' || s[b-1] == '\r')) b--;
  return s.substr(a, b-a);
}

void V1State::parse_tree(const std::string& src) {
  wrap_version = 0; root_idx = 0;
  unit_type.clear(); unit_name.clear();
  lexical_tbl.clear(); node_tbl.clear(); column_tbl.clear();
  line_tbl.clear(); attr_ref_tbl.clear(); attr_tbl.clear();
  as_list_tbl.clear(); lexical_str_tbl.clear();

  // Parse header
  size_t pos = 0;
  auto get_tok = [&]() -> std::string {
    while (pos < src.size() && (src[pos] == ' ' || src[pos] == '\t' || src[pos] == '\n' || src[pos] == '\r')) pos++;
    if (pos >= src.size()) throw std::runtime_error("Unterminated source");
    if (src[pos] == '"') {
      size_t e = src.find('"', pos+1);
      if (e == std::string::npos) throw std::runtime_error("Unterminated quote");
      std::string r = src.substr(pos, e-pos+1);
      pos = e + 1;
      return r;
    }
    size_t s = pos;
    while (pos < src.size() && (isalnum((unsigned char)src[pos]) || src[pos] == '_' || src[pos] == '$' || src[pos] == '#')) pos++;
    if (pos == s) pos++;
    std::string r = src.substr(s, pos-s);
    for (auto& c : r) c = toupper((unsigned char)c);
    return r;
  };

  // Find "CREATE"
  size_t cpos = src.find("CREATE");
  if (cpos == std::string::npos) throw std::runtime_error("No CREATE found");

  // Parse header manually
  size_t hp = cpos;
  std::string tok;
  std::function<std::string()> next_tok;
  next_tok = [&]() -> std::string {
    while (hp < src.size() && (src[hp] == ' ' || src[hp] == '\t' || src[hp] == '\n' || src[hp] == '\r')) hp++;
    if (hp >= src.size()) return {};
    if (src[hp] == '"') {
      size_t e = src.find('"', hp+1);
      if (e == std::string::npos) return {};
      std::string r = src.substr(hp, e-hp+1);
      hp = e + 1;
      return r;
    }
    // Handle comments
    if (hp + 1 < src.size() && src[hp] == '/' && src[hp+1] == '*') {
      size_t e = src.find("*/", hp+2);
      if (e == std::string::npos) return {};
      hp = e + 2;
      return next_tok();
    }
    if (hp + 1 < src.size() && src[hp] == '-' && src[hp+1] == '-') {
      size_t e = src.find('\n', hp+2);
      if (e == std::string::npos) hp = src.size();
      else hp = e + 1;
      return next_tok();
    }
    size_t s = hp;
    while (hp < src.size() && (isalnum((unsigned char)src[hp]) || src[hp] == '_' || src[hp] == '$' || src[hp] == '#')) hp++;
    if (hp == s) hp++;
    std::string r = src.substr(s, hp-s);
    for (auto& c : r) c = toupper((unsigned char)c);
    return r;
  };

  std::string t = next_tok();
  if (t != "CREATE") throw std::runtime_error("Expected CREATE");

  header_start = hp;
  unit_type = next_tok();
  if (unit_type == "OR") {
    t = next_tok();
    if (t != "REPLACE") throw std::runtime_error("Expected REPLACE");
    unit_type = next_tok();
  }

  unit_name = next_tok();
  if (unit_name == "BODY") {
    unit_type += " BODY";
    unit_name = next_tok();
  }
  if (!unit_name.empty() && unit_name[0] == '"') {
    unit_name = unit_name.substr(1, unit_name.size()-2);
  } else {
    for (auto& c : unit_name) c = toupper((unsigned char)c);
  }

  t = next_tok();
  if (t != "WRAPPED") throw std::runtime_error("Expected WRAPPED");

  // Find the last token on the WRAPPED line
  // After WRAPPED, there should be "0" and "abcd"
  // Actually the line is like: ... WRAPPED 0 abcd
  t = next_tok();
  if (t != "0") throw std::runtime_error("Expected 0 after WRAPPED");
  t = next_tok();
  // t should be "ABCD"
  header_end = hp - (int)t.size() - 1;

  // Now parse lines until we find the version line (7 digits)
  auto get_line = [&]() -> std::string {
    std::string r;
    while (hp < src.size() && src[hp] != '\n') {
      if (src[hp] != '\r') r += src[hp];
      hp++;
    }
    if (hp < src.size()) hp++; // skip \n
    return r;
  };

  // Skip lines until we find the version line
  std::string line;
  while (hp < src.size()) {
    line = trim(get_line());
    if (line.empty()) continue;
    // Version line is exactly 7 digits
    if (line.size() == 7 && line.find_first_not_of("0123456789") == std::string::npos) {
      wrap_version = std::stoi(line);
      break;
    }
  }

  if (wrap_version < 8000000 || wrap_version >= 9300000)
    throw std::runtime_error("Unsupported version");

  // Three separator lines: "1", "4", "0"
  line = trim(get_line());
  line = trim(get_line());
  line = trim(get_line());

  // Parse lexicon
  auto parse_lexicon = [&]() {
    line = trim(get_line());
    if (line.empty()) line = trim(get_line());
    int size = (int)strtol(line.c_str(), nullptr, 16);

    line = trim(get_line());
    if (line == "2 :e:") {
      // New style lexicon
      line = trim(get_line());
      bool join = false;
      while (line != "0") {
        if (line.empty()) { line = trim(get_line()); continue; }
        if (line[0] != '1' || (line.back() != ':' && line.back() != '+'))
          throw std::runtime_error("Bad lexicon line");
        std::string text;
        size_t cp = 1;
        while (cp < line.size() - 1) {
          if (line[cp] == ':') {
            if (cp + 1 < line.size()) {
              if (line[cp+1] == 'n') text += '\n';
              else if (line[cp+1] == ':') text += ':';
              else text += line.substr(cp, 2);
              cp += 2;
            } else { text += ':'; cp++; }
          } else { text += line[cp]; cp++; }
        }
        if (join && !lexical_str_tbl.empty()) {
          auto last = lexical_str_tbl.rbegin();
          lexical_str_tbl[last->first] = last->second + text;
        } else {
          lexical_str_tbl[lexical_str_tbl.size() + 1] = text;
        }
        join = (line.back() == '+');
        line = trim(get_line());
      }
    } else {
      // Old style lexicon
      bool join = false;
      while (line != "0") {
        if (line.empty()) { line = trim(get_line()); continue; }
        if (!join && line.size() >= 2 && line[0] == '0' && line[1] == ' ') {
          lexical_str_tbl[lexical_str_tbl.size() + 1] = "";
          line = line.substr(2);
        }
        size_t sp = join ? 0 : line.find(' ');
        size_t cp;
        if (join) cp = 0;
        else cp = sp + 1;
        int chars_left = join ? 0 : (int)strtol(line.substr(0, sp).c_str(), nullptr, 16);
        std::string text;
        while (cp < line.size() - 1) {
          if (line[cp] == ':') {
            if (cp + 1 < line.size()) {
              if (line[cp+1] == 'n') text += '\n';
              else if (line[cp+1] == ':') text += ':';
              else text += line.substr(cp, 2);
              cp += 2;
            } else { text += ':'; cp++; }
          } else { text += line[cp]; cp++; }
        }
        if (join && !lexical_str_tbl.empty()) {
          auto last = lexical_str_tbl.rbegin();
          lexical_str_tbl[last->first] = last->second + text;
        } else {
          lexical_str_tbl[lexical_str_tbl.size() + 1] = text;
        }
        chars_left -= (int)text.size();
        join = chars_left > 0;
        line = trim(get_line());
      }
    }
    if ((int)lexical_str_tbl.size() != size)
      throw std::runtime_error("Lexicon size mismatch");
  };
  parse_lexicon();

  // Skip "0" and "0" lines
  line = trim(get_line());
  if (line.empty()) line = trim(get_line());
  line = trim(get_line());

  // Parse DIANA tables
  auto parse_table = [&](std::map<int,int>& tbl) {
    line = trim(get_line());
    if (line.empty()) line = trim(get_line());
    int size = (int)strtol(line.c_str(), nullptr, 16);
    line = trim(get_line());
    if (line != "2" && line != "4") throw std::runtime_error("Expected 2 or 4");
    int idx = 1;
    int repeat = 1;
    while ((int)tbl.size() < size) {
      // Read tokens
      while (hp < src.size() && (src[hp] == ' ' || src[hp] == '\t' || src[hp] == '\n' || src[hp] == '\r')) hp++;
      if (hp >= src.size()) throw std::runtime_error("Unexpected EOF in table");
      std::string tok;
      size_t tk_start = hp;
      while (hp < src.size() && src[hp] != ' ' && src[hp] != '\t' && src[hp] != '\n' && src[hp] != '\r') hp++;
      tok = src.substr(tk_start, hp - tk_start);
      if (tok.empty()) continue;
      if (tok[0] == ':') {
        repeat = (int)strtol(tok.c_str() + 1, nullptr, 16);
      } else {
        int val = (int)strtol(tok.c_str(), nullptr, 16);
        for (int i = 0; i < repeat && (int)tbl.size() < size; i++) {
          tbl[idx++] = val;
        }
        repeat = 1;
      }
    }
  };
  parse_table(node_tbl);
  parse_table(attr_ref_tbl);
  parse_table(column_tbl);
  parse_table(line_tbl);
  parse_table(attr_tbl);
  parse_table(as_list_tbl);

  if (node_tbl.size() != attr_ref_tbl.size() || node_tbl.size() != column_tbl.size() || node_tbl.size() != line_tbl.size())
    throw std::runtime_error("Table size mismatch");

  // Early version fixup
  if (wrap_version <= 8105000) {
    for (auto& [k, v] : node_tbl) {
      if (v >= 4096) {
        auto ait = attr_ref_tbl.find(k);
        if (ait != attr_ref_tbl.end()) {
          ait->second += (v / 4096) * 65536;
        }
        v = v % 4096;
      }
    }
  }

  // Epilogue: "1", "4", "0" followed by root node
  auto get_tok2 = [&]() -> std::string {
    while (hp < src.size() && (src[hp] == ' ' || src[hp] == '\t' || src[hp] == '\n' || src[hp] == '\r')) hp++;
    if (hp >= src.size()) return {};
    size_t s = hp;
    while (hp < src.size() && src[hp] != ' ' && src[hp] != '\t' && src[hp] != '\n' && src[hp] != '\r') hp++;
    return src.substr(s, hp-s);
  };
  std::string e1 = get_tok2(), e2 = get_tok2(), e3 = get_tok2();
  if (e1 != "1" || e2 != "4" || e3 != "0") throw std::runtime_error("Bad epilogue start");
  std::string rts = get_tok2();
  root_idx = (int)strtol(rts.c_str(), nullptr, 16);
  if (node_tbl.find(root_idx) == node_tbl.end()) throw std::runtime_error("Root node not found");
  if (get_node_type(root_idx) != D_COMP_U) throw std::runtime_error("Root must be D_COMP_U");
}

std::string V1State::unwrap(const std::string& src) {
  invalid_ref_f = false; unknown_attr_f = false;
  infinite_loop_f = false; meta_mismatch_f = false;

  stack_reset();
  try {
    parse_tree(src);
  } catch (std::exception& e) {
    return "--- Warning: " + std::string(e.what()) + "\n" + src;
  }

  emit_init();

  if (runnable_f) output("CREATE OR REPLACE ");

  // Copy header from original source
  emit(src.substr(header_start, header_end - header_start + 1));

  do_node(root_idx);

  if (!prior_buffer.empty()) output(prior_buffer + (!next_buffer.empty() ? "\n" : ""));
  if (!next_buffer.empty()) output(next_buffer);

  if (runnable_f) output("\n/\n");
  emit_flush();

  std::string result = unwrapped;

  if (line_endings == 1) { // DOS
    for (auto& c : result) if (c == '\n') c = '\r';
    // Actually need to replace properly
    std::string r2;
    for (auto c : result) { if (c == '\n') r2 += "\r\n"; else if (c != '\r') r2 += c; }
    result = r2;
  } else if (line_endings == 2) { // UNIX
    std::string r2;
    for (auto c : result) if (c != '\r') r2 += c;
    result = r2;
  }

  return result;
}
