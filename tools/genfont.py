#!/usr/bin/env python3
# Gera font8x16 (Latin-1) a partir de DejaVu Sans Mono -> src/font.h
import sys
from PIL import Image, ImageDraw, ImageFont
path = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"
f = ImageFont.truetype(path, 13)
rows = []
prev = Image.new("L", (16*10, 16*16*0+16*16), 0)
for code in range(256):
    img = Image.new("L", (8, 16), 0)
    if code >= 32 and not (0x7f <= code < 0xa0):
        d = ImageDraw.Draw(img)
        d.text((0, 0), chr(code), font=f, fill=255, anchor="ls") if False else None
        # baseline em y=12
        bbox = f.getbbox(chr(code), anchor="ls")
        d.text((0, 12), chr(code), font=f, fill=255, anchor="ls")
    px = img.load()
    g = []
    for y in range(16):
        b = 0
        for x in range(8):
            if px[x, y] >= 110:
                b |= 0x80 >> x
        g.append(b)
    rows.append(g)
with open(sys.argv[1] if len(sys.argv) > 1 else "src/font.h", "w") as o:
    o.write("/* gerado por tools/genfont.py */\n#include <stdint.h>\nstatic const uint8_t font8x16[256][16] = {\n")
    for code, g in enumerate(rows):
        o.write("  {" + ",".join("0x%02x" % b for b in g) + "}, /* %d */\n" % code)
    o.write("};\n")
# preview
prev = Image.new("RGB", (8*64*2, 16*4*2), (0,0,0))
pp = prev.load()
for code in range(32, 160+32*3):
    if code >= 256: break
    i = code - 32
    cx, cy = (i % 64) * 8, (i // 64) * 16
    for y in range(16):
        for x in range(8):
            if rows[code][y] & (0x80 >> x):
                pp[cx+x, cy+y] = (255,255,255)
prev = prev.crop((0,0,8*64,16*4)).resize((8*64*2,16*4*2), Image.NEAREST)
prev.save("/tmp/fontprev.png")
