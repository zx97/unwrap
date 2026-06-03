PL/SQL Unwrap v3.2.0
====================

Standalone C++ binary to **unwrap** (and **wrap**) Oracle PL/SQL source
code, ported from the [PL/SQL Unwrapper][original] project by
Cameron Marshall.

C++ port by **Manuel FLURY**. Licensed under the **GNU General Public
License v3.0**.

> **Note:** The V1 unwrapper (Oracle 8/8i/9i) is fully ported but has
> not been validated against real-world V1 wrapped files. Use with
> caution. The V2 unwrapper (10g+) is verified against Oracle 19c and
> 23ai.

Features
--------
- V1 unwrap — Oracle 8/8i/9i **(ported, untested)**
- V2 unwrap — Oracle 10g+ through 23ai
- V2 wrap   — encode plain PL/SQL into native Oracle wrapped format
  - Verified byte-identical with Oracle 19c native `wrap` utility
  - Supports: TYPE, PRAGMA, RESULT_CACHE, DETERMINISTIC, parameters
  - Schema-qualified names require `--compat` (strips the prefix)
- **Obfuscate** — rename identifiers to very short names, embed
  AES-256-CBC encrypted mapping with PBKDF2 key derivation
- **Deobfuscate** — restore original names from obfuscated source
- Auto-detection of wrapped sections (mixed files, multiple sections)
- Embedded license & README in the binary
- `-p <passphrase>` auto-deobfuscates after unwrap
- `--compat` / `-c` strips schema prefix from object names

Limits
------
- Maximum input file size: **128 MB**
- V1 unwrapper is ported but lacks real-world validation
- Schema-qualified object names (`schema.func`) produce invalid
  wrapped output; use `--compat` to strip the prefix before wrapping
- Multi-object files (spec + body) need `//` separator between
  CREATE statements — each unit is wrapped independently

Usage
-----
```
unwrap [options] [-i <file>] [-o <file>]
wrap   [options]  -i <file> [-o <file>]
```

| Option                     | Description                                   |
|----------------------------|-----------------------------------------------|
| `-i`, `--input`            | input file (stdin if piped)                   |
| `-o`, `--output`           | output file (stdout if omitted)               |
| `--v1`                     | force V1 unwrapper (8/8i/9i — UNTESTED)       |
| `--v2`                     | force V2 unwrapper (10g+)                     |
| `--wrap`                   | wrap source (V2 method)                       |
| `--obfuscate`, `--obf`     | rename identifiers to short names + wrap      |
| `--deobfuscate`, `--deobf` | restore original names (needs passphrase)     |
| `-p`, `--passphrase`       | passphrase for obfuscation / auto-deobfuscate |
| `--keep-comments`          | preserve comments (default: strip them)       |
| `--compat`, `-c`           | strip schema prefix from object names         |
| `-V`, `--version`          | show version and author                       |
| `-l`, `--license`          | show license information                      |
| `--readme`                 | show embedded README                          |
| `-h`, `--help`             | show this help                                |

Oracle-compatible syntax (wrap mode)
-------------------------------------
```
wrap iname=file.sql oname=file.pls keep_comments=yes help=yes
```

When invoked without arguments and stdin is a terminal, usage is
displayed instead of reading from stdin.  The `-p` flag alone triggers
deobfuscation automatically after unwrap.

Building
--------
**Dependencies:** cmake, zlib, OpenSSL (C++17).

```
make
make test
make clean
```

Install command by distribution:
- **Debian/Ubuntu:** `apt install build-essential cmake libz-dev libssl-dev`
- **Fedora/RHEL:** `dnf install gcc-c++ cmake zlib-devel openssl-devel`
- **Alpine:** `apk add build-base cmake zlib-dev openssl-dev`

Original project
----------------
https://github.com/oddz/PL-SQL-Unwrapper
[original]: https://github.com/oddz/PL-SQL-Unwrapper
