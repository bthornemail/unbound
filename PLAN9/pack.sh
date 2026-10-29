#!/bin/sh

# Pack the author's unbound files (tracked in git, no node_modules) and the
# Plan 9 sources into run/in.tar, the guest's second disk, with a MANIFEST
# of the files to compile; make a blank run/out.img, its third. The tar is
# byte-for-byte reproducible: sorted names, one mtime, no owners.

set -eu

HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/.." && pwd)
RUN=$HERE/run
mkdir -p "$RUN"

# boxedPINE's proofs/js, copied in for conversion (its commit recorded beside it)
JS=${JS:-/home/main/Programs/boxedPINE/proofs/js}
rm -rf "$RUN/js" && cp -r "$JS" "$RUN/js"
git -C "$JS" rev-parse HEAD > "$RUN/js.commit"
# ZIGUAROTH's _CATALOG entries, Wirth syntax notation at their source, copied in to be read
rm -rf "$RUN/catalog" && mkdir -p "$RUN/catalog"
(cd /home/main/Programs/ZIGUAROTH/_CATALOG && find . -name '*.wsn' | cpio -pdm --quiet "$RUN/catalog" 2>/dev/null || tar cf - $(find . -name '*.wsn') | tar xf - -C "$RUN/catalog")

(cd "$HERE/wsn" && find js unbound -type f -name '*.wsn' | LC_ALL=C sort > LIST)

cd "$ROOT"
git -c core.quotepath=off ls-files -- HTML JS MERMAID SH SVG TS TSX |
    grep -v '/node_modules/' | LC_ALL=C sort > "$HERE/MANIFEST"
