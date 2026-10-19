#!/usr/bin/env python3
"""The points of the road and its objects gathered for the frame (0x4ffbc: the detail's
choice of each section's points, the objects' corners), the original's run on a snapshot
against the port's road_gather (port/src/road.c), from the same ring of sections.

    build/venv/bin/python tools/gather_emu.py cy1 build/snap/d2.bin [...]
    ... --fallen cy1 ...    the same with every other object knocked over, in both
"""
import ctypes, os, struct, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from snap_emu import Snap
from road_emu import Road, ROAD_SECTIONS


class TrackHead(ctypes.Structure):
    """track_state's first fields (port/src/track.h)"""
    _fields_ = [("file", ctypes.c_void_p), ("len", ctypes.c_long), ("head", ctypes.c_void_p),
                ("nodes", ctypes.c_void_p), ("speeds", ctypes.c_void_p), ("objects", ctypes.c_void_p),
                ("placements", ctypes.c_void_p), ("nplacements", ctypes.c_int),
                ("fallen", ctypes.c_void_p)]


def lib():
    out = "build/gather_test.so"
    subprocess.check_call(["cc", "-O1", "-shared", "-fPIC", "-o", out, "port/src/road.c",
                           "port/src/poly.c", "port/src/raster.c", "port/src/track.c",
                           "port/src/files.c", "port/src/exe.c", "-Iport/src"])
    return ctypes.CDLL(os.path.abspath(out))


def ints(b):
    return list(struct.unpack("<%di" % (len(b) // 4), b))


def quarter(n):
    return n // 4 if n >= 0 else -((-n) // 4)


def check(path, name, L, knock):
    s = Snap(path)
    if not L.track_load(name.encode()):
        sys.exit("no track")
    cam = s.u32(0x143df4)
    L.track_keep_sections(quarter(struct.unpack("<i", s.raw(cam + 0x38, 4))[0]))
    node, drawn = s.i32(0x136610), s.i32(0x136618)
    npl = s.i32(0x1365dc)
    fallen = s.raw(s.u32(0xea5d8), 2 * npl)
    if knock:
        fallen = bytes(1 if i % 4 == 0 else b for i, b in enumerate(fallen))
        s.mu.mem_write(s.u32(0xea5d8), fallen)
    track = TrackHead.in_dll(L, "g_track")
    road = Road.in_dll(L, "g_road")
    ctypes.memset(ctypes.addressof(road), 0, ctypes.sizeof(road))
    ctypes.memmove(track.fallen, fallen, len(fallen))
    if track.nplacements != npl:
        print("placements: port %d, original %d" % (track.nplacements, npl))
    s.call(0x4ffbc, eax=node & 0xffffffff, edx=quarter(node) & 0xffffffff, ebx=drawn)
    L.road_gather(node, quarter(node), drawn)
    n, xc = s.i32(0x13bb78), s.i32(0x1365c0)
    bad = []

    def cmp(what, want, got):
        d = sum(a != b for a, b in zip(want, got)) + abs(len(want) - len(got))
        if d:
            bad.append("%s: %d of %d differ" % (what, d, len(want)))
    cmp("counts", [n, s.i32(0x13bb74), xc, s.i32(0x1365c4), s.i32(0x1365d8), s.i32(0x1365cc),
                   s.i32(0x1365d0), s.i32(0x1365d4), s.i32(0x13660c)],
        [road.count, road.fill, road.xcount, road.xfill, road.xsets, road.xfirst, road.xnext,
         road.xnode, road.base])
    cmp("world 0x137434", ints(s.read(0x137434, 12 * n)),
        [c for p in road.world[:road.count] for c in (p.x, p.y, p.z)])
    cmp("xworld 0x12f540", ints(s.read(0x12f540, 12 * xc)),
        [c for p in road.xworld[:road.xcount] for c in (p.x, p.y, p.z)])
    sec = s.read(0x136620, 6 * ROAD_SECTIONS)
    want = [struct.unpack_from("<Bxhh", sec, 6 * i) for i in range(ROAD_SECTIONS)]
    got = [(e.kind, e.a, e.points) for e in road.sec]
    used = [i for i in range(ROAD_SECTIONS) if got[i] != (0, 0, 0)]
    cmp("sections 0x136620", [want[i] for i in used], [got[i] for i in used])
    print("%s: node %d, %d sections: %d road points, %d object points (%d objects); %s" % (
        os.path.basename(path), node, drawn, n, xc, s.i32(0x1365d8),
        "; ".join(bad) if bad else "all equal"))


if __name__ == "__main__":
    L = lib()
    L.files_init(b"NFSSE", None)
    if not L.exe_load():
        sys.exit("no NFS_DOS.EXE")
    args = sys.argv[1:]
    knock = "--fallen" in args
    args = [a for a in args if a != "--fallen"]
    for p in args[1:]:
        check(p, args[0], L, knock)
