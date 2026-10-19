#!/usr/bin/env python3
"""The original's road transform (0x59a74: into the camera's view, held off the near
plane, projected) run on a snapshot, against the port's road_transform (port/src/road.c)
given the same road, camera and view: every point in all four arrays must agree.

    build/venv/bin/python tools/road_emu.py build/snap/d2.bin [...]
"""
import ctypes, os, struct, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from snap_emu import Snap
from poly_emu import View, Vec3

ROAD_POINTS, ROAD_SECTIONS, EXTRA_POINTS = 515, 600, 900


BEHIND_POINTS = 55


class Scr2(ctypes.Structure):
    _fields_ = [("x", ctypes.c_int), ("y", ctypes.c_int)]


class Section(ctypes.Structure):
    _fields_ = [("kind", ctypes.c_ubyte), ("behind", ctypes.c_ubyte), ("a", ctypes.c_short),
                ("points", ctypes.c_short)]


class Road(ctypes.Structure):
    _fields_ = [("sec", Section * ROAD_SECTIONS), ("sec_mask", ctypes.c_uint), ("drawn", ctypes.c_int),
                ("count", ctypes.c_int), ("world", Vec3 * ROAD_POINTS), ("view", Vec3 * ROAD_POINTS),
                ("near", Vec3 * ROAD_POINTS), ("screen", Scr2 * ROAD_POINTS),
                ("xsets", ctypes.c_int), ("xcount", ctypes.c_int), ("xworld", Vec3 * EXTRA_POINTS),
                ("xmoved", Vec3 * EXTRA_POINTS), ("xview", Vec3 * EXTRA_POINTS),
                ("xscreen", Scr2 * EXTRA_POINTS), ("fill", ctypes.c_int), ("xfill", ctypes.c_int),
                ("base", ctypes.c_int), ("xfirst", ctypes.c_int), ("xnext", ctypes.c_int),
                ("xnode", ctypes.c_int), ("xcursor", ctypes.c_int), ("bcount", ctypes.c_int),
                ("bworld", Vec3 * BEHIND_POINTS), ("bview", Vec3 * BEHIND_POINTS),
                ("bscreen", Scr2 * BEHIND_POINTS)]


def lib():
    out = "build/road_test.so"
    subprocess.check_call(["cc", "-O1", "-shared", "-fPIC", "-o", out, "port/src/road.c",
                           "port/src/poly.c", "port/src/raster.c", "port/src/track.c",
                           "port/src/files.c", "port/src/exe.c", "-Iport/src"])
    return ctypes.CDLL(os.path.abspath(out))


def ints(b):
    return list(struct.unpack("<%di" % (len(b) // 4), b))


def check(path, L):
    s = Snap(path)
    n, xsets, xcount = s.i32(0x13bb78), s.i32(0x1365d8), s.i32(0x1365c0)
    cam = s.u32(0x143df4)
    pos = struct.unpack("<3i", s.raw(cam + 0x10, 12))
    node = struct.unpack("<i", s.raw(cam + 0x38, 4))[0]
    m = struct.unpack("<9i", s.raw(s.u32(0x143df8), 36))
    road = Road.in_dll(L, "g_road")
    ctypes.memset(ctypes.addressof(road), 0, ctypes.sizeof(road))
    sec = s.read(0x136620, 6 * ROAD_SECTIONS)
    for i in range(ROAD_SECTIONS):
        k, _, a, p = struct.unpack_from("<BBhh", sec, 6 * i)
        road.sec[i].kind, road.sec[i].a, road.sec[i].points = k, a, p
    road.sec_mask, road.drawn, road.count = s.u32(0x13bb90), s.i32(0x136618), n
    w = ints(s.read(0x137434, 12 * n))
    for i in range(n):
        road.world[i] = Vec3(*w[3 * i:3 * i + 3])
    road.xsets, road.xcount = xsets, xcount
    w = ints(s.read(0x12f540, 12 * xcount))
    for i in range(xcount):
        road.xworld[i] = Vec3(*w[3 * i:3 * i + 3])
    v = View.in_dll(L, "g_view")
    v.cx, v.cy, v.min_z = s.i32(0xf7a28), s.i32(0xf7a2c), s.i32(0xf7938)
    for i, r in enumerate(ints(s.raw(s.u32(0xf7a14), 4096))):
        v.recip[i] = r
    v.far_shift = s.read(0xf6aa4, 1)[0]

    s.call(0x59a74)
    near = L.road_transform(ctypes.byref(Vec3(*pos)), (ctypes.c_int * 9)(*m), node)
    bad = []

    def cmp(name, want, got):
        d = sum(a != b for a, b in zip(want, got))
        if d:
            bad.append("%s: %d of %d differ" % (name, d, len(want)))
    flat3 = lambda arr, k: [c for p in arr[:k] for c in (p.x, p.y, p.z)]
    flat2 = lambda arr, k: [c for p in arr[:k] for c in (p.x, p.y)]
    cmp("view 0x138c58", ints(s.read(0x138c58, 12 * n)), flat3(road.view, n))
    cmp("near 0x142578", ints(s.read(0x142578, 12 * n)), flat3(road.near, n))
    cmp("screen 0x13a47c", ints(s.read(0x13a47c, 8 * n)), flat2(road.screen, n))
    if xsets > 0:
        cmp("xview 0x131f70", ints(s.read(0x131f70, 12 * xcount)), flat3(road.xview, xcount))
        cmp("xscreen 0x1349a0", ints(s.read(0x1349a0, 8 * xcount)), flat2(road.xscreen, xcount))
    cmp("clip 0x143e54", [s.i32(0x143e54), s.i32(0x143e58), s.i32(0x143e5c)], [v.clip_x, v.clip_y, v.clip_z])
    cmp("near plane", [struct.unpack("<i", s.raw(s.u32(0x143dfc) + 4, 4))[0]], [near])
    print("%s: %d road points, %d others (%d sets); %s" % (
        os.path.basename(path), n, xcount, xsets, "; ".join(bad) if bad else "all equal"))


if __name__ == "__main__":
    L = lib()
    for p in sys.argv[1:]:
        check(p, L)
