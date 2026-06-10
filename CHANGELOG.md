# Changelog

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