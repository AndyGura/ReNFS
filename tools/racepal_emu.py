#!/usr/bin/env python3
"""The race's palette made by the port alone (the road's family, the cars' files, the
dashboard, the program's pieces; then the colours allowed and the inverse table, as
race_main 0x6356c does) against a snapshot's: the palette in the card (0x12ba30, 6 bits)
and the inverse table (0xf77c4, 32 KB); then the pictures re-coloured (the dashboard's,
the cars' 0x65020, the queued banks 0x55fa8), every car's near and far pictures against
the snapshot's byte for byte. The cars are the snapshot's.

    build/venv/bin/python tools/racepal_emu.py cy1 build/snap/r1a.bin [...]
"""
import ctypes, os, struct, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from snap_emu import Snap
from camera_emu import Car, lib

U8 = ctypes.c_ubyte


class Car2(ctypes.Structure):
    _fields_ = Car._fields_ + [("police", ctypes.c_int), ("cfm", ctypes.c_void_p),
                               ("cfm_len", ctypes.c_long), ("colours", ctypes.c_void_p),
                               ("pics", ctypes.c_void_p * 2), ("pics_pal", ctypes.c_void_p * 2),
                               ("rest", ctypes.c_ubyte * 4096)]   # what drawing keeps (car.h)


def race_track(s, name, L):
    """the track's info and family, before the cars (the info sets the tint they get)"""
    if not L.trackinfo_load(name.encode(), s.i32(0x12c340), s.i32(0x143e2c)):
        sys.exit("no track info")
    if not L.trackfam_load(name.encode(), s.i32(0x12c340), s.i32(0x12da12)):
        sys.exit("no track family")


def race_palette(s, name, L, cars):
    """the race's palette made as race_main does, for the snapshot's race and the port's
    cars (pointers to them, in the snapshot's order, loaded after race_track); returns
    them as an array and the marks"""
    n = len(cars)
    keep = (ctypes.c_void_p * n)(*cars)
    marks = (U8 * n)()
    L.cars_queue_colours(keep, n, marks)
    car = s.read(0x12b990, 16).split(b"\0")[0]
    number = struct.unpack("<i", s.raw(s.u32(0x13c24c + 4 * s.i32(0x12dd58)) + 0x48a, 4))[0]
    if not L.dash_load(car, number, s.i32(0x12c308)):
        sys.exit("no dashboard")
    L.dash_queue_colours()
    if not L.hud_load():       # (load_cockpit_mask 0x5f7c8: its colours and the program's)
        sys.exit("no HUD")
    L.pal_apply_pieces()
    L.race_colours(marks, n, s.i32(0x12da20))
    L.needle_colours(number)
    return keep, marks


def race_pictures(s, L, keep, marks):
    """then the pictures (race_colours' end 0x2e866, 0x65020, 0x55fa8)"""
    dash = ctypes.addressof(ctypes.c_void_p.in_dll(L, "g_dash"))
    bank, pal = ctypes.c_void_p.from_address(dash), ctypes.c_void_p.from_address(dash + 16)
    L.pal_remap(bank, pal)
    L.pal_tint(0, 0x100, pal)
    L.cars_remap(keep, len(keep), marks, s.i32(0x12dd08))
    L.hud_colours()
    L.shade_make(0x9999)
    L.pal_apply_remaps()


def check(path, name, L):
    s = Snap(path)
    n = s.i32(0x12da38)
    race_track(s, name, L)
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
    keep, marks = race_palette(s, name, L, [ctypes.addressof(c) for c in cars])
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
    race_pictures(s, L, keep, marks)
    pbad = []
    for k, c in enumerate(cars):
        a = s.u32(0x13c24c + 4 * k)
        for e, at in ((0, 0x51e), (1, 0x656)):
            bank = c.pics[e]
            nsh = struct.unpack("<I", ctypes.string_at(bank + 8, 4))[0]
            end = max(struct.unpack("<I", ctypes.string_at(bank + 20 + 8 * i, 4))[0] for i in range(nsh))
            sh = ctypes.string_at(bank + end, 16)
            end += 16 + struct.unpack("<HH", sh[4:8])[0] * struct.unpack("<HH", sh[4:8])[1]
            theirs = s.raw(struct.unpack("<I", s.raw(a + at, 4))[0], end)
            d = sum(x != y for x, y in zip(ctypes.string_at(bank, end), theirs))
            if d:
                pbad.append("car %d %s: %d bytes" % (k, "near far".split()[e], d))
    print("   pictures: %s" % ("; ".join(pbad) if pbad else "every car's near and far equal"))
    mask = ctypes.c_void_p.in_dll(L, "g_mask").value
    n = struct.unpack("<I", ctypes.string_at(mask + 4, 4))[0]
    print("   the HUD's pictures: %s" % ("equal" if ctypes.string_at(mask, n) == s.raw(s.u32(0x145604), n)
                                         else "differ"))
    shade = bytes((U8 * 256).in_dll(L, "g_shade"))
    marks = bytes((U8 * 8).in_dll(L, "g_marker_colour")) + struct.pack("<i", ctypes.c_int.in_dll(L, "g_marker_line").value)
    if marks != s.read(0x13d5e8, 8) + s.read(0x13d5c8, 4):
        print("%s: the markers' colours differ: %s, original %s" % (path, marks.hex(), (s.read(0x13d5e8, 8) + s.read(0x13d5c8, 4)).hex()))
    hud = list((ctypes.c_int * 17).in_dll(L, "g_hud_colour")) + [ctypes.c_int.in_dll(L, n).value for n in ("g_hud_slate", "g_map_frame")]
    theirs = [s.i32(0x13d5a4 + 4 * i) for i in range(17)] + [s.i32(0x12b9fc), s.i32(0x12ba08)]
    print("   the HUD's colours: %s" % ("equal" if hud == theirs else "differ: %s, original %s" % (hud, theirs)))
    needle = struct.pack("<i", ctypes.c_int.in_dll(L, "g_needle_colour").value)[:1] + bytes(
        (U8 * 4096).in_dll(L, "g_needle_blend"))
    rec = s.u32(0x12b5f4)
    theirs = s.raw(rec, 1) + s.raw(rec + 4, 4096)
    print("   the needles' colours: %s" % ("equal" if needle == theirs else "%d of 4097 differ" % sum(
        x != y for x, y in zip(needle, theirs))))
    want = s.read(0xf8118, 256)
    print("   shade table: %s" % ("equal" if shade == want else "%d entries differ" % sum(
        x != y for x, y in zip(shade, want))))
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
