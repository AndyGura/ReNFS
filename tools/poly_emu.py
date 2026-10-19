#!/usr/bin/env python3
"""The original's quad cutting (0x35348 the view, draw_quad 0x35b04 the mirror) and its
projection, caught inside a real frame on a snapshot (tools/snap_emu.py), against the port's
port/src/poly.c given the same quads: every piece painted, its picture and its corners on
the screen and in the picture, must agree in order.

    build/venv/bin/python tools/poly_emu.py build/snap/r1a.bin
"""
import ctypes, os, struct, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from snap_emu import Snap, game, flat
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_ESP, UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX,
                               UC_X86_REG_EDX)

VIEW, MIRROR, PAINT = 0x35348, 0x35b04, 0x34ed8
CALLERS = {VIEW: (0x5a4f0, 0x5a94d), MIRROR: (0x5aa84, 0x5ad44)}


class Vec3(ctypes.Structure):
    _fields_ = [("x", ctypes.c_int), ("y", ctypes.c_int), ("z", ctypes.c_int)]


class Tex(ctypes.Structure):
    _fields_ = [("pixels", ctypes.c_void_p), ("lo", ctypes.c_void_p), ("hi", ctypes.c_void_p),
                ("rowbytes", ctypes.c_int), ("w", ctypes.c_int), ("h", ctypes.c_int),
                ("see_through", ctypes.c_int)]


class View(ctypes.Structure):
    _fields_ = [("cx", ctypes.c_int), ("cy", ctypes.c_int), ("min_z", ctypes.c_int),
                ("focal", ctypes.c_int), ("recip", ctypes.c_int * 0x400), ("flags", ctypes.c_uint),
                ("clip_x", ctypes.c_int), ("clip_y", ctypes.c_int), ("clip_z", ctypes.c_int),
                ("far_shift", ctypes.c_int), ("max_level", ctypes.c_int)]


PAINT_FN = ctypes.CFUNCTYPE(None, ctypes.POINTER(Tex), ctypes.POINTER(ctypes.c_int),
                            ctypes.POINTER(ctypes.c_int))


def capture(path):
    """the top-level calls of both cutters in one frame, and what each painted"""
    s = Snap(path)
    mu = s.mu
    calls, depth = [], []

    def i32s(addr, n):
        return list(struct.unpack("<%di" % n, s.raw(addr, 4 * n)))

    def on_cut(kind):
        def h(mu, addr, size, _):
            sp = mu.reg_read(UC_X86_REG_ESP)
            ret, vindex, P, S, near = struct.unpack("<5I", s.raw(sp, 20))
            lo, hi = CALLERS[kind]
            if not lo <= (flat(ret) or 0) < hi:
                return
            pts = [i32s(p, 3) for p in struct.unpack("<4I", s.raw(P, 16))]
            shape = mu.reg_read(UC_X86_REG_EAX)
            calls.append(dict(kind=kind, shape=shape, wh=struct.unpack("<hh", s.raw(shape + 4, 4)),
                              ushift=mu.reg_read(UC_X86_REG_EDX), level=mu.reg_read(UC_X86_REG_EBX),
                              uindex=mu.reg_read(UC_X86_REG_ECX), vindex=vindex, pts=pts,
                              scr=i32s(S, 8), centre=(s.i32(0xf7a28), s.i32(0xf7a2c)),
                              focal=s.i32(0xf7a30),
                              recip=list(struct.unpack("<1024i", s.raw(s.u32(0xf7a14), 4096))),
                              win=[s.i32(a) for a in (0xf88a4, 0xf88a8, 0xf88ac, 0xf88b0)],
                              near=struct.unpack("<i", struct.pack("<I", near))[0],
                              painted=[]))
        return h

    def on_paint(mu, addr, size, _):
        if calls:
            uv, xy = mu.reg_read(UC_X86_REG_EDX), mu.reg_read(UC_X86_REG_EBX)
            calls[-1]["painted"].append((mu.reg_read(UC_X86_REG_EAX), tuple(i32s(uv, 8)),
                                         tuple(i32s(xy, 8))))
    for k in (VIEW, MIRROR):
        mu.hook_add(UC_HOOK_CODE, on_cut(k), None, game(k), game(k))
    mu.hook_add(UC_HOOK_CODE, on_paint, None, game(PAINT), game(PAINT))
    view = dict(cx=s.i32(0xf7a28), cy=s.i32(0xf7a2c), min_z=s.i32(0xf7938), focal=s.i32(0xf7a30),
                recip=list(struct.unpack("<1024i", s.raw(s.u32(0xf7a14), 4096))),
                clip=(s.i32(0x143e54), s.i32(0x143e58), s.i32(0x143e5c)),
                far_shift=s.read(0xf6aa4, 1)[0], max_level=s.i32(0xeaad8),
                win=[s.i32(a) for a in (0xf88a4, 0xf88a8, 0xf88ac, 0xf88b0)])
    s.call(0x5bc40)
    return view, calls


