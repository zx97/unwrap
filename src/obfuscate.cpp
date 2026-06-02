/* PL/SQL obfuscator / deobfuscator
 *
 * Obfuscation renames local identifiers to very short names (a, b, c, …),
 * strips all comments, and embeds an AES-256-CBC encrypted mapping that
 * allows recovering the original source with the correct passphrase.
 */

#include "obfuscate.h"
#include <iostream>
#include <sstream>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <cstring>
#include <cctype>
#include <openssl/evp.h>
#include <openssl/rand.h>

/* ------------------------------------------------------------------ */
/*  PL/SQL reserved words — never rename these                         */
/* ------------------------------------------------------------------ */
static const char* RESERVED[] = {
  /* Oracle 23ai PL/SQL Reserved Words (Table D-1) + Keywords (Table D-2) */
  "A", "ADD", "ACCESSIBLE", "AGENT", "AGGREGATE", "ALL", "ALTER", "AND",
  "ANY", "ARRAY", "AS", "ASC", "AT", "ATTRIBUTE", "AUTHID", "AVG",
  "BEGIN", "BETWEEN", "BFILE_BASE", "BINARY", "BLOB_BASE", "BLOCK",
  "BODY", "BOOLEAN", "BOTH", "BOUND", "BULK", "BY", "BYTE",
  "C", "CALL", "CALLING", "CASCADE", "CASE", "CHAR", "CHAR_BASE",
  "CHARACTER", "CHARSET", "CHARSETFORM", "CHARSETID", "CHECK",
  "CLOB_BASE", "CLONE", "CLOSE", "CLUSTER", "CLUSTERS", "COLAUTH",
  "COLLECT", "COLUMNS", "COMMENT", "COMMIT", "COMMITTED", "COMPILED",
  "COMPRESS", "CONNECT", "CONSTANT", "CONSTRUCTOR", "CONTEXT",
  "CONTINUE", "CONVERT", "COUNT", "CRASH", "CREATE", "CREDENTIAL",
  "CURRENT", "CURSOR", "CUSTOMDATUM",
  "DANGLING", "DATA", "DATE", "DATE_BASE", "DAY", "DEFINE", "DECLARE",
  "DEFAULT", "DELETE", "DESC", "DETERMINISTIC", "DIRECTORY", "DISTINCT",
  "DOUBLE", "DROP", "DURATION",
  "ELEMENT", "ELSE", "ELSIF", "EMPTY", "END", "EDITIONABLE", "ESCAPE",
  "EXCEPT", "EXCEPTION", "EXCEPTIONS", "EXCLUSIVE", "EXECUTE", "EXISTS",
  "EXIT", "EXTERNAL", "EXTRACT",
  "FALSE", "FETCH", "FINAL", "FIRST", "FIXED", "FLOAT", "FOR", "FORALL",
  "FORCE", "FOREIGN", "FROM", "FUNCTION",
  "GENERAL", "GOTO", "GRANT", "GROUP",
  "HASH", "HAVING", "HEAP", "HIDDEN", "HOUR",
  "IDENTIFIED", "IF", "IMMEDIATE", "IMMUTABLE", "IN", "INCLUDING",
  "INDEX", "INDEXES", "INDICATOR", "INDICES", "INFINITE", "INNER",
  "INSERT", "INSTANTIABLE", "INT", "INTERFACE", "INTERSECT",
  "INTERVAL", "INTO", "INVALIDATE", "IS", "ISOLATION",
  "JAVA",
  "KEY",
  "LANGUAGE", "LARGE", "LEADING", "LENGTH", "LEVEL", "LIBRARY",
  "LIKE", "LIKE2", "LIKE4", "LIKEC", "LIMIT", "LIMITED", "LOCAL",
  "LOCK", "LONG", "LOOP",
  "MAP", "MAX", "MAXLEN", "MEMBER", "MERGE", "MIN", "MINUS", "MINUTE",
  "MOD", "MODE", "MODIFY", "MONTH", "MULTISET", "MUTABLE",
  "NAME", "NAN", "NATIONAL", "NATIVE", "NATURAL", "NCHAR", "NEW",
  "NOCOMPRESS", "NOCOPY", "NOT", "NOWAIT", "NULL", "NUMBER",
  "NUMBER_BASE",
  "OBJECT", "OCICOLL", "OCIDATE", "OCIDATETIME", "OCIDURATION",
  "OCIINTERVAL", "OCILOBLOCATOR", "OCINUMBER", "OCIRAW", "OCIREF",
  "OCIREFCURSOR", "OCIROWID", "OCISTRING", "OCITYPE", "OF", "OLD",
  "ON", "ONLY", "OPAQUE", "OPEN", "OPERATOR", "OPTION", "OR",
  "ORACLE", "ORADATA", "ORDER", "ORGANIZATION", "ORLANY", "ORLVARY",
  "OTHERS", "OUT", "OVERLAPS", "OVERRIDING",
  "PACKAGE", "PARALLEL_ENABLE", "PARAMETER", "PARAMETERS", "PARENT",
  "PARTITION", "PASCAL", "PERSISTABLE", "PIPE", "PIPELINED",
  "PLUGGABLE", "POLYMORPHIC", "PRAGMA", "PRECISION", "PRIOR",
  "PRIVATE", "PROCEDURE", "PUBLIC",
  "RAISE", "RANGE", "RAW", "READ", "RECORD", "REF", "REFERENCE",
  "RELIES_ON", "REM", "REMAINDER", "RENAME", "REPLACE",
  "RESOURCE", "RESTRICT_REFERENCES",
  "RESULT", "RESULT_CACHE", "RETURN", "RETURNING", "REVERSE", "REVOKE",
  "ROLLBACK", "ROW", "ROWNUM",
  "SAMPLE", "SAVE", "SAVEPOINT", "SB1", "SB2", "SB4", "SECOND",
  "SEGMENT", "SELECT", "SELF", "SEPARATE", "SEQUENCE",
  "SERIALIZABLE", "SESSION", "SET", "SHARE", "SHORT", "SIZE",
  "SIZE_T", "SOME", "SPARSE", "SQL", "SQLCODE", "SQLDATA", "SQLNAME",
  "SQLSTATE", "STANDARD", "START", "STATIC", "STDDEV", "STORED",
  "STRING", "STRUCT", "STYLE", "SUBMULTISET", "SUBPARTITION",
  "SUBSTITUTABLE", "SUBTYPE", "SUM", "SYNONYM",
  "TABAUTH", "TABLE", "TDO", "THE", "THEN", "TIME", "TIMESTAMP",
  "TIMEZONE_ABBR", "TIMEZONE_HOUR", "TIMEZONE_MINUTE",
  "TIMEZONE_REGION", "TO", "TRAILING", "TRANSACTION",
  "TRANSACTIONAL", "TRIGGER", "TRUE", "TRUSTED", "TYPE",
  "UB1", "UB2", "UB4", "UNION", "UNIQUE", "UNPLUG", "UNSIGNED",
  "UNTRUSTED", "UPDATE", "USE", "USING",
  "VALIST", "VALUE", "VALUES", "VARIABLE", "VARIANCE", "VARRAY",
  "VARYING", "VIEW", "VIEWS", "VOID",
  "WHEN", "WHERE", "WHILE", "WITH", "WORK", "WRAPPED", "WRITE",
  "YEAR",
  "ZONE",

  /* Common Oracle SQL/PL/SQL built-in functions */
  "ABS", "ACOS", "ADD_MONTHS", "ASCII", "ASIN", "ATAN", "AVG",
  "BFILENAME", "BIN_TO_NUM",
  "CARDINALITY", "CAST", "CEIL", "CHARTOROWID", "CHR", "COALESCE",
  "COMPOSE", "CONCAT", "CONVERT", "CORR", "COS", "COSH", "COUNT",
  "COVAR_POP", "COVAR_SAMP", "CUBE_TABLE", "CUME_DIST", "CURRENT_DATE",
  "CURRENT_TIMESTAMP",
  "DBTIMEZONE", "DECODE", "DECOMPOSE", "DENSE_RANK", "DEPTH",
  "DEREF", "DUMP",
  "EMPTY_BLOB", "EMPTY_CLOB", "EXISTSNODE", "EXP", "EXTRACT",
  "FIRST_VALUE", "FLOOR", "FOLD_EQUIVALENT", "FROM_TZ",
  "GREATEST", "GROUP_ID", "GROUPING", "GROUPING_ID",
  "HEXTORAW",
  "INITCAP", "INSTR", "INSTRB",
  "LAG", "LAST_DAY", "LAST_VALUE", "LEAD", "LEAST", "LENGTH",
  "LENGTHB", "LISTAGG", "LN", "LOCALTIMESTAMP", "LOG", "LOWER",
  "LPAD", "LTRIM",
  "MAX", "MEDIAN", "MIN", "MOD", "MONTHS_BETWEEN",
  "NEW_TIME", "NEXT_DAY", "NLS_CHARSET_DECL_LEN", "NLS_INITCAP",
  "NLS_LOWER", "NLS_UPPER", "NLSSORT", "NTILE", "NULLIF",
  "NUMTODSINTERVAL", "NUMTOYMINTERVAL", "NVL", "NVL2",
  "ORA_HASH", "PATH", "PERCENT_RANK", "PERCENTILE_CONT",
  "PERCENTILE_DISC", "POWER", "POWERMULTISET", "PRESENTNNV",
  "PRESENTV", "PREVIOUS", "RANK", "RATIO_TO_REPORT", "RAWTOHEX",
  "RAWTONHEX", "REFCURSOR", "REGR_AVGX", "REGR_AVGY", "REGR_COUNT",
  "REGR_INTERCEPT", "REGR_R2", "REGR_SLOPE", "REGR_SXX", "REGR_SXY",
  "REGR_SYY", "REPLACE", "ROUND", "ROW_NUMBER", "ROWIDTOCHAR",
  "ROWIDTONCHAR", "RPAD", "RTRIM",
  "SESSIONTIMEZONE", "SIGN", "SIN", "SINH", "SOUNDEX", "SQRT",
  "STDDEV", "STDDEV_POP", "STDDEV_SAMP", "SUBSTR", "SUBSTRB",
  "SUM", "SYSDATE", "SYSTIMESTAMP",
  "TAN", "TANH", "TIMEZONE_ABBR", "TIMEZONE_OFFSET",
  "TO_BINARY_DOUBLE", "TO_BINARY_FLOAT", "TO_BLOB", "TO_CHAR",
  "TO_CLOB", "TO_DATE", "TO_DSINTERVAL", "TO_LOB", "TO_MULTI_BYTE",
  "TO_NCHAR", "TO_NCLOB", "TO_NUMBER", "TO_SINGLE_BYTE",
  "TO_TIMESTAMP", "TO_TIMESTAMP_TZ", "TO_YMINTERVAL", "TRANSLATE",
  "TREAT", "TRIM", "TRUNC", "TZ_OFFSET",
  "UID", "UNISTR", "UPDATEXML", "UPPER", "USER", "USERENV",
  "USTAT", "VARIANCE", "VAR_POP", "VAR_SAMP", "VSIZE",
  "WIDTH_BUCKET",
  "XMLAGG", "XMLCAST", "XMLCOLATTVAL", "XMLCOMMENT", "XMLCONCAT",
  "XMLDIFF", "XMLELEMENT", "XMLEXISTS", "XMLFOREST", "XMLPARSE",
  "XMLPATCH", "XMLPI", "XMLQUERY", "XMLROOT", "XMLSEQUENCE",
  "XMLSERIALIZE", "XMLTABLE", "XMLTRANSFORM",
  "ZONE",

  /* PL/SQL datatypes and additional SQL types */
  "BINARY_FLOAT", "BINARY_DOUBLE", "BINARY_INTEGER",
  "DECIMAL", "DOUBLE",
  "INTEGER", "INT",
  "NATIONAL", "NCHAR", "NCLOB", "NUMERIC", "NVARCHAR2",
  "PLS_INTEGER", "POSITIVE",
  "REAL", "ROWID",
  "SIMPLE_DOUBLE", "SIMPLE_FLOAT", "SIMPLE_INTEGER", "SMALLINT",
  "SYS_REFCURSOR",
  "UROWID",
  "VARCHAR", "VARCHAR2",
};

