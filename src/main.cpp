/* PL/SQL Unwrap — standalone binary to unwrap/wrap Oracle PL/SQL source code
 *
 * Original PL/SQL project : https://github.com/oddz/PL-SQL-Unwrapper
 * Original author          : Cameron Marshall
 * C++ port                 : Manuel FLURY
 * Copyright (C) 2026       : Manuel FLURY
 * License                  : GNU General Public License v3.0
 */

#define VERSION "3.0"

/* Maximum input file size: 128 MB */
#define MAX_INPUT_SIZE (128UL * 1024 * 1024)

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <cctype>
#include <unistd.h>
#include <termios.h>
#include "unwrap_v2.h"
#include "wrap_v2.h"
#include "obfuscate.h"
#include "unwrap_v1.h"
#include "embedded.h"

/* ------------------------------------------------------------------ */
/*  Help text                                                          */
/* ------------------------------------------------------------------ */
static void usage_unwrap(const char* prog) {
  std::cerr
    << "Usage: " << prog << " [options] [-i <file>] [-o <file>]\n"
    << "Unwrap wrapped PL/SQL source  v" VERSION "  Copyright (C) 2026  Manuel FLURY\n"
    << "  -i, --input <file>    input file (stdin if piped)\n"
    << "  -o, --output <file>   output file (stdout if omitted)\n"
    << "  --v1                  force V1 unwrapper (8/8i/9i — UNTESTED)\n"
    << "  --v2                  force V2 unwrapper (10g+)\n"
    << "  -l, --license         show license information\n"
    << "  --readme              show embedded README\n"
    << "  -V, --version         show version and author\n"
    << "  -h, --help            show this help\n"
    << "\nNote: V1 unwrapper is ported but untested with real V1 wrapped files.\n"
    << "Maximum input file size: 128 MB.\n";
}

static void usage_wrap(const char* prog) {
  std::cerr
    << "Usage: " << prog << " [options] -i <file> [-o <file>]\n"
    << "Wrap PL/SQL source (V2 method)  v" VERSION "  Copyright (C) 2026  Manuel FLURY\n"
    << "  -i, --input <file>        input file (required)\n"
    << "  -o, --output <file>       output file (default: <input>.pls)\n"
    << "  --keep-comments           preserve comments (default: strip them)\n"
    << "  -l, --license             show license information\n"
    << "  --readme                  show embedded README\n"
    << "  -V, --version             show version and author\n"
    << "  -h, --help                show this help\n"
    << "\nOracle-compatible syntax also accepted:\n"
    << "  iname=<file>  oname=<file>  keep_comments=yes  help=yes\n"
    << "Maximum input file size: 128 MB.\n";
}

static void usage_obf(const char* prog) {
  std::cerr
    << "Usage: " << prog << " --obfuscate|--deobfuscate -i <file> [-o <file>]\n"
    << "Obfuscate or restore PL/SQL source code.\n"
    << "  --obfuscate, --obf      rename identifiers to short names\n"
    << "  --deobfuscate, --deobf  restore original names (needs passphrase)\n"
    << "  -p, --passphrase <key>  encryption passphrase\n"
    << "  -i, --input <file>      input file\n"
    << "  -o, --output <file>     output file (stdout if omitted)\n"
    << "  -l, --license           show license information\n"
    << "  -h, --help              show this help\n"
    << "\nIf no passphrase is given, you will be prompted.\n";
}

/* ------------------------------------------------------------------ */
/*  Case-insensitive string helper                                     */
/* ------------------------------------------------------------------ */
static std::string to_upper(const std::string& s) {
  std::string r = s;
  for (auto& c : r) c = toupper((unsigned char)c);
  return r;
}

/* ------------------------------------------------------------------ */
/*  Wrapped-source detection helpers                                   */
/* ------------------------------------------------------------------ */
static bool is_v2_at(const std::string& s, size_t wrapped_pos) {
  auto rest = s.substr(wrapped_pos + 7);
  size_t i = 0;
  while (i < rest.size() && (rest[i] == ' ' || rest[i] == '\t'
         || rest[i] == '\n' || rest[i] == '\r')) i++;
  return i + 7 <= rest.size() && rest.substr(i, 7) == "a000000";
}

static bool is_v1_at(const std::string& s, size_t wrapped_pos) {
  auto rest = s.substr(wrapped_pos + 7);
  size_t i = 0;
  while (i < rest.size() && (rest[i] == ' ' || rest[i] == '\t'
         || rest[i] == '\n' || rest[i] == '\r')) i++;
  return i < rest.size() && rest[i] == '0';
}

