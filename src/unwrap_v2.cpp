/* V2 unwrapper — Oracle 10g+ wrapped PL/SQL
 *
 * Original PL/SQL project : https://github.com/oddz/PL-SQL-Unwrapper
 * Original author          : Cameron Marshall
 * C++ port v3.0            : Manuel FLURY
 * Copyright (C) 2026       : Manuel FLURY
 * License                  : GNU General Public License v3.0
 *
 * The V2 wrapping process is:
 *   1.  Uppercase keywords/identifiers in the source
 *   2.  Strip comments
 *   3.  ZLIB-compress
 *   4.  Prepend SHA-1 digest of the compressed data
 *   5.  Apply substitution cipher (XOR-free, byte-mapping via CIPHER_FROM)
 *   6.  Base64-encode
 *   7.  Wrap in a preamble with metadata
 *
 * Unwrapping reverses the above.
 */

#include "unwrap_v2.h"
#include <iostream>
#include <string>
#include <vector>
#include <cstdint>
#include <cctype>
#include <cstring>
#include <zlib.h>
#include <openssl/sha.h>

/* ------------------------------------------------------------------ */
/*  Substitution cipher table (256-byte permutation of 0x00-0xFF)      */
/*  Unwrapping: output_byte = CIPHER_FROM[input_byte]                  */
/*  Wrapping:   output_byte = inverse_CIPHER_FROM[input_byte]          */
/*             (position of input_byte in this table)                  */
/*  Values from the original PL/SQL Unwrapper project.                 */
/* ------------------------------------------------------------------ */
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

/* ------------------------------------------------------------------ */
/*  Check whether a line contains two space-separated hex values.      */
/*  The V2 preamble ends with such a line (the terminator).            */
/* ------------------------------------------------------------------ */
static bool is_hex_line(const std::string& line) {
  auto sp = line.find(' ');
  if (sp == std::string::npos) return false;
  auto first = line.substr(0, sp);
  auto second = line.substr(sp + 1);
  while (!second.empty() && (second.back() == '\r'
         || second.back() == ' ' || second.back() == '\t'))
    second.pop_back();
  if (first.empty() || second.empty()) return false;
  for (auto c : first) if (!isxdigit((unsigned char)c)) return false;
  for (auto c : second) if (!isxdigit((unsigned char)c)) return false;
  return true;
}

/* ------------------------------------------------------------------ */
/*  Base64 decode — strips all whitespace first.                       */
/* ------------------------------------------------------------------ */
static std::vector<unsigned char> base64_decode(const std::string& in) {
  /* Strip whitespace */
  std::string cleaned;
  cleaned.reserve(in.size());
  for (char c : in) {
    if (c != '\n' && c != '\r' && c != ' ' && c != '\t')
      cleaned += c;
  }

  static const unsigned char b64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";
  unsigned char d[256];
  memset(d, 0xFF, 256);
  for (int i = 0; i < 64; i++) d[(unsigned char)b64[i]] = i;

  std::vector<unsigned char> out;
  out.reserve(cleaned.size() / 4 * 3 + 3);
  int val = 0, bits = -8;
  for (unsigned char c : cleaned) {
    if (c == '=') break;         /* padding */
    if (d[c] == 0xFF) continue;  /* skip non-base64 chars */
    val = (val << 6) | d[c];
    bits += 6;
    if (bits >= 0) {
      out.push_back((val >> bits) & 0xFF);
      bits -= 8;
    }
  }
  return out;
}

