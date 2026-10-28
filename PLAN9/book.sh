#!/bin/sh

# Compile boxedPINE's Coq proof book module by module, in the registry's order
# (proofs/Makefile CHECKED_MODULES), with the Makefile's own flags, and keep
# going past a failure. Writes run/book/status: one line per module,
#   module <tab> dir/file.v <tab> ok | fail <tab> first error line <tab> message
# The .vo files go to run/book/vo, not to the repository's artifacts.

set -u

HERE=$(cd "$(dirname "$0")" && pwd)
P=${PROOFS:-/home/main/Programs/boxedPINE/proofs}
OUT=$HERE/run/book
rm -rf "$OUT" && mkdir -p "$OUT/vo" "$OUT/log"

MODULES=$(make -s -C "$P" -pn 2>/dev/null | sed -n 's/^CHECKED_MODULES := //p' | head -1)
[ -n "$MODULES" ] || MODULES=$(awk '/^[A-Z]+_MODULES := /{f=1;next} f&&/^\t/{print} /^$/{f=0}' "$P/Makefile" | tr -d '\\\t')
cd "$P/coq"
: > "$OUT/status"
for m in $(awk '/^(FOUNDATION|CLOSURE|INCIDENCE|PROJECTION|EXECUTION)_MODULES := /{f=1;next} f&&/^\t/{print} /^$/{f=0}' "$P/Makefile" | tr -d '\\'); do
    f=$(ls 0*/"$m.v" 2>/dev/null | head -1)
    [ -n "$f" ] || { printf '%s\t-\tfail\t0\tno source file\n' "$m" >> "$OUT/status"; continue; }
    if timeout 900 coqc -Q . OmiCore -Q "$OUT/vo" '' -Q 00-foundations '' -Q 01-incidence '' \
        -Q 02-closure '' -Q 03-projection '' -Q 04-execution '' \
        -o "$OUT/vo/$m.vo" "$f" > "$OUT/log/$m" 2>&1; then
        printf '%s\t%s\tok\t-\t-\n' "$m" "$f" >> "$OUT/status"
    else
        line=$(sed -n 's/^File "[^"]*", line \([0-9]*\),.*/\1/p' "$OUT/log/$m" | head -1)
        msg=$(grep -A2 '^Error' "$OUT/log/$m" | tr '\n' ' ' | sed 's/  */ /g' | cut -c1-200)
        printf '%s\t%s\tfail\t%s\t%s\n' "$m" "$f" "${line:-0}" "$msg" >> "$OUT/status"
    fi
done
printf 'book.sh: %s modules, %s compile, %s fail\n' "$(wc -l < "$OUT/status")" "$(grep -c '	ok	' "$OUT/status")" "$(grep -c '	fail	' "$OUT/status")"