static std::set<std::string> reserved_set;

static void init_reserved() {
  if (reserved_set.empty()) {
    for (auto w : RESERVED) reserved_set.insert(w);
  }
}

/* ------------------------------------------------------------------ */
/*  Tokenizer helpers                                                  */
/* ------------------------------------------------------------------ */

/* Check if character can be part of a PL/SQL identifier */
static bool is_id_char(char c) {
  return isalnum((unsigned char)c) || c == '_' || c == '$' || c == '#';
}

/* Check if character starts a PL/SQL identifier */
static bool is_id_start(char c) {
  return isalpha((unsigned char)c) || c == '_';
}

/* Skip string literals and comments, return next meaningful position.
 * Returns the position of the next identifier-start character. */
static size_t skip_to_next_id(const std::string& s, size_t i) {
  while (i < s.size()) {
    /* String literal */
    if (s[i] == '\'') {
      i++;
      while (i < s.size()) {
        if (s[i] == '\'') {
          if (i + 1 < s.size() && s[i+1] == '\'') { i += 2; continue; }
          i++; break;
        }
        i++;
      }
      continue;
    }
    /* Single-line comment */
    if (s[i] == '-' && i + 1 < s.size() && s[i+1] == '-') {
      i += 2;
      while (i < s.size() && s[i] != '\n' && s[i] != '\r') i++;
      continue;
    }
    /* Multi-line comment */
    if (s[i] == '/' && i + 1 < s.size() && s[i+1] == '*') {
      i += 2;
      while (i + 1 < s.size() && !(s[i] == '*' && s[i+1] == '/')) i++;
      i += 2;
      continue;
    }
    /* Quoted identifier */
    if (s[i] == '"') {
      i++;
      while (i < s.size() && s[i] != '"') i++;
      i++;
      continue;
    }
    /* Skip non-identifier chars */
    if (!is_id_start(s[i])) { i++; continue; }
    break;
  }
  return i;
}

