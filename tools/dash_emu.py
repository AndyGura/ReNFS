#!/usr/bin/env python3
"""The dashboard, the original's against the port's, on snapshots of a 320x200 race:
the whole picture as a view starts (0x30094: the dashboard's picture, the map's dim
background, the status bar) run on the snapshot's memory into its screen (0xa0000, 320
a row), and the port's hud_redraw from a screen of the same colour, with the race's
pictures and colours made by the port (as tools/cars_emu.py). Then a whole frame in the
driver's seat (0x31684 against hud_frame) with the wheel turned several ways, on the
snapshot's view, mirror and map pictures, the routines not yet ported left out on both
sides (LEFT_OUT). Writes build/shots/dash_<snap>_{orig,port,diff}.png and
dash_<snap>_frame<k>_{orig,port}.png.

    build/venv/bin/python tools/dash_emu.py cy1 build/snap/e3.bin [...]
"""
import ctypes, os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from snap_emu import Snap, game
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP
import roadpic_emu as R
import cars_emu

# not yet ported: the replay's buttons, and the status bar's numbers after the start
# (0x30d8c from step 361)
LEFT_OUT = {0x5c5d0: 0}
NUMBERS = 0x30d8c
# the wheel (straight, a little and far each way, all the way), the engine's turns and the
# speed (metres a second, 16.16) of each frame; the first the snapshot's own
STEERS = [None, -0x30000, 0x30000, 0x70000, -0x100000, 0x180000, -0x200000, 0, 0]
RPMS = [None, 900, 1500, 3333, 5000, 6100, 7000, 9999, 0]
SPEEDS = [None, 0x10000, -0x50000, 0x123456, 0x1c0000, 0x250000, 0x400000, 0x7fffff, 0]
# the map with the viewed car where it is, then on other nodes (the police's dot flashes
# from one map to the next)
MAP_NODES = [None, None, 0, 5, 100, 333, 700, 1200, 2000, 2399]
# then the track mirrored, the police on the map (its dot flashing), the view from outside
MAP_MORE = [("mirrored", None), ("cop", None), ("cop", -20), ("cop", -300), ("cop", None),
            ("cop", None), ("cop", None), ("outside", 50)]
# the race's steps (the start's lights) and the radar's level and flash
STEPS = [None, 0, 59, 60, 179, 250, 299, 300, 359]
RADARS = [None, 0, 1, 2, 3, 5, 7, 7, 0]
FLASHES = [None, 0, 1, 0, 1, 1, 0, 1, 0]

I = ctypes.c_int
U8 = ctypes.c_ubyte
FILL = 0xfe


class Picture(ctypes.Structure):
    _fields_ = [("w", I), ("h", I), ("pix", ctypes.c_void_p)]


STATUS = [("shown", 0x12b940), ("steps", 0x145c94), ("mode", 0x12b8cc), ("table", 0x12dd24),
          ("track", 0x12da12), ("segment", 0x12da0e), ("circuit", 0x13bb80), ("ahead", 0x14ea6c),
          ("splits", 0x14ea70), ("best_lap", 0x13c20c), ("race_type", 0x12da20),
          ("detail", 0x143e04), ("mirror", 0x143e08), ("map", 0x12b988), ("mirrored", 0x12dd28),
          ("running", 0xf6b74), ("kmh", None), ("radar", 0xea814), ("radar_flash", 0x12b9f8),
          ("cop_flash", 0x12ba04)]


class Status(ctypes.Structure):
    _fields_ = [(n, I) for n, _ in STATUS]


class MapState(ctypes.Structure):
    _fields_ = [("node", I), ("nodes", I * 8), ("place", I), ("places", U8 * 8), ("cop", I),
                ("cop_node", I), ("driver", I)]


class HudCar(ctypes.Structure):
    _fields_ = [("steer", I), ("number", I), ("rpm", I), ("redline", I), ("speed", I)]

W, H, VGA = 320, 200, 0xa0000


def picture(s, at):
    """an off-screen picture (0xf4578 ..): its size and pixels"""
    rec = s.u32(at)
    w, h = struct.unpack("<2I", s.raw(rec, 8))
    return w, h, bytes(s.raw(struct.unpack("<I", s.raw(rec + 0x2c, 4))[0] + 0x10, w * h))


def map_back(s):
    """the map's background (0xf4580): its picture's pixels"""
    return picture(s, 0xf4580)[2]


def player(s):
    return s.u32(0x13c24c + 4 * s.i32(0x12dd58))


def left_out(s):
    def hook(mu, addr, size, k):
        sp = mu.reg_read(UC_X86_REG_ESP)
        ret = struct.unpack("<I", bytes(mu.mem_read(sp, 4)))[0]
        mu.reg_write(UC_X86_REG_ESP, sp + 4 + 4 * k)
        mu.reg_write(UC_X86_REG_EIP, ret)
    for at, k in LEFT_OUT.items():
        s.mu.hook_add(UC_HOOK_CODE, hook, k, begin=game(at), end=game(at))

    def numbers(mu, addr, size, k):
        if s.i32(0x145c94) > 360:
            hook(mu, addr, size, 0)
    s.mu.hook_add(UC_HOOK_CODE, numbers, 0, begin=game(NUMBERS), end=game(NUMBERS))