static size_t find_next_wrapped(const std::string& s, size_t pos, bool& is_v2) {
  auto up = to_upper(s);
  while (pos < s.size()) {
    auto p = up.find("WRAPPED", pos);
    if (p == std::string::npos) return std::string::npos;
    if (is_v2_at(s, p)) { is_v2 = true; return p; }
    if (is_v1_at(s, p)) { is_v2 = false; return p; }
    pos = p + 1;
  }
  return std::string::npos;
}

static size_t find_unit_start(const std::string& s, size_t wrapped_pos) {
  size_t search_start = wrapped_pos;
  for (int i = 0; i < 4; i++) {
    auto prev_nl = s.rfind('\n', search_start - 1);
    if (prev_nl == std::string::npos) prev_nl = 0;
    else prev_nl++;
    auto line = s.substr(prev_nl, search_start - prev_nl);
    auto trimmed = line;
    while (!trimmed.empty() && (trimmed[0] == ' ' || trimmed[0] == '\t'))
      trimmed.erase(0, 1);
    if (to_upper(trimmed).find("CREATE") == 0) return prev_nl;
    if (prev_nl == 0) break;
    search_start = prev_nl;
  }
  auto nl = s.rfind('\n', wrapped_pos);
  return (nl == std::string::npos) ? 0 : nl + 1;
}

static size_t find_unit_end(const std::string& s, size_t unit_start) {
  auto search_from = unit_start + 1;
  while (search_from < s.size()) {
    auto p = to_upper(s).find("CREATE", search_from);
    if (p == std::string::npos) return s.size();
    if (p == 0 || s[p-1] == '\n') return p;
    search_from = p + 1;
  }
  return s.size();
}

/* ------------------------------------------------------------------ */
/*  I/O helpers                                                        */
/* ------------------------------------------------------------------ */
static std::string read_file(const std::string& path) {
  std::ifstream f(path, std::ios::binary | std::ios::ate);
  if (!f) { std::cerr << "Error: cannot open " << path << "\n"; exit(1); }
  auto size = f.tellg();
  if (size > (std::streamoff)MAX_INPUT_SIZE) {
    std::cerr << "Error: file too large (" << size << " bytes). "
              << "Maximum supported: " << MAX_INPUT_SIZE
              << " bytes (128 MB).\n";
    exit(1);
  }
  f.seekg(0);
  std::ostringstream oss;
  oss << f.rdbuf();
  return oss.str();
}

static void write_file(const std::string& path, const std::string& content) {
  std::ofstream f(path, std::ios::binary);
  if (!f) { std::cerr << "Error: cannot write " << path << "\n"; exit(1); }
  f << content;
}

static std::string prog_name(const char* argv0) {
  std::string s = argv0;
  auto p = s.rfind('/');
  return (p == std::string::npos) ? s : s.substr(p + 1);
}

static bool match_kv(const std::string& arg, const std::string& key,
                      std::string& val) {
  if (arg.size() <= key.size() || arg[key.size()] != '=') return false;
  auto prefix = arg.substr(0, key.size());
  for (auto& c : prefix) c = tolower((unsigned char)c);
  if (prefix != key) return false;
  val = arg.substr(key.size() + 1);
  return true;
}

/* Prompt for passphrase without echoing (portable termios version). */
static std::string prompt_passphrase() {
  std::string p;
  std::cerr << "Enter passphrase: ";
  std::cerr.flush();
  struct termios old, newt;
  if (tcgetattr(STDIN_FILENO, &old) == 0) {
    newt = old;
    newt.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    std::getline(std::cin, p);
    tcsetattr(STDIN_FILENO, TCSANOW, &old);
  }
  std::cerr << "\n";
  return p;
}

