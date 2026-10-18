#!/usr/bin/env python3
"""EA's TGV video (FRONTEND/MOVIE/*.TGV): the intro, the attract loops, the car showcases,
the police and winners' clips. Played by 0x346dc; frames decoded by 0x8d4ac.

    python3 -I tools/tgv.py info  <file.tgv>
    python3 -I tools/tgv.py png   <file.tgv> <out.png> [every=15]  a sheet of frames
    python3 -I tools/tgv.py frames <file.tgv> <outdir> [first [last]]  one PNG a frame
    python3 -I tools/tgv.py wav   <file.tgv> <out.wav>             the sound

Blocks of u32 tag + u32 size (the 8 included): "1SNh" / "1SNd" the sound (an EA stream,
tools/eacs.py), "kVGT" a key frame, "fVGT" a delta frame, 15 a second.
  kVGT (0x8d190): +8 u16 width, +0xa height, +0xc u16, +0xe u16 colours, +0x10 u32 flags
    (bit 0: the picture as it is, else packed: tools/unpack.py), +0x14 the colours (3 bytes,
    8-bit), then the picture.
  fVGT (0x8cf00): +8 u16 motion vectors, +0xa raw blocks, +0xc packed blocks, +0xe bits an
    index, then from +0x14: the vectors (x, y pairs of 10-bit signed fields, least
    significant bit first, 0x8cd60; rounded up to 32 bits), the raw 4x4 blocks (16 bytes),
    the packed ones (8 bytes: 4 colours and a u32 of sixteen 2-bit codes, top first,
    0x8cdf0), one index for every 4x4 block of the picture (0xa6ec8, rounded to 32 bits).
    The picture (0xa6f6c), block by block: an index below the number of vectors copies the
    4x4 at this place + that vector from the previous picture; another is block
    (index - vectors) of the raw blocks then the packed ones.
"""
import os, struct, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import unpack


def bits(d, o, n, width, signed):
    """n fields of width bits from byte o, least significant bit first"""
    v = int.from_bytes(d[o:o + (n * width + 7) // 8 + 4], "little")
    m = (1 << width) - 1
    out = []
    for k in range(n):
        x = (v >> (k * width)) & m
        if signed and x >> (width - 1):
            x -= 1 << width
        out.append(x)
    return out


class Movie:
    def __init__(self, path):
        self.d = open(path, "rb").read()
        self.w = self.h = 0
        self.pal = [(0, 0, 0)] * 256
        self.cur = self.prev = None

    def blocks(self):
        d, o = self.d, 0
        while o + 8 <= len(d):
            tag, size = struct.unpack_from("<4sI", d, o)
            if size < 8:
                break
            yield tag, o, size
            o += size

    def key(self, o):
        d = self.d
        w, h, _, n, flags = struct.unpack_from("<HHHHI", d, o + 8)
        self.w, self.h = w, h
        p = o + 0x14
        for k in range(n):
            self.pal[k] = tuple(d[p + 3 * k:p + 3 * k + 3])
        p += 3 * n
        pic = d[p:p + w * h] if flags & 1 else unpack.unpack(d[p:])
        self.cur = bytearray(pic[:w * h].ljust(w * h, b"\0"))

    def delta(self, o):
        d, w, h = self.d, self.w, self.h
        self.prev, self.cur = self.cur, bytearray(w * h)
        nmv, nraw, npacked, vb = struct.unpack_from("<HHHH", d, o + 8)
        p = o + 0x14
        mv = bits(d, p, 2 * nmv, 10, True)
        p += ((nmv * 20 + 31) & ~31) >> 3
        blocks = [d[p + 16 * k:p + 16 * k + 16] for k in range(nraw)]
        p += 16 * nraw
        for k in range(npacked):
            c = d[p:p + 4]
            v = struct.unpack_from("<I", d, p + 4)[0]
            blocks.append(bytes(c[(v >> (30 - 2 * j)) & 3] for j in range(16)))
            p += 8
        nb = (w // 4) * (h // 4)
        idx = bits(d, p, nb, vb, False)
        prev, cur = self.prev, self.cur
        k = 0
        for by in range(0, h, 4):
            for bx in range(0, w, 4):
                i = idx[k]; k += 1
                if i < nmv:
                    s = (by + mv[2 * i + 1]) * w + bx + mv[2 * i]
                    for r in range(4):
                        a = (by + r) * w + bx
                        cur[a:a + 4] = prev[s + r * w:s + r * w + 4]
                else:
                    b = blocks[i - nmv]
                    for r in range(4):
                        a = (by + r) * w + bx
                        cur[a:a + 4] = b[4 * r:4 * r + 4]

    def frames(self):
        """every picture, as (index bytes, palette copy)"""
        for tag, o, size in self.blocks():
            if tag == b"kVGT":
                self.key(o)
            elif tag == b"fVGT":
                self.delta(o)
            else:
                continue
            yield bytes(self.cur), list(self.pal)


def image(frame, pal, w, h):
    from PIL import Image
    im = Image.frombytes("P", (w, h), frame)
    im.putpalette([c for rgb in pal for c in rgb])
    return im.convert("RGB")


def main():
    what, path = sys.argv[1], sys.argv[2]
    m = Movie(path)
    if what == "info":
        import collections
        c = collections.Counter(t for t, _, _ in m.blocks())
        first = next(o for t, o, _ in m.blocks() if t == b"kVGT")
        w, h, x, n, fl = struct.unpack_from("<HHHHI", m.d, first + 8)
        n_frames = c[b"kVGT"] + c[b"fVGT"]
        print("%s: %dx%d, %d frames (%d key) = %.1f s at 15 a second, +0xc %d, colours %d, "
              "flags %#x; %s" % (path, w, h, n_frames, c[b"kVGT"], n_frames / 15, x, n, fl,
                                 dict(c)))
    elif what == "png":
        from PIL import Image
        every = int(sys.argv[4]) if len(sys.argv) > 4 else 15
        pics = [image(f, p, m.w, m.h) for k, (f, p) in enumerate(m.frames()) if k % every == 0]
        cols = 4
        W, H = pics[0].size
        sheet = Image.new("RGB", (cols * W, ((len(pics) + cols - 1) // cols) * H))
        for k, im in enumerate(pics):
            sheet.paste(im, ((k % cols) * W, (k // cols) * H))
        sheet.save(sys.argv[3])
        print("%s: %d frames, one every %d" % (sys.argv[3], len(pics), every))
    elif what == "frames":
        out = sys.argv[3]
        os.makedirs(out, exist_ok=True)
        a = int(sys.argv[4]) if len(sys.argv) > 4 else 0
        b = int(sys.argv[5]) if len(sys.argv) > 5 else 1 << 30
        for k, (f, p) in enumerate(m.frames()):
            if a <= k <= b:
                image(f, p, m.w, m.h).save(os.path.join(out, "%04d.png" % k))
    elif what == "wav":
        import eacs, wave
        d = b"".join(m.d[o:o + s] for t, o, s in m.blocks() if t in (b"1SNh", b"1SNd"))
        h, s, _ = eacs.stream(d)
        with wave.open(sys.argv[3], "wb") as wv:
            wv.setnchannels(h.channels); wv.setsampwidth(2); wv.setframerate(h.rate)
            wv.writeframes(struct.pack("<%dh" % len(s), *s))
        print(sys.argv[3], h)
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
