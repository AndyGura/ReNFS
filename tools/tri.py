#!/usr/bin/env python3
"""NFS SE tracks: the .TRI files (SIMDATA/MISC/<track><segment>.TRI).

    python3 -I tools/tri.py info  <file.tri>...
    python3 -I tools/tri.py nodes <file.tri>            every road node, one a line
    python3 -I tools/tri.py png   <file.tri> <out.png>  the track from above

What the game does with each field: docs/NOTES.md §3.3 (the loader load_tri 0x4f010,
the section decoder 0x4eb00, the objects 0x4f7d4).

The file, in the order load_tri reads it:
  "RoadSection", 0x1621c bytes:
    +0    u32 0x11
    +4    u16 sections in the loop (0 = an open road), +6 u16 sections (4 road nodes each)
    +8    i32 the distance from node to node (6.0 in 16.16)
    +0xc  3 x i32 (?), +0x18 / +0x20 copied to node 0 by the loader
    +0x24 u32 the size of the stored sections ("TrackSlices")
    +0x28 u32 (?)
    +0x2c 600 x u32 each section's offset in the stored sections
    +0x98c 2400 road nodes of 0x24 bytes:
          +0..3 u8 (widths?), +4 u8 lanes (high nibble one side, low the other),
          +5..7 u8, +8 3 x i32 position x, y (up), z (16.16), +0x14 u16 (slope?),
          +0x18 u16 heading (0x4000 = a full turn; atan2(dx, dz) to the next node),
          +0x1a..0x23 (?)
    +0x15b0c 600 x 3 u8 a section: an opponents' speed (scaled at load by the
          difficulty), the speed limit (the police), a traffic speed
    +0x16214 u32 object kinds (64), +0x16218 u32 most placements (1000)
  "RoadObjects": "SJBO", u32 size (with this 12-byte head), u32 0, then
    kinds x 16 bytes (read by 0x59490, 0x4f7d4): +0 u8 flags (4 = animated: +8 frames,
          +9 steps a frame), +1 u8 type = the 3D points it takes: 1 = a 3D model (+2 its
          number in the family's models), 4 = a flat picture, 6 = two flat faces; +2 u8 the
          picture (/ 4 = the family's entry-1 SHPI; + 0..3 a variant), +3 u8 (the second
          face's?), +4 i32 width, +8 i32 (the second face's width?), +0xc i32 height (16.16)
    placements x 16 bytes, ended by -1: +0 i32 node, +4 u8 kind, +5 u8 angle
          (256 a turn, on top of the road's heading), +0xa 3 x i16 offset x, y, z
          from the node (8.8)
  "TrackSlices": per section "TRKD", u32, u32, 12 bytes (textures?), then 4 rows
    (one a node) of 11 points x 3 x i16 (1/128): point 0 from the node, points 1 and 6
    from point 0, the rest each from the point before (1..5 one side, 6..10 the other).
"""
import math, struct, sys

NODES, NODE = 0x98C, 0x24
SPEEDS = 0x15B0C
HEAD = 0x1621C


class Track:
    def __init__(self, path):
        b = self.b = open(path, "rb").read()
        assert struct.unpack_from("<I", b, 0)[0] == 0x11, "not a TRI"
        self.loop, self.sections = struct.unpack_from("<HH", b, 4)
        self.step = struct.unpack_from("<i", b, 8)[0] / 65536
        self.slices_size, self.x28 = struct.unpack_from("<II", b, 0x24)
        self.kinds_n, self.most = struct.unpack_from("<II", b, 0x16214)
        n = self.sections * 4
        self.nodes = []
        for i in range(n):
            o = NODES + i * NODE
            self.nodes.append(dict(
                raw=b[o:o + NODE],
                lanes=(b[o + 4] >> 4, b[o + 4] & 15),
                pos=tuple(v / 65536 for v in struct.unpack_from("<3i", b, o + 8)),
                heading=struct.unpack_from("<H", b, o + 0x18)[0]))
        self.speeds = [tuple(b[SPEEDS + i * 3:SPEEDS + i * 3 + 3]) for i in range(self.sections)]
        # the objects
        tag, size, _ = struct.unpack_from("<4sII", b, HEAD)
        assert tag == b"SJBO", tag
        d = HEAD + 12
        self.kinds = []
        for i in range(self.kinds_n):
            r = b[d + i * 16:d + i * 16 + 16]
            self.kinds.append(dict(flags=r[0], type=r[1], pic=r[2], pic2=r[3],
                                   width=struct.unpack_from("<i", r, 4)[0] / 65536,
                                   width2=struct.unpack_from("<i", r, 8)[0] / 65536,
                                   height=struct.unpack_from("<i", r, 12)[0] / 65536))
        p = d + self.kinds_n * 16
        self.objects = []
        while struct.unpack_from("<i", b, p)[0] != -1:
            node, kind, angle = struct.unpack_from("<iBB", b, p)
            off = [v / 256 for v in struct.unpack_from("<3h", b, p + 10)]
            nx, ny, nz = self.nodes[node % n]["pos"]
            self.objects.append(dict(node=node, kind=kind, angle=angle,
                                     pos=(nx + off[0], ny + off[1], nz + off[2])))
            p += 16
        self.slices_at = HEAD + size

    def section_points(self, s):
        """the 4 rows of 11 road points of section s (0x4eb00)"""
        b = self.b
        o = self.slices_at + struct.unpack_from("<I", b, 0x2C + s * 4)[0]
        assert b[o:o + 4] == b"TRKD"
        p = o + 0x18
        rows = []
        for r in range(4):
            v = [tuple(c / 128 for c in struct.unpack_from("<3h", b, p + k * 6)) for k in range(11)]
            nx, ny, nz = self.nodes[(s * 4 + r) % len(self.nodes)]["pos"]
            pts = [(nx + v[0][0], ny + v[0][1], nz + v[0][2])]
            for k in range(1, 11):
                base = pts[0] if k in (1, 6) else pts[-1]
                pts.append(tuple(base[j] + v[k][j] for j in range(3)))
            rows.append(pts)
            p += 66
        return rows


