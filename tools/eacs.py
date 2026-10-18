#!/usr/bin/env python3
"""EA's sound files: .EAS (speech, effects), .ASF / .AS4 (music streams), .BNK (sound banks).

    python3 -I tools/eacs.py list <file>...            the header(s)
    python3 -I tools/eacs.py wav  <file> <out.wav>     a .BNK: <out>_<n>.wav, one a sound
    python3 -I tools/eacs.py png  <file> <out.png> [n] the waveform (a bank: its n-th sound)

The header, "EACS" (read by the stream reader 0x8c8f3, the sample player 0x95b4e / 0xa1f11):
  +4 u32 samples a second, +8 u8 bytes a sample (1 = 8-bit signed, 2 = 16-bit), +9 u8
  channels, +0xa u8 compression (0 = PCM, 2 = IMA ADPCM, 4 bits a sample), +0xb i8 the voice
  a stream plays on (-1 in .EAS / .BNK), +0xc u32 samples, +0x10 i32 loop start (-1 none),
  +0x14 u32 loop length, +0x18 u32 where the data starts (.EAS: in the file; .BNK: in the
  file, not in the record).
  .EAS: the header at 0, the data at +0x18 (0x20).
  .BNK: 128 x u32 offsets of 0x48-byte records (0 = none), the header at a record's +0x28.
  .ASF / .AS4: blocks of u32 tag + u32 size (the 8 bytes included): "1SNh" (8 + the header,
  then the first data), "1SNd" data, "1SNl" where the loop ends (u32: the file position of the
  block the loop starts with; loops start on a block), "1SNe" the end (what follows is not
  the sound).
The samples: channels interleaved, PCM little-endian. The IMA ADPCM (the decoders 0xadfdc
mono, 0xae084 stereo, the standard IMA tables pre-multiplied at 0x12669c / 0x127cdc): high
nibble first; in stereo one byte a sample, high nibble the first channel. A stream's data
block (and the 1SNh's data) starts with 0x14 bytes: u32 samples, u32 step index channel 0,
channel 1, u32 value channel 0, channel 1 (low 16 bits). An .EAS starts from 0, 0.
"""
import struct, sys, wave

STEPS = [7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55,
         60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
         337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411,
         1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
         5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818,
         18500, 20350, 22385, 24623, 27086, 29794, 32767]
ADJUST = [-1, -1, -1, -1, 2, 4, 6, 8]


class Ima:
    """one channel's decoder (as 0xadfdc: add the difference, clamp, then the index)"""
    def __init__(self, value=0, index=0):
        self.v, self.i = value, index

    def __call__(self, n):
        s = STEPS[self.i]
        d = s >> 3
        if n & 4: d += s
        if n & 2: d += s >> 1
        if n & 1: d += s >> 2
        self.v = max(-32768, min(32767, self.v - d if n & 8 else self.v + d))
        self.i = max(0, min(88, self.i + ADJUST[n & 7]))
        return self.v


class Header:
    def __init__(self, b, o):
        assert b[o:o + 4] == b"EACS", b[o:o + 4]
        (self.rate, self.bytes, self.channels, self.comp, self.voice, self.samples,
         self.loop, self.looplen, self.data) = struct.unpack_from("<IBBBbIiII", b, o + 4)

    def __str__(self):
        return "%d Hz, %d-bit, %s, %s, %d samples (%.2f s)%s" % (
            self.rate, 8 * self.bytes, ("mono", "stereo")[self.channels - 1],
            {0: "PCM", 2: "IMA ADPCM"}.get(self.comp, "compression %d" % self.comp),
            self.samples, self.samples / self.rate,
            ", loop %d + %d" % (self.loop, self.looplen) if self.loop >= 0 else "")


def pcm(h, b, o, n):
    """n samples from o: a list of ints, channels interleaved"""
    if h.bytes == 1:
        return [v << 8 for v in struct.unpack_from("<%db" % (n * h.channels), b, o)]
    return list(struct.unpack_from("<%dh" % (n * h.channels), b, o))


