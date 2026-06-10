# mbuffer — Secured Fork

**Version 0.1.0** — Pipe buffer with throughput display, TCP networking, multi-volume tape support, and MD5 hashing.

mbuffer is a multi-threaded replacement for the classic `buffer` program. It buffers pipe data between I/O operations with real-time throughput display, TCP/IP networking (IPv4/IPv6), autoloader-aware multi-volume tape support, and optional MD5 integrity verification.

This is a **security-hardened fork** of Thomas Maier-Komor's [original mbuffer](https://github.com/mnott/mbuffer) with multiple critical and medium-severity vulnerabilities fixed. See [Security Fixes](#security-fixes) below.

---

## Security Fixes

Four confirmed vulnerabilities were discovered and fixed in this release. Full details in [`CHANGELOG.md`](CHANGELOG.md).

| Severity | Finding | Fix |
|----------|---------|-----|
| **CRITICAL** | `system()` command injection via `-A` and `-i` flags — filenames with shell metacharacters (``;&`'"$()|``) flow unsanitized into `system()` calls in both input and output autoloaders. **Working PoC confirmed.** | New `shell_escape()` utility escapes 22 shell metacharacters before filename interpolation into autoloader commands. Both input (`input.c`) and output (`mbuffer.c`) autoloaders covered. |
| **MEDIUM** | Output autoloader uses `Infile` (global input filename) instead of `outfile` (its own parameter) — copy-paste bug in `requestOutputVolume()`. On a dual autoloader setup, the output-side tape command references the input device. | Changed `Infile` → `outfile` at the output autoloader call site. |
| **MEDIUM** | Config file permission bypass — only UID ownership was checked. A world-writable config file owned by root (`chmod 0666`) was silently accepted by any user. | Rejects world-writable and group-writable config files (`S_IWGRP \| S_IWOTH`) with a warning message. |
| **MEDIUM** | `mt` command resolved via `PATH` — `system("mt -f ... offline")` searches `PATH` at runtime, enabling PATH poisoning. | Hardcoded `/usr/bin/mt` via configure-time `AC_PATH_PROG` (`-DMT_PATH`). |
| **LOW** | Signal handler calls `close()` and `pthread_cond_signal()` — neither is async-signal-safe per POSIX. | Signal handler reduced to only `write(TermQ[1])` and volatile flag sets. Output thread uses `pthread_cond_timedwait` with Terminate check for timely SIGINT shutdown. |

### Severity Scoring

All findings were verified with working proof-of-concept exploits during a structured security review (5-agent parallel audit). Integer overflow and world-writable file creation findings from the audit were **falsified** — the semaphore cap prevents overflow on 64-bit systems, and the default umask (0022) reduces 0666 → 0644 in practice.

### Security Gate Tests

A dedicated test suite (`tests/test8.sh`, run via `make check`) verifies all fixes:

```
Test A: input filename injection neutralization... ok
Test B: output filename injection neutralization... ok
Test C: world-writable config rejection......... ok
Test D: SIGINT graceful shutdown............... ok
Test E: normal operation with --md5............ ok
All tests passed.
```

---

## Features

- **Speed display**: Real-time I/O throughput and transfer rate
- **TCP networking**: Client/server mode over IPv4 and IPv6
- **Multi-volume tape**: Autoloader support with configurable volume sizes
- **MD5 hashing**: Optional integrity verification via OpenSSL
- **Memory-mapped I/O**: Optional huge buffer files via mmap
- **Threaded architecture**: Multi-threaded instead of shared-memory IPC
- **Buffer-compatible CLI**: Drop-in replacement for `buffer` command-line options

## Quick Start

```sh
./configure
make
make check       # run test suite (includes security gate tests)
sudo make install
```

Basic usage — pipe data through mbuffer with speed display:

```sh
tar cf - /usr | mbuffer | tar tf - > out
```

## Build

### Autoconf (primary)

```sh
./configure
make
make check
```

### CMake (secondary)

```sh
cmake -B build -DENABLE_TESTING=ON
cmake --build build
ctest --test-dir build
```

## Documentation

- `INSTALL` — Build and install instructions
- [`CHANGELOG.md`](CHANGELOG.md) — Full release history
- Man page: `man mbuffer`

## Requirements

- Linux (primary target), FreeBSD, NetBSD, Solaris
- **macOS**: Not supported — lacks `sem_getvalue()` (upstream author declined to work around it)
- Dependencies: pthreads, OpenSSL (libcrypto), math library (libm)
- Compiler: C99-compatible (`AC_PROG_CC_C99`)

## License

GNU General Public License v3.0 — see [`LICENSE`](LICENSE).

## Credits

**Author:** Thomas Maier-Komor — [original mbuffer](http://www.maier-komor.de/mbuffer.html) (GPLv3, 2001–2025)

**Security audit and fixes:** [FirebirdRender](https://github.com/FirebirdRender) — [mbuffer2026](https://github.com/FirebirdRender/mbuffer2026)