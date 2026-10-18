#!/usr/bin/env python3
"""EA's packers, as the game's unpack 0xa7020 picks them by the first two bytes:
10FB RefPack (0xaf2c8, in fsh.py), 30FB / 32FB / 34FB Huffman (0xaf440), 46FB (0xb0674),
4AFB (0xb0890, not on the CD).

    python3 -I tools/unpack.py <packed file> <out>

Checked against the game's own code run in an emulator (tools/unpack_emu.py).

The Huffman packing (0xaf440), a stream of bits read from the top of each byte:
  16 bits the kind (bit 8 set: 24 bits more to skip), 24 bits the unpacked size, 8 bits the
  escape byte.
  A "number" (read inline five times): 1 x x = 4..7, or z zeros, a one and z + 2 bits b =
  2^(z+2) + b (so 01xxx = 8..15, 001xxxx = 16..31 ..).
  The code: for length 1, 2, ..: a number - 4 = the codes of that length (canonical: the
  codes of a length follow the previous length's, doubled), until the code space is full.
  Then the symbols, in code order: each a number - 3 = how many still-unused byte values to
  step forward from the previous symbol (starting before 0, wrapping at 256).
  The data: codes; a symbol other than the escape is the next byte. The escape is followed
  by a number - 4 = n: n > 0 repeats the last byte n times; n = 0 then 1 bit: 1 = the end,
  0 = 8 bits of a byte as it is.
  After the end, 32FB adds each byte to the sum before it (deltas), 34FB does that twice.

The pair packing 46FB (0xb0674; 47FB has 3 bytes more before the size): 24 bits the
unpacked size, the escape byte, the number of pairs, then that many (code, first, second):
the code stands for its two bytes, each of which may be a code again (0xb0630). The data:
a byte that is no code is itself; a code is its pair; the escape and 0 is the end, the
escape and another byte is that byte as it is.
"""
import os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))


class Bits:
    def __init__(self, d):
        self.d, self.p = d, 0                  # p counts bits

    def get(self, n):
        v = 0
        for _ in range(n):
            byte = self.d[self.p >> 3] if self.p >> 3 < len(self.d) else 0
            v = v << 1 | (byte >> (7 - (self.p & 7))) & 1
            self.p += 1
        return v

    def peek(self, n):
        p = self.p
        v = self.get(n)
        self.p = p
        return v

    def number(self):
        if self.get(1):
            return 4 | self.get(2)
        z = 1
        while not self.get(1):
            z += 1
        return (1 << (z + 2)) | self.get(z + 2)


def huffman(d):
    b = Bits(d)
    kind = b.get(16)
    if kind & 0x100:
        b.get(24)
    size = b.get(24)
    escape = b.get(8)
    counts = [0]                               # counts[L]: codes of length L
    first = [0]                                # first[L]: the first code of length L
    code = 0
    while True:
        L = len(counts)
        code <<= 1
        n = b.number() - 4
        counts.append(n); first.append(code)
        code += n
        if n and code == 1 << L:
            break
    syms, used, s = [], [False] * 256, 255
    for _ in range(sum(counts)):
        step = b.number() - 3
        while step:
            s = (s + 1) & 255
            if not used[s]:
                step -= 1
        used[s] = True
        syms.append(s)
    index = [0] * len(counts)                  # where length L's symbols start in syms
    for L in range(1, len(counts)):
        index[L] = index[L - 1] + counts[L - 1]
    out = bytearray()
    while True:
        c, L = 0, 0
        while True:
            c = c << 1 | b.get(1)
            L += 1
            if counts[L] and c - first[L] < counts[L]:
                break
        sym = syms[index[L] + c - first[L]]
        if sym != escape:
            out.append(sym)
            continue
        n = b.number() - 4
        if n:
            out += bytes([out[-1]]) * n
        elif b.get(1):
            break
        else:
            out.append(b.get(8))
    kind &= 0xFEFF
    if kind in (0x32FB, 0x34FB):
        for _ in range(1 if kind == 0x32FB else 2):
            t = 0
            for i in range(len(out)):
                t = (t + out[i]) & 255
                out[i] = t
    return bytes(out[:size])


def pairs(d):
    p = 5 if d[0] == 0x47 else 2
    size = d[p] << 16 | d[p + 1] << 8 | d[p + 2]
    escape, n = d[p + 3], d[p + 4]
    p += 5
    pair = {}
    for _ in range(n):
        pair[d[p]] = (d[p + 1], d[p + 2])
        p += 3
    out = bytearray()

    def put(c):
        while c in pair:
            put(pair[c][0])
            c = pair[c][1]
        out.append(c)

    while True:
        c = d[p]; p += 1
        if c == escape:
            c = d[p]; p += 1
            if c == 0:
                break
            out.append(c)
        else:
            put(c)
    return bytes(out[:size])


def unpack(d):
    k = d[0] & 0xFE, d[1]
    if d[1] != 0xFB:
        return d
    if k[0] == 0x10:
        import fsh
        return fsh.refpack(d)
    if k[0] in (0x30, 0x32, 0x34):
        return huffman(d)
    if k[0] == 0x46:
        return pairs(d)
    raise ValueError("packing %02X%02X not read yet" % (d[0], d[1]))


if __name__ == "__main__":
    out = unpack(open(sys.argv[1], "rb").read())
    open(sys.argv[2], "wb").write(out)
    print("%s: %d bytes" % (sys.argv[2], len(out)))