def ima(h, b, o, n, dec):
    out = []
    if h.channels == 2:
        for k in range(n):
            c = b[o + k]
            out += (dec[0](c >> 4), dec[1](c & 15))
    else:
        for k in range(n):
            c = b[o + k // 2]
            out.append(dec[0](c >> 4 if k % 2 == 0 else c & 15))
    return out


def stream(b):
    """an .ASF / .AS4: (header, samples, the 1SNl's file position)"""
    o, h, out, loopval = 0, None, [], None
    while o + 8 <= len(b):
        tag, size = struct.unpack_from("<4sI", b, o)
        if tag in (b"1SNh", b"1SNd"):
            if tag == b"1SNh":
                h = Header(b, o + 8)
                d = o + 0x28
            else:
                d = o + 8
            if h.comp == 2:
                n, i0, i1, v0, v1 = struct.unpack_from("<5I", b, d)
                dec = [Ima(struct.unpack("<h", struct.pack("<H", v0 & 0xFFFF))[0], i0),
                       Ima(struct.unpack("<h", struct.pack("<H", v1 & 0xFFFF))[0], i1)]
                out += ima(h, b, d + 0x14, n, dec)
            else:
                out += pcm(h, b, d, (o + size - d) // (h.bytes * h.channels))
        elif tag == b"1SNl":
            loopval = struct.unpack_from("<I", b, o + 8)[0]
        elif tag == b"1SNe":
            break
        else:
            raise ValueError("block %r at 0x%x" % (tag, o))
        o += size
    return h, out, loopval


def sounds(path):
    """[(name, header, samples)] for any of the four kinds"""
    b = open(path, "rb").read()
    if b[:4] == b"1SNh":
        h, s, _ = stream(b)
        return [("", h, s)]
    if b[:4] == b"EACS":
        hs = [("", 0)]
    else:                                   # a bank
        hs = [("_%d" % i, o + 0x28) for i, o in enumerate(struct.unpack_from("<128I", b, 0)) if o]
    out = []
    for name, o in hs:
        h = Header(b, o)
        if h.comp == 2:
            s = ima(h, b, h.data, h.samples, [Ima(), Ima()])
        else:
            s = pcm(h, b, h.data, h.samples)
        out.append((name, h, s))
    return out


def main():
    what = sys.argv[1]
    if what == "list":
        for p in sys.argv[2:]:
            b = open(p, "rb").read()
            if b[:4] == b"1SNh":
                print("%s: a stream, %s" % (p, Header(b, 8)))
            elif b[:4] == b"EACS":
                print("%s: %s" % (p, Header(b, 0)))
            else:
                offs = [(i, o) for i, o in enumerate(struct.unpack_from("<128I", b, 0)) if o]
                print("%s: a bank of %d" % (p, len(offs)))
                for i, o in offs:
                    print("  %3d: %s" % (i, Header(b, o + 0x28)))
    elif what == "wav":
        path, out = sys.argv[2], sys.argv[3]
        for name, h, s in sounds(path):
            fn = out[:-4] + name + ".wav" if name else out
            with wave.open(fn, "wb") as w:
                w.setnchannels(h.channels); w.setsampwidth(2); w.setframerate(h.rate)
                w.writeframes(struct.pack("<%dh" % len(s), *s))
            print(fn, h)
    elif what == "png":
        from PIL import Image, ImageDraw
        path, out = sys.argv[2], sys.argv[3]
        name, h, s = sounds(path)[int(sys.argv[4]) if len(sys.argv) > 4 else 0]
        W, H = 1600, 200 * h.channels
        im = Image.new("RGB", (W, H), (20, 20, 28)); d = ImageDraw.Draw(im)
        n = len(s) // h.channels
        for c in range(h.channels):
            ch = s[c::h.channels]
            mid = 100 + 200 * c
            for x in range(W):
                part = ch[x * n // W:(x + 1) * n // W] or [0]
                d.line([(x, mid - max(part) * 95 // 32768), (x, mid - min(part) * 95 // 32768)],
                       fill=(90, 200, 120))
        im.save(out)
        print(out, h)
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
