# Changelog

## 0.2.0 (2026-06-10)

### Multiplexing (Mux) Mode — NEW

mbuffer now supports **multi-stream multiplexing** over TCP for high-throughput bulk data transfer. Data is striped across 2–8 parallel TCP streams with automatic reordering on the receiver side.

- **Striped data transfer**: Incoming blocks are distributed round-robin across `-M N` TCP data streams
- **Receiver-side reordering**: Min-heap reorder queue reassembles blocks in sequence order regardless of delivery order
- **Control channel**: Dedicated control connection for handshake, flow control, ACK/NAK, and heartbeats
- **CRC-16 integrity**: Optional per-frame CRC verification (`--crc` toggle)
- **Retransmission**: NAK-based gap detection triggers selective retransmit from sender
- **Dynamic stream failure**: Dead streams are detected and drained with automatic retransmission of in-flight blocks

Basic usage — send 10MB across 8 streams:

```sh
# Receiver (port 9999)
mbuffer -M 8 --cport 9999 -I :9999 > output.bin

# Sender
mbuffer -M 8 --cport 9999 -O 127.0.0.1:9999 < input.bin
```

Performance (loopback, 100MB with 8 streams): **~128 MB/s**

### Bug Fixes

- **FIXED** (partial-block data corruption): Propagated actual `payload_len` through `slot_meta_t` → `reorder_entry_t` → output write. Previously `outputThreadMux` always wrote `Blocksize` bytes, corrupting partial last blocks with buffer garbage. MD5 mismatch resolved.
- **FIXED** (input double-push on EOF): `inputThread()` was pushing the last block to `ReadyPool` again after `readBlock()` already pushed it on EOF. Removed duplicate push.
- **FIXED** (sender shutdown): Added `InputDone` flag for graceful mux sender shutdown sequence — sender threads drain the ready pool before sending EOF frames instead of hard-terminating.
- **FIXED** (receiver flush): Receiver now drains remaining reorder queue entries after all reader threads join, ensuring no data loss on completion.
- **FIXED** (control handshake): Fixed handshake order — server now waits for client HELLO before responding, matching established protocol convention.

### Build

- Added new source files to autoconf (`Makefile.in`) and CMake (`CMakeLists.txt`): `control.c`, `mux_proto.c`, `ready_pool.c`, `reorder.c`, `sender_thread.c`, `reader_thread.c`

### Testing

- Updated `test8.sh` — now runs a full 8-stream mux transfer with MD5 verification (10MB random data)

---

## 0.1.0 (2026-06-09)

### Security Fixes

- **CRITICAL**: Fixed `system()` command injection in autoloader — filenames are now shell-escaped before interpolation into autoloader commands (`shell_escape()` in `common.c`). Input and output autoloader paths both covered.
- **MEDIUM**: Fixed output autoloader using `Infile` (global input) instead of `outfile` parameter — copy-paste bug in `requestOutputVolume()`.
- **MEDIUM**: Harden config file permission check — rejects world-writable and group-writable config files (`S_IWGRP | S_IWOTH`).
- **MEDIUM**: Hardened `mt` command path — `mt` is now resolved at configure time via `AC_PATH_PROG` and hardcoded into the binary as `MT_PATH`, eliminating PATH poisoning.
- **LOW**: Made signal handler async-signal-safe — removed `close()` and `pthread_cond_signal()` calls; only `write(TermQ[1])` and volatile flag sets remain.
- **LOW**: Output thread no longer blocks indefinitely on `pthread_cond_wait` — uses `pthread_cond_timedwait` with 100ms timeout and Terminate check for timely SIGINT response.

### Testing

- Added `test8.sh` — security gate verification suite with 5 tests:
  - Input filename injection neutralization
  - Output filename injection neutralization
  - World-writable config rejection
  - SIGINT graceful shutdown
  - Normal `--md5` operation

### Build

- Added `MT_PATH` define to build system (`configure.in` / `Makefile.in`)
- Added `test8` target to `Makefile.in`

---

## Earlier Releases (upstream)

### R20250809
- fix cmake build issue
- fix some warnings on intentionally ignored return values

### 20250429
- get number of available pages on NetBSD via sysctl
- use SYSCONFDIR instead of PREFIX "/etc"
- clang configure fix for tapedrive emulation

### 20241007
- build fix for -Wincompatible-pointer-types
- configure script update to autoconf version 2.71

### 20240929
- updated default buffer calculation for more sane defaults

### 20240818
- fix port range check (Peter Pentchev)
- fix ipv6 only mode
- enhanced incoming host check

### 20240707
- fix IPv4 address printing
- listen on IPv6 and IPv4 if possible

### 20240107
- corrections for documentation and help output (patch by Andreas Hartmann)
- added environment variables for auto-loader command

### 20231216
- changed from `which` to `command` for Debian forward compatibility

### 20230301
- Fix breaking connections on WAN links: retry on EAGAIN (fix by Nico Schümann)

### 20220418
- fix handling of filesystem full on stdout

### 20211018
- fixes related to TCP timeout handling
- support setting config file via env var MBUFFERRC

### 20211004
- make TCPTimeout=0 disable the TCP timeout
- changed default TCP timeout from 10s to 100s

### 20210829
- accept IPv6 addresses in square bracket format

### 20210328
- removed cancel after join for reader thread

### 20210209
- fix: hash algos should not suppress stdout

### 20200929
- added option --no-direct to disable use of O_DIRECT
- raised default TCP timeout to 10ms for WAN connections
- performance optimization: use recv with MSG_WAITALL
- automatic version string generation

### 20200505
- configure fix for some powerpc toolchains
- added option to perform direct I/Os on temporary file

### 20191016
- autoadjust dependent parameters
- human readable buffer size information

### 20190725
- hashing infrastructure should also be enabled with libdl available

### 20181221
- TCP network mode: added server-side filter mode

### 20180920
- Fix problem with data loss due to SIGHUP

### 20180815
- multithreading: use semaphores and condition variables
- removed delay options
- fixes for Solaris

### 20171122
- added hash verification support

### 20160319
- fix for data corruption issue with multi-volume

### 20160225
- large file support fixes

### 20151123
- using mkostemp using O_EXCL for temp file creation

### 20100512
- initial autoconf/automake based build system
- initial IPv6 support

### 2001 (initial release)
- initial release by Thomas Maier-Komor