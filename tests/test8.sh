#!/bin/bash
#
# Security gate verification tests for mbuffer
#
# Tests:
#   A - Input filename injection neutralization (autoloader)
#   B - Output filename injection neutralization (autoloader)
#   C - World-writable config file rejection
#   D - SIGINT graceful shutdown
#   E - Normal operation unchanged
#
# Copyright (C) 2025, Thomas Maier-Komor
#

FAILED=0
TS=$(date +%s)

cleanup() {
    rm -rf /tmp/mbuffer_test8_*
}

trap cleanup EXIT

# Resolve paths relative to script location (works from project root or tests/ dir)
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
MBUFFER="$SCRIPT_DIR/../mbuffer"
PROJECT_DIR="$SCRIPT_DIR/.."

# ------------------------------------------------------------------
# Test A: Input filename injection neutralization
# ------------------------------------------------------------------
echo -n "Test A: input filename injection neutralization... "

# Create a file with shell metacharacters in the name
INJECTION_FILE_A="/tmp/mbuffer_test8_${TS}_;touch /tmp/PWNED_A;.bin"
dd if=/dev/zero bs=1k count=10 of="$INJECTION_FILE_A" 2>/dev/null

# Run mbuffer with injection filename as input.
# The autoloader triggers after EOF and runs: mt -f <escaped_name> offline
# shell_escape() must neutralize the semicolons so the touch command never runs.
# Output goes to stdout (pipe) to avoid the "both Infile and OutFile set" fatal.
$MBUFFER -i "$INJECTION_FILE_A" -n 1 -a 1 > /dev/null 2>/dev/null
RC=$?

if [ -f /tmp/PWNED_A ]; then
    echo "FAILED (injection succeeded - /tmp/PWNED_A created)"
    FAILED=1
else
    echo "ok"
fi

# ------------------------------------------------------------------
# Test B: Output filename injection neutralization
# ------------------------------------------------------------------
echo -n "Test B: output filename injection neutralization... "

# Create a small input file
dd if=/dev/zero bs=1k count=10 of=/tmp/mbuffer_test8_input.bin 2>/dev/null

# Create an output filename with shell metacharacters
INJECTION_FILE_B="/tmp/mbuffer_test8_${TS}_;touch /tmp/PWNED_B;.out"

# Pipe input, use injection filename as output.
# The output autoloader triggers when the volume is full.
# Use -V 4096 to force volume-full after 4KB, and -n 2 to allow a second volume.
cat /tmp/mbuffer_test8_input.bin | $MBUFFER -o "$INJECTION_FILE_B" -n 2 -a 1 -V 4096 2>/dev/null
RC=$?

if [ -f /tmp/PWNED_B ]; then
    echo "FAILED (injection succeeded - /tmp/PWNED_B created)"
    FAILED=1
elif [ $RC -ne 0 ]; then
    echo "FAILED (exit code $RC)"
    FAILED=1
else
    echo "ok"
fi

# ------------------------------------------------------------------
# Test C: World-writable config file rejection
# ------------------------------------------------------------------
echo -n "Test C: world-writable config rejection... "

# Config files are auto-loaded from ~/.mbuffer.rc, /etc/mbuffer.rc, SYSCONFDIR/mbuffer.rc
# Create a temp home dir with a world-writable .mbuffer.rc
TMP_HOME="/tmp/mbuffer_test8_home_${TS}"
mkdir -p "$TMP_HOME"
touch "$TMP_HOME/.mbuffer.rc"
chmod 0666 "$TMP_HOME/.mbuffer.rc"

OUTPUT=$(HOME="$TMP_HOME" $MBUFFER -i /dev/null -o /dev/null 2>&1)
RC=$?

if ! echo "$OUTPUT" | grep -q "ignoring"; then
    echo "FAILED (no 'ignoring' message in output)"
    FAILED=1
elif [ $RC -ne 0 ]; then
    echo "FAILED (exit code $RC)"
    FAILED=1
else
    echo "ok"
fi

# ------------------------------------------------------------------
# Test D: SIGINT graceful shutdown
# ------------------------------------------------------------------
echo -n "Test D: SIGINT graceful shutdown... "

# Start mbuffer with a large pipe in background
dd if=/dev/zero bs=1M count=500 2>/dev/null | $MBUFFER -o /dev/null -D 50M 2>/dev/null &
MBPID=$!

# Give it a moment to start
sleep 1

# Send SIGINT
kill -INT $MBPID 2>/dev/null

# Wait up to 5 seconds for it to exit
WAIT=0
EXITED=0
while [ $WAIT -lt 5 ]; do
    if ! kill -0 $MBPID 2>/dev/null; then
        EXITED=1
        break
    fi
    sleep 1
    WAIT=$((WAIT + 1))
done

if [ $EXITED -eq 0 ]; then
    # Force kill if still running
    kill -KILL $MBPID 2>/dev/null
    echo "FAILED (did not exit within 5 seconds)"
    FAILED=1
else
    echo "ok"
fi

# ------------------------------------------------------------------
# Test E: Normal operation unchanged
# ------------------------------------------------------------------
echo -n "Test E: normal operation with --md5... "

$MBUFFER -P90 --md5 -i "$PROJECT_DIR/INSTALL" -o /dev/null 2>/dev/null
RC=$?

if [ $RC -ne 0 ]; then
    echo "FAILED (exit code $RC)"
    FAILED=1
else
    echo "ok"
fi

# ------------------------------------------------------------------
# Summary
# ------------------------------------------------------------------
if [ $FAILED -ne 0 ]; then
    echo "Some tests FAILED."
    exit 1
fi

echo "All tests passed."
exit 0
