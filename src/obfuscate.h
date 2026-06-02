#pragma once
#include <string>
#include <vector>

/* Obfuscate PL/SQL source: rename local identifiers to short names,
 * strip comments (unless keep_comments is true), embed encrypted mapping.
 * Returns obfuscated source, or empty string on error. */
std::string obfuscate_plsql(const std::string& source,
                            const std::string& passphrase,
                            bool keep_comments = false);

/* Deobfuscate: extract encrypted mapping, decrypt, restore original names.
 * Returns restored source, or empty string on error. */
std::string deobfuscate_plsql(const std::string& source,
                              const std::string& passphrase);
