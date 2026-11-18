#!/bin/sh
# Arranca o AuroraOS no QEMU com memoria interna persistente (memoria.aum).
cd "$(dirname "$0")"
[ -f aurora.iso ] || ./build.sh
[ -f memoria.aum ] || python3 tools/aum.py new memoria.aum 8
exec qemu-system-i386 -m 64 -boot d -cdrom aurora.iso \
    -drive file=memoria.aum,format=raw,if=ide,index=1 "$@"
