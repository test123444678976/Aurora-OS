#!/bin/sh
# Constroi aurora.iso: so precisa de gcc, as, ld, objcopy e python3.
set -e
cd "$(dirname "$0")"
mkdir -p build
CFLAGS="-m32 -O2 -Wall -Wno-misleading-indentation -ffreestanding -fno-pic -fno-pie -fno-stack-protector \
 -fno-builtin -fno-tree-loop-distribute-patterns -fno-asynchronous-unwind-tables -fcf-protection=none \
 -mno-sse -mno-mmx -mno-sse2 -fexec-charset=ISO-8859-1"
[ -f src/font.h ] || python3 tools/genfont.py src/font.h
for f in kernel gfx gui apps fs aum users login libc; do gcc $CFLAGS -c src/$f.c -o build/$f.o; done
gcc -m32 -c src/boot.S -o build/boot.o
ld -m elf_i386 -nostdlib -T src/link.ld -o build/kernel.elf build/boot.o build/kernel.o build/gfx.o build/gui.o build/apps.o build/fs.o build/aum.o build/users.o build/login.o build/libc.o
objcopy -O binary build/kernel.elf build/boot.bin
python3 tools/mkiso.py build/boot.bin aurora.iso src/mbr.S
