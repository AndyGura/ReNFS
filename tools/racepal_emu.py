#!/usr/bin/env python3
"""The race's palette made by the port alone (the road's family, the cars' files, the
dashboard, the program's pieces; then the colours allowed and the inverse table, as
race_main 0x6356c does) against a snapshot's: the palette in the card (0x12ba30, 6 bits)
and the inverse table (0xf77c4, 32 KB). The cars are the snapshot's.

    build/venv/bin/python tools/racepal_emu.py cy1 build/snap/r1a.bin [...]
"""
import ctypes, os, struct, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from snap_emu import Snap
from camera_emu import Car, lib

U8 = ctypes.c_ubyte


class Car2(ctypes.Structure):
    _fields_ = Car._fields_ + [("police", ctypes.c_int), ("cfm", ctypes.c_void_p),
                               ("colours", ctypes.c_void_p)]


def check(path, name, L):
    s = Snap(path)
    n = s.i32(0x12da38)
    if not L.trackinfo_load(name.encode(), s.i32(0x12c340), s.i32(0x143e2c)):
        sys.exit("no track info")
    if not L.trackfam_load(name.encode(), s.i32(0x12c340), s.i32(0x12da12)):
        sys.exit("no track family")
    cars = []
    for k in range(n):
        a = s.u32(0x13c24c + 4 * k)
        c = Car2()
        c.player = k
        c.police = 1 if s.raw(a + 0x174, 1)[0] & 8 else 0
        cid = 400 if c.police else s.i32(0x12dce4 + 4 * k)
        if not L.car_load(ctypes.byref(c), cid):
            sys.exit("no car %d" % cid)
        cars.append(c)
    keep = (ctypes.POINTER(Car2) * n)(*[ctypes.pointer(c) for c in cars])
    marks = (U8 * n)()
    L.cars_queue_colours(keep, n, marks)
    car = s.read(0x12b990, 16).split(b"\0")[0]
    if not L.dash_load(car, s.i32(0x12c308)):
        sys.exit("no dashboard")
    L.dash_queue_colours()
    L.cockpit_queue_colours()
    L.pal_apply_pieces()
    L.race_colours(marks, n, s.i32(0x12da20))
    L.video_palette8.restype = ctypes.POINTER(U8 * 768)
    got = bytes(v >> 2 for v in L.video_palette8().contents)
    want = s.read(0x12ba30, 768)
    bad = [i for i in range(254) if got[3 * i:3 * i + 3] != want[3 * i:3 * i + 3]]
    inv = ctypes.cast(ctypes.c_void_p.in_dll(L, "g_inv").value, ctypes.POINTER(U8 * 32768)).contents
    winv = s.raw(s.u32(0xf77c4), 32768)
    ibad = sum(a != b for a, b in zip(bytes(inv), winv))
    for i, (a, b) in enumerate(zip(bytes(inv), winv)):
        if a != b and os.environ.get("SHOW"):
            print("  cell %d (r%d g%d b%d): the port %d %s, the original %d %s" % (
                i, i >> 10, i >> 5 & 31, i & 31, a, got[3 * a:3 * a + 3].hex(), b, want[3 * b:3 * b + 3].hex()))
    print("%s: palette %s; inverse table %s; marks %s" % (
        os.path.basename(path),
        "entries 0..253 equal" if not bad else "%d entries differ (%s)" % (len(bad), bad[:10]),
        "all 32768 cells equal" if not ibad else "%d cells differ" % ibad, list(marks)))
    L.pal_apply_remaps()
    for c in cars:
        L.car_free(ctypes.byref(c))
    L.dash_free()


def main():
    name, snaps = sys.argv[1], sys.argv[2:]
    L = lib()
    L.files_init(b"NFSSE", None)
    if not L.exe_load():
        sys.exit("no NFS_DOS.EXE")
    for p in snaps:
        check(p, name, L)


if __name__ == "__main__":
    main()
