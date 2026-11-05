#!/bin/sh
# lalr.sh - on the host, after src/lalr.rc has run in the guest: build src/wsn.y with GNU Bison, read the same files
# in the same order (wsn/*.wsn, wsn/LIST, in/lalr/mutants/LIST), and compare with the guest's answers on run/out.img
# (Plan 9's yacc, zlalr.txt, and zwsn's recursive descent, zwsn.norm). All three must be the same, byte for byte.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
cd "$HERE"
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
cp src/wsn.y "$T/" && (cd "$T" && bison -y wsn.y && cc -w -o zlalr y.tab.c)
tar xf run/out.img -C "$T" 2>/dev/null || { echo 'FAIL: no answers on run/out.img'; exit 1; }
( export LC_ALL=C
  "$T/zlalr" $(ls wsn/*.wsn)
  sed 's|^|wsn/|' wsn/LIST | tr '\n' '\0' | xargs -0 "$T/zlalr"
  sed 's|^|in/lalr/mutants/|' in/lalr/mutants/LIST | tr '\n' '\0' | xargs -0 "$T/zlalr" ) > "$T/bison.txt"
cmp -s "$T/bison.txt" "$T/zout/zlalr.txt" || { echo 'FAIL: Bison and Plan 9 yacc differ'; exit 1; }
cmp -s "$T/bison.txt" "$T/zout/zwsn.norm" || { echo 'FAIL: Bison and zwsn differ'; exit 1; }
echo "PASS: $(wc -l < "$T/bison.txt") files, Bison, Plan 9 yacc and zwsn the same: $(grep -c productions "$T/bison.txt") accepted, $(grep -c rejected "$T/bison.txt") rejected"
