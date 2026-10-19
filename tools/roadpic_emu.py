#!/usr/bin/env python3
"""The road painted into the screen, pixel by pixel: the original's 0x597e4 and 0x5a4f0
run on a snapshot with its own painters (the cars, the walls and the 3D objects left
out on both sides), against the port's road_draw with its own (port/src/roaddraw.c,
poly.c, raster.c) given the same road points and view. Writes both pictures, and one of
the pixels that differ, to build/shots/road_<snapshot>_{orig,port,diff}.png.

    build/venv/bin/python tools/roadpic_emu.py cy1 build/snap/d2.bin [...]
"""
import ctypes, os, struct, sys, zlib
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP
from snap_emu import Snap, game
from roaddraw_emu import lib, setup, Painters
from poly_emu import View
from setup_check import Fam

W, H = 320, 200
LEFT_OUT = {0x679ec: 2, 0x6837c: 0, 0x59294: 3, 0x66738: 0}


def png(path, pix, pal):
    rows = b"".join(b"\0" + bytes(c for p in pix[y * W:(y + 1) * W] for c in pal[p]) for y in range(H))
    chunk = lambda t, d: struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d))
    open(path, "wb").write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", W, H, 8, 2, 0, 0, 0)) +
                           chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b""))


CLIP = (0, 0, 320, 127)          # the 3D view above the dashboard (some snapshots were taken
                                 # while the game drew into a smaller picture)
SCREEN, ROWS = 0x3800000, 0x3840000


def original(s, n, node):
    mu = s.mu
    mu.mem_map(SCREEN, 0x80000)
    mu.mem_write(ROWS, struct.pack("<%dI" % H, *[SCREEN + W * y for y in range(H)]))
    s.write(0xf88c4, struct.pack("<II", ROWS, 0))
    s.write(0xf88a4, struct.pack("<4i", *CLIP))
    base = SCREEN

    def hook(mu, addr, size, k):
        sp = mu.reg_read(UC_X86_REG_ESP)
        ret = struct.unpack("<I", bytes(mu.mem_read(sp, 4)))[0]
        mu.reg_write(UC_X86_REG_ESP, sp + 4 + 4 * k)
        mu.reg_write(UC_X86_REG_EIP, ret)
    for at, k in LEFT_OUT.items():
        mu.hook_add(UC_HOOK_CODE, hook, k, begin=game(at), end=game(at))
    s.call(0x597e4)
    s.call(0x5a4f0, eax=n, edx=node)
    return bytes(mu.mem_read(base, W * H))


def port(s, name, L, n, node):
    near, win = setup(s, name, L, n, node)
    v = View.in_dll(L, "g_view")
    v.cx, v.cy, v.min_z = s.i32(0xf7a28), s.i32(0xf7a2c), s.i32(0xf7938)
    for i, r in enumerate(struct.unpack("<1024i", s.raw(s.u32(0xf7a14), 4096))):
        v.recip[i] = r
    v.far_shift, v.max_level = s.read(0xf6aa4, 1)[0], s.i32(0xeaad8)
    v.clip_x, v.clip_y, v.clip_z = s.i32(0x143e54), s.i32(0x143e58), s.i32(0x143e5c)
    paint = Painters.in_dll(L, "g_road_paint")
    paint.roadside = paint.roadside.__class__()
    for f in ("model", "car_row", "cars"):
        setattr(paint, f, getattr(paint, f).__class__())
    for f, sym in (("quad", "quad_view"), ("whole", "paint_whole"), ("picture", "paint_picture")):
        ctypes.memmove(ctypes.addressof(paint) + getattr(Painters, f).offset,
                       ctypes.byref(ctypes.c_void_p(ctypes.cast(getattr(L, sym), ctypes.c_void_p).value)), 8)
    ctypes.c_void_p.in_dll(L, "g_paint").value = ctypes.cast(L.raster_quad, ctypes.c_void_p).value
    fb = (ctypes.c_ubyte * (W * H))()
    L.raster_window(fb, W, *CLIP)
    L.road_objects_start()
    L.road_draw(n, node, near, ctypes.byref(win))
    return bytes(fb)


def main():
    name, snaps = sys.argv[1], sys.argv[2:]
    L = lib()
    L.files_init(b"NFSSE", None)
    if not L.exe_load():
        sys.exit("no NFS_DOS.EXE")
    os.makedirs("build/shots", exist_ok=True)
    for p in snaps:
        s = Snap(p)
        n = s.i32(0x136618)
        node = struct.unpack("<i", s.raw(s.u32(0x143df4) + 0x38, 4))[0]
        s.write(0xf88a4, struct.pack("<4i", *CLIP))     # what the far quads' outcode reads
        got = port(s, name, L, n, node)
        # a snapshot taken inside a painter (r1a) has one see-through picture's mark (0x45)
        # switched to 0x7b while it paints: put it back
        mine, base = ctypes.c_void_p.in_dll(L, "g_fam").value, s.u32(0x145c50)
        f = Fam.in_dll(L, "g_fam")
        marks = 0
        for q in set(f.texture[t][k] for t in range(144) for k in range(8)) | \
                set(f.picture[t][k] for t in range(64) for k in range(4)):
            if q and ctypes.string_at(q, 1) != s.raw(base + q - mine, 1):
                s.mu.mem_write(base + q - mine, ctypes.string_at(q, 1))
                marks += 1
        want = original(s, n, node)
        L.video_get_palette.restype = None
        raw = (ctypes.c_ubyte * 768)()
        L.video_get_palette(raw, 0, 256)
        pal = [tuple(raw[3 * i:3 * i + 3]) for i in range(256)]
        tag = os.path.splitext(os.path.basename(p))[0]
        png("build/shots/road_%s_orig.png" % tag, want, pal)
        png("build/shots/road_%s_port.png" % tag, got, pal)
        diff = [255 if a != b else 0 for a, b in zip(want, got)]
        png("build/shots/road_%s_diff.png" % tag, diff, [(0, 0, 0)] * 255 + [(255, 255, 255)])
        d = sum(1 for x in diff if x)
        painted = sum(1 for x in want if x)
        print("%s: %d pixels painted by the original, %d differ%s" % (
            tag, painted, d, " (%d mark put back)" % marks if marks else ""))


if __name__ == "__main__":
    main()