static std::string to_upper(const std::string& s) {
  std::string r = s;
  for (auto& c : r) c = toupper((unsigned char)c);
  return r;
}

/* ------------------------------------------------------------------ */
/*  Collect identifiers from source, filtering reserved words           */
/* ------------------------------------------------------------------ */
static std::map<std::string, int> collect_identifiers(const std::string& s) {
  init_reserved();
  std::map<std::string, int> freq;
  size_t i = 0;
  while (i < s.size()) {
    i = skip_to_next_id(s, i);
    if (i >= s.size() || !is_id_start(s[i])) break;
    size_t start = i;
    while (i < s.size() && is_id_char(s[i])) i++;
    auto word = s.substr(start, i - start);
    /* Uppercase for comparison with reserved list */
    auto upper = word;
    for (auto& c : upper) c = toupper((unsigned char)c);
    /* Skip reserved words, single-char names, numbers-like tokens */
    if (reserved_set.count(upper)) continue;
    if (word.size() <= 2) continue;           /* already short */
    if (isdigit((unsigned char)word[0])) continue;
    freq[word]++;
  }
  return freq;
}

/* ------------------------------------------------------------------ */
/*  Generate short-name mapping: longer/higher-frequency get shorter   */
/* ------------------------------------------------------------------ */
static std::map<std::string, std::string> make_mapping(
    const std::map<std::string, int>& freq,
    const std::set<std::string>& all_ids) {
  /* Sort by frequency descending, then by length descending */
  std::vector<std::pair<std::string, int>> sorted(freq.begin(), freq.end());
  std::sort(sorted.begin(), sorted.end(),
    [](const auto& a, const auto& b) {
      if (a.second != b.second) return a.second > b.second;
      if (a.first.size() != b.first.size()) return a.first.size() > b.first.size();
      return a.first > b.first;
    });

  /* Generate short names: a, b, ..., z, aa, ab, ..., zz, aaa, aab, ...
   * Skip names that collide with reserved words or existing identifiers. */
  std::map<std::string, std::string> mapping;

  size_t idx = 0;
  for (auto& [name, _] : sorted) {
    std::string short_name;
    while (true) {
      /* Convert idx to a short name like: a, b, ..., z, aa, ab, ... */
      short_name.clear();
      size_t n = idx;
      do {
        short_name += 'a' + (n % 26);
        n /= 26;
      } while (n > 0);
      idx++;
      /* Skip if collides with reserved words or existing identifiers */
      auto up = short_name;
      for (auto& c : up) c = toupper((unsigned char)c);
      if (!reserved_set.count(up) && !all_ids.count(up)) break;
    }
    mapping[name] = short_name;
  }
  return mapping;
}

