#!/bin/sh

# Validate the reference-template frame grammar with BusyBox regular
# expressions. This checks shape across full, list, and full_list; it does not
# derive values or assign semantics to applet names.

set -eu

BUSYBOX=${BUSYBOX:-busybox}
ROOT=${1:-.}

fail() {
    printf 'FAIL: reference-template frame: %s\n' "$1" >&2
    exit 1
}

require_file() {
    [ -r "$ROOT/$1" ] || fail "missing $1"
}

reject_nonmatching() {
    file=$1
    pattern=$2
    if "$BUSYBOX" grep -Ev "$pattern" "$ROOT/$file" | "$BUSYBOX" grep '[^[:space:]]' >/dev/null; then
        fail "malformed line in $file"
    fi
}

require_file full
require_file list
require_file full_list

# The two outer frames are whitespace-free token streams.
reject_nonmatching full '^[^[:space:]]+$'
reject_nonmatching list '^[^[:space:]]+$'

# The composite frame must retain cat -An line numbers and end markers.
reject_nonmatching full_list '^ *[0-9]+[[:space:]].*\$$'

# The center frame must visibly contain the side-by-side comparison separator.
"$BUSYBOX" grep -E '\^I[|]' "$ROOT/full_list" >/dev/null ||
    fail 'missing reversed comparison center'

# The composite must contain both path-qualified and short-name forms.
"$BUSYBOX" grep -E '[[:space:]](bin|sbin|usr/)[^[:space:]]*\$$' "$ROOT/full_list" >/dev/null ||
    fail 'missing path-qualified outer view'
"$BUSYBOX" grep -E '[[:space:]](ash|cat|diff|rev)\$$' "$ROOT/full_list" >/dev/null ||
    fail 'missing short-name outer view'

printf 'PASS: reference-template regex grammar in %s\n' "$ROOT"
