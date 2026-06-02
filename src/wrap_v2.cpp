/* V2 wrapper — 10g+ style PL/SQL wrapping
 *
 * Original PL/SQL project : https://github.com/oddz/PL-SQL-Unwrapper
 * Original author          : Cameron Marshall
 * C++ port v3.0            : Manuel FLURY
 * Copyright (C) 2026       : Manuel FLURY
 * License                  : GNU General Public License v3.0
 *
 * Reverses the V2 unwrap process:
 *   1.  Strip comments (unless keep_comments is set)
 *   2.  Strip "CREATE OR REPLACE " from source, uppercase the rest
 *   3.  ZLIB-compress the normalized source
 *   4.  SHA-1 hash of compressed data
 *   5.  Prepend hash to compressed data
 *   6.  Apply inverse substitution cipher
 *   7.  Base64-encode
 *   8.  Wrap in preamble + terminator line
 */

#include "wrap_v2.h"
#include <string>
#include <vector>
#include <cstdint>
#include <cctype>
#include <cstring>
#include <cstdio>
#include <zlib.h>
#include <openssl/sha.h>

/* Same CIPHER_FROM table as the unwrapper (256-byte permutation). */
static const unsigned char CIPHER_FROM[256] = {
  0x3D, 0x65, 0x85, 0xB3, 0x18, 0xDB, 0xE2, 0x87, 0xF1, 0x52, 0xAB, 0x63, 0x4B, 0xB5, 0xA0, 0x5F,
  0x7D, 0x68, 0x7B, 0x9B, 0x24, 0xC2, 0x28, 0x67, 0x8A, 0xDE, 0xA4, 0x26, 0x1E, 0x03, 0xEB, 0x17,
  0x6F, 0x34, 0x3E, 0x7A, 0x3F, 0xD2, 0xA9, 0x6A, 0x0F, 0xE9, 0x35, 0x56, 0x1F, 0xB1, 0x4D, 0x10,
  0x78, 0xD9, 0x75, 0xF6, 0xBC, 0x41, 0x04, 0x81, 0x61, 0x06, 0xF9, 0xAD, 0xD6, 0xD5, 0x29, 0x7E,
  0x86, 0x9E, 0x79, 0xE5, 0x05, 0xBA, 0x84, 0xCC, 0x6E, 0x27, 0x8E, 0xB0, 0x5D, 0xA8, 0xF3, 0x9F,
  0xD0, 0xA2, 0x71, 0xB8, 0x58, 0xDD, 0x2C, 0x38, 0x99, 0x4C, 0x48, 0x07, 0x55, 0xE4, 0x53, 0x8C,
  0x46, 0xB6, 0x2D, 0xA5, 0xAF, 0x32, 0x22, 0x40, 0xDC, 0x50, 0xC3, 0xA1, 0x25, 0x8B, 0x9C, 0x16,
  0x60, 0x5C, 0xCF, 0xFD, 0x0C, 0x98, 0x1C, 0xD4, 0x37, 0x6D, 0x3C, 0x3A, 0x30, 0xE8, 0x6C, 0x31,
  0x47, 0xF5, 0x33, 0xDA, 0x43, 0xC8, 0xE3, 0x5E, 0x19, 0x94, 0xEC, 0xE6, 0xA3, 0x95, 0x14, 0xE0,
  0x9D, 0x64, 0xFA, 0x59, 0x15, 0xC5, 0x2F, 0xCA, 0xBB, 0x0B, 0xDF, 0xF2, 0x97, 0xBF, 0x0A, 0x76,
  0xB4, 0x49, 0x44, 0x5A, 0x1D, 0xF0, 0x00, 0x96, 0x21, 0x80, 0x7F, 0x1A, 0x82, 0x39, 0x4F, 0xC1,
  0xA7, 0xD7, 0x0D, 0xD1, 0xD8, 0xFF, 0x13, 0x93, 0x70, 0xEE, 0x5B, 0xEF, 0xBE, 0x09, 0xB9, 0x77,
  0x72, 0xE7, 0xB2, 0x54, 0xB7, 0x2A, 0xC7, 0x73, 0x90, 0x66, 0x20, 0x0E, 0x51, 0xED, 0xF8, 0x7C,
  0x8F, 0x2E, 0xF4, 0x12, 0xC6, 0x2B, 0x83, 0xCD, 0xAC, 0xCB, 0x3B, 0xC4, 0x4E, 0xC0, 0x69, 0x36,
  0x62, 0x02, 0xAE, 0x88, 0xFC, 0xAA, 0x42, 0x08, 0xA6, 0x45, 0x57, 0xD3, 0x9A, 0xBD, 0xE1, 0x23,
  0x8D, 0x92, 0x4A, 0x11, 0x89, 0x74, 0x6B, 0x91, 0xFB, 0xFE, 0xC9, 0x01, 0xEA, 0x1B, 0xF7, 0xCE,
};