def original(s):
    """the screen after the redraw, from a screen of FILL"""
    s.mu.mem_write(VGA, bytes([FILL]) * (W * H))
    s.write(0x12b8d4, struct.pack("<I", 1))
    s.call(0x30094)
    return bytes(s.mu.mem_read(VGA, W * H))


def original_frames(path):
    """the screen after a frame from a screen of FILL (the dashboard drawn again first),
    then after one more with each turn of the wheel"""
    s = Snap(path)
    left_out(s)
    car = player(s)
    s.mu.mem_write(VGA, bytes([FILL]) * (W * H))
    s.write(0x12b8d4, struct.pack("<I", 1))
    got = []
    for k in range(len(STEERS)):
        for at, v in ((0x18, STEERS[k]), (0x3c1, RPMS[k]), (0x68, SPEEDS[k])):
            if v is not None:
                s.mu.mem_write(car + at, struct.pack("<i", v))
        for at, v in ((0x145c94, STEPS[k]), (0xea814, RADARS[k]), (0x12b9f8, FLASHES[k])):
            if v is not None:
                s.write(at, struct.pack("<i", v))
        s.call(0x31684, eax=car)
        got.append(bytes(s.mu.mem_read(VGA, W * H)))
    return got


def port_frames(s, L):
    """the same frames by the port, after hud_redraw's screen"""
    for at, name in ((0xf4578, "g_hud_view"), (0xf457c, "g_hud_mirror"), (0xf4584, "g_hud_map")):
        w, h, pix = picture(s, at)
        p = Picture.in_dll(L, name)
        assert (p.w, p.h) == (w, h) or name == "g_hud_view", (name, p.w, p.h, w, h)
        ctypes.memmove(p.pix, pix, w * h)
    scr = Picture.in_dll(L, "g_hud_screen")
    st = Status.in_dll(L, "g_hud")
    ctypes.memset(scr.pix, FILL, W * H)
    L.hud_redraw_due()
    a = player(s)
    g = lambda at, o: struct.unpack("<i", s.raw(at + o, 4))[0]
    car = HudCar(g(a, 0x18), g(a, 0x48a), g(a, 0x3c1), g(g(a, 0x479), 0xac), g(a, 0x68))
    got = []
    for k in range(len(STEERS)):
        for f, v in (("steer", STEERS[k]), ("rpm", RPMS[k]), ("speed", SPEEDS[k])):
            if v is not None:
                setattr(car, f, v)
        for f, v in (("steps", STEPS[k]), ("radar", RADARS[k]), ("radar_flash", FLASHES[k])):
            if v is not None:
                setattr(st, f, v)
        L.hud_frame(ctypes.byref(car))
        got.append(ctypes.string_at(scr.pix, W * H))
    return got


KEEP = []


def node(s, a):
    return struct.unpack("<h", s.raw(a + 0x48, 2))[0]


def map_state(s):
    """what the map shows, from the snapshot"""
    m = MapState()
    m.node = node(s, s.u32(0x13c24c + 4 * s.i32(0x12dd54)))
    for k in range(8):
        a = s.u32(0x13c24c + 4 * k)
        m.nodes[k] = node(s, a) if a else 0
    m.place = s.i32(0x12b8c8)
    m.places[:] = s.read(0x12bd33, 8)
    cop = s.u32(0x13c254)
    m.cop = int(bool(cop and s.raw(cop + 0x51a, 1)[0] & 4 and s.i32(0x12dd08) and s.i32(0x12da20) == 1))
    m.cop_node = node(s, cop) if cop else 0
    m.driver = int(s.u32(0x143df4) and s.raw(s.u32(0x143df4), 4) == bytes(4) and s.i32(0x12ba2c) == 0)
    return m


def original_map(path, picks):
    """the map (0x31cb4) on the snapshot, then with the viewed car on other nodes, then the
    other cases"""
    s = Snap(path)
    got = []
    viewed = s.u32(0x13c24c + 4 * s.i32(0x12dd54))
    for n in picks:
        if n is not None:
            s.mu.mem_write(viewed + 0x48, struct.pack("<h", n))
        s.call(0x31cb4)
        got.append(picture(s, 0xf4584)[2])
    cop = s.u32(0x13c254)
    for what, n in MAP_MORE:
        if what == "mirrored":
            s.write(0x12dd28, struct.pack("<i", 1 - s.i32(0x12dd28)))
        elif what == "cop":
            s.mu.mem_write(cop + 0x51a, bytes([s.raw(cop + 0x51a, 1)[0] | 4]))
            s.write(0x12dd08, struct.pack("<i", 1))
            if n is not None:
                s.mu.mem_write(cop + 0x48, struct.pack("<h", node(s, viewed) + n))
        else:
            s.write(0x12ba2c, struct.pack("<i", 1))
            s.mu.mem_write(viewed + 0x48, struct.pack("<h", n))
        s.call(0x31cb4)
        got.append(picture(s, 0xf4584)[2])
    return got


