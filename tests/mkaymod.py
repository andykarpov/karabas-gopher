#!/usr/bin/env python3
"""Synthetic STC / ASC modules for the player tests (tests/data/ay): random but well-formed, every
pattern command used. mkaymod.py stc|asc SEED OUT. The reference register dumps of these modules
(*.regs) were made once by an independent player (AYFly, outside this repository)."""
import random, struct, sys

ROWS = 32


def channel(r, event, skip_base):
    """Pattern bytes of one channel taking exactly ROWS rows: events (each ends with a row) with
    skip commands (skip_base + n: n more empty rows after each event)."""
    out, left, skip = [], ROWS, None
    while left > 0:
        s = r.randint(0, min(3, left - 1))
        if s != skip:
            out.append(skip_base + s)
            skip = s
        out += event(r)
        left -= s + 1
    return out + [0xFF]


def stc(seed):
    r = random.Random(seed)
    nsmp, norn, npat, npos = 5, 4, 4, 6
    smp = b''
    for k in range(nsmp):
        d = bytearray()
        for i in range(32):
            b0 = r.randint(0, 15) | (r.randint(0, 15) << 4 if r.random() < 0.3 else 0)
            b1 = r.randint(0, 31) | (0x80 if r.random() < 0.5 else 0) | (0x40 if r.random() < 0.2 else 0) \
                | (0x20 if r.random() < 0.5 else 0)
            d += bytes([b0, b1, r.randint(0, 255) if r.random() < 0.3 else 0])
        loop = r.choice([0, r.randint(1, 31)])
        smp += bytes([k]) + d + bytes([loop, r.randint(0, 31) if loop else 0])
    # ornament 0 is all zeros (the players and trackers expect it first)
    orn = b''.join(bytes([k]) + bytes(r.choice([0, 0, 12, 24, 7, 3, 244]) if k else 0 for _ in range(32))
                   for k in range(norn))

    def event(r):
        c = []
        if r.random() < 0.2: c.append(0x60 + r.randint(0, nsmp - 1))
        x = r.random()
        if x < 0.1: c.append(0x70 + r.randint(0, norn - 1))
        elif x < 0.15: c.append(0x82)
        elif x < 0.25: c += [0x80 + r.randint(3, 14), r.randint(0, 255)]
        y = r.random()
        c.append(0x80 if y < 0.1 else 0x81 if y < 0.2 else r.randint(0, 0x5F))
        return c

    pats = [[[0x60 + r.randint(0, nsmp - 1)] + channel(r, event, 0xA1) for _ in range(3)]
            for _ in range(npat)]
    smp_at = 0x1B
    pos_at = smp_at + len(smp)
    positions = bytes([npos - 1]) + b''.join(bytes([r.randint(1, npat), r.choice([0, 0, 2, 254])])
                                             for _ in range(npos))
    orn_at = pos_at + len(positions)
    pat_at = orn_at + len(orn)
    data_at = pat_at + 7 * npat + 1
    table, data = b'', b''
    for k, chans in enumerate(pats):
        offs = []
        for c in chans:
            offs.append(data_at + len(data))
            data += bytes(c)
        table += struct.pack('<BHHH', k + 1, *offs)
    body = smp + positions + orn + table + b'\xff' + data
    hdr = struct.pack('<BHHH', r.randint(2, 6), pos_at, orn_at, pat_at) + b'KARABAS TEST STC  '
    return hdr + struct.pack('<H', 27 + len(body)) + body


