#!/usr/bin/env python3
"""The menus' .INV files: an inverse palette, 32768 bytes, read by nearestcolour 0x80c80 in
8-bit modes: colour 0xRRGGBB -> the palette entry at (R >> 3) << 10 | (G >> 3) << 5 | B >> 3.
Without one the game searches the palette itself (the entries flagged at 0xf77c8).

    python3 -I tools/inv.py check <file.inv> <picture.qfs|fsh>   how well it fits the palette
    python3 -I tools/inv.py png   <file.inv> <picture> <out.png>  the 32 slices through it

36 of the 51 fit their screen's own palette (the nearest entry, ties aside); 15 do not
(made for other art: NOTES §3.12). The game uses them as they are.
"""
import os, sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fsh


def palette(path):
    d = fsh.load(path)
    for n, o in fsh.shapes(d):
        if n.upper() == "!PAL":
            for t, bo in fsh.blocks(d, o):
                return np.array(fsh.palette_of(d, bo), float)


def colours():
    i = np.arange(32768)
    return np.stack([(i >> 10) & 31, (i >> 5) & 31, i & 31], 1) * 8 + 4.0


def main():
    what, invf, pic = sys.argv[1:4]
    inv = np.frombuffer(open(invf, "rb").read(), np.uint8)
    P = palette(pic)
    if what == "check":
        dist = ((colours()[:, None, :] - P[None, :, :]) ** 2).sum(2)
        print("%s against %s: the nearest entry %.1f%%, colour error %.1f (the best %.1f)" % (
            invf, pic, 100 * (dist.argmin(1) == inv).mean(),
            np.sqrt(dist[np.arange(32768), inv]).mean(), np.sqrt(dist.min(1)).mean()))
    elif what == "png":
        from PIL import Image
        img = np.zeros((4 * 40, 16 * 40, 3), np.uint8)
        g, b = np.meshgrid(np.arange(32), np.arange(32), indexing="ij")
        cube = inv.reshape(32, 32, 32)
        for k in range(32):           # left: through the palette; right: the colour asked for
            y, x = (k // 8) * 40, (k % 8) * 40
            img[y:y + 32, x:x + 32] = P.astype(np.uint8)[cube[k]]
            img[y:y + 32, x + 320:x + 352] = np.stack([np.full((32, 32), k), g, b], 2) * 8
        Image.fromarray(img).resize((1280, 320), Image.NEAREST).save(sys.argv[4])
        print(sys.argv[4])
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
