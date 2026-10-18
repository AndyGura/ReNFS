#!/usr/bin/env python3
"""EA's pictures: FSH ("SHPI" shape files) and QFS (the same, RefPack-compressed).

    python3 -I tools/fsh.py list  <file.fsh|file.qfs>...
    python3 -I tools/fsh.py png   <file> <outdir> [palette.fsh]

An FSH: "SHPI", uint32 file size, uint32 count, 4-byte directory id, then count
records of (4-byte name, uint32 offset).  Each shape: a byte type (0x7B 8-bit
indexed, 0x7D 32-bit, 0x78 16-bit 565, 0x22/0x24 a palette), 24-bit offset to the
next block of the same shape (0 = none), uint16 width, height, x and y centres,
x and y positions, then the pixels; after them may come a palette (type 0x22,
0x24, 0x2A, 0x2D) and other attached blocks.
"""
import os, struct, sys, zlib


def refpack(d):
    """RefPack (EA's LZ77): 0x10FB / 0x11FB header, big-endian 24-bit size."""
    flags = struct.unpack_from(">H", d, 0)[0]
    assert flags & 0xFF == 0xFB, "not RefPack: %04x" % flags
    p = 2
    if flags & 0x0100:                 # compressed size first
        p += 3
    size = int.from_bytes(d[p:p + 3], "big")
    p += 3
    out = bytearray()
    while p < len(d):
        b0 = d[p]
        if b0 < 0x80:                  # 2 bytes: 0-3 literals, copy 3-10 from up to 1024 back
            b1 = d[p + 1]; p += 2
            lit = b0 & 3
            out += d[p:p + lit]; p += lit
            n = ((b0 >> 2) & 7) + 3
            off = ((b0 & 0x60) << 3) + b1 + 1
        elif b0 < 0xC0:                # 3 bytes: copy 4-67 from up to 16384 back
            b1, b2 = d[p + 1], d[p + 2]; p += 3
            lit = b1 >> 6
            out += d[p:p + lit]; p += lit
            n = (b0 & 0x3F) + 4
            off = ((b1 & 0x3F) << 8) + b2 + 1
        elif b0 < 0xE0:                # 4 bytes: copy 5-1028 from up to 131072 back
            b1, b2, b3 = d[p + 1], d[p + 2], d[p + 3]; p += 4
            lit = b0 & 3
            out += d[p:p + lit]; p += lit
            n = ((b0 & 0x0C) << 6) + b3 + 5
            off = ((b0 & 0x10) << 12) + (b1 << 8) + b2 + 1
        elif b0 < 0xFC:                # literals only, 4-112
            lit = ((b0 & 0x1F) << 2) + 4; p += 1
            out += d[p:p + lit]; p += lit
            continue
        else:                          # end, 0-3 literals
            lit = b0 & 3; p += 1
            out += d[p:p + lit]
            break
        for _ in range(n):
            out.append(out[-off])
    if len(out) != size:
        print("refpack: %d bytes, header says %d" % (len(out), size), file=sys.stderr)
    return bytes(out)


def load(path):
    d = open(path, "rb").read()
    if d[:4] != b"SHPI" and d[1] == 0xFB:
        d = refpack(d)
    assert d[:4] == b"SHPI", path
    return d


def shapes(d):
    n = struct.unpack_from("<I", d, 8)[0]
    end = len(d)
    recs = [(d[16 + 8 * i:20 + 8 * i].decode("latin1"), struct.unpack_from("<I", d, 20 + 8 * i)[0])
            for i in range(n)]
    for name, off in recs:
        yield name, off


def blocks(d, off):
    """The blocks of one shape: (type, offset of its header)."""
    while True:
        t = d[off]
        nxt = int.from_bytes(d[off + 1:off + 4], "little")
        yield t, off
        if not nxt:
            return
        off += nxt