/* ------------------------------------------------------------------ */
/*  Replace identifiers in source according to mapping                  */
/*  Preserves all non-identifier text (whitespace, keywords, etc).      */
/* ------------------------------------------------------------------ */
static std::string replace_identifiers(const std::string& s,
    const std::map<std::string, std::string>& mapping) {
  std::string out;
  size_t i = 0;
  while (i < s.size()) {
    /* Skip strings and comments, copy them verbatim */
    if (s[i] == '\'') {
      out += s[i++];
      while (i < s.size()) {
        out += s[i];
        if (s[i] == '\'') {
          if (i + 1 < s.size() && s[i+1] == '\'') { i++; out += s[i]; }
          else { i++; break; }
        }
        i++;
      }
      continue;
    }
    if (s[i] == '-' && i + 1 < s.size() && s[i+1] == '-') {
      out += s[i]; out += s[i+1]; i += 2;
      while (i < s.size() && s[i] != '\n' && s[i] != '\r') { out += s[i]; i++; }
      continue;
    }
    if (s[i] == '/' && i + 1 < s.size() && s[i+1] == '*') {
      out += s[i]; out += s[i+1]; i += 2;
      while (i + 1 < s.size() && !(s[i] == '*' && s[i+1] == '/')) { out += s[i]; i++; }
      if (i + 1 < s.size()) { out += s[i]; out += s[i+1]; i += 2; }
      continue;
    }
    if (s[i] == '"') {
      out += s[i++];
      while (i < s.size() && s[i] != '"') { out += s[i]; i++; }
      if (i < s.size()) { out += s[i]; i++; }
      continue;
    }
    /* Identifier starts — check for dotted reference first */
    if (is_id_start(s[i])) {
      size_t start = i;
      while (i < s.size() && is_id_char(s[i])) i++;
      auto word = s.substr(start, i - start);

      /* Check if this is part of a dotted reference (pkg.func) */
      bool dotted = false;
      {
        size_t peek = i;
        while (peek < s.size() && (s[peek] == ' ' || s[peek] == '\t')) peek++;
        if (peek < s.size() && s[peek] == '.') dotted = true;
      }

      if (dotted) {
        /* Keep the entire dotted chain unchanged */
        out += word;
        /* Skip the dot and following identifier(s) */
        while (i < s.size()) {
          if (s[i] == '.') { out += s[i++]; continue; }
          if (is_id_start(s[i])) {
            while (i < s.size() && is_id_char(s[i])) { out += s[i]; i++; }
            /* Peek for another dot */
            size_t peek = i;
            while (peek < s.size() && (s[peek] == ' ' || s[peek] == '\t')) peek++;
            if (!(peek < s.size() && s[peek] == '.')) break;
          } else break;
        }
      } else {
        auto it = mapping.find(word);
        out += (it != mapping.end()) ? it->second : word;
      }
      continue;
    }
    /* Everything else (whitespace, numbers, operators, etc.) */
    out += s[i++];
  }
  return out;
}

/* ------------------------------------------------------------------ */
/*  AES-256-CBC encryption / decryption using PBKDF2 key derivation    */
/*                                                                      *
/*  Format:  salt (16) + IV (16) + ciphertext                          *
/*  Key derivation: PBKDF2-HMAC-SHA256, 100000 iterations               */
/* ------------------------------------------------------------------ */

#define PBKDF2_ITERATIONS 100000
#define SALT_LEN 16

static std::vector<unsigned char> aes_encrypt(
    const std::string& plaintext, const std::string& passphrase) {
  /* Generate random salt and IV */
  unsigned char salt[SALT_LEN], iv[16], key[32];
  if (!RAND_bytes(salt, SALT_LEN)) return {};
  if (!RAND_bytes(iv, 16)) return {};

  /* Derive key via PBKDF2 */
  if (PKCS5_PBKDF2_HMAC(passphrase.data(), passphrase.size(),
        salt, SALT_LEN, PBKDF2_ITERATIONS,
        EVP_sha256(), 32, key) != 1) return {};

  EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
  if (!ctx) return {};
  if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key, iv) != 1) {
    EVP_CIPHER_CTX_free(ctx); return {};
  }

  std::vector<unsigned char> result(SALT_LEN + 16 + plaintext.size() + 16);
  int outlen = 0, tmplen = 0;

  /* Prepend salt + IV */
  memcpy(result.data(), salt, SALT_LEN);
  memcpy(result.data() + SALT_LEN, iv, 16);
  outlen = SALT_LEN + 16;

  if (EVP_EncryptUpdate(ctx, result.data() + outlen, &tmplen,
        (const unsigned char*)plaintext.data(), plaintext.size()) != 1) {
    EVP_CIPHER_CTX_free(ctx); return {};
  }
  outlen += tmplen;

  if (EVP_EncryptFinal_ex(ctx, result.data() + outlen, &tmplen) != 1) {
    EVP_CIPHER_CTX_free(ctx); return {};
  }
  outlen += tmplen;

  EVP_CIPHER_CTX_free(ctx);
  result.resize(outlen);
  return result;
}

static std::vector<unsigned char> aes_decrypt(
    const std::vector<unsigned char>& ciphertext,
    const std::string& passphrase) {
  if (ciphertext.size() < (size_t)(SALT_LEN + 16 + 16)) return {};

  unsigned char key[32];
  if (PKCS5_PBKDF2_HMAC(passphrase.data(), passphrase.size(),
        ciphertext.data(), SALT_LEN, PBKDF2_ITERATIONS,
        EVP_sha256(), 32, key) != 1) return {};

  EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
  if (!ctx) return {};
  if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key,
        ciphertext.data() + SALT_LEN) != 1) {
    EVP_CIPHER_CTX_free(ctx); return {};
  }

  std::vector<unsigned char> result(ciphertext.size());
  int outlen = 0, tmplen = 0;

  if (EVP_DecryptUpdate(ctx, result.data(), &tmplen,
        ciphertext.data() + SALT_LEN + 16,
        ciphertext.size() - SALT_LEN - 16) != 1) {
    EVP_CIPHER_CTX_free(ctx); return {};
  }
  outlen = tmplen;

  if (EVP_DecryptFinal_ex(ctx, result.data() + outlen, &tmplen) != 1) {
    EVP_CIPHER_CTX_free(ctx); return {};
  }
  outlen += tmplen;

  EVP_CIPHER_CTX_free(ctx);
  result.resize(outlen);
  return result;
}

