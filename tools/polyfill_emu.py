#!/usr/bin/env python3
"""EA's polygon filler, the original's against the port's: random convex polygons (the
cars' shadows' kind: quads and triangles, small and large, many crossing the window's
edges) shaded through a random table by the original's 0x8a094 (0x8bc71 with the row
painter 0xa5dd4), run on a snapshot, and by the port's poly_fill (port/src/polyfill.c)
on the same screen. Prints how many polygons differ and the first few.
With `solid` the polygons are filled in one colour (0x8bc71, the racers' markers), with
`line` random lines are drawn (0x8bb3c, their outlines), with `wide` smoothed wide lines
through a random blend table (0x8b195, the gauges' needles).

    build/venv/bin/python tools/polyfill_emu.py build/snap/e3.bin [count] [seed] [shade|solid|line|wide]
"""
import ctypes, math, os, random, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from snap_emu import Snap
import roadpic_emu as R

U8 = ctypes.c_ubyte
PTS = R.SCREEN + 0x70000
BLEND = R.SCREEN + 0x60000


def lib():
    import subprocess
    out = "build/polyfill_test.so"
    subprocess.check_call(["cc", "-O1", "-g", "-shared", "-fPIC", "-o", out,
                           "port/src/polyfill.c", "port/src/raster.c", "-Iport/src"])
    return ctypes.CDLL(os.path.abspath(out))


def polygon(rng):
    """a convex polygon: corners on an ellipse, turned, somewhere round the window"""
    n = rng.choice((3, 4, 4, 4, 5))
    size = rng.random()
    top = 3 if size < 0.2 else 40 if size < 0.7 else 200 if size < 0.95 else 2000
    rx = rng.uniform(0, top)
    ry = rng.uniform(0, top * 0.7)
    cx = rng.uniform(-60, 380)
    cy = rng.uniform(-40, 170)
    turn = rng.uniform(0, 2 * math.pi)
    angles = sorted(rng.uniform(0, 2 * math.pi) for _ in range(n))
    if rng.random() < 0.5:
        angles.reverse()
    pts = []
    for a in angles:
        x, y = rx * math.cos(a), ry * math.sin(a)
        pts.append((int(round(cx + x * math.cos(turn) - y * math.sin(turn))),
                    int(round(cy + x * math.sin(turn) + y * math.cos(turn)))))
    return pts


def main():
    path = sys.argv[1]
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 2000
    rng = random.Random(int(sys.argv[3]) if len(sys.argv) > 3 else 1)
    mode = sys.argv[4] if len(sys.argv) > 4 else "shade"
    s = Snap(path)
    mu = s.mu
    W, H = R.W, R.H
    mu.mem_map(R.SCREEN, 0x80000)
    mu.mem_write(R.ROWS, struct.pack("<%dI" % H, *[R.SCREEN + W * y for y in range(H)]))
    s.write(0xf88c4, struct.pack("<II", R.ROWS, 0))
    L = lib()
    shade = bytes(rng.randrange(256) for _ in range(256))
    s.write(0xf8118, shade)
    ctypes.memmove(ctypes.addressof((U8 * 256).in_dll(L, "g_shade")), shade, 256)
    fbuf = (U8 * (W * H))()
    blend = bytes(rng.randrange(256) for _ in range(16 * 256))
    mu.mem_write(BLEND, bytes([rng.randrange(256), 0, 0, 0]) + blend)
    cblend = (U8 * (16 * 256)).from_buffer_copy(blend)
    s.write(0x15ecc0, struct.pack("<i", 0))
    s.write(0x15ec90, struct.pack("<i", W << 16))
    s.write(0x15ecbc, struct.pack("<i", 0))
    s.write(0x15ec98, struct.pack("<i", H << 16))
    bad = painted = 0
    for k in range(count):
        clip = rng.choice([(0, 0, 320, 127), (0, 0, 320, 200), (16, 10, 300, 120)])
        s.write(0xf88a4, struct.pack("<4i", *clip))
        L.raster_window(fbuf, W, *clip)
        pts = polygon(rng)
        if mode == "line":
            pts = pts[:2]
            if rng.random() < 0.3:     # flat, upright, a point, diagonal
                x, y = pts[0]
                d = rng.randrange(-300, 300)
                pts[1] = rng.choice(((x + d, y), (x, y + d), (x, y), (x + d, y + d), (x + d, y - d)))
        colour = rng.randrange(256)
        screen = bytes(rng.randrange(256) for _ in range(W * H))
        mu.mem_write(R.SCREEN, screen)
        ctypes.memmove(fbuf, screen, W * H)
        mu.mem_write(PTS, struct.pack("<%di" % (2 * len(pts)), *[v for p in pts for v in p]))
        flat = [v for p in pts for v in p]
        xy = (ctypes.c_int * (2 * len(pts)))(*flat)
        if mode == "wide":
            colour = mu.mem_read(BLEND, 1)[0]
            ends = [rng.randrange(-40 << 16, 360 << 16), rng.randrange(-40 << 16, 240 << 16)]
            if rng.random() < 0.7:     # short ones, as the needles
                ends += [ends[0] + rng.randrange(-30 << 16, 30 << 16), ends[1] + rng.randrange(-30 << 16, 30 << 16)]
            else:
                ends += [rng.randrange(-40 << 16, 360 << 16), rng.randrange(-40 << 16, 240 << 16)]
            # (below a pixel wide the original reads the bytes before its table)
            ws = [rng.choice((0x10000, 0x20000, rng.randrange(0x10000, 0x60000))) for _ in range(2)]
            if rng.random() < 0.5:
                ws[1] = ws[0]
            s.call(0x8b195, *ends, BLEND, *ws)
            L.poly_wide_line(*ends, colour, cblend, *ws, W, H)
            pts = [(ends[0] / 65536, ends[1] / 65536), (ends[2] / 65536, ends[3] / 65536), ws]
        elif mode == "line":
            s.call(0x8bb3c, *flat, colour)
            L.poly_line(*flat, colour)
        elif mode == "solid":
            s.call(0x8bc71, colour, len(pts), *flat)
            L.poly_fill(len(pts), xy, L.poly_solid_rows, colour)
        else:
            s.call(0x8a094, len(pts), PTS)
            L.poly_fill(len(pts), xy, L.poly_shade_rows, 0)
        want = bytes(mu.mem_read(R.SCREEN, W * H))
        got = bytes(fbuf)
        painted += sum(1 for a, b in zip(want, screen) if a != b) if k < 50 else 0
        if got != want:
            bad += 1
            if bad <= 5:
                d = [i for i in range(W * H) if got[i] != want[i]]
                print("shape %d %s window %s: %d pixels differ, first (%d, %d)" % (
                    k, pts, clip, len(d), d[0] % W, d[0] // W))
    print("%d %s: %s (the first 50 changed %d pixels)" % (
        count, "lines" if mode in ("line", "wide") else "polygons",
        "%d differ" % bad if bad else "all equal", painted))


if __name__ == "__main__":
    main()
