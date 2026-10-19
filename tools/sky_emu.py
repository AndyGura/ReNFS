#!/usr/bin/env python3
"""The sky behind the road: the original's 0x4a9d0 (the flat picture of the open roads,
0x4a8b4 and 0x49bdc) run on a snapshot into a 320 x 200 screen with the 3D view's window,
against the port's sky_draw (port/src/sky.c), and the port's stretch tables (sky_tables,
0x49a78) against the snapshot's. Pictures in build/shots/sky_<snapshot>_{orig,port}.png.

    build/venv/bin/python tools/sky_emu.py cy1 build/snap/d2.bin [...]
"""
import ctypes, os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from snap_emu import Snap, game
from roaddraw_emu import lib, setup
from roadpic_emu import png, sky_setup, sky, CLIP, SCREEN, ROWS, W, H
from poly_emu import View


def original(s):
    mu = s.mu
    mu.mem_map(SCREEN, 0x80000)
    mu.mem_write(ROWS, struct.pack("<%dI" % H, *[SCREEN + W * y for y in range(H)]))
    s.write(0xf88c4, struct.pack("<II", ROWS, 0))
    s.write(0xf88a4, struct.pack("<4i", *CLIP))
    s.call(0x4a9d0, eax=s.u32(0x143df4))
    return bytes(mu.mem_read(SCREEN, W * H))


def port(s, name, L, n, node):
    near, win = setup(s, name, L, n, node)
    v = View.in_dll(L, "g_view")
    v.cx, v.cy, v.min_z = s.i32(0xf7a28), s.i32(0xf7a2c), s.i32(0xf7938)
    for i, r in enumerate(struct.unpack("<1024i", s.raw(s.u32(0xf7a14), 4096))):
        v.recip[i] = r
    v.far_shift = s.read(0xf6aa4, 1)[0]
    sky_setup(s, L)
    bad = []
    steps = (ctypes.c_int * 0x400).in_dll(L, "g_sky_steps")
    want = struct.unpack("<321i", s.read(0x12df40, 321 * 4))
    if list(steps[:321]) != list(want):
        bad.append("column table: %d differ" % sum(a != b for a, b in zip(steps[:321], want)))
    rows = (ctypes.c_ubyte * 480).in_dll(L, "g_sky_rows")
    if bytes(rows) != s.read(0x12dd60, 480):
        bad.append("row table differs")
    fb = (ctypes.c_ubyte * (W * H))()
    L.raster_window(fb, W, *CLIP)
    sky(s, L, win)
    return bytes(fb), bad


def main():
    name, snaps = sys.argv[1], sys.argv[2:]
    L = lib()
    L.files_init(b"NFSSE", None)
    if not L.exe_load():
        sys.exit("no NFS_DOS.EXE")
    for p in snaps:
        s = Snap(p)
        tag = os.path.splitext(os.path.basename(p))[0]
        if s.u32(0x143dfc) != struct.unpack("<I", s.raw(s.u32(0x143df4) + 0x88, 4))[0]:
            print("%s: skipped (taken while the mirror's view was current)" % tag)
            continue
        n = s.i32(0x136618)
        node = struct.unpack("<i", s.raw(s.u32(0x143df4) + 0x38, 4))[0]
        got, bad = port(s, name, L, n, node)
        want = original(s)
        raw = (ctypes.c_ubyte * 768)()
        L.video_get_palette(raw, 0, 256)
        pal = [tuple(raw[3 * i:3 * i + 3]) for i in range(256)]
        png("build/shots/sky_%s_orig.png" % tag, want, pal)
        png("build/shots/sky_%s_port.png" % tag, got, pal)
        d = sum(a != b for a, b in zip(want, got))
        print("%s: tables %s; %d pixels differ" % (tag, "; ".join(bad) if bad else "equal", d))


if __name__ == "__main__":
    main()
