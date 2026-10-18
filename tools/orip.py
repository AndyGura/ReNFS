#!/usr/bin/env python3
"""EA's 3D models ("ORIP") and the "wwww" containers that hold them (.CFM cars, .FAM
track objects).

    python3 -I tools/orip.py list <file.cfm|file.fam>          the containers and models
    python3 -I tools/orip.py png  <file> <out.png> [n...]  every model (or the n-th), textured:
                                       8 sides for one model, 4 for several

A container (read by 0x60134): "wwww", u32 count, count x u32 offsets from its start
(0 = empty), entries nest. A car (.CFM): [0] ORIP + [1] its SHPI textures (high detail),
[2] + [3] (low detail).

ORIP (made usable by 0x978a8, drawn by 0x98482 through 0x9868a; NOTES §3.5), offsets from
the "ORIP":
  +4 size, +8 0x2bc, +0x10 points, +0x14 points of the body (the rest: wheels?),
  +0x18 the points: i32 x, y (up), z (forward) in 1/128 units,
  +0x1c / +0x20 texture positions: u32 u, v in pixels,
  +0x24 / +0x28 polygons of 12 bytes: u32 flags, u32 + 4 index the corners' points and
        texture positions in the index table (+0x50, u32 each, counted in u32s),
  +0x38 / +0x3c textures of 20 bytes: 8 bytes (a part's name or 0), the SHPI name (4),
  +0x40 / +0x44 named point groups (20 bytes: 8-char name, count, first, ...),
  +0x48 / +0x4c named polygon groups (28 bytes: name, ...), +0x50 the index table,
  +0x54 / +0x58 and +0x5c / +0x60 named points (12 bytes: 8-char name, u32 point).
The polygon flags (0x98482): bits 0-2 corners (3/4), 0x80 textured, bits 3-6 the drawing
routine, 0x100 / 0x400 both sides, 0x200 the other side is the front, 0x800 draw the next
one too, 0x1000 the texture positions are used (else the whole picture goes on corners 0-3 =
top left, top right, bottom right, bottom left: 0x91b90), 0x2000 another mode, bits 16-23
the texture. Colour 255 is see-through (magenta in the palettes). A texture with a part's
name and no picture name (the wheels: "frntires", "rertires") is filled in by the game.
"""
import math, os, struct, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fsh


def container(d, off=0):
    """the entries of a wwww at off: (offset, data kind) a slot, None when empty"""
    n = struct.unpack_from("<I", d, off + 4)[0]
    return [None if not o else off + o for o in struct.unpack_from("<%dI" % n, d, off + 8)]


def kind(d, off):
    return d[off:off + 4].decode("latin1")


class Model:
    def __init__(self, d, off):
        assert d[off:off + 4] == b"ORIP"
        u = lambda a: struct.unpack_from("<I", d, off + a)[0]
        self.size = u(4)
        npts, self.body = u(0x10), u(0x14)
        self.pts = [struct.unpack_from("<3i", d, off + u(0x18) + 12 * i) for i in range(npts)]
        self.uv = [struct.unpack_from("<2I", d, off + u(0x20) + 8 * i) for i in range(u(0x1C))]
        idx = off + u(0x50)
        ix = lambda k: struct.unpack_from("<I", d, idx + 4 * k)[0]
        self.polys = []
        for i in range(u(0x24)):
            flags, pv, pt = struct.unpack_from("<3I", d, off + u(0x28) + 12 * i)
            n = flags & 7
            self.polys.append(dict(flags=flags, n=n, tex=(flags >> 16) & 0xFF,
                                   pts=[ix(pv + k) for k in range(n)],
                                   uv=[ix(pt + k) for k in range(n)]))
        self.tex = []
        for i in range(u(0x38)):
            r = d[off + u(0x3C) + 20 * i:off + u(0x3C) + 20 * i + 20]
            self.tex.append((r[:8].rstrip(b"\0").decode("latin1"), r[8:12].decode("latin1"),
                             r[12:20].hex()))
        name = lambda r: r.split(b"\0")[0].decode("latin1")
        self.groups = [(name(d[off + u(0x44) + 20 * i:][:8]),) + struct.unpack_from(
            "<3I", d, off + u(0x44) + 20 * i + 8) for i in range(u(0x40))]
        self.polygroups = [(name(d[off + u(0x4C) + 28 * i:][:12]),) + struct.unpack_from(
            "<4I", d, off + u(0x4C) + 28 * i + 12) for i in range(u(0x48))]
        self.named = [(name(d[off + a + 12 * i:][:8]), struct.unpack_from(
            "<I", d, off + a + 12 * i + 8)[0]) for c, a in ((u(0x54), u(0x58)), (u(0x5C), u(0x60)))
            for i in range(c)]


