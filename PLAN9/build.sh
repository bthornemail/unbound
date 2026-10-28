#!/bin/sh

# Build the Plan 9 runtime disk from the archived 9legacy QEMU image. The
# image is checked against its recorded SHA-256 before it is unpacked; the
# unpacked disk is sparse and stays out of git (disk/).

set -eu

HERE=$(cd "$(dirname "$0")" && pwd)
SRC=${1:-/home/main/Programs/boxed/_archive/_inbox/9legacy-qemu.img.bz2}
SUM=$(sed -n 's/^\([0-9a-f]\{64\}\)  9legacy-qemu.img.bz2$/\1/p' "$HERE/SHA256SUMS")
DISK=$HERE/disk/9legacy.img

[ -f "$SRC" ] || { printf 'Usage: sh build.sh [9legacy-qemu.img.bz2]\n' >&2; exit 2; }
[ ! -e "$DISK" ] || { printf 'FAIL: %s exists; remove it to rebuild\n' "$DISK" >&2; exit 1; }

GOT=$(sha256sum "$SRC" | cut -d ' ' -f 1)
[ "$GOT" = "$SUM" ] || {
    printf 'FAIL: %s has SHA-256 %s, expected %s\n' "$SRC" "$GOT" "$SUM" >&2
    exit 1
}

mkdir -p "$HERE/disk"
bzip2 -dc "$SRC" | cp --sparse=always /dev/stdin "$DISK"
printf 'PASS: %s (%s)\n' "$DISK" "$(du -h "$DISK" | cut -f 1) on disk"