def port_map(s, L, picks):
    w, h, pix = picture(s, 0xf4584)
    p = Picture.in_dll(L, "g_hud_map")
    assert (p.w, p.h) == (w, h), (p.w, p.h, w, h)
    ctypes.memmove(p.pix, pix, w * h)
    m = map_state(s)
    got = []
    for n in picks:
        if n is not None:
            m.node = m.nodes[s.i32(0x12dd54)] = n
        L.hud_map(ctypes.byref(m))
        got.append(ctypes.string_at(p.pix, w * h))
    st = Status.in_dll(L, "g_hud")
    for what, n in MAP_MORE:
        if what == "mirrored":
            st.mirrored = 1 - st.mirrored
        elif what == "cop":
            m.cop = int(st.race_type == 1)
            if n is not None:
                m.cop_node = ctypes.c_short(m.node + n).value
        else:
            m.driver = 0
            m.node = m.nodes[s.i32(0x12dd54)] = n
        L.hud_map(ctypes.byref(m))
        got.append(ctypes.string_at(p.pix, w * h))
    st.mirrored = s.i32(0x12dd28)
    return got


def port(s, name, L):
    cars, _ = cars_emu.load_cars(s, name, L)
    ctypes.memmove(ctypes.addressof((U8 * 0x5f52).in_dll(L, "g_config")), s.read(0x156c14, 0x5f52), 0x5f52)
    st = Status.in_dll(L, "g_hud")
    for n, a in STATUS:
        setattr(st, n, s.i32(a) if a else struct.unpack("<h", s.read(0x15d140, 2))[0])
    scr = Picture.in_dll(L, "g_hud_screen")
    ctypes.memset(scr.pix, FILL, W * H)
    L.hud_redraw()
    back = Picture.in_dll(L, "g_hud_map_back")
    got = ctypes.string_at(scr.pix, W * H), ctypes.string_at(back.pix, back.w * back.h), port_frames(s, L)
    if not L.track_load(name.encode()):
        sys.exit("no track")
    got += (port_map(s, L, MAP_NODES),)
    for c in cars:
        L.car_free(ctypes.byref(c))
    return got


def palette(s):
    """the race's colours as the game read them back (0x13cb48, 6 bits)"""
    return [tuple(4 * c for c in s.read(0x13cb48 + 3 * i, 3)) for i in range(256)]


def main():
    name, args = sys.argv[1], sys.argv[2:]
    os.makedirs("build/shots", exist_ok=True)
    L = cars_emu.lib()
    L.files_init(b"NFSSE", None)
    if not L.exe_load():
        sys.exit("no NFS_DOS.EXE")
    for path in args:
        s = Snap(path)
        tag = os.path.splitext(os.path.basename(path))[0]
        pal = palette(s)
        frames = original_frames(path)
        want, want_back = original(s), map_back(s)
        maps = original_map(path, MAP_NODES)
        got, got_back, got_frames, got_maps = port(s, name, L)
        R.png("build/shots/dash_%s_orig.png" % tag, want, pal)
        R.png("build/shots/dash_%s_port.png" % tag, got, pal)
        diff = [255 if a != b else 0 for a, b in zip(want, got)]
        R.png("build/shots/dash_%s_diff.png" % tag, diff, [(0, 0, 0)] * 255 + [(255, 255, 255)])
        d = [i for i, x in enumerate(diff) if x]
        db = sum(1 for a, b in zip(want_back, got_back) if a != b)
        print("%s: %d pixels differ%s; the map's background: %d differ" % (
            tag, len(d), " (first %d, %d)" % (d[0] % W, d[0] // W) if d else "", db))
        for k, (a, b) in enumerate(zip(frames, got_frames)):
            d = [i for i in range(W * H) if a[i] != b[i]]
            R.png("build/shots/dash_%s_frame%d_orig.png" % (tag, k), a, pal)
            R.png("build/shots/dash_%s_frame%d_port.png" % (tag, k), b, pal)
            print("  frame %d (wheel %s, rpm %s, speed %s, step %s, radar %s): %d pixels differ%s" % (
                k, STEERS[k], RPMS[k], SPEEDS[k], STEPS[k], RADARS[k], len(d), " (first %d, %d)" % (d[0] % W, d[0] // W) if d else ""))

        w, h, _ = picture(s, 0xf4584)
        for k, (a, b) in enumerate(zip(maps, got_maps)):
            d = [i for i in range(w * h) if a[i] != b[i]]
            R.png("build/shots/dash_%s_map%d_orig.png" % (tag, k), a, pal, w, h)
            R.png("build/shots/dash_%s_map%d_port.png" % (tag, k), b, pal, w, h)
            print("  map %d (%s): %d pixels differ%s" % (
                k, ("node %s" % MAP_NODES[k]) if k < len(MAP_NODES) else "%s %s" % MAP_MORE[k - len(MAP_NODES)], len(d), " (first %d, %d)" % (d[0] % w, d[0] // w) if d else ""))


if __name__ == "__main__":
    main()

