#!/usr/bin/env python3
"""The dashboard, the original's against the port's, on snapshots of a 320x200 race:
the whole picture as a view starts (0x30094: the dashboard's picture, the map's dim
background, the status bar) run on the snapshot's memory into its screen (0xa0000, 320
a row), and the port's hud_redraw from a screen of the same colour, with the race's
pictures and colours made by the port (as tools/cars_emu.py). Writes
build/shots/dash_<snap>_{orig,port,diff}.png; `frame` also shows the original's whole
frame (0x31684 after the redraw) as dash_<snap>_frame.png.

    build/venv/bin/python tools/dash_emu.py cy1 build/snap/e3.bin [...] [frame]
"""
import ctypes, os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from snap_emu import Snap
import roadpic_emu as R
import cars_emu

I = ctypes.c_int
U8 = ctypes.c_ubyte
FILL = 0xfe


class Picture(ctypes.Structure):
    _fields_ = [("w", I), ("h", I), ("pix", ctypes.c_void_p)]


STATUS = [("shown", 0x12b940), ("steps", 0x145c94), ("mode", 0x12b8cc), ("table", 0x12dd24),
          ("track", 0x12da12), ("segment", 0x12da0e), ("circuit", 0x13bb80), ("ahead", 0x14ea6c),
          ("splits", 0x14ea70), ("best_lap", 0x13c20c), ("race_type", 0x12da20)]


class Status(ctypes.Structure):
    _fields_ = [(n, I) for n, _ in STATUS]

W, H, VGA = 320, 200, 0xa0000


def map_back(s):
    """the map's background (0xf4580): its picture's pixels"""
    rec = s.u32(0xf4580)
    w, h = struct.unpack("<2I", s.raw(rec, 8))
    return bytes(s.raw(struct.unpack("<I", s.raw(rec + 0x2c, 4))[0] + 0x10, w * h))


def original(s, frame=False):
    """the screen after the redraw (and one frame), from a screen of FILL"""
    s.mu.mem_write(VGA, bytes([FILL]) * (W * H))
    s.write(0x12b8d4, struct.pack("<I", 1))
    if frame:
        s.call(0x31684, eax=s.u32(0x13c24c + 4 * s.i32(0x12dd58)))
    else:
        s.call(0x30094)
    return bytes(s.mu.mem_read(VGA, W * H))


def port(s, name, L):
    cars, _ = cars_emu.load_cars(s, name, L)
    ctypes.memmove(ctypes.addressof((U8 * 0x5f52).in_dll(L, "g_config")), s.read(0x156c14, 0x5f52), 0x5f52)
    st = Status.in_dll(L, "g_hud")
    for n, a in STATUS:
        setattr(st, n, s.i32(a))
    scr = Picture.in_dll(L, "g_hud_screen")
    ctypes.memset(scr.pix, FILL, W * H)
    L.hud_redraw()
    back = Picture.in_dll(L, "g_hud_map_back")
    got = ctypes.string_at(scr.pix, W * H), ctypes.string_at(back.pix, back.w * back.h)
    for c in cars:
        L.car_free(ctypes.byref(c))
    return got


def palette(s):
    """the race's colours as the game read them back (0x13cb48, 6 bits)"""
    return [tuple(4 * c for c in s.read(0x13cb48 + 3 * i, 3)) for i in range(256)]


def main():
    name, args = sys.argv[1], sys.argv[2:]
    frame = "frame" in args
    os.makedirs("build/shots", exist_ok=True)
    L = cars_emu.lib()
    L.files_init(b"NFSSE", None)
    if not L.exe_load():
        sys.exit("no NFS_DOS.EXE")
    for path in (a for a in args if a != "frame"):
        s = Snap(path)
        tag = os.path.splitext(os.path.basename(path))[0]
        pal = palette(s)
        if frame:
            R.png("build/shots/dash_%s_frame.png" % tag, original(Snap(path), True), pal)
        want, want_back = original(s), map_back(s)
        got, got_back = port(s, name, L)
        R.png("build/shots/dash_%s_orig.png" % tag, want, pal)
        R.png("build/shots/dash_%s_port.png" % tag, got, pal)
        diff = [255 if a != b else 0 for a, b in zip(want, got)]
        R.png("build/shots/dash_%s_diff.png" % tag, diff, [(0, 0, 0)] * 255 + [(255, 255, 255)])
        d = [i for i, x in enumerate(diff) if x]
        db = sum(1 for a, b in zip(want_back, got_back) if a != b)
        print("%s: %d pixels differ%s; the map's background: %d differ" % (
            tag, len(d), " (first %d, %d)" % (d[0] % W, d[0] // W) if d else "", db))


if __name__ == "__main__":
    main()
