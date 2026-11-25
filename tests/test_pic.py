"""src/pic.c on the host (build/pic_host): each format decoded and rendered from the picture buffer,
compared pixel by pixel (CRAM words) with an independent decoding in Python."""
import os, random, struct, subprocess, tempfile, unittest
HERE = os.path.dirname(__file__)
HOST = os.path.join(HERE, '..', 'build', 'pic_host')
SCR_REF = os.path.join(HERE, 'data', 'test.scr')
RES = [(256, 192), (320, 200), (320, 240), (360, 288)]


def cw(r, g, b):
    return (r << 10) | (g << 5) | b


def zx(c):
    l = 24 if c & 8 else 16
    return cw(l if c & 2 else 0, l if c & 4 else 0, l if c & 1 else 0)


def zx_line(y):
    return ((y & 0xC0) << 5) | ((y & 7) << 8) | ((y & 0x38) << 2)


def ink(a): return (a & 7) | ((a >> 3) & 8)
def paper(a): return ((a >> 3) & 7) | ((a >> 3) & 8)


def scr_pixels(d, off=0):
    out = []
    for y in range(192):
        row = []
        for x in range(256):
            b = d[off + zx_line(y) + x // 8]
            a = d[off + 6144 + (y // 8) * 32 + x // 8]
            row.append(ink(a) if b & (0x80 >> (x & 7)) else paper(a))
        out.append(row)
    return out


def render(path):
    d = open(path, 'rb').read()
    vconf = d[0]
    pal = struct.unpack('<256H', d[1:513])
    m = d[513:]
    vm, (w, h) = vconf & 3, RES[vconf >> 6]
    if vm == 0:
        return [[pal[c] for c in row] for row in scr_pixels(m)]
    if vm == 1:
        return [[pal[(m[y * 256 + x // 2] >> (0 if x & 1 else 4)) & 15] for x in range(w)] for y in range(h)]
    return [[pal[m[y * 512 + x]] for x in range(w)] for y in range(h)]


class TestPic(unittest.TestCase):
    def run_pic(self, data, fmt):
        with tempfile.TemporaryDirectory() as t:
            src, out = os.path.join(t, 'in'), os.path.join(t, 'out')
            open(src, 'wb').write(data)
            p = subprocess.run([HOST, src, str(fmt), out], capture_output=True, text=True)
            self.assertEqual(p.returncode, 0, p.stdout)
            return render(out)

    def check(self, got, want):
        self.assertEqual(len(got), len(want))
        bad = sum(1 for y in range(len(want)) for x in range(len(want[0])) if got[y][x] != want[y][x])
        self.assertEqual(bad, 0)

    def test_scr(self):
        d = open(SCR_REF, 'rb').read()
        self.check(self.run_pic(d, 1), [[zx(c) for c in r] for r in scr_pixels(d)])

    def test_scr_no_attrs(self):
        d = open(SCR_REF, 'rb').read()[:6144]
        want = [[zx(ink(0x38) if d[zx_line(y) + x // 8] & (0x80 >> (x & 7)) else paper(0x38))
                 for x in range(256)] for y in range(192)]
        self.check(self.run_pic(d, 1), want)

    def test_mc(self):
        rnd = random.Random(1)
        d = bytes(rnd.randrange(256) for _ in range(12288))
        want = [[zx(ink(d[6144 + y * 32 + x // 8]) if d[y * 32 + x // 8] & (0x80 >> (x & 7))
                    else paper(d[6144 + y * 32 + x // 8])) for x in range(256)] for y in range(192)]
        self.check(self.run_pic(d, 2), want)

    def test_img(self):
        d = open(SCR_REF, 'rb').read()
        rnd = random.Random(2)
        d2 = bytes(rnd.randrange(256) for _ in range(6912))
        a, b = scr_pixels(d), scr_pixels(d2)
        def mix(c1, c2):
            u, v = zx(c1), zx(c2)
            return cw(((u >> 10 & 31) + (v >> 10 & 31)) // 2, ((u >> 5 & 31) + (v >> 5 & 31)) // 2,
                      ((u & 31) + (v & 31)) // 2)
        self.check(self.run_pic(d + d2, 3), [[mix(a[y][x], b[y][x]) for x in range(256)] for y in range(192)])

    def bmp(self, w, h, bpp, top_down=False):
        from PIL import Image
        rnd = random.Random(w * h + bpp)
        n = 1 << bpp
        pal = [(rnd.randrange(256), rnd.randrange(256), rnd.randrange(256)) for _ in range(n)]
        pix = [[rnd.randrange(n) for _ in range(w)] for _ in range(h)]
        stride = (w * bpp + 31) // 32 * 4
        rows = []
        for row in pix:
            if bpp == 8:
                r = bytes(row)
            else:
                r = bytes((row[i] << 4) | (row[i + 1] if i + 1 < w else 0) for i in range(0, w, 2))
            rows.append(r + bytes(stride - len(r)))
        body = b''.join(rows if top_down else rows[::-1])
        palb = b''.join(bytes((b, g, r, 0)) for r, g, b in pal)
        off = 14 + 40 + len(palb)
        hdr = b'BM' + struct.pack('<IHHI', off + len(body), 0, 0, off)
        hdr += struct.pack('<IiiHHIIiiII', 40, w, -h if top_down else h, 1, bpp, 0, len(body), 0, 0, n, 0)
        data = hdr + palb + body
        res = next(i for i, (sw, sh) in enumerate(RES) if w <= sw and h <= sh)
        sw, sh = RES[res]
        x0, y0 = (sw - w) // 2 & ~1, (sh - h) // 2
        cram = [cw((r * 3 + 16) >> 5, (g * 3 + 16) >> 5, (b * 3 + 16) >> 5) for r, g, b in pal]
        want = [[cram[0]] * sw for _ in range(sh)]
        for y in range(h):
            for x in range(w):
                want[y0 + y][x0 + x] = cram[pix[y][x]]
        self.check(self.run_pic(data, 4), want)

    def test_bmp8_small(self): self.bmp(100, 50, 8)
    def test_bmp8_full(self): self.bmp(360, 288, 8)
    def test_bmp4_odd(self): self.bmp(301, 199, 4, top_down=True)
    def test_bmp4_320(self): self.bmp(320, 240, 4)


if __name__ == '__main__':
    unittest.main()