/* ------------------------------------------------------------------ */
/*  ZLIB decompression                                                 */
/* ------------------------------------------------------------------ */
static std::vector<unsigned char> zlib_decompress(
    const std::vector<unsigned char>& in) {
  z_stream strm = {};
  strm.next_in = const_cast<unsigned char*>(in.data());
  strm.avail_in = in.size();
  if (inflateInit(&strm) != Z_OK) return {};

  std::vector<unsigned char> out(65536);
  size_t pos = 0;
  int ret;
  do {
    if (pos + 32768 > out.size()) out.resize(out.size() * 2);
    strm.next_out = out.data() + pos;
    strm.avail_out = out.size() - pos;
    ret = inflate(&strm, Z_NO_FLUSH);
    pos = strm.total_out;
  } while (ret == Z_OK);
  inflateEnd(&strm);
  out.resize(pos);

  if (ret != Z_STREAM_END) return {};
  return out;
}

/* ------------------------------------------------------------------ */
/*  Case conversion helper                                             */
/* ------------------------------------------------------------------ */
static std::string to_upper(const std::string& s) {
  std::string r = s;
  for (auto& c : r) c = toupper((unsigned char)c);
  return r;
}

/* ------------------------------------------------------------------ */
/*  unwrap_v2 — main entry point for V2 unwrapping                     *
 *                                                                      *
 *  Expects source that starts with "CREATE … wrapped a000000 …" and    *
 *  returns the decompressed, deciphered source text.                   *
 * ------------------------------------------------------------------ */
std::string unwrap_v2(const std::string& source) {
  /* ---- 1. Find the "wrapped a000000" marker ---- */
  auto upper = to_upper(source);
  auto wpos = upper.find("WRAPPED");
  if (wpos == std::string::npos) return source;  /* not V2 */

  /* Skip past "wrapped" and any whitespace/newlines to "a000000" */
  size_t p = wpos + 7;
  while (p < source.size() && (source[p] == ' ' || source[p] == '\t'
         || source[p] == '\n' || source[p] == '\r')) p++;
  if (p + 7 > source.size()
      || to_upper(source.substr(p, 7)) != "A000000")
    return source;

  /* ---- 2. Scan forward for the terminator line (hex hex) ---- */
  p = p + 7;  /* skip "a000000" */
  auto term_pos = std::string::npos;
  while (p < source.size()) {
    auto nl = source.find('\n', p);
    if (nl == std::string::npos) nl = source.size();
    std::string line = source.substr(p, nl - p);
    while (!line.empty() && (line.back() == '\r'
           || line.back() == ' ' || line.back() == '\t'))
      line.pop_back();
    if (is_hex_line(line)) { term_pos = p; break; }
    if (nl >= source.size()) break;
    p = nl + 1;
  }
  if (term_pos == std::string::npos) return source;

  /* ---- 3. Base64 data starts after the terminator line ---- */
  auto data_start = source.find('\n', term_pos);
  if (data_start == std::string::npos) return source;
  data_start++;
  auto b64str = source.substr(data_start);

  /* ---- 4. Base64 decode ---- */
  auto decoded = base64_decode(b64str);
  if (decoded.size() < 21) return source;  /* need digest + data */

  /* ---- 5. Reverse substitution cipher ---- */
  for (auto& b : decoded) b = CIPHER_FROM[b];

  /* ---- 6. Strip 20-byte SHA-1 digest, keep compressed data ---- */
  std::vector<unsigned char> compressed(decoded.begin() + 20,
                                         decoded.end());

  /* ---- 7. ZLIB decompress ---- */
  auto uncompressed = zlib_decompress(compressed);
  if (uncompressed.empty()) {
    std::cerr << "Warning: corrupt or invalid V2 wrapped data "
              << "(zlib decompression failed). "
              << "Returning original source.\n";
    return source;  /* corrupt data */
  }

  /* Oracle adds a trailing NUL byte — remove it */
  while (!uncompressed.empty() && uncompressed.back() == 0)
    uncompressed.pop_back();

  /* ---- 8. Assemble output ---- */
  std::string result = "CREATE OR REPLACE ";
  result.append(reinterpret_cast<const char*>(uncompressed.data()),
                uncompressed.size());
  /* Ensure trailing slash for SQL*Plus compatibility */
  if (result.back() != '\n') result += '\n';
  result += "/\n";

  return result;
}
