#!/bin/bash
FAILED=0
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
MBUFFER="$SCRIPT_DIR/../mbuffer"
DD=$(which dd)
MD5SUM=$(which openssl || which md5sum || which md5)

cleanup() {
    rm -rf "$TMPDIR"
}

trap cleanup EXIT

echo -n "Test 8: basic 8-stream transfer with MD5 verify... "

TMPDIR=$(mktemp -d)
INPUT="$TMPDIR/input.bin"
OUTPUT="$TMPDIR/output.bin"

$DD if=/dev/urandom bs=1M count=10 of="$INPUT" 2>/dev/null

$MBUFFER -M 8 --cport 9991 -I :9991 > "$OUTPUT" 2>/dev/null &
RECV_PID=$!
sleep 1

$MBUFFER -M 8 --cport 9991 -O 127.0.0.1:9991 < "$INPUT" 2>/dev/null
SEND_RC=$?

wait $RECV_PID 2>/dev/null
RECV_RC=$?

if [ -x "$(which openssl)" ]; then
    IN_MD5=$(openssl md5 < "$INPUT" 2>/dev/null | awk '{print $2}')
    OUT_MD5=$(openssl md5 < "$OUTPUT" 2>/dev/null | awk '{print $2}')
else
    IN_MD5=$($MD5SUM "$INPUT" 2>/dev/null | awk '{print $1}')
    OUT_MD5=$($MD5SUM "$OUTPUT" 2>/dev/null | awk '{print $1}')
fi

if [ "$IN_MD5" != "$OUT_MD5" ]; then
    echo "FAILED (MD5 mismatch)"
    FAILED=1
elif [ $SEND_RC -ne 0 ] && [ $SEND_RC -ne 141 ]; then
    echo "FAILED (sender exit $SEND_RC)"
    FAILED=1
else
    echo "ok"
fi

exit $FAILED
