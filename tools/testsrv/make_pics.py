# Test pictures for the viewer: python3 tools/testsrv/make_pics.py OUTDIR tests/data/test.scr
import struct, math, sys, os
from PIL import Image, ImageDraw
out = sys.argv[1]; scr = open(sys.argv[2], 'rb').read()
open(os.path.join(out, 'one.scr'), 'wb').write(scr)
# gigascreen: the picture and the same with ink / paper swapped in the attributes -> mixed colours
attrs = bytes(((a & 7) << 3) | ((a >> 3) & 7) | (a & 0xC0) for a in scr[6144:6912])
open(os.path.join(out, 'two.img'), 'wb').write(scr + scr[:6144] + attrs)
# multicolour (lines in order, as WC MCGSV): stripes, every line its own colours
pix = bytearray(); att = bytearray()
for y in range(192):
    for x in range(32):
        pix.append(0xF0 if (x + y // 8) & 1 else 0x3C)
        att.append(((y // 4) % 7 + 1) | ((((y // 4) + 3) % 8) << 3) | (0x40 if y & 32 else 0))
open(os.path.join(out, 'three.mc'), 'wb').write(bytes(pix) + bytes(att))
# 8-bit BMP 320x240: a colour wheel with a title
im = Image.new('RGB', (320, 240))
d = ImageDraw.Draw(im)
for y in range(240):
    for x in range(320):
        a = math.atan2(y - 120, x - 160); r = math.hypot(x - 160, y - 120)
        im.putpixel((x, y), (int(127 + 127 * math.cos(a)), int(127 + 127 * math.cos(a - 2.1)),
                             int(min(255, r * 2))))
d.rectangle((60, 100, 260, 140), fill=(0, 0, 0)); d.text((80, 112), 'karabas gopher 256c', fill=(255, 255, 255))
im.convert('P', palette=Image.ADAPTIVE, colors=256).save(os.path.join(out, 'four.bmp'))
# 4-bit BMP 256x192: 16 vertical bars
w, h = 256, 192
pal = [(i * 17, 255 - i * 17, (i * 53) % 256) for i in range(16)]
rows = [bytes(((x // 16) << 4) | (x // 16) for x in range(0, w, 2)) for _ in range(h)]
body = b''.join(rows[::-1]); palb = b''.join(bytes((b, g, r, 0)) for r, g, b in pal)
off = 54 + len(palb)
hdr = b'BM' + struct.pack('<IHHI', off + len(body), 0, 0, off) + struct.pack('<IiiHHIIiiII', 40, w, h, 1, 4, 0, len(body), 0, 0, 16, 0)
open(os.path.join(out, 'five.bmp'), 'wb').write(hdr + palb + body)