LC_ALL=C sort > "$RUN/files" <<LIST
$(cat "$HERE/MANIFEST")
PLAN9/MANIFEST
PLAN9/src/TYPES
PLAN9/src/compile.rc
PLAN9/src/html.rc
PLAN9/src/zhtml.c
PLAN9/src/trans.rc
PLAN9/src/ztrans.c
PLAN9/src/zdiff.c
PLAN9/src/zspan.c
PLAN9/in/span.txt
PLAN9/in/codex.txt
PLAN9/in/yahweh.txt
PLAN9/in/yhwh.txt
PLAN9/src/zbuf.c
$(cd "$HERE" && ls wsn/*.wsn 2>/dev/null | sed 's|^|PLAN9/|')
PLAN9/src/wsn.rc
PLAN9/src/zwsn.c
PLAN9/src/conv.rc
PLAN9/src/wsn.awk
PLAN9/src/back.awk
PLAN9/src/split.awk
PLAN9/src/prec.awk
PLAN9/src/prec.rc
PLAN9/src/manboy.c
PLAN9/src/manboy.rc
PLAN9/src/fill.awk
PLAN9/src/zrender.c
PLAN9/src/tcll2wsn.c
PLAN9/src/zcodex0.c
PLAN9/src/euler.rc
PLAN9/src/four.rc
PLAN9/src/ztetra.c
PLAN9/src/tetra.rc
PLAN9/src/zsync.c
PLAN9/src/distrib.rc
PLAN9/src/modal.rc
PLAN9/src/zquery.c
PLAN9/src/query.rc
PLAN9/src/hello.c
PLAN9/src/hello.rc
PLAN9/src/zperm.c
PLAN9/src/perm.rc
PLAN9/src/zhand.c
PLAN9/src/hand.rc
PLAN9/src/zcodep.c
PLAN9/src/codep.rc
PLAN9/src/zblob.c
PLAN9/src/blob.rc
PLAN9/src/zplaces.c
PLAN9/src/places.rc
PLAN9/src/ztrie.c
PLAN9/src/tries.rc
PLAN9/src/zframe.c
PLAN9/src/frame.rc
PLAN9/src/zkey.c
PLAN9/src/key.rc
PLAN9/src/zvtt.c
PLAN9/src/vtt.rc
PLAN9/src/step.c
PLAN9/src/step.h
PLAN9/src/stepd.c
PLAN9/src/steptest.c
PLAN9/src/step.rc
PLAN9/src/prec.c
PLAN9/src/zprec.c
PLAN9/src/precheck.rc
PLAN9/src/zpss.c
PLAN9/src/pss.rc
PLAN9/src/zfano.c
PLAN9/src/fano.rc
PLAN9/src/zpe.c
PLAN9/src/pe.rc
PLAN9/src/zpcas.c
PLAN9/src/pcas.rc
PLAN9/src/zobs.c
PLAN9/src/obs.rc
PLAN9/src/vtttime.rc
PLAN9/src/zgrex.c
PLAN9/src/grex.rc
PLAN9/src/zpoly.c
PLAN9/src/poly.rc
PLAN9/in/poly/apply9
PLAN9/in/poly/apply10
PLAN9/in/poly/cascade
PLAN9/src/zprobe.c
PLAN9/src/probe.rc
PLAN9/src/zbar.c
PLAN9/src/bar.rc
PLAN9/src/zpipe.c
PLAN9/src/pipe.rc
PLAN9/src/resolve.rc
PLAN9/src/zver.c
PLAN9/src/zorch.c
PLAN9/src/orch.rc
PLAN9/src/zgnomon.c
PLAN9/src/gnomon.rc
PLAN9/space/gnomon.c
PLAN9/src/gnomonanim.rc
PLAN9/src/zborder.c
PLAN9/src/border.rc
PLAN9/src/zak.c
PLAN9/src/ak.rc
PLAN9/src/zconv.c
PLAN9/src/convcheck.rc
PLAN9/src/element.rc
PLAN9/src/zskel.c
PLAN9/src/skel.rc
PLAN9/wsn/skeleton.re
PLAN9/src/zmk.c
PLAN9/src/mk.rc
PLAN9/src/mkfs.c
PLAN9/src/mkd.c
PLAN9/src/mkworld.rc
PLAN9/src/zctl.c
PLAN9/src/ctl.rc
PLAN9/src/zcons.c
PLAN9/src/cons.rc
PLAN9/in/cons/decisions
$(cd "$HERE" && ls in/ctl/* | grep -v 'gen.py$' | sed 's|^|PLAN9/|')
$(cd "$HERE" && ls in/mk/* | grep -v 'gen.ts$' | sed 's|^|PLAN9/|')
$(cd "$HERE" && ls in/ak/* | sed 's|^|PLAN9/|')
$(cd "$HERE" && find in/border -type f | LC_ALL=C sort | sed 's|^|PLAN9/|')
$(cd "$HERE" && ls in/orch/* | sed 's|^|PLAN9/|')
$(cd "$HERE" && ls in/resolve/* | sed 's|^|PLAN9/|')
$(cd "$HERE" && ls in/pipe/* | sed 's|^|PLAN9/|')
$(cd "$HERE" && ls in/grex/* | sed 's|^|PLAN9/|')
$(cd "$HERE" && ls in/vtttime/* | sed 's|^|PLAN9/|')
PLAN9/in/pcas/table
PLAN9/in/fano/table
$(cd "$HERE" && ls in/pss/* | sed 's|^|PLAN9/|')
PLAN9/src/zcue.c
PLAN9/src/cues.rc
PLAN9/in/cues.txt
PLAN9/space/floor.c
PLAN9/src/space.rc
PLAN9/space/observe.c
PLAN9/src/observe.rc
PLAN9/space/agentd.c
PLAN9/src/agentd.rc
PLAN9/src/zcast.c
PLAN9/src/cast.rc
PLAN9/src/stack.c
PLAN9/src/stack.rc
PLAN9/src/zaudit.c
PLAN9/src/audit.rc
$(cd "$HERE" && ls in/audit/* | sed 's|^|PLAN9/|')

$(cd "$HERE" && ls in/narrative/* | sed 's|^|PLAN9/|')
$(cd "$HERE" && ls in/tries/*.paths | sed 's|^|PLAN9/|')
PLAN9/in/gen1.txt
PLAN9/src/template.rc
PLAN9/in/template.txt
PLAN9/in/algol.txt
PLAN9/in/euler.grm
PLAN9/src/yaml2wsn.awk
PLAN9/src/catalog.rc
PLAN9/src/htmlfs.c
PLAN9/src/htmlfs.rc
PLAN9/src/book.rc
PLAN9/src/busybox.rc
PLAN9/src/zlazy.c
PLAN9/src/lazy.rc
$(cd "$HERE" && ls busybox/*.rc busybox/*.c | sed 's|^|PLAN9/|')
PLAN9/src/coq2wsn.awk
$(cd "$HERE" && [ -d run/book/coq ] && find run/book/coq -type f -name '*.v' | LC_ALL=C sort | sed 's|^|PLAN9/|'; [ -f run/book/status ] && echo PLAN9/run/book/status)
PLAN9/src/proofs.rc
$(cd "$HERE" && ls proofs/*.c proofs/*.h | sed 's|^|PLAN9/|')
$(cd "$HERE" && find run/catalog -type f | LC_ALL=C sort | sed 's|^|PLAN9/|')
PLAN9/src/figures.rc
PLAN9/in/zeroth.txt
PLAN9/in/term.html
$(cd "$HERE" && ls in/figures/* | sed 's|^|PLAN9/|')
PLAN9/wsn/LIST
$(cd "$HERE" && find wsn/js wsn/unbound -type f -name '*.wsn' | LC_ALL=C sort | sed 's|^|PLAN9/|')
PLAN9/run/js.commit
$(cd "$HERE" && find run/js -type f | LC_ALL=C sort | sed 's|^|PLAN9/|')
LIST
tar --create --file "$RUN/in.tar" --format=ustar --no-recursion \
    --mtime=@0 --owner=0 --group=0 --numeric-owner --mode='a+r,u+w' \
    --verbatim-files-from --files-from "$RUN/files"
truncate -s 32M "$RUN/in.tar"   # one fixed size, so a running guest sees the whole tar
truncate -s 0 "$RUN/out.img" 2>/dev/null || :   # in place: a running guest keeps the same file
truncate -s 64M "$RUN/out.img"
printf 'PASS: %s files in %s; %s blank\n' "$(wc -l < "$HERE/MANIFEST")" "$RUN/in.tar" "$RUN/out.img"
