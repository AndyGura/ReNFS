#!/usr/bin/env python3
"""The watching bar (racing, another car watched or the player stopped), the original's
code against the port's, on snapshots of a race (their pictures, colours and screen
size), with random states:

  - the step 0x5e948 → 0x7f010 (the mouse 0x7f470, the keys 0x7f5e8): random bar
    states, cars watched, racers, mouse places and buttons, keys held (',' '.' 1..8, the
    arrows, Space, Enter, Esc, Tab, none): the bar's fields after, the car watched, the
    key kept, the mouse (put on the lit button after a key), and the calls made
    (camera_set_car with the car and mode, camera_next, the engine heard);
  - the drawing 0x5c5d0 → 0x7ef14: random states, camera modes and drivers' names, on a
    screen kept from one case to the next: every pixel, and the fields after.

The original's pointer (0x52428, 0x52394), mouse limits (0x84885), button reader (the
pointer at 0xf7964), joystick (0x4dfe0) and calls out are stepped over; the port's
replay.c and menu.c are built with the keys, the mouse and the calls out renamed to
tools/watch_test.c's.

    build/venv/bin/python tools/watch_emu.py cy1 [--n=400] snapshot ..
"""
import ctypes, os, random, struct, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from snap_emu import Snap, game
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EAX, UC_X86_REG_EDX, UC_X86_REG_EBX
import cars_emu as C
import roadpic_emu as R

I, U8 = ctypes.c_int, ctypes.c_ubyte
FIELDS = 0x15d6f8              # pressed .. mouse x (0x15d724), 12 ints
NAMES = 0x15cb68
FILL = 0xfe
RENAMED = ["-Dkey_down=watch_test_key_down", "-Dmouse_read=watch_test_mouse_read",
           "-Dmouse_place=watch_test_mouse_place", "-Dmouse_limits=watch_test_mouse_limits",
           "-Dcamera_set_car=watch_test_set_car", "-Dcamera_next=watch_test_next",
           "-Drsnd_engine_on=watch_test_engine"]


def lib():
    tmp = "build/watch_test_%d" % os.getpid()
    sdl = subprocess.check_output(["sdl2-config", "--cflags", "--libs"]).decode().split()
    flags = ["-O1", "-g", "-fwrapv", "-fPIC", "-Iport/src"]
    objs = []
    for f in ("replay", "menu"):
        objs.append("%s_%s.o" % (tmp, f))
        subprocess.check_call(["cc", "-c"] + flags + RENAMED + ["port/src/%s.c" % f, "-o", objs[-1]] +
                              [x for x in sdl if x.startswith("-I") or x.startswith("-D")])
    out = tmp + ".so"
    subprocess.check_call(["cc", "-shared"] + flags + ["-o", out] + ["port/src/" + f for f in C.SOURCES] +
                          objs + ["tools/timing_test.c", "tools/collide_test.c", "tools/replay_stub.c",
                                  "tools/cars_test.c", "tools/skid_test.c", "tools/watch_test.c"] + sdl)
    L = ctypes.CDLL(os.path.abspath(out))
    for f in objs + [out]:
        os.unlink(f)
    return L


class Picture(ctypes.Structure):
    _fields_ = [("w", I), ("h", I), ("pix", ctypes.c_void_p)]


def step_over(s, at, ret=None):
    def hook(mu, addr, size, _):
        sp = mu.reg_read(UC_X86_REG_ESP)
        if ret is not None:
            mu.reg_write(UC_X86_REG_EAX, ret())
        mu.reg_write(UC_X86_REG_EIP, struct.unpack("<I", bytes(mu.mem_read(sp, 4)))[0])
        mu.reg_write(UC_X86_REG_ESP, sp + 4)
    s.mu.hook_add(UC_HOOK_CODE, hook, None, begin=at, end=at)


