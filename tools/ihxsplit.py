#!/usr/bin/env python3
"""Split the linked .ihx into page images for mkspg.py and check the memory map (src/bank.h).

  ihxsplit.py IN.ihx IN.map OUTDIR   ->  OUTDIR/page20.bin, page30.bin, page31.bin, ...

Address 0x0000-0x1FFF = common code (page #20); bank n is linked at n * 0x10000 + 0x4000 .. 0xBFFF
(-Wl-b_BANKn=0xn4000) and goes to pages #30 + 2n (#4000-#7FFF) and #31 + 2n (#8000-#BFFF).
Prints the sizes; exits 1 if a part overflows (common code into the data, data into the stack room)."""
import os, re, sys

COMMON_END = 0x2000
DATA_END = 0x3C00          # 1 KB of stack below #4000
STACK_TOP = 0x4000


def read_ihx(path):
    mem, base = {}, 0
    for line in open(path):
        line = line.strip()
        if not line.startswith(':'):
            continue
        b = bytes.fromhex(line[1:])
        n, addr, typ, data = b[0], (b[1] << 8) | b[2], b[3], b[4:4 + b[0]]
        if typ == 0:
            for i, v in enumerate(data):
                mem[base + addr + i] = v
        elif typ == 2:
            base = ((data[0] << 8) | data[1]) << 4
        elif typ == 4:
            base = ((data[0] << 8) | data[1]) << 16
        elif typ == 1:
            break
    return mem


def area(mapfile, name):
    for line in open(mapfile):
        m = re.match(r'^%s\s+([0-9A-F]{8})\s+([0-9A-F]{8})' % re.escape(name), line)
        if m:
            return int(m.group(1), 16), int(m.group(2), 16)
    return None


def main():
    ihx, mapfile, out = sys.argv[1:4]
    mem = read_ihx(ihx)
    ok = True
    pages = {}
    common = [a for a in mem if a < 0x10000]
    hi = max(common) + 1 if common else 0
    print('common code  #%04X of #%04X' % (hi, COMMON_END))
    if hi > COMMON_END:
        print('  OVERFLOW into the data'); ok = False
    pages[0x20] = bytes(mem.get(a, 0) for a in range(0, min(hi, COMMON_END)))
    d = area(mapfile, '_DATA'); i = area(mapfile, '_INITIALIZED')
    dend = max((d[0] + d[1]) if d else 0, (i[0] + i[1]) if i else 0)
    print('data         #2000-#%04X, stack room %d bytes' % (dend, STACK_TOP - dend))
    if dend > DATA_END:
        print('  OVERFLOW into the stack room'); ok = False
    banks = sorted({a >> 16 for a in mem if a >= 0x10000})
    for n in banks:
        addrs = [a & 0xFFFF for a in mem if a >> 16 == n]
        lo, hi = min(addrs), max(addrs) + 1
        print('bank %d       #%04X-#%04X (%d of 32768 bytes)' % (n, lo, hi, hi - 0x4000))
        if lo < 0x4000 or hi > 0xC000:
            print('  OUT of #4000-#BFFF'); ok = False
            continue
        for k, w in enumerate((0x4000, 0x8000)):
            if hi > w:
                pages[0x30 + 2 * n + k] = bytes(mem.get((n << 16) + a, 0) for a in range(w, min(hi, w + 0x4000)))
    os.makedirs(out, exist_ok=True)
    for f in os.listdir(out):
        if re.match(r'page[0-9a-f]{2}\.bin$', f):
            os.remove(os.path.join(out, f))
    for pg, blob in pages.items():
        open(os.path.join(out, 'page%02x.bin' % pg), 'wb').write(blob)
    sys.exit(0 if ok else 1)


if __name__ == '__main__':
    main()
