#!/usr/bin/env python3
"""Gera uma ISO9660 com El Torito (BIOS, sem emulacao) a partir de boot.bin.
Escrito de raiz: sem xorriso/genisoimage."""
import struct, sys, subprocess, datetime, os

SEC = 2048
boot = open(sys.argv[1], "rb").read()
out = sys.argv[2]
mbr_src = sys.argv[3] if len(sys.argv) > 3 else None

README = (b"AuroraOS 1.0\r\n"
          b"Sistema operativo feito de raiz: bootloader (assembly), kernel (C) e interface grafica.\r\n"
          b"Arranca em BIOS/Legacy. Em QEMU: qemu-system-i386 -cdrom aurora.iso\r\n")

def both32(v): return struct.pack("<I", v) + struct.pack(">I", v)
def both16(v): return struct.pack("<H", v) + struct.pack(">H", v)
def pad(b, n, ch=b" "): return b[:n].ljust(n, ch)
def sectors(n): return (n + SEC - 1) // SEC

boot_padded = boot + b"\0" * (-len(boot) % 512)
boot_padded += b"\0" * (-len(boot_padded) % SEC)
boot_sectors512 = len(boot_padded) // 512
assert boot_sectors512 < 0xFFFF

LBA_PVD, LBA_BR, LBA_TERM, LBA_CAT = 16, 17, 18, 19
LBA_PT_L, LBA_PT_M, LBA_ROOT, LBA_README, LBA_BOOT = 20, 21, 22, 23, 24
total = LBA_BOOT + len(boot_padded) // SEC

now = datetime.datetime.utcnow()
def dt17(): return now.strftime("%Y%m%d%H%M%S").encode() + b"00" + b"\x00"
def dt7(): return bytes([now.year - 1900, now.month, now.day, now.hour, now.minute, now.second, 0])

def dirrec(name, lba, size, isdir):
    nl = len(name)
    ln = 33 + nl + (1 if nl % 2 == 0 else 0)
    r = bytes([ln, 0]) + both32(lba) + both32(size) + dt7()
    r += bytes([2 if isdir else 0, 0, 0]) + both16(1) + bytes([nl]) + name
    if nl % 2 == 0: r += b"\0"
    assert len(r) == ln
    return r

root = (dirrec(b"\x00", LBA_ROOT, SEC, True) + dirrec(b"\x01", LBA_ROOT, SEC, True) +
        dirrec(b"BOOT.BIN;1", LBA_BOOT, len(boot), False) +
        dirrec(b"README.TXT;1", LBA_README, len(README), False))
root = root.ljust(SEC, b"\0")

# tabela de caminhos (so a raiz)
ptL = bytes([1, 0]) + struct.pack("<I", LBA_ROOT) + struct.pack("<H", 1) + b"\x00" + b"\x00"
ptM = bytes([1, 0]) + struct.pack(">I", LBA_ROOT) + struct.pack(">H", 1) + b"\x00" + b"\x00"

pvd = bytearray(SEC)
pvd[0] = 1; pvd[1:6] = b"CD001"; pvd[6] = 1
pvd[8:40] = pad(b"AURORAOS", 32)
pvd[40:72] = pad(b"AURORAOS", 32)
pvd[80:88] = both32(total)
pvd[120:124] = both16(1); pvd[124:128] = both16(1); pvd[128:132] = both16(SEC)
pvd[132:140] = both32(len(ptL))
pvd[140:144] = struct.pack("<I", LBA_PT_L)
pvd[148:152] = struct.pack(">I", LBA_PT_M)
pvd[156:190] = dirrec(b"\x00", LBA_ROOT, SEC, True)
pvd[190:318] = pad(b"", 128); pvd[318:446] = pad(b"", 128)
pvd[446:574] = pad(b"", 128); pvd[574:702] = pad(b"AURORAOS MKISO.PY", 128)
pvd[702:739] = pad(b"", 37); pvd[739:776] = pad(b"", 37); pvd[776:813] = pad(b"", 37)
pvd[813:830] = dt17(); pvd[830:847] = dt17()
pvd[847:864] = b"0000000000000000\x00"; pvd[864:881] = b"0000000000000000\x00"
pvd[881] = 1

br = bytearray(SEC)
br[0] = 0; br[1:6] = b"CD001"; br[6] = 1
br[7:7 + 23] = b"EL TORITO SPECIFICATION"
br[71:75] = struct.pack("<I", LBA_CAT)

term = bytearray(SEC); term[0] = 255; term[1:6] = b"CD001"; term[6] = 1

cat = bytearray(SEC)
cat[0] = 1; cat[1] = 0                      # validation entry, plataforma x86
cat[4:4 + 8] = b"AURORAOS"
cat[30] = 0x55; cat[31] = 0xAA
s = sum(struct.unpack("<16H", bytes(cat[:32]))) & 0xFFFF
cat[28:30] = struct.pack("<H", (-s) & 0xFFFF)
assert sum(struct.unpack("<16H", bytes(cat[:32]))) & 0xFFFF == 0
cat[32] = 0x88                              # arrancavel
cat[33] = 0                                 # sem emulacao
cat[34:36] = struct.pack("<H", 0)           # segmento 0 => 0x07C0
cat[36] = 0
cat[38:40] = struct.pack("<H", boot_sectors512)
cat[40:44] = struct.pack("<I", LBA_BOOT)

img = bytearray(total * SEC)
def put(lba, data): img[lba * SEC: lba * SEC + len(data)] = data
put(LBA_PVD, pvd); put(LBA_BR, br); put(LBA_TERM, term); put(LBA_CAT, cat)
put(LBA_PT_L, ptL); put(LBA_PT_M, ptM); put(LBA_ROOT, root)
put(LBA_README, README); put(LBA_BOOT, boot_padded)

# ---- MBR opcional (arranque a partir de USB) ----
if mbr_src:
    d = os.path.dirname(os.path.abspath(out))
    tmp = "/tmp/mbr_build"; os.makedirs(tmp, exist_ok=True)
    subprocess.check_call(["gcc", "-m32", "-c", mbr_src, "-o", tmp + "/mbr.o",
                           "-Wa,--defsym,TOTAL=%d" % boot_sectors512,
                           "-Wa,--defsym,LBA=%d" % (LBA_BOOT * 4)])
    subprocess.check_call(["ld", "-m", "elf_i386", "-Ttext", "0x600", "-o", tmp + "/mbr.elf", tmp + "/mbr.o"])
    subprocess.check_call(["objcopy", "-O", "binary", tmp + "/mbr.elf", tmp + "/mbr.bin"])
    m = open(tmp + "/mbr.bin", "rb").read()
    assert len(m) <= 440, len(m)
    sec = bytearray(512); sec[:len(m)] = m
    part = bytes([0x80, 0x00, 0x01, 0x00, 0x17, 0xFE, 0xFF, 0xFF]) + struct.pack("<II", 0, total * 4)
    sec[446:462] = part
    sec[510] = 0x55; sec[511] = 0xAA
    img[0:512] = sec

open(out, "wb").write(img)
print("ISO: %s  (%d sectores de 2 KiB, imagem de arranque: %d bytes = %d sectores de 512)" %
      (out, total, len(boot), boot_sectors512))