/* ------------------------------------------------------------------ */
/*  Main entry point                                                   */
/* ------------------------------------------------------------------ */
int main(int argc, char* argv[]) {
  std::string inpath, outpath, passphrase;
  bool force_v1 = false, force_v2 = false, do_wrap = false;
  bool do_obfuscate = false, do_deobfuscate = false;
  bool keep_comments = false;

  auto name = prog_name(argv[0]);
  bool is_wrap_symlink = (name == "wrap" || name == "wrap_v2");

  /* ---- Parse command-line arguments ---- */
  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];

    /* Help / info */
    if (a == "--help" || a == "-h") {
      if (do_obfuscate || do_deobfuscate) usage_obf(argv[0]);
      else if (is_wrap_symlink || do_wrap) usage_wrap(argv[0]);
      else usage_unwrap(argv[0]);
      return 0;
    }
    if (a == "--license" || a == "-l") {
      std::cout << EMBEDDED_LICENSE; return 0;
    }
    if (a == "--readme" || a == "--doc") {
      std::cout << EMBEDDED_README; return 0;
    }
    if (a == "--version" || a == "-V") {
      std::cerr << "PL/SQL Unwrap v" VERSION "  Copyright (C) 2026  Manuel FLURY\n"
                << "C++ port of the PL/SQL Unwrapper by Cameron Marshall\n"
                << "https://github.com/oddz/PL-SQL-Unwrapper\n";
      return 0;
    }

    /* Obfuscation */
    if (a == "--obfuscate" || a == "--obf") { do_obfuscate = true; continue; }
    if (a == "--deobfuscate" || a == "--deobf") { do_deobfuscate = true; continue; }
    if ((a == "--passphrase" || a == "-p") && i + 1 < argc) {
      passphrase = argv[++i]; continue;
    }

    /* Wrap-mode flags */
    if (a == "--keep-comments" || a == "--keepcomments") {
      keep_comments = true; continue;
    }
    if (a == "--wrap") { do_wrap = true; continue; }

    /* Unwrap-mode flags */
    if (a == "--v1") { force_v1 = true; continue; }
    if (a == "--v2") { force_v2 = true; continue; }

    /* Input / output */
    if ((a == "-i" || a == "--input" || a == "--inputfile"
         || a == "--input-file") && i + 1 < argc) {
      inpath = argv[++i]; continue;
    }
    if ((a == "-o" || a == "--output" || a == "--outputfile"
         || a == "--output-file") && i + 1 < argc) {
      outpath = argv[++i]; continue;
    }

    /* Oracle-compatible key=value syntax */
    std::string kv;
    if (match_kv(a, "iname", kv)) { inpath = kv; continue; }
    if (match_kv(a, "oname", kv)) { outpath = kv; continue; }
    if (match_kv(a, "help", kv)) {
      for (auto& c : kv) c = tolower((unsigned char)c);
      if (kv == "yes" || kv == "y" || kv == "true" || kv == "1") {
        if (do_obfuscate || do_deobfuscate) usage_obf(argv[0]);
        else if (is_wrap_symlink || do_wrap) usage_wrap(argv[0]);
        else usage_unwrap(argv[0]);
        return 0;
      }
      continue;
    }
    if (match_kv(a, "keep_comments", kv)) {
      for (auto& c : kv) c = tolower((unsigned char)c);
      keep_comments = (kv == "yes" || kv == "true" || kv == "1");
      continue;
    }

    if (inpath.empty() && a[0] != '-') { inpath = a; continue; }
    std::cerr << "Unknown option: " << a << "\n";
    return 1;
  }

  if (is_wrap_symlink) do_wrap = true;

  if (do_wrap && outpath.empty() && !inpath.empty()) {
    auto p = inpath.rfind('.');
    outpath = (p == std::string::npos)
                ? inpath + ".pls"
                : inpath.substr(0, p) + ".pls";
  }

  /* ---- Read input ---- */
  std::string input;
  if (!inpath.empty()) {
    input = read_file(inpath);
  } else if (!isatty(STDIN_FILENO)) {
    std::ostringstream oss;
    oss << std::cin.rdbuf();
    input = oss.str();
  } else {
    if (do_obfuscate || do_deobfuscate) usage_obf(argv[0]);
    else if (do_wrap) usage_wrap(argv[0]);
    else usage_unwrap(argv[0]);
    return 1;
  }

  /* ---- Prompt for passphrase if needed ---- */
  if ((do_obfuscate || do_deobfuscate) && passphrase.empty()) {
    passphrase = prompt_passphrase();
    if (passphrase.empty()) {
      std::cerr << "Error: passphrase is required.\n";
      return 1;
    }
  }

  /* ---- Process ---- */
  std::string result;

  if (do_obfuscate) {
    /* Obfuscate the entire input (single --ENC section at end). */
    auto obf = obfuscate_plsql(input, passphrase, keep_comments);
    if (!obf.empty()) {
      /* Split at --ENC: to separate source from ENC data */
      auto enc_pos = obf.rfind("--ENC:\n");
      if (enc_pos != std::string::npos) {
        std::string obf_src = obf.substr(0, enc_pos);
        std::string enc_part = obf.substr(enc_pos);
        /* Wrap each CREATE unit independently, preserve DDL/DML between. */
        size_t cursor = 0;
        while (true) {
          auto cr = to_upper(obf_src).find("CREATE", cursor);
          if (cr == std::string::npos) {
            result.append(obf_src, cursor, obf_src.size() - cursor);
            break;
          }
          /* Find unit boundaries */
          size_t unit_start = cr;
          /* Walk back to line start */
          while (unit_start > cursor && obf_src[unit_start-1] != '\n') unit_start--;
          size_t unit_end = obf_src.size();
          /* Find end: next CREATE at start of line, or EOF */
          auto next_cr = to_upper(obf_src).find("CREATE", cr + 1);
          while (next_cr != std::string::npos) {
            if (next_cr == 0 || obf_src[next_cr-1] == '\n') {
              unit_end = next_cr;
              break;
            }
            next_cr = to_upper(obf_src).find("CREATE", next_cr + 1);
          }
          /* Preserve content before this unit */
          result.append(obf_src, cursor, unit_start - cursor);
          /* Wrap this unit independently */
          auto unit = obf_src.substr(unit_start, unit_end - unit_start);
          result += wrap_v2(unit, keep_comments, true);
          cursor = unit_end;
        }
        /* Append the --ENC: section at the end */
        result += "\n" + enc_part;
      } else {
        result = wrap_v2(obf, keep_comments, true);
      }
    }
  } else if (do_deobfuscate) {
    /* Deobfuscate: strip ENC from end, unwrap, restore names */
    auto enc_pos = input.rfind("--ENC:\n");
    if (enc_pos == std::string::npos) {
      std::cerr << "Error: no --ENC: obfuscation data found.\n";
      result.clear();
    } else {
      std::string to_unwrap = input.substr(0, enc_pos);
      std::string enc_suffix = input.substr(enc_pos);
      /* Unwrap each section */
      std::string unwrapped;
      bool dummy;
      if (find_next_wrapped(to_unwrap, 0, dummy) != std::string::npos) {
        size_t cursor = 0;
        bool is_v2 = false;
        while (true) {
          auto wpos = find_next_wrapped(to_unwrap, cursor, is_v2);
          if (wpos == std::string::npos) {
            unwrapped.append(to_unwrap, cursor, to_unwrap.size() - cursor);
            break;
          }
          auto unit_start = find_unit_start(to_unwrap, wpos);
          auto unit_end   = find_unit_end(to_unwrap, unit_start);
          unwrapped.append(to_unwrap, cursor, unit_start - cursor);
          auto wp = to_unwrap.substr(unit_start, unit_end - unit_start);
          unwrapped += is_v2 ? unwrap_v2(wp) : unwrap_v1(wp);
          cursor = unit_end;
        }
      } else {
        unwrapped = to_unwrap;
      }
      if (!unwrapped.empty()) {
        result = deobfuscate_plsql(unwrapped + "\n" + enc_suffix, passphrase);
      }
    }
  } else if (do_wrap) {
    result = wrap_v2(input, keep_comments);
  } else if (force_v1) {
    result = unwrap_v1(input);
  } else if (force_v2) {
    result = unwrap_v2(input);
  } else {
    /* Auto-detect: unwrap all wrapped sections.
     * Pre-strip any trailing --ENC: data so it doesn't interfere. */
    std::string process_input = input;
    std::string enc_suffix;
    {
      auto enc_pos = input.rfind("--ENC:\n");
      if (enc_pos != std::string::npos) {
        /* The --ENC: is always at the end, after the last wrapped section */
        enc_suffix = input.substr(enc_pos);
        process_input = input.substr(0, enc_pos);
      }
    }
    size_t cursor = 0;
    bool is_v2 = false;
    while (true) {
      auto wpos = find_next_wrapped(process_input, cursor, is_v2);
      if (wpos == std::string::npos) {
        result.append(process_input, cursor, process_input.size() - cursor);
        break;
      }
      auto unit_start = find_unit_start(process_input, wpos);
      auto unit_end   = find_unit_end(process_input, unit_start);
      result.append(process_input, cursor, unit_start - cursor);
      auto wrapped_part = process_input.substr(unit_start, unit_end - unit_start);
      result += is_v2 ? unwrap_v2(wrapped_part) : unwrap_v1(wrapped_part);
      cursor = unit_end;
    }
    /* If a passphrase was given and --ENC: data found, auto-deobfuscate */
    if (!passphrase.empty() && !enc_suffix.empty()) {
      auto r2 = deobfuscate_plsql(result + "\n" + enc_suffix, passphrase);
      if (!r2.empty()) result = r2;
    }
  }

  /* ---- Output ---- */
  if (!outpath.empty()) {
    write_file(outpath, result);
  } else if (!result.empty()) {
    std::cout << result;
  }

  return result.empty() ? 1 : 0;
}