def main():
    view, calls = capture(sys.argv[1] if len(sys.argv) > 1 else "build/snap/r1a.bin")
    out = "build/poly_test.so"
    subprocess.check_call(["cc", "-O1", "-shared", "-fPIC", "-o", out, "port/src/poly.c",
                           "port/src/raster.c"])
    lib = ctypes.CDLL(os.path.abspath(out))
    v = View.in_dll(lib, "g_view")
    lib.view_set_focal(view["focal"])
    same_recip = list(v.recip) == view["recip"]
    v.cx, v.cy, v.min_z = view["cx"], view["cy"], view["min_z"]
    v.clip_x, v.clip_y, v.clip_z = view["clip"]
    v.far_shift, v.max_level = view["far_shift"], view["max_level"]
    lib.raster_window(None, 0, *view["win"])
    got = []

    def paint(t, uv, xy):
        got.append((t.contents.pixels, tuple(uv[i] for i in range(8)), tuple(xy[i] for i in range(8))))
    cb = PAINT_FN(paint)
    ctypes.c_void_p.in_dll(lib, "g_paint").value = ctypes.cast(cb, ctypes.c_void_p).value
    bad = pieces = 0
    for n, c in enumerate(calls):
        v.cx, v.cy = c["centre"]
        # each view (the road ahead, the mirror) has its own table (its record +0x14,
        # switched by 0x83ae0); the focal 0xf7a30 is the last one built
        if list(v.recip) != c["recip"]:
            for i, r in enumerate(c["recip"]):
                v.recip[i] = r
        lib.raster_window(None, 0, *c["win"])
        tex = Tex(c["shape"], 0, 0, 0, c["wh"][0], c["wh"][1], 0)
        pts = [Vec3(*p) for p in c["pts"]]
        P = (ctypes.POINTER(Vec3) * 4)(*[ctypes.pointer(p) for p in pts])
        S = (ctypes.c_int * 8)(*c["scr"])
        del got[:]
        fn = lib.quad_view if c["kind"] == VIEW else lib.quad_mirror
        fn(ctypes.byref(tex), c["ushift"], c["level"], c["uindex"], c["vindex"], P, S, c["near"])
        pieces += len(c["painted"])
        if got != c["painted"]:
            bad += 1
            if bad <= 5:
                print("call %d (%s): original %d pieces, port %d" % (
                    n, "view" if c["kind"] == VIEW else "mirror", len(c["painted"]), len(got)))
                for a, b in zip(c["painted"], got):
                    if a != b:
                        print("  original", a, "\n  port    ", b)
                        break
    print("focal table %s; %d view and %d mirror quads, %d pieces painted by the original; %d differ"
          % ("as the game's" if same_recip else "not the game's at the start (paused mid-mirror)", sum(c["kind"] == VIEW for c in calls),
             sum(c["kind"] == MIRROR for c in calls), pieces, bad))


if __name__ == "__main__":
    main()