/* ------------------------------------------------------------------ */
/*  Base64 (URL-safe variant, no padding issues)                       */
/* ------------------------------------------------------------------ */
static const char B64[] =
  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static std::string b64enc(const std::vector<unsigned char>& in) {
  std::string out;
  int val = 0, bits = -6;
  for (auto c : in) {
    val = (val << 8) | c;
    bits += 8;
    while (bits >= 0) { out += B64[(val >> bits) & 0x3F]; bits -= 6; }
  }
  if (bits > -6) out += B64[((val << 8) >> (bits + 8)) & 0x3F];
  while (out.size() % 4) out += '=';
  return out;
}

static std::vector<unsigned char> b64dec(const std::string& in) {
  unsigned char d[256];
  memset(d, 0xFF, 256);
  for (int i = 0; i < 64; i++) d[(unsigned char)B64[i]] = i;
  std::vector<unsigned char> out;
  int val = 0, bits = -8;
  for (auto c : in) {
    if (c == '=') break;
    if (d[(unsigned char)c] == 0xFF) continue;
    val = (val << 6) | d[(unsigned char)c];
    bits += 6;
    if (bits >= 0) { out.push_back((val >> bits) & 0xFF); bits -= 8; }
  }
  return out;
}

/* ------------------------------------------------------------------ */
/*  Build mapping string for encryption: "orig1=short1|orig2=short2|" */
/* ------------------------------------------------------------------ */
static std::string mapping_to_string(
    const std::map<std::string, std::string>& m) {
  std::string result;
  for (auto& [orig, short_name] : m) {
    result += orig + "=" + short_name + "|";
  }
  return result;
}

static std::map<std::string, std::string> string_to_mapping(
    const std::string& s) {
  std::map<std::string, std::string> m;
  size_t pos = 0;
  while (pos < s.size()) {
    auto eq = s.find('=', pos);
    if (eq == std::string::npos) break;
    auto bar = s.find('|', eq + 1);
    if (bar == std::string::npos) break;
    auto orig = s.substr(pos, eq - pos);
    auto short_name = s.substr(eq + 1, bar - eq - 1);
    m[orig] = short_name;
    pos = bar + 1;
  }
  return m;
}

/* ------------------------------------------------------------------ */
/* ------------------------------------------------------------------ */
/*  Strip formatting — join lines to make code maximally compact.      *
/*  Removes all newlines, joins statements after ;, keeps -- comments   *
/*  on their own line.  Output is near-unreadable.                     */
/* ------------------------------------------------------------------ */
static std::string strip_formatting(const std::string& src) {
  std::string out;
  bool need_space = false;
  bool at_line_start = true;
  for (size_t i = 0; i < src.size(); i++) {
    char c = src[i];
    if (c == '\'') {
      if (need_space && !out.empty() && out.back() != ' ' && out.back() != '\n') out += ' ';
      need_space = false; at_line_start = false;
      out += c; i++;
      while (i < src.size()) {
        out += src[i];
        if (src[i] == '\'') {
          if (i + 1 < src.size() && src[i+1] == '\'') { i++; out += src[i]; }
          else break;
        }
        i++;
      }
      continue;
    }
    /* -- comments stay on their own line */
    if (c == '-' && i+1 < src.size() && src[i+1] == '-') {
      if (!out.empty() && out.back() != '\n') out += '\n';
      out += c; out += src[i+1]; i += 2;
      while (i < src.size() && src[i] != '\n' && src[i] != '\r') { out += src[i]; i++; }
      out += '\n';
      need_space = false; at_line_start = true;
      continue;
    }
    /* Strip all newlines — join everything */
    if (c == '\n' || c == '\r') {
      need_space = true; at_line_start = true;
      continue;
    }
    /* Strip leading whitespace */
    if (at_line_start && (c == ' ' || c == '\t')) continue;
    at_line_start = false;
    /* Collapse consecutive spaces to one */
    if (c == ' ' || c == '\t') {
      if (!out.empty() && out.back() != ' ' && out.back() != '\n')
        out += ' ';
      continue;
    }
    if (need_space && !out.empty() && out.back() != ' ') out += ' ';
    need_space = false;
    out += c;
  }
  return out;
}

