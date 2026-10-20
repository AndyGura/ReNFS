#!/usr/bin/env python3
"""The dashboard, the original's against the port's, on snapshots of a 320x200 race:
the whole picture as a view starts (0x30094: the dashboard's picture, the map's dim
background, the status bar) run on the snapshot's memory into its screen (0xa0000, 320
a row), and the port's hud_redraw from a screen of the same colour, with the race's
pictures and colours made by the port (as tools/cars_emu.py). Then a whole frame in the
driver's seat (0x31684 against hud_frame) with the wheel turned several ways, on the
snapshot's view, mirror and map pictures, the routines not yet ported left out on both
sides (LEFT_OUT); then frames after the start with the status bar's numbers (0x30d8c: the
speed, gear, place, time, split, laps or miles) as NUMBERS changes the race. Writes build/shots/dash_<snap>_{orig,port,diff}.png and
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
import timing_emu

# not yet ported: the replay's buttons
LEFT_OUT = {0x5c5d0: 0}
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
# and the detail (0x143e04): the view's pixels 1:1, doubled across, doubled both ways
DETAILS = [None, 0, 1, 2, 1, 0, 2, 1, 0]
# then frames after the start, each changing the race (on both sides) for the status
# bar's numbers: the race's steps, the viewed car's speed, gear, node and lap, the split,
# the mode (0 the segment, 1 the race), a circuit, kilometres, the finish, a new best lap,
# the race type
NUMBERS = [
    {"steps": 361}, {"steps": 371}, {"steps": 400, "speed": 0x1c0000, "gear": 1},
    {"steps": 501, "speed": 0x50000, "gear": -1}, {"steps": 502, "gear": -2, "node": 600},
    {"steps": 760, "split": 125, "shown": 50, "speed": 0x3c0000, "gear": 4},
    {"steps": 800, "split": -340}, {"steps": 801, "split": 0},
    {"steps": 4000, "shown": 0, "node": 1700}, {"steps": 4001, "mode": 1, "split": -3999},
    {"steps": 4002, "split": 77, "shown": 9},
    {"steps": 4100, "mode": 0, "circuit": 1, "laps": 3, "lap limit": 8, "lap": 2,
     "lap time 1": 3500, "split": 0, "shown": 0},
    {"steps": 4200}, {"steps": 4500, "lap": 3, "lap time 2": 4300}, {"steps": 4900},
    {"steps": 5000, "mode": 1}, {"steps": 5001, "mode": 0, "lap": 11, "laps": 12,
                                 "lap limit": 12},
    {"steps": 5002, "circuit": 0, "kmh": 1, "speed": -0x420000, "node": 100},
    {"steps": 5003, "node": 1, "gear": 0}, {"steps": 5004, "node": 1500},
    {"steps": 9000, "node": 1800, "finish": 7000, "kmh": 0},
    {"steps": 9001, "new best": 1}, {"steps": 9002, "node": 900, "race type": 0, "split": 9},
    {"steps": 9003, "split": -9}, {"steps": 9004, "race type": 2},
    {"steps": 9005, "speed": 0x7fffff}, {"steps": 99999, "speed": 0}]

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
          ("cop_flash", 0x12ba04), ("laps", 0x13c160), ("lap_limit", 0x12dd14),
          ("finish_node", 0x13c164)]


class Status(ctypes.Structure):
    _fields_ = [(n, I) for n, _ in STATUS]


class MapState(ctypes.Structure):
    _fields_ = [("node", I), ("nodes", I * 8), ("place", I), ("places", U8 * 8), ("cop", I),
                ("cop_node", I), ("driver", I)]


class HudCar(ctypes.Structure):
    _fields_ = [("steer", I), ("number", I), ("rpm", I), ("redline", I), ("speed", I),
                ("viewed", I), ("human", I), ("road_speed", I), ("gear", I), ("player", I),
                ("lap", I), ("node", I)]


REC = 0x1d2


def number_change(s, car, k, v):
    """a NUMBERS change on the original's side"""
    w = lambda a, x: s.write(a, struct.pack("<i", x))
    c = lambda o, x: s.mu.mem_write(car + o, struct.pack("<i", x))
    p = struct.unpack("<i", s.raw(car + 0x47d, 4))[0]
    if k.startswith("lap time "):
        w(0x14ea80 + REC * p + 4 * int(k[9:]), v)
        return
    {"steps": lambda: w(0x145c94, v), "speed": lambda: c(0x68, v), "gear": lambda: c(0x3ed, v),
     "node": lambda: c(0x48, v), "lap": lambda: c(0x50, v), "split": lambda: w(0x14ea6c, v),
     "shown": lambda: w(0x14ea70, v), "mode": lambda: w(0x12b8cc, v),
     "circuit": lambda: w(0x13bb80, v), "laps": lambda: w(0x13c160, v),
     "lap limit": lambda: w(0x12dd14, v), "kmh": lambda: s.write(0x15d140, struct.pack("<h", v)),
     "finish": lambda: w(0x14ec3c + REC * p, v), "new best": lambda: w(0x13c208, v),
     "race type": lambda: w(0x12da20, v)}[k]()


