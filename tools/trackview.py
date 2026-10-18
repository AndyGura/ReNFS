#!/usr/bin/env python3
"""A view along an NFS SE track from its own files: the road (.TRI points and strips,
the family's textures), the flat roadside objects and the 3D models (.FAM).

    python3 -I tools/trackview.py <NFSSE dir> <track, e.g. CY1> <node> <out.png>
            [across=0] [eye=1.0] [fov=55] [far=60 sections]

A check of what NOTES §3.2-3.5 say about the formats, against a screenshot of the original
at the same place. The camera (eye height, field of view) is a guess here, not the game's.

The road (draw_road_sections 0x5aa84, the quads at 0xf4880): a section's rows 0..3 (one a
node) of 11 points; strip s (header byte 2 + s) lies between points s and s+1 (s = 0..4,
one side) or 0/6, 6/7 .. 9/10 (s = 5..9, the other). The inner strips (0-2, 5-7) go row by
row; the outer ones (3, 4, 8, 9) over two rows. A strip's texture t is the family's bank
t / 3, picture "A000" / "B000" / "C000" by t % 3 ("..04" far away); the picture's width
runs along the road over two rows, its height across the strip (top = outer side).
"""
import math, os, struct, sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fsh, orip, tri

W, H = 640, 400


class Canvas:
    def __init__(self, fov, eye):
        self.img = np.zeros((H, W, 3), np.uint8)
        self.img[:H // 2] = (120, 160, 210)
        self.img[H // 2:] = (60, 70, 60)
        self.zb = np.full((H, W), np.inf)
        self.f = (W / 2) / math.tan(math.radians(fov) / 2)

    def poly(self, cam, P, UV, tex):
        """P: world points, UV: texture coordinates in pixels, tex (w, h, pixels, pal)"""
        V = [cam(p) for p in P]
        near = 0.3
        pts = list(zip(V, UV))
        out = []                                    # clip against the near plane
        for i in range(len(pts)):
            (a, ua), (b, ub) = pts[i], pts[(i + 1) % len(pts)]
            if a[2] >= near:
                out.append((a, ua))
            if (a[2] >= near) != (b[2] >= near):
                k = (near - a[2]) / (b[2] - a[2])
                out.append((tuple(a[j] + k * (b[j] - a[j]) for j in range(3)),
                            tuple(ua[j] + k * (ub[j] - ua[j]) for j in range(2))))
        if len(out) < 3:
            return
        S = [(W / 2 + v[0] * self.f / v[2], H / 2 - v[1] * self.f / v[2], v[2], uv)
             for v, uv in out]
        for i in range(1, len(S) - 1):
            self.tri(S[0], S[i], S[i + 1], tex)

    def tri(self, A, B, C, tex):
        xs = [A[0], B[0], C[0]]; ys = [A[1], B[1], C[1]]
        x0, x1 = max(int(min(xs)), 0), min(int(max(xs)) + 1, W - 1)
        y0, y1 = max(int(min(ys)), 0), min(int(max(ys)) + 1, H - 1)
        if x0 > x1 or y0 > y1:
            return
        gx, gy = np.meshgrid(np.arange(x0, x1 + 1) + .5, np.arange(y0, y1 + 1) + .5)
        den = (B[1] - C[1]) * (A[0] - C[0]) + (C[0] - B[0]) * (A[1] - C[1])
        if abs(den) < 1e-9:
            return
        l0 = ((B[1] - C[1]) * (gx - C[0]) + (C[0] - B[0]) * (gy - C[1])) / den
        l1 = ((C[1] - A[1]) * (gx - C[0]) + (A[0] - C[0]) * (gy - C[1])) / den
        l2 = 1 - l0 - l1
        ins = (l0 >= 0) & (l1 >= 0) & (l2 >= 0)
        iz = l0 / A[2] + l1 / B[2] + l2 / C[2]      # perspective: interpolate 1/z, u/z, v/z
        z = 1 / np.where(iz > 0, iz, 1e-9)
        sub = self.zb[y0:y1 + 1, x0:x1 + 1]
        ok = ins & (z < sub)
        if not ok.any():
            return
        w, h, px, pal = tex
        u = (l0 * A[3][0] / A[2] + l1 * B[3][0] / B[2] + l2 * C[3][0] / C[2]) * z
        v = (l0 * A[3][1] / A[2] + l1 * B[3][1] / B[2] + l2 * C[3][1] / C[2]) * z
        u = np.mod(u.astype(int), w); v = v.astype(int).clip(0, h - 1)
        ci = np.frombuffer(px, np.uint8).reshape(h, w)[v, u]
        ok &= ci != 255
        sub[ok] = z[ok]
        self.img[y0:y1 + 1, x0:x1 + 1][ok] = np.array(pal, np.uint8)[ci][ok]


def main():
    root, name, node, out = sys.argv[1:5]
    node = int(node)
    across = float(sys.argv[5]) if len(sys.argv) > 5 else 0.0
    eye = float(sys.argv[6]) if len(sys.argv) > 6 else 1.0
    fov = float(sys.argv[7]) if len(sys.argv) > 7 else 55.0
    far = int(sys.argv[8]) if len(sys.argv) > 8 else 60
    t = tri.Track(os.path.join(root, "SIMDATA", "MISC", name + ".TRI"))
    fam = open(os.path.join(root, "SIMDATA", "NTRACKFM", name + "_M01.FAM"), "rb").read()
    top = orip.container(fam, 0)
    banks = [orip.textures(fam, b) for b in orip.container(fam, top[0])]

    def road_tex(tn, far_away):
        texs, pal = banks[tn // 3]
        for nm in (("ABC"[tn % 3] + "004") if far_away else None, "ABC"[tn % 3] + "000"):
            if nm and nm in texs:
                return texs[nm] + (pal,)
        return None

    # the flat pictures: entry 1, one SHPI a picture or one SHPI of "%02d00"
    pics = []
    e1 = orip.container(fam, top[1])
    if len(e1) == 1:
        texs, pal = orip.textures(fam, e1[0])
        pics = [texs.get("%02d00" % i, None) and texs["%02d00" % i] + (pal,) for i in range(64)]
    else:
        for e in e1:
            texs, pal = orip.textures(fam, e)
            pics.append(texs.get("0000") and texs["0000"] + (pal,))
    models = []
    m3 = orip.container(fam, top[3])
    for e in m3:
        if e is None:
            models.append(None); continue
        pair = orip.container(fam, e)
        models.append((orip.Model(fam, pair[0]), orip.textures(fam, pair[1])))

    n = t.nodes[node]
    h = n["heading"] / 0x4000 * 2 * math.pi
    rx, rz = math.cos(h), -math.sin(h)               # the driver's right
    fx, fz = math.sin(h), math.cos(h)
    ex = n["pos"][0] + rx * across
    ez = n["pos"][2] + rz * across
    ey = n["pos"][1] + eye

    def cam(p):
        dx, dy, dz = p[0] - ex, p[1] - ey, p[2] - ez
        return (dx * rx + dz * rz, dy, dx * fx + dz * fz)

    cv = Canvas(fov, eye)
    s0 = node // 4
    for s in range(max(s0 - 2, 0), min(s0 + far, t.sections)):
        rows = t.section_points(s)
        nxt = t.section_points(s + 1)[0] if s + 1 < t.sections else None
        allrows = rows + ([nxt] if nxt else [])
        b = t.b
        o = t.slices_at + struct.unpack_from("<I", b, 0x2C + s * 4)[0]
        hdr = b[o + 12:o + 24]
        far_away = s - s0 > 12
        for strip in range(10):
            side = (0, 1, 2, 3, 4, 5) if strip < 5 else (0, 6, 7, 8, 9, 10)
            k = strip % 5
            pa, pb = side[k], side[k + 1]                # inner point, outer point
            tex = road_tex(hdr[2 + strip], far_away)
            if tex is None:
                continue
            tw, th = tex[0], tex[1]
            step = 1 if k < 3 else 2
            for r in range(0, len(allrows) - step, step):
                r0, r1 = allrows[r], allrows[r + step]
                if step == 1:
                    u0, u1 = (0, tw / 2) if r % 2 == 0 else (tw / 2, tw)
                else:
                    u0, u1 = 0, tw
                # corners as the game's quad: TL = later row outer, TR = row outer,
                # BR = row inner, BL = later row inner
                P = [r1[pb], r0[pb], r0[pa], r1[pa]]
                UV = [(u0, 0), (u1, 0), (u1, th), (u0, th)]
                cv.poly(cam, P, UV, tex)
    # objects
    lo, hi = s0 * 4 - 8, (s0 + far) * 4
    for o in t.objects:
        if not lo <= o["node"] < hi:
            continue
        kd = t.kinds[o["kind"]]
        x, y, z = o["pos"]
        nn = t.nodes[o["node"] % len(t.nodes)]
        a = -(o["angle"] / 256 + nn["heading"] / 0x4000) * 2 * math.pi
        if kd["type"] == 1:
            m = models[kd["pic"]] if kd["pic"] < len(models) else None
            if not m:
                continue
            mdl, (texs, pal) = m
            ca, sa = math.cos(a), math.sin(a)
            for p in mdl.polys:
                if p["n"] < 3 or not p["flags"] & 0x80:
                    continue
                tn = mdl.tex[p["tex"]][1]
                if tn not in texs:
                    continue
                tw, th, px = texs[tn]
                pts = []
                for i in p["pts"]:
                    mx, my, mz = (c / 128 for c in mdl.pts[i])
                    # the model's x/z turned by the object's angle (as the flat ones)
                    pts.append((x + mx * ca - mz * sa, y + my, z + mx * sa + mz * ca))
                if p["flags"] & 0x1000:
                    UV = [mdl.uv[i] for i in p["uv"]]
                else:
                    UV = [((0, 0), (tw, 0), (tw, th), (0, th))[j % 4] for j in range(p["n"])]
                cv.poly(cam, pts, UV, (tw, th, px, pal))
            continue
        pic = pics[kd["pic"] // 4] if kd["pic"] // 4 < len(pics) else None
        if not pic:
            continue
        hw = kd["width"] / 2
        c, s_ = hw * math.cos(a), hw * math.sin(a)
        P = [(x - c, y + kd["height"], z - s_), (x + c, y + kd["height"], z + s_),
             (x + c, y, z + s_), (x - c, y, z - s_)]
        # Corner 0 takes the picture's right edge: with (0, 0) there the original's "MALL"
        # sign on CY1 reads "LLAM" (the corners' places agree with its screen either way).
        flip = lambda p: [(p[0], 0), (0, 0), (0, p[1]), (p[0], p[1])]
        cv.poly(cam, P, flip(pic), pic)
        if kd["type"] == 6:
            a2 = a + math.pi / 2
            c2, s2 = kd["width2"] * math.cos(a2), kd["width2"] * math.sin(a2)
            q = P[3]
            P2 = [(q[0], y + kd["height"], q[2]), (q[0] + c2, y + kd["height"], q[2] + s2),
                  (q[0] + c2, y, q[2] + s2), q]
            pic2 = pics[kd["pic2"] // 4] if kd["pic2"] // 4 < len(pics) else None
            if pic2:
                cv.poly(cam, P2, flip(pic2), pic2)
    Image.fromarray(cv.img).save(out)
    print("%s: %s node %d (section %d), across %.1f, eye %.1f, fov %.0f" % (
        out, name, node, s0, across, eye, fov))


if __name__ == "__main__":
    main()
