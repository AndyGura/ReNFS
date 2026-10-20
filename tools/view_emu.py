#!/usr/bin/env python3
"""The race's views (port/src/view.c) against the original on snapshots: the windows
fitted to the player's car (0x14630's end, 0x145d0) against the snapshot's 0xe85c0; the
reciprocal table of the view ahead (0x14154) against the snapshot's; the camera's two
matrices (0x5868c, run in Unicorn) and the centre and lens of both views (0x1486c).

    build/venv/bin/python tools/view_emu.py build/snap/d2.bin [...]
"""
import ctypes, os, struct, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from snap_emu import Snap, game
from poly_emu import Vec3

SOURCES = ["view.c", "sky.c", "roaddraw.c", "trackinfo.c", "road.c", "poly.c", "raster.c", "track.c",
           "files.c", "exe.c", "trackfam.c", "model.c", "racepal.c", "font.c", "video.c", "fsh.c", "unpack.c", "config.c", "polyfill.c"]


def lib():
    out = "build/view_test.so"
    subprocess.check_call(["cc", "-O1", "-g", "-shared", "-fPIC", "-o", out] +
                          ["port/src/" + f for f in SOURCES] + ["-Iport/src"] +
                          subprocess.check_output(["sdl2-config", "--cflags", "--libs"]).decode().split())
    return ctypes.CDLL(os.path.abspath(out))


I = ctypes.c_int


class Window(ctypes.Structure):
    _fields_ = [(n, I) for n in ("id", "squash", "dx", "dy", "w", "h", "x", "y", "cw", "ch")]


class Rec(ctypes.Structure):
    _fields_ = [("fov", I), ("near", I), ("mode", I), ("win", ctypes.POINTER(Window))]


class Camera(ctypes.Structure):
    """race_camera (view.h); the car pointer left out of the test"""
    _fields_ = [("mode", I), ("wanted", I), ("frames", I), ("pos", Vec3), ("off", Vec3), ("node", I),
                ("flags", I), ("pitch", I), ("yaw", I), ("roll", I), ("tilt", I * 3), ("eye", Vec3),
                ("mirror_on", I), ("yaw_follow", I), ("step", Vec3), ("ahead", ctypes.POINTER(Rec)),
                ("mirror", ctypes.POINTER(Rec)), ("car", ctypes.c_void_p), ("m", I * 9), ("mm", I * 9)]


class View(ctypes.Structure):
    _fields_ = [("cx", I), ("cy", I), ("min_z", I), ("focal", I), ("recip", I * 0x400)]


def ints(b):
    return list(struct.unpack("<%di" % (len(b) // 4), b))


def check(path, L):
    s = Snap(path)
    bad = []
    def cmp(name, want, got):
        d = sum(a != b for a, b in zip(want, got))
        if d or len(want) != len(got):
            bad.append("%s: %d of %d differ" % (name, d, len(want)))
    car = s.u32(0x13c24c + 4 * s.i32(0x12dd58))
    spec = struct.unpack("<I", s.raw(car + 0x479, 4))[0]
    number = struct.unpack("<i", s.raw(car + 0x48a, 4))[0]
    horizon = struct.unpack("<i", s.raw(spec + 0x370, 4))[0]
    res = s.i32(0x143e2c)
    L.view_init(res, number, horizon)
    wins = (Window * 34).in_dll(L, "g_windows")
    cmp("windows 0xe85c0", ints(s.read(0xe85c0, 0x2a8 * 2)),
        [getattr(w, f) for w in wins for f, _ in Window._fields_])

    ahead, mirror = Rec.in_dll(L, "g_view_ahead"), Rec.in_dll(L, "g_view_mirror")
    rec = lambda a: ints(s.read(a, 32))
    L.view_setup(ctypes.byref(ahead), rec(0xe8de8)[4])
    L.view_setup(ctypes.byref(mirror), rec(0xe8e28)[4])
    L.view_setup(ctypes.byref(ahead), rec(0xe8de8)[4])
    v = View.in_dll(L, "g_view")
    cmp("reciprocals", ints(s.raw(s.u32(0xf7a14), 4096)), list(v.recip))

    cam = s.u32(0x143df4)
    c = Camera()
    c.pitch, c.yaw, c.roll = struct.unpack("<3i", s.raw(cam + 0x40, 12))
    c.ahead, c.mirror = ctypes.pointer(ahead), ctypes.pointer(mirror)
    ctypes.c_int.in_dll(L, "g_road_mirror").value = s.i32(0x12dd28)
    L.view_select.restype = ctypes.POINTER(I)
    for mirror_on in (0, 1):
        s2 = Snap(path)
        s2.call(0x1486c, eax=s2.u32(0x143df4), edx=mirror_on)
        L.view_select(ctypes.byref(c), mirror_on)
        cmp("centre and lens, %s" % ("mirror" if mirror_on else "ahead"),
            [s2.i32(0xf7a28), s2.i32(0xf7a2c), s2.i32(0xf7a30)], [v.cx, v.cy, v.focal])
        if not mirror_on:
            s2.call(0x5868c)
            L.view_matrix(ctypes.byref(c))
            cmp("matrix ahead", ints(s2.raw(cam + 0x94, 36)), list(c.m))
            cmp("matrix of the mirror", ints(s2.raw(cam + 0xb8, 36)), list(c.mm))
    print("%s: car %d, horizon %d; %s" % (os.path.basename(path), number, horizon,
                                          "; ".join(bad) if bad else "all equal"))


if __name__ == "__main__":
    L = lib()
    L.files_init(b"NFSSE", None)
    if not L.exe_load():
        sys.exit("no NFS_DOS.EXE")
    for p in sys.argv[1:]:
        check(p, L)
