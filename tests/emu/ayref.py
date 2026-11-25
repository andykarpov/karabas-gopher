#!/usr/bin/env python3
"""For tests/emu/aybench.c: ayref.py REGS -> "FRAMES HASH" (the frames before the loop of the song:
the dump holds 2 x its length + 50; the hash as aybench.c computes it)."""
import sys
lines = open(sys.argv[1]).read().split('\n')[:-1]
n = (len(lines) - 50) // 2
h = 0
for l in lines[:n]:
    for v in l.split()[:14]:
        h = (h * 31 + int(v, 16)) & 0xFFFF
    h = (h * 31 + int(l.split()[14])) & 0xFFFF
print(n, h)
