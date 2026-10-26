#!/bin/sh

# Validate only the XOR-to-XNOR complement relation for two equal-length raw
# byte streams. For every pair: xor ^ xnor == 0xff.

set -eu

BUSYBOX=${BUSYBOX:-busybox}
A=${1:-}
B=${2:-}

[ -n "$A" ] && [ -n "$B" ] || {
    printf 'Usage: sh check-xor-xnor.sh FILE1 FILE2\n' >&2
    exit 2
}

TMP=$("$BUSYBOX" mktemp -d "${TMPDIR:-/tmp}/xor-xnor.XXXXXX")
trap '"$BUSYBOX" rm -rf "$TMP"' EXIT HUP INT TERM

bytes() {
    "$BUSYBOX" od -An -v -tu1 "$1" |
        "$BUSYBOX" tr -s '[:space:]' '\n' |
        "$BUSYBOX" grep '[0-9]' > "$2" || :
}

bytes "$A" "$TMP/a"
bytes "$B" "$TMP/b"
LA=$("$BUSYBOX" wc -l < "$TMP/a")
LB=$("$BUSYBOX" wc -l < "$TMP/b")
[ "$LA" -eq "$LB" ] || {
    printf 'FAIL: XOR/XNOR inputs have unequal lengths\n' >&2
    exit 1
}

"$BUSYBOX" paste -d ' ' "$TMP/a" "$TMP/b" > "$TMP/pairs"
COUNT=0
while read -r X Y; do
    XOR=$((X ^ Y))
    XNOR=$((XOR ^ 255))
    [ $((XOR ^ XNOR)) -eq 255 ] || {
        printf 'FAIL: XOR/XNOR complement relation\n' >&2
        exit 1
    }
    COUNT=$((COUNT + 1))
done < "$TMP/pairs"

printf 'PASS: XOR/XNOR complement relation for %s bytes\n' "$COUNT"