def textures(d, off):
    """an SHPI at off: name -> (w, h, pixels, palette)"""
    s = d[off:]
    pal = None
    out = {}
    for name, so in fsh.shapes(s):
        for t, bo in fsh.blocks(s, so):
            if t in (0x22, 0x24, 0x2A, 0x2D) and name.upper() == "!PAL":
                pal = fsh.palette_of(s, bo)
            elif t == 0x7B:
                w, h = struct.unpack_from("<HH", s, bo + 4)
                out[name] = (w, h, s[bo + 16:bo + 16 + w * h])
    return out, pal


def render(m, tex, pal, yaw, pitch, W, H):
    import numpy as np
    img = np.zeros((H, W, 3), np.uint8); img[:] = (40, 40, 48)
    zb = np.full((H, W), np.inf)
    cy, sy, cp, sp = math.cos(yaw), math.sin(yaw), math.cos(pitch), math.sin(pitch)
    r = max(max(abs(c) for c in p) for p in m.pts) * 1.15
    view = []
    for x, y, z in m.pts:
        x1, z1 = x * cy - z * sy, x * sy + z * cy           # turn round the up axis
        y2, z2 = y * cp - z1 * sp, y * sp + z1 * cp          # then tilt
        view.append((x1, y2, z2))
    k = min(W, H) / 2 / r
    scr = [(W / 2 + x * k, H / 2 - y * k, z) for x, y, z in view]
    for p in m.polys:
        if p["n"] < 3:
            continue
        t = m.tex[p["tex"]][1] if p["tex"] < len(m.tex) else None
        if t is not None and not t.strip("\0 ") and m.tex[p["tex"]][0].endswith("tires"):
            t = "tyr1"          # the wheels: filled in by the game as they turn (tyr1..4)
        T = tex.get(t)
        for a, b, c in [(0, i, i + 1) for i in range(1, p["n"] - 1)]:
            P = [scr[p["pts"][j]] for j in (a, b, c)]
            if p["flags"] & 0x1000:     # its own texture positions (0xab105)
                UV = [m.uv[p["uv"][j]] for j in (a, b, c)]
            else:                       # the whole picture on the corners (0x91b90)
                w, h = T[:2] if T else (1, 1)
                C = [(0, 0), (w, 0), (w, h), (0, h)]
                UV = [C[j % 4] for j in (a, b, c)]
            xs = [q[0] for q in P]; ys = [q[1] for q in P]
            x0, x1 = max(int(min(xs)), 0), min(int(max(xs)) + 1, W - 1)
            y0, y1 = max(int(min(ys)), 0), min(int(max(ys)) + 1, H - 1)
            if x0 > x1 or y0 > y1:
                continue
            gx, gy = np.meshgrid(np.arange(x0, x1 + 1) + .5, np.arange(y0, y1 + 1) + .5)
            (ax, ay, az), (bx, by, bz), (qx, qy, qz) = P
            den = (by - qy) * (ax - qx) + (qx - bx) * (ay - qy)
            if abs(den) < 1e-9:
                continue
            l0 = ((by - qy) * (gx - qx) + (qx - bx) * (gy - qy)) / den
            l1 = ((qy - ay) * (gx - qx) + (ax - qx) * (gy - qy)) / den
            l2 = 1 - l0 - l1
            ins = (l0 >= 0) & (l1 >= 0) & (l2 >= 0)
            z = l0 * az + l1 * bz + l2 * qz
            sub = zb[y0:y1 + 1, x0:x1 + 1]
            ok = ins & (z < sub)
            if not ok.any():
                continue
            if T and p["flags"] & 0x80:
                w, h, px = T
                u = (l0 * UV[0][0] + l1 * UV[1][0] + l2 * UV[2][0]).astype(int).clip(0, w - 1)
                v = (l0 * UV[0][1] + l1 * UV[1][1] + l2 * UV[2][1]).astype(int).clip(0, h - 1)
                ci = np.frombuffer(px, np.uint8).reshape(h, w)[v, u]
                col = np.array(pal, np.uint8)[ci]
                ok &= ci != 255         # colour 255 is see-through
            else:
                col = np.zeros(gx.shape + (3,), np.uint8); col[:] = (200, 60, 200)
            sub[ok] = z[ok]
            img[y0:y1 + 1, x0:x1 + 1][ok] = col[ok]
    return img


