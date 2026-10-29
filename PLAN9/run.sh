#!/bin/sh

# Boot the Plan 9 runtime headless. Every forwarded port is bound to
# 127.0.0.1 only: 17010 cpu (drawterm), 564 9fs, 567 auth, as 1xxxx on the
# host; 8000 agentd (src/agentd.rc) as 18000; 8001 mkd (src/mkworld.rc -s) as 18001.
# The QEMU monitor is a Unix socket in run/; screens are dumped there.

set -eu

HERE=$(cd "$(dirname "$0")" && pwd)
DISK=$HERE/disk/9legacy.img
RUN=$HERE/run

[ -f "$DISK" ] || { printf 'FAIL: no disk; run sh build.sh first\n' >&2; exit 1; }
mkdir -p "$RUN"

# pack.sh's input tar and blank output become /dev/sd01 and /dev/sd02.
DISKS=
[ -f "$RUN/in.tar" ] && [ -f "$RUN/out.img" ] && DISKS="-device scsi-hd,drive=d1 -drive if=none,id=d1,file=$RUN/in.tar,format=raw,readonly=on -device scsi-hd,drive=d2 -drive if=none,id=d2,file=$RUN/out.img,format=raw"

KVM=
[ -w /dev/kvm ] && KVM='-machine accel=kvm'

# shellcheck disable=SC2086 # KVM and DISKS split into words
qemu-system-x86_64 $KVM -m 1024 -vga std -display none \
    -monitor "unix:$RUN/monitor,server,nowait" \
    -serial "file:$RUN/serial.log" \
    -nic "user,model=virtio,hostfwd=tcp:127.0.0.1:17010-:17010,hostfwd=tcp:127.0.0.1:10564-:564,hostfwd=tcp:127.0.0.1:10567-:567,hostfwd=tcp:127.0.0.1:18000-:8000,hostfwd=tcp:127.0.0.1:18001-:8001" \
    -device virtio-scsi-pci,id=scsi \
    -device scsi-hd,drive=d0 \
    -drive "if=none,id=d0,file=$DISK,format=raw" \
    $DISKS \
    -pidfile "$RUN/qemu.pid" -daemonize
printf 'PASS: booting; monitor at %s/monitor\n' "$RUN"
