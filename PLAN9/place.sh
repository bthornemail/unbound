#!/bin/sh

# Place what figures.rc rendered (run/fig/zout/<path>/) into ZIGUAROTH, under
# the letter its name starts with (the catalog's name, upper case): the codex
# indices in _CODICES/<LLLL>/<path>/<term>.wsn, the braille and Aegean in
# _CODEX/<LL>/<path>/<term> ([0], the braille and Aegean themselves) and
# _CODEX/<LL>/<path>/<term>.wsn ([1], the same in Wirth syntax notation).

set -eu

HERE=$(cd "$(dirname "$0")" && pwd)
Z=${ZIGUAROTH:-/home/main/Programs/ZIGUAROTH}
OUT=$HERE/run/fig

rm -rf "$OUT" && mkdir -p "$OUT" && tar -xf "$HERE/run/out.img" -C "$OUT"
cd "$OUT/zout"
find . -name codices.wsn | sed 's|^\./||; s|/codices.wsn$||' | sort | while read -r P; do
    L=$(head -c 1 "$P/codices.wsn")
    T=$(basename "$P" | tr 'A-Z' 'a-z')
    mkdir -p "$Z/_CODICES/$L$L$L$L/$P" "$Z/_CODEX/$L$L/$P"
    cp "$P/codices.wsn" "$Z/_CODICES/$L$L$L$L/$P/$T.wsn"
    cp "$P/codex" "$Z/_CODEX/$L$L/$P/$T"
    cp "$P/codex.wsn" "$Z/_CODEX/$L$L/$P/$T.wsn"
    printf 'rendered: _CODICES/%s/%s/%s.wsn, _CODEX/%s/%s/%s and %s.wsn\n' "$L$L$L$L" "$P" "$T" "$L$L" "$P" "$T" "$T"
done