/* ------------------------------------------------------------------ */
/*  PL/SQL re-indenter — splits compacted code, tracks block depth     *
/*  using a stack of opened block types.  Properly handles END IF,      *
/*  END LOOP, plain END, IS/AS, BEGIN, etc.                            */
/* ------------------------------------------------------------------ */
static std::string reindent_plsql(const std::string& src) {
  auto is_stmt_start = [](const std::string& w) {
    static const char* words[] = {
      "BEGIN", "DECLARE", "EXCEPTION", "CREATE",
      "IF", "THEN", "ELSIF", "ELSE", "END",
      "LOOP", "FOR", "WHILE", "CASE",
      "WHEN", "RETURN", "RAISE", "NULL",
      "COMMIT", "ROLLBACK", "OPEN", "FETCH", "CLOSE",
      "DELETE", "INSERT", "UPDATE", "SELECT",
      "EXECUTE", "PIPE", "CONTINUE", "EXIT", "GOTO", "PIPELINED",
    };
    for (auto kw : words) if (w == kw) return true;
    return false;
  };

  std::string out;
  std::vector<std::string> blocks; /* stack of opened block types */
  std::string line;
    bool prev_was_semi = false;

  auto flush_line = [&]() {
    if (line.empty()) return;
    while (!line.empty() && (line.back() == ' ' || line.back() == '\t'))
      line.pop_back();
    if (line.empty()) return;
    auto trim = line;
    while (!trim.empty() && (trim[0]==' '||trim[0]=='\t')) trim.erase(0,1);
    auto up = trim;
    for (auto& ch : up) ch = toupper((unsigned char)ch);

    /* Close blocks for END keywords */
    if (up.find("END ") == 0 || up == "END" || up.find("END;") == 0) {
      /* END IF / END LOOP / END CASE → close that specific block */
      if (up.find("END IF") == 0) {
        for (int i = (int)blocks.size()-1; i >= 0; i--)
          if (blocks[i] == "IF") { blocks.resize(i); break; }
      } else if (up.find("END LOOP") == 0) {
        for (int i = (int)blocks.size()-1; i >= 0; i--)
          if (blocks[i] == "LOOP") { blocks.resize(i); break; }
      } else if (up.find("END CASE") == 0) {
        for (int i = (int)blocks.size()-1; i >= 0; i--)
          if (blocks[i] == "CASE") { blocks.resize(i); break; }
      } else {
        /* Plain END or END name — pop two blocks (BEGIN + parent) */
        if (!blocks.empty()) blocks.pop_back();
        if (!blocks.empty()) blocks.pop_back();
      }
    } else if (up.find("ELSIF") == 0 || up == "ELSE") {
      /* ELSIF/ELSE at same level as IF — pop IF before output,
       * re-push in opening section after output */
      for (int i = (int)blocks.size()-1; i >= 0; i--)
        if (blocks[i] == "IF") { blocks.resize(i); break; }
    } else if (up.find("EXCEPTION") == 0) {
      /* EXCEPTION is at same level as BEGIN */
    } else if (up.find("WHEN ") == 0) {
      /* WHEN is inside EXCEPTION, at same level as statements */
    }

    /* Output the line */
    out += std::string(blocks.size() * 2, ' ') + line + "\n";
    line.clear();

    /* Open blocks for keywords on this line */
    if (up.find("THEN") != std::string::npos) {
      if (blocks.empty() || blocks.back() != "IF") blocks.push_back("IF");
    }
    /* ELSE/ELSIF re-open the IF block for statements inside it */
    if (up == "ELSE" || up.find("ELSIF") == 0) {
      blocks.push_back("IF");
    }
    if (up.find("LOOP") != std::string::npos &&
        up.find("END LOOP") == std::string::npos &&
        up.find("LOOP;") == std::string::npos) {
      blocks.push_back("LOOP");
    }
    if (up == "DECLARE") blocks.push_back("DECLARE");
    if (up == "ELSE") { /* ELSE doesn't open a new block */ }
    if (up.find("EXCEPTION") == 0 && up.size() == 9) blocks.push_back("EXCEP");
    if (up == "BEGIN" || up.find("BEGIN ") == 0) blocks.push_back("BEGIN");
    if (up.find("CASE") == 0 && up != "CASE" &&
        up.find("END CASE") == std::string::npos)
      blocks.push_back("CASE");

    /* IS/AS after PROCEDURE/FUNCTION/PACKAGE opens a block */
    bool has_is_as = (up.find("IS ") != std::string::npos ||
         up == "IS" || (up.size() >= 3 && up.substr(up.size()-3) == " IS") ||
         up.find("AS ") != std::string::npos ||
         up == "AS" || (up.size() >= 3 && up.substr(up.size()-3) == " AS"));
    if (has_is_as && up.find("END") != 0)
      blocks.push_back("BLOCK");

    prev_was_semi = false;
  };

  for (size_t i = 0; i < src.size(); i++) {
    char c = src[i];
    if (c == '\'') {
      line += c; i++;
      while (i < src.size()) {
        line += src[i];
        if (src[i] == '\'') {
          if (i + 1 < src.size() && src[i+1] == '\'') { i++; line += src[i]; }
          else break;
        }
        i++;
      } continue;
    }
    if (c == '-' && i+1 < src.size() && src[i+1] == '-') {
      flush_line();
      line += c; line += src[i+1]; i += 2;
      while (i < src.size() && src[i] != '\n' && src[i] != '\r') { line += src[i]; i++; }
      flush_line();
      continue;
    }
    if (c == '/' && i+1 < src.size() && src[i+1] == '*') {
      line += "/*"; i += 2;
      while (i+1 < src.size() && !(src[i] == '*' && src[i+1] == '/')) { line += src[i]; i++; }
      if (i+1 < src.size()) { line += "*/"; i += 2; }
      continue;
    }
    if (c == '(') {
      line += c; int d = 1; i++;
      while (i < src.size() && d > 0) {
        if (src[i] == '(') d++;
        else if (src[i] == ')') d--;
        if (d > 0) { line += src[i]; i++; } else break;
      }
      line += ')'; continue;
    }
    if (c == ';') {
      line += ';';
      /* Peek ahead: flush now unless followed by a statement keyword */
      size_t pk = i + 1;
      bool should_flush = true;
      while (pk < src.size() && (src[pk] == ' ' || src[pk] == '\t')) pk++;
      if (pk < src.size() && is_id_start(src[pk])) {
        size_t ws = pk;
        while (pk < src.size() && is_id_char(src[pk])) pk++;
        auto word = src.substr(ws, pk - ws);
        for (auto& ch : word) ch = toupper((unsigned char)ch);
        if (is_stmt_start(word)) should_flush = false;
      }
      if (should_flush) flush_line();
      prev_was_semi = true;
      continue;
    }
    if (c == ' ' || c == '\t') {
      size_t pk = i + 1;
      while (pk < src.size() && (src[pk] == ' ' || src[pk] == '\t')) pk++;
      if (pk < src.size() && is_id_start(src[pk])) {
        size_t ws = pk;
        while (pk < src.size() && is_id_char(src[pk])) pk++;
        auto word = src.substr(ws, pk - ws);
        for (auto& ch : word) ch = toupper((unsigned char)ch);
        /* Don't split END IF / END LOOP / END CASE */
        auto lc = line;
        for (auto& ch : lc) ch = toupper((unsigned char)ch);
        while (!lc.empty() && lc.back() == ' ') lc.pop_back();
        if ((lc == "END" || lc.find("END ") != std::string::npos) &&
            (word == "IF" || word == "LOOP" || word == "CASE")) {
          if (!line.empty() && line.back() != ' ') line += ' ';
          continue;
        }
        /* Split before statement-starting keywords (without ;) */
        if (is_stmt_start(word) &&
            (word == "BEGIN" || word == "DECLARE" || word == "ELSE" ||
             word == "ELSIF" || word == "END" || word == "EXCEPTION" ||
             word == "WHEN" || word == "LOOP" || word == "CREATE" ||
             word == "IF" || word == "FOR" || word == "WHILE" ||
             word == "CASE" || word == "RETURN" || word == "RAISE" ||
             word == "NULL" || word == "PIPE" || word == "CONTINUE" ||
             word == "FUNCTION" || word == "PROCEDURE" || word == "TYPE")) {
          flush_line();
          i = ws - 1; continue;
        }
      }
      if (!line.empty() && line.back() != ' ') line += ' ';
      continue;
    }
    if (c == '\n' || c == '\r') continue;
    line += c;
  }
  flush_line();
  return out;
}

