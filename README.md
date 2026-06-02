PL/SQL Unwrap v3.0
==================

Standalone C++ binary to **unwrap** (and **wrap**) Oracle PL/SQL source
code, ported from the [PL/SQL Unwrapper][original] project by
Cameron Marshall.

C++ port by **Manuel FLURY**. Licensed under the **GNU General Public
License v3.0**.

> **Note:** The V1 unwrapper (Oracle 8/8i/9i) is fully ported but has
> not been validated against real-world V1 wrapped files. Use with
> caution. The V2 unwrapper (10g+) is verified and stable.

Features
--------
- V1 unwrap — Oracle 8/8i/9i **(ported, untested)**
- V2 unwrap — Oracle 10g+ through 23ai
- V2 wrap   — encode plain PL/SQL into wrapped format (comments stripped
  by default, `--keep-comments` to preserve them)
- **Obfuscate** — rename identifiers to very short names, embed
  AES-256-CBC encrypted mapping for reversible deobfuscation
- Auto-detection of wrapped sections
- Mixed files (unwrapped + wrapped content)
- Multiple wrapped sections in a single file
- Embedded license & README in the binary

Limits
------
- Maximum input file size: **128 MB**
- V1 unwrapper is ported but lacks real-world validation
- V1 unwrap — Oracle 8/8i/9i (full DIANA grammar)
- V2 unwrap — Oracle 10g+ through 23ai
- V2 wrap   — encode plain PL/SQL into wrapped format
- Auto-detection of wrapped sections
- Mixed files (unwrapped + wrapped content)
- Multiple wrapped sections in a single file
- Embedded license & README in the binary

Usage
-----
```
unwrap [options] [-i <file>] [-o <file>]
wrap   [options]  -i <file> [-o <file>]
```

| Option              | Description                                |
|---------------------|--------------------------------------------|
| `-i`, `--input`     | input file (stdin if piped)                |
| `-o`, `--output`    | output file (stdout if omitted)            |
| `--v1`              | force V1 unwrapper (8/8i/9i)               |
| `--v2`              | force V2 unwrapper (10g+)                  |
| `--wrap`            | wrap source (V2 method)                    |
| `--keep-comments`   | preserve comments when wrapping            |
| `--obfuscate`, `--obf` | rename identifiers to short names        |
| `--deobfuscate`, `--deobf` | restore original names (needs passphrase) |
| `-p`, `--passphrase` | encryption passphrase for obfuscation     |
| `-l`, `--license`   | show license information                   |
| `-h`, `--help`      | show this help                             |

When invoked without arguments and stdin is a terminal, usage is
displayed instead of reading from stdin.

The `--license` and `--readme` flags work on any copy of the binary
— the information is compiled in.

Oracle-compatible syntax (wrap mode)
-------------------------------------
```
wrap iname=file.sql oname=file.pls keep_comments=yes
```

When invoked as `wrap` (symlink to `unwrap`), defaults to wrap mode.

 Building
 --------
 **Dependencies:** cmake, zlib, OpenSSL development libraries (C++17 compiler required).

 On Debian/Ubuntu:
 ```
 sudo apt install build-essential cmake libz-dev libssl-dev
 ```
 On Fedora/RHEL:
 ```
 sudo dnf install gcc-c++ cmake zlib-devel openssl-devel
 ```
 On Alpine:
 ```
 sudo apk add build-base cmake zlib-dev openssl-dev
 ```

 ```
 make
 make test
 make clean
 ```

Original project
----------------
https://github.com/oddz/PL-SQL-Unwrapper
[original]: https://github.com/oddz/PL-SQL-Unwrapper
