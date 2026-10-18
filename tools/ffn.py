#!/usr/bin/env python3
"""EA's fonts (.FFN, "FNTF"), read by setfont 0x91ed0.

    python3 -I tools/ffn.py list <file.ffn>
    python3 -I tools/ffn.py png  <file.ffn> <out.png> ["text"]   the bitmap and a line

Header: "FNTF" (setfont also takes FNT3 FNTP FNTS FNTU), u32 size, u16 100, u16 characters,
u16 +0xc, u16 +0xe (setfont: & 3 = 2 picks the other drawer), bytes +0x10 +0x11 +0x12 +0x13
(+0x12 + +0x13 = the line's height), u32 +0x14 the character table, u32 +0x1c the bitmap.
A character, 11 bytes: u16 code, u8 width, u8 height, u16 x, u16 y (in the bitmap),
u8 advance, i8 x offset, i8 y offset (from the line's top).
The bitmap: an FSH shape header (16 bytes: type 0x7a = 4 bits a pixel, u24, u16 width,
height, ...), then rows of (4 x width + 7) / 8 bytes; a pixel is a level 0..15 (the edge's
smoothing), coloured by the text's colour when drawn.
"""
import struct, sys


class Font:
    def __init__(self, path):
        b = self.b = open(path, "rb").read()
        assert b[:4] in (b"FNTF", b"FNT3", b"FNTP", b"FNTS", b"FNTU"), b[:4]
        self.count = struct.unpack_from("<H", b, 0xA)[0]
        self.bytes10 = tuple(b[0x10:0x14])
        self.height = b[0x12] + b[0x13]
        ct, bm = struct.unpack_from("<I", b, 0x14)[0], struct.unpack_from("<I", b, 0x1C)[0]
        self.chars = {}
        for i in range(self.count):
            code, w, h, x, y, adv, xo, yo = struct.unpack_from("<HBBHHBbb", b, ct + 11 * i)
            self.chars[code] = (w, h, x, y, adv, xo, yo)
        self.type = b[bm]
        self.bw, self.bh = struct.unpack_from("<HH", b, bm + 4)
        self.pix = bm + 16
        self.row = (4 * self.bw + 7) // 8

    def level(self, x, y):
        v = self.b[self.pix + y * self.row + x // 2]
        return (v >> 4) if x % 2 == 0 else (v & 15)


def main():
    what, path = sys.argv[1], sys.argv[2]
    f = Font(path)
    if what == "list":
        print("%s: %d characters, line %d (bytes 0x10..0x13 %s), bitmap type 0x%x %dx%d" % (
            path, f.count, f.height, f.bytes10, f.type, f.bw, f.bh))
        for code, c in sorted(f.chars.items()):
            print("  %3d %r  %dx%d at (%d,%d)  advance %d  offset %d,%d" % (
                code, chr(code) if 32 <= code < 256 else "?", c[0], c[1], c[2], c[3], c[4], c[5], c[6]))
    elif what == "png":
        from PIL import Image
        text = sys.argv[4] if len(sys.argv) > 4 else "THE NEED FOR SPEED  Ferrari 512TR 0123456789 !?"
        width = max(f.bw, sum(f.chars.get(ord(ch), (0,) * 5)[4] for ch in text) + 16)
        im = Image.new("RGB", (width, f.bh + f.height + 24), (20, 24, 60))
        px = im.load()
        for y in range(f.bh):
            for x in range(f.bw):
                l = f.level(x, y) * 17
                px[x, y] = (l, l, l)
        pen, top = 8, f.bh + 12
        for ch in text:
            c = f.chars.get(ord(ch))
            if not c:
                continue
            w, h, x0, y0, adv, xo, yo = c
            for y in range(h):
                for x in range(w):
                    l = f.level(x0 + x, y0 + y)
                    if l:
                        X, Y = pen + xo + x, top + yo + y
                        bg = px[X, Y]
                        px[X, Y] = tuple((bg[k] * (15 - l) + (255, 220, 60)[k] * l) // 15 for k in range(3))
            pen += adv
        im.save(sys.argv[3])
        print(sys.argv[3])
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
