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
    if not mem:
        sys.exit('%s: no code in it' % ihx)
    m = open(mapf).read()
    sym = {}
    for name in ('s__CODE', 'l__CODE', 's__DATA', 'l__DATA'):
        x = re.search(r'0000([0-9A-F]{4})\s+%s\b' % name, m)
        if not x:
            sys.exit('%s: no %s in the map' % (mapf, name))
        sym[name] = int(x.group(1), 16)
    lo, hi = sym['s__CODE'], sym['s__CODE'] + sym['l__CODE']   # the code area, from the map
    if lo != 0x4000 or min(mem) < lo or max(mem) >= hi:
        sys.exit('%s: code %04X-%04X, bytes at %04X-%04X: not a blob at #4000' % (ihx, lo, hi - 1, min(mem), max(mem)))
    ds, dl = sym['s__DATA'], sym['l__DATA']
    if ds < hi or ds + dl > 0x5200:
        sys.exit('%s: code %04X-%04X, data %04X-%04X: overlap or past #5200' % (ihx, lo, hi, ds, ds + dl))
    open(out, 'wb').write(bytes(mem.get(a, 0) for a in range(lo, hi)))
    print('%-11s code %04X-%04X, data %04X-%04X' % (out.split('/')[-1], lo, hi - 1, ds, ds + dl - 1))

main()