def number_port(L, st, car, k, v):
    """the same change on the port's side"""
    var = lambda i: L.timing_test_var(i)
    arr = lambda i: L.timing_test_array(i)
    if k.startswith("lap time "):
        arr(6)[16 * car.player + int(k[9:])] = v
        return
    if k == "steps":
        st.steps = var(0)[0] = v
    elif k in ("speed", "gear", "node", "lap"):
        setattr(car, k, v)
        L.timing_test_car_set(L_cars[car.viewed], car.lap, car.node, car.node)
    elif k == "split":
        st.ahead = var(18)[0] = v
    elif k == "shown":
        st.splits = var(19)[0] = v
    elif k == "mode":
        st.mode = v
    elif k == "circuit":
        st.circuit = var(15)[0] = v
    elif k == "laps":
        st.laps = var(4)[0] = v
    elif k == "lap limit":
        st.lap_limit = var(13)[0] = v
    elif k == "kmh":
        st.kmh = v
    elif k == "finish":
        arr(4)[car.player] = v
    elif k == "new best":
        var(23)[0] = v
    elif k == "race type":
        st.race_type = var(6)[0] = v


L_cars = []

W, H, VGA = 320, 200, 0xa0000
HIRES = 0                  # a 640x480 race (0x12c308): the screen in banks (Snap.vesa)


def scr_start(s):
    """the screen filled with FILL (at 640x480 all of the card's memory)"""
    if HIRES:
        s.vesa()
        s.screen[:] = bytes([FILL]) * len(s.screen)
        s.mu.mem_write(VGA, bytes([FILL]) * 0x10000)
    else:
        s.mu.mem_write(VGA, bytes([FILL]) * (W * H))


def scr_read(s):
    return s.vesa_screen() if HIRES else bytes(s.mu.mem_read(VGA, W * H))


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



def original(s):
    """the screen after the redraw, from a screen of FILL"""
    scr_start(s)
    s.write(0x12b8d4, struct.pack("<I", 1))
    s.call(0x30094)
    return scr_read(s)


def original_frames(path):
    """the screen after a frame from a screen of FILL (the dashboard drawn again first),
    then after one more with each turn of the wheel"""
    s = Snap(path)
    left_out(s)
    car = player(s)
    scr_start(s)
    s.write(0x12b8d4, struct.pack("<I", 1))
    got = []
    for k in range(len(STEERS)):
        for at, v in ((0x18, STEERS[k]), (0x3c1, RPMS[k]), (0x68, SPEEDS[k])):
            if v is not None:
                s.mu.mem_write(car + at, struct.pack("<i", v))
        for at, v in ((0x145c94, STEPS[k]), (0xea814, RADARS[k]), (0x12b9f8, FLASHES[k]),
                      (0x143e04, DETAILS[k])):
            if v is not None:
                s.write(at, struct.pack("<i", v))
        s.call(0x31684, eax=car)
        got.append(scr_read(s))
    for change in NUMBERS:
        for k, v in change.items():
            number_change(s, car, k, v)
        s.call(0x31684, eax=car)
        got.append(scr_read(s))
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
    viewed = s.i32(0x12dd54)
    car = HudCar(g(a, 0x18), g(a, 0x48a), g(a, 0x3c1), g(g(a, 0x479), 0xac), g(a, 0x68),
                 viewed, int(viewed < s.i32(0x12da30)), g(a, 0x15c), g(a, 0x3ed), g(a, 0x47d),
                 g(a, 0x50), g(a, 0x48))
    got = []
    for k in range(len(STEERS)):
        for f, v in (("steer", STEERS[k]), ("rpm", RPMS[k]), ("speed", SPEEDS[k])):
            if v is not None:
                setattr(car, f, v)
        for f, v in (("steps", STEPS[k]), ("radar", RADARS[k]), ("radar_flash", FLASHES[k]),
                     ("detail", DETAILS[k])):
            if v is not None:
                setattr(st, f, v)
        L.hud_frame(ctypes.byref(car))
        got.append(ctypes.string_at(scr.pix, W * H))
    for change in NUMBERS:
        for k, v in change.items():
            number_port(L, st, car, k, v)
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


