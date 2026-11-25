#!/usr/bin/env python3
"""A player blob from its linked .ihx: ihx2blob.py IN.ihx IN.map OUT.bin -> the bytes from #4000 to the
end of the code. Checks the map: the variables (_DATA) after the code and below #5200 (the module)."""
import re, sys

def main():
    ihx, mapf, out = sys.argv[1:4]
    mem = {}
    for line in open(ihx):
        line = line.strip()
        if not line.startswith(':'):
            continue
        b = bytes.fromhex(line[1:])
        if b[3] == 0:
            a = b[1] << 8 | b[2]
            for i, v in enumerate(b[4:4 + b[0]]):
                mem[a + i] = v
    lo, hi = min(mem), max(mem) + 1
    if lo != 0x4000:
        sys.exit('%s: code starts at %04X, not 4000' % (ihx, lo))
    m = open(mapf).read()
    s = re.search(r'0000([0-9A-F]{4})\s+s__DATA', m)
    l = re.search(r'0000([0-9A-F]{4})\s+l__DATA', m)
    ds, dl = int(s.group(1), 16), int(l.group(1), 16)
    if ds < hi or ds + dl > 0x5200:
        sys.exit('%s: code %04X-%04X, data %04X-%04X: overlap or past #5200' % (ihx, lo, hi, ds, ds + dl))
    open(out, 'wb').write(bytes(mem.get(a, 0) for a in range(lo, hi)))
    print('%-11s code %04X-%04X, data %04X-%04X' % (out.split('/')[-1], lo, hi - 1, ds, ds + dl - 1))

main()
