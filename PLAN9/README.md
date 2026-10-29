# PLAN9

The Plan 9 runtime for unbound: a 9legacy CPU server (`helix`) under QEMU.
The disk is rebuilt from the archived image, checked by SHA-256; nothing
here is the image itself.

| File | What it does |
|---|---|
| `SHA256SUMS` | The checksum of `9legacy-qemu.img.bz2` (132,168,329 bytes) |
| `build.sh` | Checks the image, unpacks it sparse into `disk/` (10 GB, about 500 MB used) |
| `run.sh` | Boots it headless; ports bound to 127.0.0.1 only |
| `console.py` | Types into the console and dumps the screen, through the QEMU monitor |

```sh
sh build.sh            # once; default source is boxed/_archive/_inbox
sh run.sh
python3 console.py $'cat /dev/sysname\n' run/screen.png
```

## Compiling unbound in Plan 9

Following EULER (Wirth and Weber, 1966: a language defined by its compiler),
the buffer rule of ZIGUAROTH is compiled twice, by two compilers that must agree:

| Step | Where | What |
|---|---|---|
| `sh pack.sh` | host | the author's 119 git-tracked files in `HTML JS MERMAID SH SVG TS TSX` (no `node_modules`), `MANIFEST` and `src/` into `run/in.tar` (reproducible: same SHA-256 on every run), a blank `run/out.img` |
| `sh run.sh` | host | the tar is `/dev/sd01` (read-only), the blank is `/dev/sd02` |
| `rc compile.rc` | Plan 9 | unpacks `/dev/sd01`, compiles `src/zbuf.c` with `8c`/`8l`, frames every file as a buffer, writes them as a tar to `/dev/sd02` |
| `node verify.mjs` | host | each buffer must equal ZIGUAROTH's `buffer.js wrap` of the same file, byte for byte, and pass `buffer.js check` |

In the guest (the first line takes `compile.rc` off the disk):

```rc
mkdir -p /tmp/b; @{cd /tmp/b; tar xf /dev/sd01/data PLAN9/src/compile.rc}
rc /tmp/b/PLAN9/src/compile.rc
```

`src/TYPES` is the one content-type table both compilers read. `in` is an rc
keyword, so the script names its disks `src` and `dst`.

## Ports (host 127.0.0.1 → guest)

| Host | Guest | Service | State in the image |
|---|---|---|---|
| 17010 | 17010 | cpu (drawterm) | listening |
| 10564 | 564 | 9fs | disabled (`/rc/bin/service/!tcp564`) |
| 10567 | 567 | auth | not running |

## Provenance

| File | Source | Server's Last-Modified | Bytes |
|---|---|---|---|
| `9legacy-qemu.img.bz2` | https://9legacy.org/download/9legacy-qemu.img.bz2 | 2019-04-21 19:29:31 GMT | 132,168,329 |
| `v8-simh.tar.bz2` (not used yet) | https://9legacy.org/download/unix/v8-simh.tar.bz2 | 2017-03-30 20:44:00 GMT | 15,985,169 |

Listed at https://9legacy.org/download.html ("QEMU Disk image (2019-04-21)").
The author's copies are in `boxed/_archive/_inbox` (2026-10-27); their lengths
match the server's on 2026-10-27. 9legacy publishes no checksum for the disk
images, so `SHA256SUMS` records the archived copy, not an upstream value.
9legacy's own build scripts (rc, run inside Plan 9) are at
https://9legacy.org/9legacy/build; the GCE notes, which also cover QEMU, are
at https://9legacy.org/9legacy/doc/gce/notes. The download page states no
licence. `v8-simh` (8th Edition Research Unix) needs SIMH's VAX emulator,
which is not installed.

As shipped, `/lib/ndb/local` points
authentication at `sources.cs.bell-labs.com` (`authdom=outside.plan9.bell-labs.com`),
which no longer answers, so drawterm cannot log in until `helix` is made its own
auth server.

## Rebuilding

`build.sh` refuses to overwrite `disk/9legacy.img`. Remove it to start
again from the archived image; anything written inside the guest is lost.