def timing_put(s, L, cars):
    """the race's timing and the cars' places as the snapshot has them"""
    n = s.i32(0x12da38)
    timing_emu.state_put(L, timing_emu.original_state(s, n), n)
    for c, a in zip(cars, [s.u32(0x13c24c + 4 * k) for k in range(n)]):
        g = lambda o: struct.unpack("<i", s.raw(a + o, 4))[0]
        L.timing_test_car_set(ctypes.byref(c), g(0x50), g(0x48), g(0x4c))


def port(s, name, L):
    cars, _ = cars_emu.load_cars(s, name, L)
    L_cars[:] = [ctypes.byref(c) for c in cars]
    timing_emu.typed(L)
    timing_put(s, L, cars)
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
    for n, a in STATUS:          # the numbers' changes undone for the map
        setattr(st, n, s.i32(a) if a else struct.unpack("<h", s.read(0x15d140, 2))[0])
    got += (port_map(s, L, MAP_NODES),)
    timing_put(s, L, cars)       # and the timing's
    got += (port_outside(Snap(s.path), L),)
    for c in cars:
        L.car_free(ctypes.byref(c))
    return got


# the views from outside: the camera put in the crash's view (13), the screen round the
# view drawn (0x3081c), then frames of 0x314e4 (the first a whole copy), the engine's
# turns of each and the pattern standing in for the road
OUT_RPMS = [800, 2500, 4000, 6100, 7240, 9999, 300]
# the detail of each (0x143e04: the lists that double the view's pixels across, both ways)
OUT_DETAILS = [0, 0, 1, 2, 1, 0, 2]
LAYOUT = 1


VIEW_TOP, VIEW_ROWS = 17, 160     # (640x480: from row 33, 384 rows)


def pattern(k):
    """the view's 3D window (320 x 160 from row 17) as frame k paints it"""
    return bytes((x * 7 + y * 3 + k * 11) % 200 + 20 for y in range(VIEW_ROWS) for x in range(W))


def original_outside(path):
    s = Snap(path)
    left_out(s)
    cam, car = s.u32(0x143df4), player(s)
    s.call(0x13530, eax=cam, edx=car, ebx=13)
    s.call(0x1486c, eax=cam, edx=0)
    view = s.u32(0xf4578)
    s.call(0x8118c, view)
    s.call(0x14568, eax=LAYOUT, edx=0)
    s.call(0x3081c)
    s.write(0x12b94c, struct.pack("<i", 1))    # the status bar's strip to be blacked, as
                                               # the port's after hud_load
    base = struct.unpack("<I", s.raw(view + 0x2c, 4))[0] + 0x10
    scr_start(s)
    got = []
    for k, rpm in enumerate(OUT_RPMS):
        s.mu.mem_write(base + VIEW_TOP * W, pattern(k))
        s.mu.mem_write(car + 0x3c1, struct.pack("<i", rpm))
        s.write(0x143e04, struct.pack("<i", OUT_DETAILS[k]))
        s.call(0x314e4, eax=car, edx=int(k == 0))
        got.append(scr_read(s))
    return got