/* ------------------------------------------------------------------ */
/*  Public API                                                         */
/* ------------------------------------------------------------------ */

std::string obfuscate_plsql(const std::string& source,
                            const std::string& passphrase,
                            bool keep_comments) {
  /* ---- Strip all comments (unless keep_comments is set) ---- */
  std::string clean;
  if (keep_comments) {
    clean = source;
  } else {
    size_t i = 0;
    while (i < source.size()) {
      if (source[i] == '\'') {
        clean += source[i++];
        while (i < source.size()) {
          clean += source[i];
          if (source[i] == '\'') {
            if (i + 1 < source.size() && source[i+1] == '\'') {
              i++; clean += source[i];
            } else { i++; break; }
          }
          i++;
        }
      } else if (source[i] == '-' && i+1 < source.size() && source[i+1] == '-') {
        i += 2;
        while (i < source.size() && source[i] != '\n' && source[i] != '\r') i++;
      } else if (source[i] == '/' && i+1 < source.size() && source[i+1] == '*') {
        i += 2;
        while (i+1 < source.size() && !(source[i] == '*' && source[i+1] == '/')) i++;
        if (i+1 < source.size()) i += 2; else i++;
      } else {
        clean += source[i++];
      }
    }
  }

  /* Collect identifiers from CREATE headers and add them to reserved set,
   * so that package/function/procedure names stay readable (not renamed). */
  init_reserved();
  {
    auto up = to_upper(clean);
    size_t pos = 0;
    while (pos < clean.size()) {
      auto cr = up.find("CREATE", pos);
      if (cr == std::string::npos) break;
      /* Must be at start of a line or at start of source */
      if (cr != 0 && clean[cr-1] != '\n' && clean[cr-1] != '\r') { pos = cr + 6; continue; }
      /* Also check it's a standalone word, not part of an identifier */
      if (cr + 6 < clean.size() && (isalnum((unsigned char)clean[cr+6]) || clean[cr+6] == '_'))
        { pos = cr + 6; continue; }
      /* Find AS or IS on this CREATE line (or within the next few chars) */
      auto eol = clean.find('\n', cr);
      if (eol == std::string::npos) eol = clean.size();
      auto header_line = clean.substr(cr, eol - cr);
      /* Find all identifiers in the header line */
      for (size_t i = 0; i < header_line.size(); i++) {
        if (is_id_start(header_line[i])) {
          size_t s = i;
          while (i < header_line.size() && is_id_char(header_line[i])) i++;
          auto id = header_line.substr(s, i - s);
          auto up_id = to_upper(id);
          if (up_id != "CREATE" && up_id != "OR" && up_id != "REPLACE"
              && !reserved_set.count(up_id)) {
            reserved_set.insert(up_id);
          }
        }
      }
      pos = eol + 1;
    }
  }

  /* Protect ALL identifiers from the PACKAGE SPEC section (everything
   * before the first PACKAGE BODY).  These are public API: function names,
   * procedure names, variable names, type names, etc.  Must not be renamed.
   * Only applies when BOTH a spec and body exist in the same file. */
  {
    auto body_pos = to_upper(clean).find("PACKAGE BODY");
    if (body_pos != std::string::npos) {
      for (size_t i = 0; i < body_pos; i++) {
        if (clean[i] == '\'') {
          i++;
          while (i < body_pos) {
            if (clean[i] == '\'') {
              if (i + 1 < body_pos && clean[i+1] == '\'') i++;
              else break;
            }
            i++;
          }
          continue;
        }
        if (clean[i] == '-' && i+1 < clean.size() && clean[i+1] == '-') {
          i += 2;
          while (i < body_pos && clean[i] != '\n' && clean[i] != '\r') i++;
          continue;
        }
        if (clean[i] == '/' && i+1 < clean.size() && clean[i+1] == '*') {
          i += 2;
          while (i+1 < body_pos && !(clean[i] == '*' && clean[i+1] == '/')) i++;
          if (i+1 < body_pos) i += 2; else i++;
          continue;
        }
        if (is_id_start(clean[i])) {
          size_t s = i;
          while (i < body_pos && is_id_char(clean[i])) i++;
          auto id = clean.substr(s, i - s);
          auto up = to_upper(id);
          if (!reserved_set.count(up)) {
            reserved_set.insert(up);
          }
          continue;
        }
      }
    }
  }

  /* Protect all parameter names of PROCEDURE/FUNCTION declarations.
   * These are part of the public API and must not be renamed. */
  {
    auto up = to_upper(clean);
    const char* keywords[] = {"PROCEDURE", "FUNCTION"};
    for (auto kw : keywords) {
      size_t pos = 0;
      while ((pos = up.find(kw, pos)) != std::string::npos) {
        /* Find the opening parenthesis after the name */
        size_t paren = up.find('(', pos);
        if (paren == std::string::npos) { pos++; continue; }
        /* Find the matching closing paren (handles nesting) */
        int depth = 1;
        size_t end = paren + 1;
        while (end < up.size() && depth > 0) {
          if (up[end] == '(') depth++;
          else if (up[end] == ')') depth--;
          end++;
        }
        if (depth != 0) { pos++; continue; }
        /* Extract parameter names between the parentheses */
        std::string params = clean.substr(paren + 1, end - paren - 2);
        size_t pi = 0;
        while (pi < params.size()) {
          /* Skip IN | OUT | IN OUT | NOCOPY keywords */
          while (pi < params.size() && (params[pi] == ' ' || params[pi] == '\t' || params[pi] == '\n')) pi++;
          auto pk = to_upper(params).substr(pi);
          if (pk.find("IN OUT") == 0) { pi += 6; continue; }
          if (pk.find("IN") == 0) { pi += 2; continue; }
          if (pk.find("OUT") == 0) { pi += 3; continue; }
          if (pk.find("NOCOPY") == 0) { pi += 6; continue; }
          if (!is_id_start(params[pi])) { pi++; continue; }
          /* Found a param name — extract it */
          size_t ns = pi;
          while (pi < params.size() && is_id_char(params[pi])) pi++;
          auto pname = params.substr(ns, pi - ns);
          auto up_pname = to_upper(pname);
          if (!reserved_set.count(up_pname)) {
            reserved_set.insert(up_pname);
          }
          /* Skip past the datatype */
          while (pi < params.size() && params[pi] != ',' && params[pi] != ')') pi++;
        }
        pos = end;
      }
    }
  }

  /* Collect identifiers */
  auto freq = collect_identifiers(clean);

  /* Build set of ALL identifiers in source to avoid short name collisions */
  std::set<std::string> all_ids;
  {
    init_reserved();
    size_t i = 0;
    while (i < clean.size()) {
      if (is_id_start(clean[i])) {
        size_t s = i;
        while (i < clean.size() && is_id_char(clean[i])) i++;
        auto id = clean.substr(s, i - s);
        auto up = to_upper(id);
        all_ids.insert(up);
      } else {
        i++;
      }
    }
  }

  /* Generate short-name mapping */
  auto mapping = make_mapping(freq, all_ids);

  /* Replace identifiers (if any) */
  auto obfuscated = mapping.empty() ? clean : replace_identifiers(clean, mapping);

  /* Strip formatting so unwrapped obfuscated code is maximally unreadable.
   * Formatting is restored during deobfuscation via reindent_plsql. */
  obfuscated = strip_formatting(obfuscated);

  /* Build reverse mapping (short -> original) for deobfuscation.
   * Add both lowercase and uppercase short names so deobfuscation
   * works whether wrap_v2 uppercased the source or preserve_case was used. */
  std::map<std::string, std::string> reverse;
  for (auto& [orig, short_name] : mapping) {
    reverse[short_name] = orig;
    auto up = short_name;
    for (auto& c : up) c = toupper((unsigned char)c);
    if (up != short_name) reverse[up] = orig;
  }
  auto map_str = mapping_to_string(reverse);
  if (map_str.empty()) map_str = " ";  /* ensure >= 1 byte for AES */

  /* Encrypt the mapping */
  auto encrypted = aes_encrypt(map_str, passphrase);
  if (encrypted.empty()) {
    std::cerr << "Error: encryption failed.\n";
    return {};
  }

  /* Embed as a special --ENC: comment at the END of the source */
  auto enc_b64 = b64enc(encrypted);
  std::string result = obfuscated;
  result += "\n--ENC:\n";
  for (size_t i = 0; i < enc_b64.size(); i += 72) {
    result += enc_b64.substr(i, std::min(static_cast<size_t>(72),
                                          enc_b64.size() - i));
    result += '\n';
  }
  result += "--ENCEND\n";
  return result;
}