def info(t, path):
    xs = [n["pos"][0] for n in t.nodes]; zs = [n["pos"][2] for n in t.nodes]
    ys = [n["pos"][1] for n in t.nodes]
    print("%s: %s, %d sections, %d nodes %.2f apart (%.0f long), x %.0f..%.0f, z %.0f..%.0f, "
          "height %.0f..%.0f" % (path, "a loop of %d" % t.loop if t.loop else "an open road",
                                  t.sections, len(t.nodes), t.step, t.step * (len(t.nodes) - 1),
                                  min(xs), max(xs), min(zs), max(zs), min(ys), max(ys)))
    used = sorted({o["kind"] for o in t.objects})
    print("  %d objects of %d kinds (of %d); +0x28 = %d; speeds: opponents %d..%d, limit %s, "
          "traffic %d..%d" % (len(t.objects), len(used), t.kinds_n, t.x28,
                              min(s[0] for s in t.speeds), max(s[0] for s in t.speeds),
                              sorted({s[1] for s in t.speeds}),
                              min(s[2] for s in t.speeds), max(s[2] for s in t.speeds)))


def nodes(t):
    for i, n in enumerate(t.nodes):
        s = t.speeds[i // 4]
        print("%4d  %10.2f %8.2f %10.2f  heading %5d  lanes %d|%d  %s  speeds %s" % (
            i, *n["pos"], n["heading"], *n["lanes"], n["raw"][:8].hex(" "), s))


def png(t, out):
    from PIL import Image, ImageDraw
    pts = [p for s in range(t.sections) for row in t.section_points(s) for p in row]
    xs = [p[0] for p in pts]; zs = [p[2] for p in pts]
    W = 1600
    span = max(max(xs) - min(xs), max(zs) - min(zs)) or 1
    k = (W - 40) / span
    H = int((max(zs) - min(zs)) * k) + 40
    W2 = int((max(xs) - min(xs)) * k) + 40
    f = lambda x, z: (20 + (x - min(xs)) * k, H - 20 - (z - min(zs)) * k)
    im = Image.new("RGB", (W2, H), (24, 24, 28))
    d = ImageDraw.Draw(im)
    rows = [row for s in range(t.sections) for row in t.section_points(s)]
    for a, b in zip(rows, rows[1:] + ([rows[0]] if t.loop else [])):
        # each side: 0 -> 1..5 and 0 -> 6..10; a strip between neighbours is a quad
        for side in ((0, 1, 2, 3, 4, 5), (0, 6, 7, 8, 9, 10)):
            for j, (u, v) in enumerate(zip(side, side[1:])):
                shade = (110, 110, 118) if j < 2 else (60, 90, 60) if j < 4 else (90, 80, 60)
                d.polygon([f(a[u][0], a[u][2]), f(a[v][0], a[v][2]),
                           f(b[v][0], b[v][2]), f(b[u][0], b[u][2])], fill=shade)
    c = [f(n["pos"][0], n["pos"][2]) for n in t.nodes]
    d.line(c + ([c[0]] if t.loop else []), fill=(240, 220, 60), width=1)
    for o in t.objects:
        x, y = f(o["pos"][0], o["pos"][2])
        col = (80, 160, 255) if t.kinds[o["kind"]]["type"] == 1 else (255, 90, 90)
        d.ellipse([x - 2, y - 2, x + 2, y + 2], fill=col)
    x, y = c[0]
    d.ellipse([x - 6, y - 6, x + 6, y + 6], outline=(255, 255, 255), width=2)
    im.save(out)
    print("%s: %dx%d, %.2f pixels a unit" % (out, W2, H, k))


if __name__ == "__main__":
    what, files = sys.argv[1], sys.argv[2:]
    if what == "info":
        for p in files:
            info(Track(p), p)
    elif what == "nodes":
        nodes(Track(files[0]))
    elif what == "png":
        png(Track(files[0]), files[1])
    else:
        sys.exit(__doc__)
