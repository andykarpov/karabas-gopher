#!/usr/bin/env python3
"""Packs the AY player blobs (each assembled at #4000, up to #1200 bytes) into one page for
PG_PLAYERS: a table of (offset, length) words, one per blob in the order given, then the blobs.
player.c copies blob n into page PG_MUSIC (#54) before a track of its format starts."""
import struct, sys

def main():
    out, blobs = sys.argv[1], sys.argv[2:]
    data = [open(b, 'rb').read() for b in blobs]
    off = 4 * len(data)
    table, body = b'', b''
    for b, d in zip(blobs, data):
        if len(d) > 0x1200:
            sys.exit('%s: %d bytes, more than #1200 (the module starts at #5200)' % (b, len(d)))
        table += struct.pack('<HH', off + len(body), len(d))
        body += d
    page = table + body
    if len(page) > 0x4000:
        sys.exit('players: %d bytes, more than a page' % len(page))
    open(out, 'wb').write(page)
    print('players     %d blobs, %d of 16384 bytes' % (len(data), len(page)))

main()
