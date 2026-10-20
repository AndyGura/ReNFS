#!/usr/bin/env python3
"""A menu picture of the port against the original's, by palette entry.

    python3 tools/fecompare.py <port.ppm> <port.raw> <original.png> [diff.png]

port.ppm / port.raw: a 640x480 --shot and its palette numbers (RENFS_SHOTRAW). The
original's: a screenshot from tools/nfsdos.py (DOSBox at 1280x1024: the picture doubled,
32 lines of black above and below). DOSBox changes the colours, but each of the port's
colours always the same way: each is taken as the original's colour most often under
it, and the pixels where the original shows another are counted, those under either
mouse pointer left out (--pointer=x,y[,x,y..]: the places of both, else none). diff.png
marks them in red. (The raw file is not needed: the port's colours stand for its
entries; it is read to check the sizes.)
"""
import sys
from PIL import Image


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    opts = [a for a in sys.argv[1:] if a.startswith("--")]
    ppm, raw, orig = args[:3]
    out = args[3] if len(args) > 3 else None
    port = Image.open(ppm).convert("RGB")
    W, H = port.size
    ent = open(raw, "rb").read()
    skip = []
    for a in opts:
        if a.startswith("--pointer="):
            v = [int(t) for t in a.split("=", 1)[1].split(",")]
            skip += [(v[i], v[i + 1]) for i in range(0, len(v), 2)]

    def hidden(x, y):
        return any(px <= x < px + 16 and py <= y < py + 24 for px, py in skip)

    pp = port.load()
    o = Image.open(orig).convert("RGB")
    sx, sy = o.size[0] / W, (o.size[1] - 64) / H
    op = o.load()

    def seen(x, y):
        return op[int(x * sx + sx / 2), int(32 + y * sy + sy / 2)]

    # each of the port's colours as DOSBox shows it: the original's colour most often
    # under it
    votes = {}
    for y in range(H):
        for x in range(W):
            if not hidden(x, y):
                v = votes.setdefault(pp[x, y], {})
                c = seen(x, y)
                v[c] = v.get(c, 0) + 1
    shown = {k: max(v, key=v.get) for k, v in votes.items()}
    bad = 0
    diff = port.copy()
    dp = diff.load()
    for y in range(H):
        for x in range(W):
            if not hidden(x, y) and seen(x, y) != shown[pp[x, y]]:
                bad += 1
                dp[x, y] = (255, 0, 0)
    print("%s: %d pixels differ" % (orig, bad))
    if out:
        diff.save(out)


main()
