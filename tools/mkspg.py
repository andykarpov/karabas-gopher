#!/usr/bin/env python3
"""Build a TS-Conf SPG v1.0 file (zx-evo pentevo/docs/Formats/SPGv1_0.txt), blocks uncompressed.

  mkspg.py OUT.spg --run 0xC000 --sp 0xBFFE --page 0x20 --desc "Gopher" \
           --block 0x20:IMG0.bin [--block 0x21:IMG1.bin ...]

Each --block PAGE:FILE loads FILE at #C000 of PAGE; a file over 16 KB is an error (one page). The
header is 1024 bytes, the blocks follow in 512-byte units; block descriptors at #100, 3 bytes each:
address in page / 512 (bit 7 = last block), size / 512 - 1 (bits 6..7 = compression, 0 = none), page.
"""
import argparse, datetime, struct, sys


def build(blocks, run, sp, page, desc='', clock=2, int_en=False, resident=0x5B00, creator='karabas mkspg'):
    if not blocks:
        raise ValueError('no blocks')
    if not 0x4000 <= run <= 0xFFFF:
        raise ValueError('run address must be in #4000..#FFFF')
    h = bytearray(1024)
    h[0:32] = desc.encode('ascii')[:32].ljust(32, b'\0')
    h[0x20:0x2C] = b'SpectrumProg'
    h[0x2C] = 0x10                      # version 1, sub-version 0
    now = datetime.datetime.now()
    h[0x2D], h[0x2E], h[0x2F] = now.day, now.month, now.year - 2000
    struct.pack_into('<HHB', h, 0x30, run, sp, page)
    h[0x35] = (clock & 3) | (4 if int_en else 0)
    struct.pack_into('<HHH', h, 0x36, 0, resident, len(blocks))
    h[0x3C], h[0x3D], h[0x3E] = now.second, now.minute, now.hour
    h[0x50:0x70] = creator.encode('ascii')[:32].ljust(32, b'\0')
    data = bytearray()
    for k, (pg, blob) in enumerate(blocks):
        if not 0 <= pg <= 0xDF:
            raise ValueError('page %02X not allowed (#00..#DF)' % pg)
        if not blob:
            raise ValueError('block %d is empty' % k)
        if len(blob) > 0x4000:
            raise ValueError('block %d is %d bytes, over one page' % (k, len(blob)))
        n = (len(blob) + 511) // 512
        last = 0x80 if k == len(blocks) - 1 else 0
        h[0x100 + 3 * k: 0x103 + 3 * k] = bytes([0x00 | last, n - 1, pg])
        data += blob + bytes(n * 512 - len(blob))
    return bytes(h + data)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('out')
    ap.add_argument('--run', type=lambda s: int(s, 0), default=0xC000)
    ap.add_argument('--sp', type=lambda s: int(s, 0), default=0xBFFE)
    ap.add_argument('--page', type=lambda s: int(s, 0), required=True, help='page at #C000 on start')
    ap.add_argument('--desc', default='')
    ap.add_argument('--block', action='append', default=[], help='PAGE:FILE')
    a = ap.parse_args()
    blocks = []
    for b in a.block:
        pg, fn = b.split(':', 1)
        blocks.append((int(pg, 0), open(fn, 'rb').read()))
    try:
        out = build(blocks, a.run, a.sp, a.page, a.desc)
    except ValueError as e:
        sys.exit('mkspg: %s' % e)
    open(a.out, 'wb').write(out)


if __name__ == '__main__':
    main()