static std::string to_upper(const std::string& s) {
  std::string r = s;
  for (auto& c : r) c = toupper((unsigned char)c);
  return r;
}

/* ZLIB compression */
static std::vector<unsigned char> zlib_compress(const std::string& in) {
  z_stream strm = {};
  strm.next_in = const_cast<unsigned char*>(
    reinterpret_cast<const unsigned char*>(in.data()));
  strm.avail_in = in.size();
  if (deflateInit(&strm, Z_DEFAULT_COMPRESSION) != Z_OK) return {};
  std::vector<unsigned char> out(deflateBound(&strm, in.size()));
  strm.next_out = out.data();
  strm.avail_out = out.size();
  int ret = deflate(&strm, Z_FINISH);
  deflateEnd(&strm);
  if (ret != Z_STREAM_END) return {};
  out.resize(strm.total_out);
  return out;
}

/* Build inverse cipher: position of each byte in CIPHER_FROM.
 * This is the wrapping-direction mapping: output = position_of(input). */
static unsigned char make_cipher_to(int i) {
  for (int j = 0; j < 256; j++) {
    if (CIPHER_FROM[j] == (unsigned char)i) return j;
  }
  return i;
}

static std::string base64_encode(const std::vector<unsigned char>& in) {
  static const char b64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";
  std::string out;
  out.reserve((in.size() + 2) / 3 * 4);
  int val = 0, bits = -6;
  for (unsigned char c : in) {
    val = (val << 8) | c;
    bits += 8;
    while (bits >= 0) {
      out += b64[(val >> bits) & 0x3F];
      bits -= 6;
    }
  }
  if (bits > -6) out += b64[((val << 8) >> (bits + 8)) & 0x3F];
  while (out.size() % 4) out += '=';
  return out;
}

/* Extract the CREATE … AS/IS header line from the source.
 * Everything up to "AS" or "IS" on the first CREATE line. */
static std::string extract_header(const std::string& source) {
  std::string result;
  auto p = to_upper(source).find("CREATE");
  if (p == std::string::npos) return {};

  bool in_quotes = false;
  for (size_t i = p; i < source.size(); i++) {
    char c = source[i];
    if (c == '"') { in_quotes = !in_quotes; result += c; continue; }
    if (!in_quotes) {
      if (c == '\n' || c == '\r') break;
      if (i > p && (c == 'I' || c == 'i') && i + 2 < source.size()
          && (source[i+1] == 'S' || source[i+1] == 's')
          && (i + 2 >= source.size() || source[i+2] == ' '
              || source[i+2] == '\n')) break;
      if (i > p && (c == 'A' || c == 'a') && i + 2 < source.size()
          && (source[i+1] == 'S' || source[i+1] == 's')
          && (i + 2 >= source.size() || source[i+2] == ' '
              || source[i+2] == '\n')) break;
    }
    result += c;
  }
  while (!result.empty()
         && (result.back() == ' ' || result.back() == '\t'))
    result.pop_back();
  return result;
}