def asc(seed):
    r = random.Random(seed)
    nsmp, norn, npat, npos = 6, 4, 4, 6

    def sample():
        n = r.randint(2, 12)
        loop = r.randint(0, n - 1)
        d = bytearray()
        for i in range(n):
            s0 = r.randint(0, 31) if r.random() < 0.5 else 0
            if i == loop: s0 |= 0x80
            if i == n - 1: s0 |= 0x40 | (0x20 if r.random() < 0.3 else 0)
            s2 = (r.randint(0, 15) << 4 | r.choice([0, 1, 8, 9]) | r.choice([0, 2, 2, 4, 6])) if r.random() < 0.9 else 0
            d += bytes([s0, r.choice([0, 0, 1, 255, 2, 254]), s2])
        return bytes(d)

    def ornament():
        n = r.randint(1, 6)
        loop = r.randint(0, n - 1)
        d = bytearray()
        for i in range(n):
            o0 = r.randint(0, 31) if r.random() < 0.3 else 0
            if i == loop: o0 |= 0x80
            if i == n - 1: o0 |= 0x40
            d += bytes([o0, r.choice([0, 0, 12, 7, 244, 1])])
        return bytes(d)

    smps = [sample() for _ in range(nsmp)]
    orns = [ornament() for _ in range(norn)]

    def events(env):
        def note(r):
            return [r.randint(0, 0x55)] + ([r.randint(0, 255)] if env else [])

        def event(r):
            c = []
            for _ in range(r.randint(0, 3)):
                x = r.random()
                if x < 0.12: c.append(0xA0 + r.randint(0, nsmp - 1))
                elif x < 0.2: c.append(0xC0 + r.randint(0, norn - 1))
                elif x < 0.28 and not env: c.append(0xE1 + r.randint(0, 14))
                elif x < 0.34: c += [0xF0, r.randint(0, 31)]
                elif x < 0.42: c.append(r.choice([0xF1, 0xF2, 0xF3, 0xF8, 0xFA, 0xFC, 0xFE]))
                elif x < 0.45: c += [0xF4, r.randint(2, 5)]
                elif x < 0.55: c += [r.choice([0xF5, 0xF6]), r.choice([1, 2, 5, 250, 255])]
                elif x < 0.65: c += [0xFB, r.randint(0, 63)]
                elif x < 0.75:        # a glide (to the note that follows) and that note
                    return c + [r.choice([0xF7, 0xF9]), r.choice([1, 2, 3, 4, 8, 250])] + note(r)
            y = r.random()
            if y < 0.08: return c + [r.randint(0x56, 0x5D)]
            if y < 0.12: return c + [0x5E]
            if y < 0.15: return c + [0x5F]
            return c + note(r)
        return event

    pats = []
    for _ in range(npat):
        chans = []
        for ch in range(3):
            env = ch == 2                         # channel C: always the envelope (a byte after notes)
            head = [0xA0 + r.randint(0, nsmp - 1), 0xC0 + r.randint(0, norn - 1),
                    0xE0 if env else 0xE1 + r.randint(0, 14)]
            chans.append(head + channel(r, events(env), 0x60))
        pats.append(chans)
    positions = list(range(npat)) + [r.randint(0, npat - 1) for _ in range(npos - npat)]
    r.shuffle(positions)
    positions = bytes(positions)              # every pattern used (the table starts at 6 * npat)
    pat_at = 9 + npos
    pdata, offs = b'', []
    for chans in pats:
        o = []
        for c in chans:
            o.append(6 * npat + len(pdata))
            pdata += bytes(c)
        offs.append(o)
    pat_blob = b''.join(struct.pack('<HHH', *o) for o in offs) + pdata

    def table(items):
        data, off = b'', []
        for it in items:
            off.append(64 + len(data))
            data += it
        return b''.join(struct.pack('<H', off[i % len(items)]) for i in range(32)) + data

    smp_blob, orn_blob = table(smps), table(orns)
    smp_at = pat_at + len(pat_blob)
    orn_at = smp_at + len(smp_blob)
    head = struct.pack('<BBHHHB', r.randint(2, 5), r.randint(0, npos - 1), pat_at, smp_at, orn_at, npos)
    return head + positions + pat_blob + smp_blob + orn_blob


if __name__ == '__main__':
    kind, seed, out = sys.argv[1], int(sys.argv[2]), sys.argv[3]
    open(out, 'wb').write((stc if kind == 'stc' else asc)(seed))
