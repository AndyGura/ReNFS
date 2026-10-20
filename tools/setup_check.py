#!/usr/bin/env python3
"""What the port sets up for a race against the original's memory in a snapshot taken
during one: the track's sky settings and tint (trackinfo.c), the family's textures and roadside
pictures (trackfam.c: which picture each slot holds, the see-through marks, the tinted
palettes, and the pixels after the re-colouring, done here with the original's own
inverse colour table of the race palette).

    build/venv/bin/python tools/setup_check.py cy1 build/snap/d2.bin [...]
"""
import ctypes, os, struct, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from snap_emu import Snap

SOURCES = ["trackinfo.c", "racepal.c", "exe.c", "files.c", "trackfam.c", "model.c", "roaddraw.c", "road.c", "poly.c", "raster.c", "view.c", "sky.c", "polyfill.c", "track.c", "font.c",
           "video.c", "fsh.c", "unpack.c", "config.c"]


class Info(ctypes.Structure):
    _fields_ = [(n, ctypes.c_int) for n in
                ("vert_near vert_far z_near z_far radius_near height_near radius_far height_far "
                 "colour_top colour_bottom ring_type ring_y ring_height").split()] + \
               [("tint", (ctypes.c_int * 5) * 3), ("last", ctypes.c_int), ("last_used", ctypes.c_int)]

INFO_AT = {"vert_near": 0x12ef44, "vert_far": 0x12ef48, "z_near": 0x12ef4c, "z_far": 0x12ef50,
           "radius_near": 0x12ef54, "height_near": 0x12ef58, "radius_far": 0x12ef5c,
           "height_far": 0x12ef60, "colour_top": 0x12ef64, "colour_bottom": 0x12ef68,
           "ring_type": 0x12ef6c, "ring_y": 0x12ef70, "ring_height": 0x12ef74,
           "last": 0x12efb4, "last_used": 0x12efc4}


def lib():
    out = "build/setup_test.so"
    subprocess.check_call(["cc", "-O1", "-shared", "-fPIC", "-o", out] +
                          ["port/src/" + f for f in SOURCES] + ["-Iport/src"] +
                          subprocess.check_output(["sdl2-config", "--cflags", "--libs"]).decode().split())
    return ctypes.CDLL(os.path.abspath(out))


def check(path, name, L):
    s = Snap(path)
    family, high = s.i32(0x12c340), s.i32(0x143e2c)
    bad = []
    if not L.trackinfo_load(name.encode(), family, high):
        sys.exit("no track info")
    info = Info.in_dll(L, "g_info")
    for f, a in INFO_AT.items():
        if getattr(info, f) != s.i32(a):
            bad.append("%s: %d, the original %d" % (f, getattr(info, f), s.i32(a)))
    for g in range(3):
        for k in range(5):
            if info.tint[g][k] != s.i32(0x12ef78 + 4 * g + 12 * k):
                bad.append("tint %d %d" % (g, k))
    print("%s: family %d: sky settings %s" % (os.path.basename(path), family,
                                               "; ".join(bad) if bad else "equal"))
    check_textures(s, name, family, L)


class Model(ctypes.Structure):
    """port/src/model.h's model"""
    _fields_ = [("orip", ctypes.c_void_p), ("lo", ctypes.c_void_p), ("hi", ctypes.c_void_p),
                ("npoints", ctypes.c_int), ("flags", ctypes.c_uint), ("m", ctypes.c_int * 9),
                ("pos", ctypes.c_int * 3), ("view", ctypes.c_void_p), ("screen", ctypes.c_void_p),
                ("count", ctypes.c_int), ("list", ctypes.POINTER(ctypes.c_uint)), ("tex", ctypes.c_void_p),
                ("ntex", ctypes.c_int)]


class Fam(ctypes.Structure):
    _fields_ = [("file", ctypes.c_void_p), ("len", ctypes.c_long), ("banks", ctypes.c_void_p),
                ("pictures", ctypes.c_void_p), ("horizon", ctypes.c_void_p),
                ("texture", (ctypes.c_void_p * 8) * 144), ("picture", (ctypes.c_void_p * 4) * 64),
                ("road_pal", ctypes.c_void_p), ("horizon_pal", ctypes.c_void_p),
                ("nmodels", ctypes.c_int), ("models", Model * 16)]


def check_textures(s, name, family, L):
    if not L.track_load(name.encode()) or not L.trackfam_load(name.encode(), family, s.i32(0x12da12)):
        sys.exit("no track family")
    fam = Fam.in_dll(L, "g_fam")
    base, mine = s.u32(0x145c50), fam.file
    off = lambda p, b: p - b if p else None
    bad = []
    want = struct.unpack("<%dI" % (144 * 8), s.read(0x140cd8, 144 * 8 * 4))
    got = [fam.texture[t][k] for t in range(144) for k in range(8)]
    d = sum(off(a, base) != off(b, mine) for a, b in zip(want, got))
    if d:
        bad.append("%d texture slots differ" % d)
    want = struct.unpack("<%dI" % (64 * 4), s.read(0x141ed8, 64 * 4 * 4))
    got = [fam.picture[t][k] for t in range(64) for k in range(4)]
    d = sum(off(a, base) != off(b, mine) for a, b in zip(want, got))
    if d:
        bad.append("%d roadside slots differ" % d)
    for what, p in (("road palette", fam.road_pal), ("horizon palette", fam.horizon_pal)):
        if ctypes.string_at(p, 768) != s.raw(base + p - mine, 768):
            bad.append(what + " differs")
    # the re-colouring through the original's inverse table
    inv = s.raw(s.u32(0xf77c4), 32768)
    keep = ctypes.create_string_buffer(inv, 32768)
    ctypes.c_void_p.in_dll(L, "g_inv").value = ctypes.addressof(keep)
    L.pal_apply_remaps()
    shapes = sorted(set(p for p in got + [fam.texture[t][k] for t in range(144) for k in range(8)] if p))
    hz = fam.horizon
    n = struct.unpack("<I", ctypes.string_at(hz + 8, 4))[0]
    for i in range(n):
        if ctypes.string_at(hz + 16 + 8 * i, 1) != b"!":
            shapes.append(hz + struct.unpack("<I", ctypes.string_at(hz + 20 + 8 * i, 4))[0])
    pix = 0
    for p in shapes:
        w, h = struct.unpack("<HH", ctypes.string_at(p + 4, 4))
        a, b = ctypes.string_at(p, 16 + w * h), s.raw(base + p - mine, 16 + w * h)
        if a != b:
            pix += 1
            if "-v" in sys.argv:
                print("      picture at +0x%x: %d of %d bytes differ" % (
                    p - mine, sum(x != y for x, y in zip(a, b)), len(a)))
    if pix:
        bad.append("%d of %d pictures differ" % (pix, len(shapes)))
    print("    textures: %d pictures; %s" % (len(shapes), "; ".join(bad) if bad else "all equal"))


if __name__ == "__main__":
    L = lib()
    L.files_init(b"NFSSE", None)
    if not L.exe_load():
        sys.exit("no NFS_DOS.EXE")
    for p in [a for a in sys.argv[2:] if a != "-v"]:
        check(p, sys.argv[1], L)