def port_outside(s, L):
    w, h, pix = picture(s, 0xf4578)
    view = Picture.in_dll(L, "g_hud_view")
    ctypes.memmove(view.pix, pix, w * h)
    p = Picture.in_dll(L, "g_hud_map")
    ctypes.memmove(p.pix, picture(s, 0xf4584)[2], p.w * p.h)
    st = Status.in_dll(L, "g_hud")
    for n, a in STATUS:
        setattr(st, n, s.i32(a) if a else struct.unpack("<h", s.read(0x15d140, 2))[0])
    for k in range(4):
        (I * 4).in_dll(L, "g_hud_small_needle")[k] = s.i32(0xeaa98 + 4 * k)
    a = player(s)
    g = lambda at, o: struct.unpack("<i", s.raw(at + o, 4))[0]
    viewed = s.i32(0x12dd54)
    car = HudCar(g(a, 0x18), g(a, 0x48a), g(a, 0x3c1), g(g(a, 0x479), 0xac), g(a, 0x68),
                 viewed, int(viewed < s.i32(0x12da30)), g(a, 0x15c), g(a, 0x3ed), g(a, 0x47d),
                 g(a, 0x50), g(a, 0x48))
    box = (I * 4)(*[struct.unpack("<i", s.read(0xe8b10 + 0x16c * HIRES + 52 * LAYOUT + 0x24 + 4 * i, 4))[0]
                    for i in range(4)])
    L.hud_outside_screen(box)
    scr = Picture.in_dll(L, "g_hud_screen")
    ctypes.memset(scr.pix, FILL, W * H)
    got = []
    for k, rpm in enumerate(OUT_RPMS):
        ctypes.memmove(view.pix + VIEW_TOP * W, pattern(k), VIEW_ROWS * W)
        car.rpm = rpm
        st.detail = OUT_DETAILS[k]
        L.hud_outside_frame(ctypes.byref(car), int(k == 0), LAYOUT)
        got.append(ctypes.string_at(scr.pix, W * H))
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
    global W, H, HIRES, VIEW_TOP, VIEW_ROWS
    for path in args:
        s = Snap(path)
        s.path = path
        HIRES = s.i32(0x12c308)
        W, H = (640, 480) if HIRES else (320, 200)
        VIEW_TOP, VIEW_ROWS = (33, 384) if HIRES else (17, 160)
        tag = os.path.splitext(os.path.basename(path))[0]
        pal = palette(s)
        frames = original_frames(path)
        want, want_back = original(s), map_back(s)
        maps = original_map(path, MAP_NODES)
        outs = original_outside(path)
        got, got_back, got_frames, got_maps, got_outs = port(s, name, L)
        R.png("build/shots/dash_%s_orig.png" % tag, want, pal, W, H)
        R.png("build/shots/dash_%s_port.png" % tag, got, pal, W, H)
        diff = [255 if a != b else 0 for a, b in zip(want, got)]
        R.png("build/shots/dash_%s_diff.png" % tag, diff, [(0, 0, 0)] * 255 + [(255, 255, 255)], W, H)
        d = [i for i, x in enumerate(diff) if x]
        db = sum(1 for a, b in zip(want_back, got_back) if a != b)
        print("%s: %d pixels differ%s; the map's background: %d differ" % (
            tag, len(d), " (first %d, %d)" % (d[0] % W, d[0] // W) if d else "", db))
        for k, (a, b) in enumerate(zip(frames, got_frames)):
            d = [i for i in range(W * H) if a[i] != b[i]]
            R.png("build/shots/dash_%s_frame%d_orig.png" % (tag, k), a, pal, W, H)
            R.png("build/shots/dash_%s_frame%d_port.png" % (tag, k), b, pal, W, H)
            what = ("wheel %s, rpm %s, speed %s, step %s, radar %s" % (
                STEERS[k], RPMS[k], SPEEDS[k], STEPS[k], RADARS[k]) if k < len(STEERS)
                else ", ".join("%s %s" % kv for kv in NUMBERS[k - len(STEERS)].items()))
            print("  frame %d (%s): %d pixels differ%s" % (
                k, what, len(d), " (first %d, %d)" % (d[0] % W, d[0] // W) if d else ""))

        for k, (a, b) in enumerate(zip(outs, got_outs)):
            d = [i for i in range(W * H) if a[i] != b[i]]
            R.png("build/shots/dash_%s_out%d_orig.png" % (tag, k), a, pal, W, H)
            R.png("build/shots/dash_%s_out%d_port.png" % (tag, k), b, pal, W, H)
            print("  outside %d (rpm %d): %d pixels differ%s" % (
                k, OUT_RPMS[k], len(d), " (first %d, %d)" % (d[0] % W, d[0] // W) if d else ""))
        w, h, _ = picture(s, 0xf4584)
        for k, (a, b) in enumerate(zip(maps, got_maps)):
            d = [i for i in range(w * h) if a[i] != b[i]]
            R.png("build/shots/dash_%s_map%d_orig.png" % (tag, k), a, pal, w, h)
            R.png("build/shots/dash_%s_map%d_port.png" % (tag, k), b, pal, w, h)
            print("  map %d (%s): %d pixels differ%s" % (
                k, ("node %s" % MAP_NODES[k]) if k < len(MAP_NODES) else "%s %s" % MAP_MORE[k - len(MAP_NODES)], len(d), " (first %d, %d)" % (d[0] % w, d[0] // w) if d else ""))


if __name__ == "__main__":
    main()

