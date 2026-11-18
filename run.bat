@echo off
rem Arranca o AuroraOS no QEMU com memoria interna persistente (memoria.aum).
cd /d "%~dp0"
if not exist memoria.aum python tools\aum.py new memoria.aum 8
qemu-system-i386 -m 64 -boot d -cdrom aurora.iso -drive file=memoria.aum,format=raw,if=ide,index=1 %*
