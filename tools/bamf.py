#!/usr/bin/env python3
"""EA's BAMF blit lists: which spans of an off-screen picture (the rendered 3D view) are
copied to the screen, around the race's HUD and dashboards. Copied by 0x8b8f0, by VESA
64 KB bank. Files: SIMDATA/MISC/MASKHI.B<s><n> / MASKLO.B<s><n> (one BAMF each),
MASKHI.MMB / MASKLO.MMB (a wwww of 12), SIMDATA/DASH/<car>DH.FMM / DL.FMM (a wwww of 4).

    python3 -I tools/bamf.py list <file>...
    python3 -I tools/bamf.py png  <file> <out.png> [width=640|320]

A BAMF: "BAMF", u32 size, u16 banks, u16 the longest span, u32 +0xc and u32 +0x10 (0x10000
or 0x8000: 0x8000 = the source is read at half the rate across / down: pixels doubled),
then per 64 KB bank: u16 spans, each u32 offset in the source picture, u16 offset in the
bank, u16 length. load_cockpit_mask 0x5f7c8 loads the 12 .Bsn (s = the view's size: full,
medium, small; n = how it is copied: 1:1, doubled across, doubled both ways, doubled with
every other line) and the .MMB's 12 (the same without the map's box).
"""
import os, struct, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import orip


class Bamf:
    def __init__(self, d, o=0):
        assert d[o:o + 4] == b"BAMF", d[o:o + 4]
        self.size, self.banks, self.longest, self.xstep, self.ystep = struct.unpack_from(
            "<IHHII", d, o + 4)
        p, self.spans = o + 0x14, []
        for bank in range(self.banks):
            n = struct.unpack_from("<H", d, p)[0]; p += 2
            for _ in range(n):
                src, dst, ln = struct.unpack_from("<IHH", d, p); p += 8
                self.spans.append((src, bank * 65536 + dst, ln))


def all_in(path):
    d = open(path, "rb").read()
    if d[:4] == b"wwww":
        return [Bamf(d, e) for e in orip.container(d, 0) if e is not None]
    return [Bamf(d)]


def main():
    what, path = sys.argv[1], sys.argv[2]
    bs = all_in(path)
    if what == "list":
        for path in sys.argv[2:]:
            for i, b in enumerate(all_in(path)):
                print("%s[%d]: %d banks, %d spans, longest %d, steps %#x %#x" % (
                    path, i, b.banks, len(b.spans), b.longest, b.xstep, b.ystep))
    elif what == "png":
        from PIL import Image
        W = int(sys.argv[4]) if len(sys.argv) > 4 else 640
        H = W * 3 // 4
        sheet = Image.new("L", (4 * (W // 2 + 8), ((len(bs) + 3) // 4) * (H // 2 + 8)), 0)
        for i, b in enumerate(bs):
            im = Image.new("L", (W, H), 40); px = im.load()
            for src, dst, ln in b.spans:
                for q in range(dst, dst + ln):
                    if q // W < H:
                        px[q % W, q // W] = 255
            sheet.paste(im.resize((W // 2, H // 2)), ((i % 4) * (W // 2 + 8), (i // 4) * (H // 2 + 8)))
        sheet.save(sys.argv[3])
        print(sys.argv[3])
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
