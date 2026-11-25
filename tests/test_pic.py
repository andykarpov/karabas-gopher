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

    def gif(self, w, h, ncol, interlace):
        """A GIF made by Pillow, the expected screen from Pillow's own decoding of it."""
        from PIL import Image
        import io
        rnd = random.Random(w * 7 + h + ncol)
        im = Image.new('P', (w, h))
        im.putpalette([rnd.randrange(256) for _ in range(3 * ncol)])
        # blocks and noise: long LZW strings and short ones
        px = []
        for y in range(h):
            for x in range(w):
                px.append(((x // 13 + y // 7) % ncol) if rnd.random() < 0.8 else rnd.randrange(ncol))
        im.putdata(px)
        buf = io.BytesIO()
        im.save(buf, 'GIF', interlace=interlace, optimize=False)
        data = buf.getvalue()
        ref = Image.open(io.BytesIO(data))
        idx = list(ref.getdata())
        n = 2 << (data[10] & 7) if data[10] & 0x80 else 0
        rp = ref.getpalette()
        cram = [cw((rp[3 * i] * 3 + 16) >> 5, (rp[3 * i + 1] * 3 + 16) >> 5, (rp[3 * i + 2] * 3 + 16) >> 5)
                if i < n else 0 for i in range(256)]
        f = 1
        while -(-w // f) > 360 or -(-h // f) > 288: f += 1
        ow, oh = -(-w // f), -(-h // f)
        sw, sh = next((sw, sh) for sw, sh in RES if ow <= sw and oh <= sh)
        x0, y0 = (sw - ow) // 2, (sh - oh) // 2
        want = [[cram[255 if n < 256 else 0]] * sw for _ in range(sh)]
        for oy in range(oh):
            for ox in range(ow):
                want[y0 + oy][x0 + ox] = cram[idx[oy * f * w + ox * f]]
        self.check(self.run_pic(data, 5), want)

    def test_gif_small(self): self.gif(100, 50, 16, False)
    def test_gif_full_interlaced(self): self.gif(360, 288, 256, True)
    def test_gif_scaled(self): self.gif(800, 600, 64, True)
    def test_gif_wide(self): self.gif(1000, 300, 2, False)

    def png(self, mode, w, h, **save):
        """A PNG made by Pillow; the expected screen from Pillow's decoding of it: palette indices,
        greys, or the 6 x 7 x 6 colour cube with the 4 x 4 ordered dither of pic.c."""
        from PIL import Image
        import io
        rnd = random.Random(w * 3 + h + len(mode))
        if mode == 'P':
            ncol = save.pop('ncol')
            im = Image.new('P', (w, h))
            im.putpalette([rnd.randrange(256) for _ in range(3 * ncol)])
            im.putdata([((x // 9 + y // 5) % ncol) if rnd.random() < 0.7 else rnd.randrange(ncol)
                        for y in range(h) for x in range(w)])
        else:
            base = Image.new('RGB', (w, h))
            base.putdata([((x * 255) // w, (y * 255) // h, rnd.randrange(256) if rnd.random() < 0.2 else 128)
                          for y in range(h) for x in range(w)])
            im = base.convert(mode) if mode not in ('I;16',) else base.convert('L').point(lambda v: v * 257, 'I').convert('I;16')
        buf = io.BytesIO()
        im.save(buf, 'PNG', **save)
        data = buf.getvalue()
        ref = Image.open(io.BytesIO(data))
        ref.load()
        f = 1
        while -(-w // f) > 360 or -(-h // f) > 288: f += 1
        ow, oh = -(-w // f), -(-h // f)
        sw, sh = next((sw, sh) for sw, sh in RES if ow <= sw and oh <= sh)
        x0, y0 = (sw - ow) // 2, (sh - oh) // 2
        def c8(r, g, b): return cw((r * 3 + 16) >> 5, (g * 3 + 16) >> 5, (b * 3 + 16) >> 5)
        bayer = [0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5]
        fill = 0
        if mode == 'P':
            pl = ref.getpalette()
            np_ = len(pl) // 3
            cram = [c8(*pl[3 * i:3 * i + 3]) if i < np_ else 0 for i in range(256)]
            fill = 255 if np_ < 256 else 0
            px = lambda x, y: cram[ref.getpixel((x, y))]
        elif mode in ('L', 'LA', '1', 'I;16'):
            def px(x, y):
                v = ref.getpixel((x, y))
                v = v[0] if isinstance(v, tuple) else v
                v = 255 if (mode == '1' and v) else v >> 8 if mode == 'I;16' else v
                return c8(v, v, v)
        else:
            cube = [c8(r * 51, g * 255 // 6, b * 51) for r in range(6) for g in range(7) for b in range(6)]
            rgb = ref.convert('RGB')
            def px(x, y, ox=0, oy=0):
                r, g, b = rgb.getpixel((x, y))
                t = bayer[(oy & 3) * 4 + (ox & 3)] * 16 + 8
                return cube[((r * 5 + t) >> 8) * 42 + ((g * 6 + t) >> 8) * 6 + ((b * 5 + t) >> 8)]
        want = [[0] * sw for _ in range(sh)]
        fc = 0 if mode != 'P' else cram[fill]
        want = [[fc] * sw for _ in range(sh)]
        for oy in range(oh):
            for ox in range(ow):
                want[y0 + oy][x0 + ox] = px(ox * f, oy * f, ox, oy) if mode in ('RGB', 'RGBA') else px(ox * f, oy * f)
        self.check(self.run_pic(data, 6), want)

    def test_png_pal(self): self.png('P', 200, 100, ncol=200)
    def test_png_pal4(self): self.png('P', 101, 77, ncol=16, bits=4)
    def test_png_grey(self): self.png('L', 300, 200)
    def test_png_grey_alpha(self): self.png('LA', 120, 90)
    def test_png_grey16(self): self.png('I;16', 64, 48)
    def test_png_bits1(self): self.png('1', 333, 111)
    def test_png_rgb(self): self.png('RGB', 320, 240)
    def test_png_rgba_scaled(self): self.png('RGBA', 700, 500)
    def test_png_stored(self): self.png('RGB', 90, 60, compress_level=0)


if __name__ == '__main__':
    unittest.main()