def main():
    what, path = sys.argv[1], sys.argv[2]
    d = open(path, "rb").read()
    if what == "list":
        def walk(off, depth):
            k = kind(d, off)
            if k == "wwww":
                es = container(d, off)
                print("  " * depth + "wwww %d" % len(es))
                for e in es:
                    if e is None:
                        print("  " * (depth + 1) + "-")
                    else:
                        walk(e, depth + 1)
            elif k == "ORIP":
                m = Model(d, off)
                print("  " * depth + "ORIP %d points (%d body), %d polygons, textures %s" % (
                    len(m.pts), m.body, len(m.polys), " ".join(t[1] for t in m.tex)))
                if m.groups or m.named:
                    print("  " * depth + "  groups %s; polygon groups %s; points %s" % (
                        [g[0] for g in m.groups], [g[0] for g in m.polygroups],
                        [n[0] for n in m.named]))
            elif k == "SHPI":
                print("  " * depth + "SHPI %s" % " ".join(n for n, _ in fsh.shapes(d[off:])))
            else:
                print("  " * depth + repr(d[off:off + 4]))
        walk(0, 0)
    elif what == "png":
        import numpy as np
        from PIL import Image
        pairs = []                      # every ORIP with the SHPI after it, at any depth

        def walk(off):
            es = container(d, off)
            for i, e in enumerate(es):
                if e is None:
                    continue
                if kind(d, e) == "wwww":
                    walk(e)
                elif kind(d, e) == "ORIP" and i + 1 < len(es) and es[i + 1] is not None:
                    pairs.append((e, es[i + 1]))
        walk(0)
        only = [int(a) for a in sys.argv[4:]]
        if only:
            pairs = [pairs[i] for i in only]
        W, H = 320, 240
        views = 8 if len(pairs) == 1 else 4
        sheet = np.zeros((H * len(pairs) * (views // 4), 4 * W, 3), np.uint8)
        for r, (mo, so) in enumerate(pairs):
            m = Model(d, mo)
            tex, pal = textures(d, so)
            for i in range(views):
                img = render(m, tex, pal, math.radians(360 / views * i), math.radians(-20), W, H)
                y = (r * (views // 4) + i // 4) * H
                sheet[y:y + H, (i % 4) * W:(i % 4 + 1) * W] = img
            print("model %d: %d points, %d polygons, textures %s" % (
                r, len(m.pts), len(m.polys), " ".join(t[1] for t in m.tex)))
        Image.fromarray(sheet).save(sys.argv[3])
        print("%s: %d models, %d sides each, from 20 degrees above" % (sys.argv[3], len(pairs), views))
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