class Check:
    def __init__(self, path, name, L, rng):
        self.s = s = Snap(path)
        self.L, self.rng = L, rng
        self.tag = os.path.splitext(os.path.basename(path))[0]
        self.hires = s.i32(0x12c308)
        self.W, self.H = (640, 480) if self.hires else (320, 200)
        self.cars = [s.u32(0x13c24c + 4 * k) for k in range(s.i32(0x12da38))]
        self.ncars = len(self.cars)
        # the port: the race's pictures and colours as the snapshot's
        self.port_cars, _ = C.load_cars(s, name, L)
        self.colours = bytes(s.read(0x13d5e8, 8))
        self.names0 = [bytes(s.read(NAMES + 0x95 * i, 9)) for i in range(8)]
        # the original's calls out
        self.calls, self.buttons = [], 0
        for at in (0x52428, 0x52394, 0x84885):
            step_over(s, game(at))
        step_over(s, game(0x4dfe0), lambda: 0)
        step_over(s, s.u32(0xf7964), lambda: self.buttons)

        def on_call(mu, addr, size, what):
            sp = mu.reg_read(UC_X86_REG_ESP)
            if what == 1:
                car = mu.reg_read(UC_X86_REG_EDX)
                self.calls.append((1, self.cars.index(car) if car in self.cars else -1,
                                   mu.reg_read(UC_X86_REG_EBX)))
            else:
                self.calls.append((what, 0, 0))
            mu.reg_write(UC_X86_REG_EIP, struct.unpack("<I", bytes(mu.mem_read(sp, 4)))[0])
            mu.reg_write(UC_X86_REG_ESP, sp + 4)
        for at, what in ((0x13530, 1), (0x1494c, 2), (0x53214, 3)):
            s.mu.hook_add(UC_HOOK_CODE, on_call, what, begin=game(at), end=game(at))
        # both screens FILL
        if self.hires:
            s.vesa()
            s.screen[:] = bytes([FILL]) * len(s.screen)
            s.mu.mem_write(0xa0000, bytes([FILL]) * 0x10000)
        else:
            s.mu.mem_write(0xa0000, bytes([FILL]) * (self.W * self.H))
        scr = Picture.in_dll(L, "g_hud_screen")
        assert (scr.w, scr.h) == (self.W, self.H), (scr.w, scr.h)
        ctypes.memset(scr.pix, FILL, self.W * self.H)
        self.scr = scr
        self.first = True
        self.stats = {"pixels": 0, "calls": 0, "keyed": 0, "pressed": {}}

    def screen(self):
        s = self.s
        return s.vesa_screen()[:self.W * self.H] if self.hires else bytes(s.mu.mem_read(0xa0000, self.W * self.H))

    # --- the state, both sides

    def name(self):
        rng = self.rng
        k = rng.random()
        if k < 0.4:
            return rng.choice(self.names0)
        n = rng.choice([0, 1, 3, 5, 8, 8, 9])
        abc = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 -.!_~"
        t = "".join(rng.choice(abc) for _ in range(n)).encode()
        return (t + bytes(9))[:9] if n < 9 else t[:9]

    def state(self, racing_fields=True):
        rng, s = self.rng, self.s
        # (never more racers than cars: a time trial's snapshot has two)
        field = min(rng.choice([1, 2, 2, 8, 8, s.i32(0x12da2c)]), self.ncars)
        humans = 1
        player = 0
        stopped = 1 if rng.random() < 0.15 else 0
        viewed = rng.randrange(min(8, self.ncars)) if rng.random() < 0.7 else rng.randrange(min(field, self.ncars))
        if not stopped and viewed == player:
            viewed = 1 if self.ncars > 1 else 0
            stopped = viewed == player
        f = [0] * 12
        f[0] = rng.choice([0, 0, 0, rng.randint(1, 12)])            # pressed
        f[1] = rng.randint(1, 12)                                   # lit
        f[2] = rng.choice([0, rng.randint(1, 5), rng.randint(1, 5)])  # label
        f[3] = rng.randrange(8)                                     # name_for
        f[5] = rng.choice([0, f[0], rng.randint(0, 12)])            # last pressed
        f[6] = rng.choice([f[1], rng.randint(1, 12)])               # lit drawn
        f[8] = rng.choice([0, 0, 1])                                # redraw
        f[9] = 1 if self.first else rng.choice([0, 0, 0, 1])        # whole
        self.first = False
        top = 450 if self.hires else 180
        f[10] = rng.randint(top, self.H - 1) if rng.random() < 0.9 else rng.randint(0, self.H - 1)
        f[11] = rng.randint(0, self.W - 1)
        cam = rng.choice([0, 1, 2, 3, 4, 5, 6, 8, 10, 12, 13, rng.randint(0, 13)])
        names = b"".join(self.name() for _ in range(8))
        for at, v in ((0x12dd54, viewed), (0x12dd58, player), (0x12da2c, field), (0x12da30, humans),
                      (0x129ecc, cam), (0x145c90, stopped), (0xf6b74, 1), (0x14554c, 0)):
            s.write(at, struct.pack("<i", v))
        s.write(0x15d13a, b"\0\0")
        s.write(FIELDS, struct.pack("<12i", *f))
        s.write(NAMES, b"".join(names[9 * i:9 * i + 9] + s.read(NAMES + 0x95 * i + 9, 0x95 - 9)
                                for i in range(8))[:0x95 * 8])
        self.L.watch_test_set(viewed, player, field, humans, cam, stopped, (I * 12)(*f), names,
                              (U8 * 8)(*self.colours))
        return f

    def fields(self):
        """the original's fields after, as watch_test_get gives the port's"""
        s = self.s
        f = list(struct.unpack("<12i", s.read(FIELDS, 48))) + [s.i32(0x12dd54), s.i32(0xf7764)]
        g = (I * 14)()
        self.L.watch_test_get(g)
        mine = list(g)
        for k in (4, 7):           # (0x15d708, 0x15d714: not the bar's)
            f[k] = mine[k] = 0
        return f, mine

    # --- the cases

    def step(self):
        rng, s, L = self.rng, self.s, self.L
        f = self.state()
        W, H = self.W, self.H
        # the mouse on the bar's rows (as the limits keep it), often where it was; a button
        top = 180 if not self.hires else 450
        if rng.random() < 0.3 and f[10] >= top:
            x, y = f[11], f[10]
        else:
            x, y = rng.randint(0, W - 1), rng.randint(top, H - 1)
        self.buttons = rng.choice([0, 0, 1])
        held = bytearray(0x59)
        keys = [0x33, 0x34] + list(range(2, 10)) + [0x39, 0x1c, 0x01, 0x0f, 0x48, 0x50, 0x4b, 0x4d]
        for _ in range(rng.choice([0, 0, 1, 1, 1, 2])):
            held[rng.choice(keys)] = 1
        codes = [0, 0x20, 0x0d, 0x1b, 0x09, 0x4800, 0x5000, 0x4b00, 0x4d00]
        last = rng.choice(codes)
        s.write(0xf7ac8, bytes(held))
        s.write(0xf78e0, struct.pack("<ii", x, y))
        s.write(0x156604, b"\1\0")
        s.write(0xf7764, struct.pack("<i", last))
        x640 = next(v for v in range(640) if v * W // 640 == x)
        y480 = next(v for v in range(480) if v * H // 480 == y)
        L.watch_test_input(bytes(held), x640, y480, self.buttons, last)
        self.calls = []
        s.call(0x5e948)
        L.watch_test_step()
        want, got = self.fields()
        xy, out = (I * 2)(), (I * 48)()
        n = L.watch_test_mouse(xy, out)
        mine_calls = [tuple(out[3 * k:3 * k + 3]) for k in range(min(n, 16))]
        want_m = (s.i32(0xf78e0), s.i32(0xf78e4))
        got_m = (xy[0] * W // 640, xy[1] * H // 480)
        self.stats["calls"] += len(self.calls)
        if want_m != (x, y):
            self.stats["keyed"] += 1
        p = want[0]
        self.stats["pressed"][p] = self.stats["pressed"].get(p, 0) + 1
        bad = []
        if want != got:
            bad.append("fields %s / %s" % (want, got))
        if self.calls != mine_calls:
            bad.append("calls %s / %s" % (self.calls, mine_calls))
        if want_m != got_m:
            bad.append("mouse %s / %s" % (want_m, got_m))
        if bad:
            print("%s step (keys %s, mouse %d,%d %d, last %x): %s" % (
                self.tag, [k for k in range(0x59) if held[k]], x, y, self.buttons, last, "; ".join(bad)))
        return not bad

    def draw(self):
        s, L = self.s, self.L
        self.state()
        s.call(0x5c5d0)
        L.watch_test_draw()
        want, got = self.fields()
        a = self.screen()
        b = ctypes.string_at(self.scr.pix, self.W * self.H)
        d = [i for i in range(self.W * self.H) if a[i] != b[i]] if a != b else []
        self.last = a
        bad = []
        if want != got:
            bad.append("fields %s / %s" % (want, got))
        if d:
            bad.append("%d pixels differ (first %d, %d)" % (len(d), d[0] % self.W, d[0] // self.W))
            pal = [tuple(4 * c for c in s.read(0x13cb48 + 3 * i, 3)) for i in range(256)]
            R.png("build/shots/watch_%s_orig.png" % self.tag, a, pal, self.W, self.H)
            R.png("build/shots/watch_%s_port.png" % self.tag, b, pal, self.W, self.H)
        if bad:
            print("%s draw: %s" % (self.tag, "; ".join(bad)))
            # the screens again the same, so that the next cases start equal
            ctypes.memmove(self.scr.pix, a, self.W * self.H)
        return not bad


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    n = next((int(a[4:]) for a in sys.argv[1:] if a.startswith("--n=")), 400)
    name, paths = args[0], args[1:]
    os.makedirs("build/shots", exist_ok=True)
    L = lib()
    L.files_init(b"NFSSE", None)
    if not L.exe_load():
        sys.exit("no NFS_DOS.EXE")
    total = {"step": [0, 0], "draw": [0, 0]}
    for path in paths:
        c = Check(path, name, L, random.Random(path))
        for k in range(n):
            for what in ("step", "draw"):
                total[what][1] += 1
                total[what][0] += getattr(c, what)()
        drawn = sum(1 for v in c.last if v != FILL)
        print("%s: %d calls out, the mouse put on a button %d times, pressed %s; %d of the bar's pixels drawn" % (
            c.tag, c.stats["calls"], c.stats["keyed"], sorted(c.stats["pressed"].items()), drawn))
    print("; ".join("%s %d of %d equal" % (w, a, b) for w, (a, b) in total.items()))


if __name__ == "__main__":
    main()