std::string deobfuscate_plsql(const std::string& source,
                              const std::string& passphrase) {
  /* Find the ENC comment header at the END of the source */
  auto enc_start = source.rfind("--ENC:\n");
  if (enc_start == std::string::npos) {
    /* Fallback: try old /*ENC: format at the start */
    if (source.size() >= 6 && source.substr(0, 5) == "/*ENC") {
      enc_start = 0;
    } else {
      return {};
    }
  }
  auto end = source.find("--ENCEND", enc_start);
  if (end == std::string::npos) {
    /* Fallback: look for star-slash */
    end = source.find("*/", enc_start);
    if (end == std::string::npos) return {};
  }

  /* Base64 data is between --ENC: (after newline) and --ENCEND */
  auto b64_start = source.find('\n', enc_start + 5);
  if (b64_start == std::string::npos) return {};
  b64_start++;
  auto b64_data = source.substr(b64_start, end - b64_start);

  /* Remove any whitespace/newlines within the base64 data */
  std::string b64_clean;
  for (auto c : b64_data) {
    if (c != '\n' && c != '\r' && c != ' ' && c != '\t') b64_clean += c;
  }

  /* Decode base64 */
  auto encrypted = b64dec(b64_clean);
  if (encrypted.empty()) {
    std::cerr << "Error: invalid encrypted data in source.\n";
    return {};
  }

  /* Decrypt */
  auto decrypted = aes_decrypt(encrypted, passphrase);
  if (decrypted.empty()) {
    std::cerr << "Error: decryption failed — wrong passphrase?\n";
    return {};
  }

  std::string map_str(reinterpret_cast<const char*>(decrypted.data()),
                      decrypted.size());

  /* Parse mapping — empty is valid (no renames) */
  auto reverse_map = string_to_mapping(map_str);

  /* Restore: the source before --ENC: has short names, restore them */
  auto body = source.substr(0, enc_start);
  /* Strip trailing whitespace from body */
  while (!body.empty() && (body.back() == '\n' || body.back() == '\r'
         || body.back() == ' ' || body.back() == '\t')) body.pop_back();
  auto result = replace_identifiers(body, reverse_map);
  return reindent_plsql(result);
}