def palette_of(d, off):
    """A palette block: 0x22/0x24 (24-bit 6-bit or 8-bit), 0x2A (32-bit)."""
    t = d[off]
    w = struct.unpack_from("<H", d, off + 4)[0]
    if t == 0x22:      # 6 bits a channel
        return [tuple(c * 255 // 63 for c in d[off + 16 + 3 * i:off + 19 + 3 * i]) for i in range(w)]
    if t == 0x24:
        return [tuple(d[off + 16 + 3 * i:off + 19 + 3 * i]) for i in range(w)]
    if t == 0x2A:
        return [(d[off + 18 + 4 * i], d[off + 17 + 4 * i], d[off + 16 + 4 * i]) for i in range(w)]
    if t == 0x2D:      # 16-bit 1555
        out = []
        for i in range(w):
            v = struct.unpack_from("<H", d, off + 16 + 2 * i)[0]
            out.append((((v >> 10) & 31) * 255 // 31, ((v >> 5) & 31) * 255 // 31, (v & 31) * 255 // 31))
        return out
    return None


def write_png(path, w, h, rows):
    raw = b"".join(b"\0" + bytes(r) for r in rows)
    def chunk(k, v):
        return struct.pack(">I", len(v)) + k + v + struct.pack(">I", zlib.crc32(k + v) & 0xFFFFFFFF)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
                + chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b""))


def main():
    cmd, path = sys.argv[1], sys.argv[2]
    if cmd == "list":
        for p in sys.argv[2:]:
            d = load(p)
            print("%s: %s, %d shapes" % (p, d[12:16].decode("latin1"), struct.unpack_from("<I", d, 8)[0]))
            for name, off in shapes(d):
                parts = []
                for t, o in blocks(d, off):
                    w, h, cx, cy, x, y = struct.unpack_from("<6H", d, o + 4)
                    parts.append("%02x %dx%d c(%d,%d) at(%d,%d)" % (t, w, h, cx, cy, x, y))
                print("  %-4s @%06x  %s" % (name, off, " | ".join(parts)))
        return
    out = sys.argv[3]
    os.makedirs(out, exist_ok=True)
    d = load(path)
    gpal = None
    if len(sys.argv) > 4:
        pd = load(sys.argv[4])
        for name, off in shapes(pd):
            for t, o in blocks(pd, off):
                gpal = gpal or palette_of(pd, o)
    # a shape file's own palette: a shape that is a palette ("!pal"), else the shape's attached one
    fpal = None
    for name, off in shapes(d):
        if d[off] in (0x22, 0x24, 0x2A, 0x2D):
            fpal = palette_of(d, off)
    for name, off in shapes(d):
        t = d[off]
        w, h = struct.unppack_from if False else struct.unpack_from("<2H", d, off + 4)
        pal = None
        for bt, o in blocks(d, off):
            if o != off:
                pal = pal or palette_of(d, o)
        pal = pal or fpal or gpal or [(i, i, i) for i in range(256)]
        px = off + 16
        rows = []
        if t == 0x7B:
            for y in range(h):
                rows.append([c for i in d[px + y * w:px + (y + 1) * w] for c in pal[i % len(pal)]])
        elif t == 0x7D:
            for y in range(h):
                r = d[px + y * w * 4:px + (y + 1) * w * 4]
                rows.append([r[i + k] for i in range(0, len(r), 4) for k in (2, 1, 0)])
        elif t == 0x78:
            for y in range(h):
                r = []
                for x in range(w):
                    v = struct.unpack_from("<H", d, px + 2 * (y * w + x))[0]
                    r += [((v >> 11) & 31) * 255 // 31, ((v >> 5) & 63) * 255 // 63, (v & 31) * 255 // 31]
                rows.append(r)
        else:
            continue
        safe = "".join(c if c.isalnum() else "_" for c in name)
        write_png(os.path.join(out, "%s.png" % safe), w, h, rows)
    print("wrote", out)


if __name__ == "__main__":
    main()