/* Strip PL/SQL comments: -- single-line and /* *​/ multi-line.
 * Respects string literals: comments inside '...' are preserved. */
static std::string strip_plsql_comments(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  size_t i = 0;
  while (i < s.size()) {
    if (s[i] == '\'') {
      /* String literal — copy until closing quote (handles '' escape) */
      out += s[i++];
      while (i < s.size()) {
        out += s[i];
        if (s[i] == '\'') {
          if (i + 1 < s.size() && s[i+1] == '\'') {
            i++; out += s[i]; /* escaped quote '' */
          } else {
            i++; break;       /* end of string */
          }
        }
        i++;
      }
    } else if (s[i] == '-' && i + 1 < s.size() && s[i+1] == '-') {
      /* Single-line comment — skip to end of line */
      i += 2;
      while (i < s.size() && s[i] != '\n' && s[i] != '\r') i++;
      /* Keep the newline so line numbering is preserved */
    } else if (s[i] == '/' && i + 1 < s.size() && s[i+1] == '*') {
      /* Multi-line comment — skip to star-slash */
      i += 2;
      while (i + 1 < s.size() && !(s[i] == '*' && s[i+1] == '/')) i++;
      if (i + 1 < s.size()) i += 2; else i++;
    } else {
      out += s[i++];
    }
  }
  return out;
}

/* ------------------------------------------------------------------ */
/*  wrap_v2 — main entry point                                         *
 *                                                                      *
 *  Takes plain PL/SQL source text and produces a V2-wrapped output.    *
 *  If keep_comments is false (default), comments are stripped first.   *
 *  If preserve_case is true, source is NOT uppercased before compress. */
/* ------------------------------------------------------------------ */
std::string wrap_v2(const std::string& source, bool keep_comments,
                    bool preserve_case) {
  /* ---- 1. Optionally strip comments ---- */
  std::string cleaned = keep_comments ? source : strip_plsql_comments(source);

  /* ---- 2. Extract the CREATE header for the preamble ---- */
  auto header = extract_header(cleaned);
  if (header.empty()) header = "CREATE OR REPLACE";

  /* Strip "CREATE OR REPLACE " prefix before compressing.
   * The unwrapper adds it back on decode. */
  std::string compress_source = cleaned;
  {
    auto up = to_upper(compress_source);
    auto cr = up.find("CREATE OR REPLACE ");
    if (cr != std::string::npos)
      compress_source = compress_source.substr(cr + 18);
  }

  /* ---- 3. Uppercase (basic normalization) — skip if preserve_case ---- */
  auto normalized = preserve_case ? compress_source : to_upper(compress_source);

  /* ---- 4. ZLIB compress ---- */
  auto compressed = zlib_compress(normalized);
  if (compressed.empty()) return source;

  /* ---- 5. SHA-1 digest of compressed data ---- */
  unsigned char digest[20];
  SHA1(compressed.data(), compressed.size(), digest);

  /* ---- 6. Prepend digest to compressed data ---- */
  std::vector<unsigned char> payload;
  payload.reserve(20 + compressed.size());
  payload.insert(payload.end(), digest, digest + 20);
  payload.insert(payload.end(), compressed.begin(), compressed.end());

  /* ---- 7. Inverse cipher ---- */
  for (auto& b : payload) b = make_cipher_to(b);

  /* ---- 8. Base64 encode ---- */
  auto b64 = base64_encode(payload);

  /* ---- 9. Build wrapped output ---- */
  std::string result = header + " wrapped \n";
  result += "a000000\n";
  result += "0\n";

  char term[64];
  std::snprintf(term, sizeof(term), "%zx %zx\n",
                compressed.size(), normalized.size());
  result += term;

  for (size_t i = 0; i < b64.size(); i += 72) {
    result += b64.substr(i, std::min(static_cast<size_t>(72),
                                     b64.size() - i));
    result += '\n';
  }

  result += "/\n";
  return result;
}
