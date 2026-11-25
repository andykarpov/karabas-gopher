#!/usr/bin/env python3
"""Convert an 8x8 PSF console font (psf1 / psf2, .gz ok, with a unicode table) to the 2048-byte
CP866 font of the TS-Conf text mode: 256 glyphs x 8 rows, MSB = left pixel.

  mkfont.py IN.psf[.gz] OUT.bin

A CP866 code with no glyph in the font gets '?' (0x00..0x1F stay empty)."""
import gzip, struct, sys


def load_psf(path):
    raw = open(path, 'rb').read()
    if raw[:2] == b'\x1f\x8b':
        raw = gzip.decompress(raw)
    if raw[:2] == b'\x36\x04':                       # psf1
        mode, h = raw[2], raw[3]
        n = 512 if mode & 1 else 256
        w, hdr, size, has_uni = 8, 4, h, bool(mode & 2)
    elif raw[:4] == b'\x72\xb5\x4a\x86':             # psf2
        _, hdr, flags, n, size, h, w = struct.unpack_from('<7I', raw, 4)
        has_uni = bool(flags & 1)
    else:
        raise ValueError('not a PSF font')
    if w != 8 or h != 8:
        raise ValueError('font is %dx%d, need 8x8' % (w, h))
    glyphs = [raw[hdr + i * size: hdr + (i + 1) * size] for i in range(n)]
    umap = {}
    if has_uni:
        p, g = hdr + n * size, 0
        if raw[:2] == b'\x36\x04':
            while g < n and p + 1 < len(raw):
                u = struct.unpack_from('<H', raw, p)[0]; p += 2
                if u == 0xFFFF: g += 1
                elif u != 0xFFFE: umap.setdefault(u, g)
        else:
            text = raw[p:]
            for entry in text.split(b'\xff')[:n]:
                for seq in entry.split(b'\xfe')[:1]:
                    for ch in seq.decode('utf-8', 'ignore'):
                        umap.setdefault(ord(ch), g)
                g += 1
    else:
        umap = {i: i for i in range(n)}
    return glyphs, umap


def main():
    glyphs, umap = load_psf(sys.argv[1])
    out = bytearray(2048)
    q = umap.get(ord('?'))
    missing = []
    # CP437 glyphs of the control codes the UI uses: arrows and triangles
    ctl = {0x10: 0x25BA, 0x11: 0x25C4, 0x18: 0x2191, 0x19: 0x2193, 0x1A: 0x2192, 0x1B: 0x2190,
           0x1E: 0x25B2, 0x1F: 0x25BC}
    for c, u in ctl.items():
        if u in umap:
            out[c * 8: c * 8 + 8] = glyphs[umap[u]][:8]
        else:
            missing.append(c)
    for c in range(32, 256):
        u = ord(bytes([c]).decode('cp866'))
        g = umap.get(u, q)
        if umap.get(u) is None:
            missing.append(c)
        out[c * 8: c * 8 + 8] = glyphs[g][:8]
    open(sys.argv[2], 'wb').write(out)
    if missing:
        print('mkfont: no glyph for CP866 %s' % ' '.join('%02X' % c for c in missing), file=sys.stderr)


if __name__ == '__main__':
    main()
