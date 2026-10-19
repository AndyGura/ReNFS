#!/usr/bin/env python3
"""The race's timing, the original's against the port's (port/src/timing.c), on snapshots
of a City 1 race: race_start 0x506dc (with 0x685e4: the results cleared, the split points,
the times to beat), then step after step race_update_cars 0x519d8 (car_check_finish
0x51760 for the racers, the race's end, the orders 0x518c0, the splits 0x68d7c / 0x68d00;
the cars' driving and the rest of it left out) and the place the status bar works out
(0x2e368). The cars are moved by a script, the same on both sides: races won and lost,
turned back, time trials on each segment, circuits, a single race, two players. Every
global and table of the timing is compared after the setup and after each step.

    build/venv/bin/python tools/timing_emu.py build/snap/e1.bin [...]
"""
import ctypes, os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from snap_emu import Snap, game
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP
import drive_emu as D

SOURCES = D.SOURCES

# the globals (timing_test_var's order): the original's address, a name
VARS = [(0x145c94, "steps"), (0xf6b74, "state"), (0x12da12, "track"), (0x12da0e, "segment"),
        (0x13c160, "laps"), (0x13c164, "finish node"), (0x12da20, "race type"),
        (0x12da2c, "field"), (0x12da30, "humans"), (0x12dd58, "player"), (0x13c18c, "racers"),
        (0x12da38, "cars"), (0x13c190, "humans racing"), (0x12dd14, "lap limit"),
        (0x12dd24, "table"), (0x13bb80, "circuit"), (0x14e794, "passed"),
        (0x14e798, "rival passed"), (0x14ea6c, "split"), (0x14ea70, "split shown"),
        (0x13c178, "result"), (0x145c90, "stopped"), (0x13c20c, "best lap"),
        (0x13c208, "new best"), (0x12b8c8, "place shown"), (0x12dd54, "viewed")]
# the tables (timing_test_array's order): a name, the address of entry i, how many
REC = 0x1d2
ARRAYS = [("split points", lambda i: 0x14e62c + 4 * i, 85),
          ("my splits", lambda i: 0x14e79c + 4 * i, 85),
          ("their splits", lambda i: 0x14e904 + 4 * i, 85),
          ("segment times", lambda i: 0x14ea74 + 4 * i, 3),
          ("finish", lambda i: 0x14ec3c + REC * i, 8),
          ("place", lambda i: 0x14ec4a + REC * i, 8),
          ("lap times", lambda i: 0x14ea80 + REC * (i // 16) + 4 * (i % 16), 128),
          ("by road", lambda i: 0x13c298 + 4 * i, None),
          ("by race", lambda i: 0x13c274 + 4 * i, None),
          ("top speed", lambda i: 0x14ec44 + REC * i, 8)]
PLACES = 0x12bd33

# race_update_cars' calls that are not the timing's, left out of the original's
LEFT_OUT = [0x5efb0, 0x14370, 0x144cc, 0x13530, 0x53214, 0x71cec, 0x83908, 0x51598, 0x1c878,
            0x6324c, 0x136b0, 0x84304, 0x4d9a8, 0x4d994, 0x4d928, 0x80974, 0x7fef8, 0x7fcbc,
            0x7fe88, 0x800a0]

# the scenarios: a name, globals set before the race starts, each racer's road nodes a
# step (the player first), the steps at most
SCENARIOS = [
    ("won", {}, [14, 11], 260),
    ("lost", {}, [10, 13], 260),
    ("dead heat", {}, [12, 12], 260),
    ("turned back", {}, [-3, -2], 80),
    ("time trial", {0x12da20: 0, 0x12da2c: 1}, [12], 260),
    ("time trial, segment 2", {0x12da20: 0, 0x12da2c: 1, 0x12da0e: 1}, [12], 260),
    ("time trial, segment 3", {0x12da20: 0, 0x12da2c: 1, 0x12da0e: 2}, [12], 260),
    ("circuit", {0x13bb80: 1, 0x12da12: 4}, [20, 18], 400),
    ("circuit time trial", {0x13bb80: 1, 0x12da12: 5, 0x12da20: 0, 0x12da2c: 1}, [17], 400),
    ("circuit, record table 0", {0x13bb80: 1, 0x12da12: 6, 0x12da20: 0, 0x12da2c: 1,
                                 0x12dd24: 0}, [25], 400),
    ("single race", {0x12da20: 2, 0x12da2c: 2}, [12, 12], 260),
    ("tournament", {0x12da20: 3, 0x12da2c: 2}, [11, 14], 260),
    ("two players", {0x12da30: 2}, [15, 12], 300),
]
# (finish node, laps) given to race_start: an open road's from the snapshot; a circuit's
CIRCUIT = (0x3c0, 2)


def lib():
    import subprocess
    out = "build/timing_test.so"
    subprocess.check_call(["cc", "-O1", "-g", "-fwrapv", "-shared", "-fPIC", "-o", out] +
                          ["port/src/" + f for f in SOURCES] +
                          ["tools/drive_test.c", "tools/timing_test.c", "tools/collide_test.c", "-Iport/src"] +
                          subprocess.check_output(["sdl2-config", "--cflags", "--libs"]).decode().split())
    return ctypes.CDLL(os.path.abspath(out))


def i32(s, a):
    return struct.unpack("<i", s.read(a, 4))[0]


def w32(s, a, v):
    s.write(a, struct.pack("<i", v))


class Port:
    """the port's side: its globals and tables, its cars (the original's records' fields
    through drive_test.c)"""

    def __init__(self, L, s):
        self.L, self.F = L, D.fields(L)
        typed(L)
        self.n = i32(s, 0x12da38)
        size = L.timing_test_car_size()
        self.cars = [ctypes.create_string_buffer(size) for _ in range(self.n)]
        for k, c in enumerate(self.cars):
            self.set_car(k, s.raw(s.u32(0x13c24c + 4 * k), 0x800))
            L.drive_test_car(k, c)

    def var(self, k):
        return self.L.timing_test_var(k)

    def get(self):
        return state_get(self.L, self.n)

    def put(self, vals):
        state_put(self.L, vals, self.n)

    def car(self, k):
        raw = ctypes.create_string_buffer(0x800)
        self.L.drive_test_out(self.cars[k], raw)
        return raw

    def set_car(self, k, raw):
        buf = ctypes.create_string_buffer(bytes(raw), 0x800)
        self.L.drive_test_in(self.cars[k], buf, None)


def typed(L):
    L.timing_test_var.restype = ctypes.POINTER(ctypes.c_int)
    L.timing_test_array.restype = ctypes.POINTER(ctypes.c_int)
    L.timing_test_places.restype = ctypes.POINTER(ctypes.c_ubyte)


def state_get(L, n):
    """the port's timing: VARS, ARRAYS, the map's places"""
    out = [L.timing_test_var(k)[0] for k in range(len(VARS))]
    for k, (_, _, m) in enumerate(ARRAYS):
        a = L.timing_test_array(k)
        out += [a[i] for i in range(m or n)]
    p = L.timing_test_places()
    return out + [p[i] for i in range(n)]


def state_put(L, vals, n):
    for k in range(len(VARS)):
        L.timing_test_var(k)[0] = vals[k]
    at = len(VARS)
    for k, (_, _, m) in enumerate(ARRAYS):
        a = L.timing_test_array(k)
        for i in range(m or n):
            a[i] = vals[at]
            at += 1
    p = L.timing_test_places()
    for i in range(n):
        p[i] = vals[at]
        at += 1


def original_state(s, n):
    out = [i32(s, a) for a, _ in VARS]
    for _, at, m in ARRAYS:
        out += [i32(s, at(i)) for i in range(m or n)]
    return out + list(s.read(PLACES, n))


def labels(n):
    out = [name for _, name in VARS]
    for name, _, m in ARRAYS:
        out += ["%s[%d]" % (name, i) for i in range(m or n)]
    return out + ["map place[%d]" % i for i in range(n)]


def leave_out(s):
    def hook(mu, addr, size, k):
        sp = mu.reg_read(UC_X86_REG_ESP)
        ret = struct.unpack("<I", bytes(mu.mem_read(sp, 4)))[0]
        mu.reg_write(UC_X86_REG_ESP, sp + 4)
        mu.reg_write(UC_X86_REG_EIP, ret)
    targets = set(LEFT_OUT)
    delta = game(0x10000) - 0x10000            # (the tables hold the running addresses)
    for table in (0x13c1b8, 0x13c1e0):          # the kinds' routines, the sounds'
        for k in range(8):
            a = s.u32(table + 4 * k) - delta
            if 0x10000 <= a < 0xc0000:
                targets.add(a)
    for a in targets:
        s.mu.hook_add(UC_HOOK_CODE, hook, 0, begin=game(a), end=game(a))


def move(raw, k, v, step, circuit, finish, laps, done):
    """a racer's record moved v nodes on (an open road: its node; a circuit: round its
    laps), its speed, a little across; past the finish (done) it slows to a stop"""
    g = lambda o: struct.unpack_from("<i", raw, o)[0]
    p = lambda o, x: struct.pack_into("<i", raw, o, x)
    node, lap = g(0x48), g(0x50)
    if done:
        v = max(0, abs(v) - 2 * done) * (1 if v > 0 else -1)
    node += v
    if circuit:
        while node >= finish:
            node -= finish
            lap += 1
        while node < 0:
            node += finish
            lap -= 1
    p(0x48, node)
    p(0x50, lap)
    p(0x4c, (lap - 1) * finish + node if circuit else node)
    p(0x39d, (((step * 37 + k * 11) % 64) - 32) << 16)
    p(0x70, abs(v) * 0x40000)
    if node < 12 and not circuit:
        p(0x8, 0x80000)                          # (before the start: kept off z < 18)


def run(L, path, name, setup, speeds, most):
    s = Snap(path)
    leave_out(s)
    n = i32(s, 0x12da38)
    P = Port(L, s)
    ctypes.memmove(ctypes.addressof((ctypes.c_ubyte * 0x5f52).in_dll(L, "g_config")),
                   s.read(0x156c14, 0x5f52), 0x5f52)
    for a, v in setup.items():
        w32(s, a, v)
    circuit = i32(s, 0x13bb80)
    finish, laps = CIRCUIT if circuit else (i32(s, 0x13c164), i32(s, 0x13c160))
    # (the racers' splits kept from the race before, the record's +0x44: none in the port)
    for p in range(8):
        s.write(0x14ea80 + REC * p + 0x44, bytes(85 * 4))
    P.put(original_state(s, n))
    names = labels(n)

    def compare(when):
        a, b = original_state(s, n), P.get()
        bad = [(names[i], a[i], b[i]) for i in range(len(a)) if a[i] != b[i]]
        for k in range(n):                       # and the cars (the start's clamp)
            if s.raw(s.u32(0x13c24c + 4 * k) + 8, 4) != bytes(P.car(k)[8:12]):
                bad.append(("car %d z" % k, None, None))
        if bad:
            print("  %s, %s: %d differ: %s" % (name, when, len(bad), bad[:6]))
        return not bad

    # race_start: the port's from the same globals
    s.call(0x506dc, eax=finish, edx=laps)
    P.var(5)[0], P.var(4)[0] = finish, laps
    L.timing_start()
    P.var(8)[0] = i32(s, 0x12da30)
    if not compare("the setup"):
        return False
    racers = i32(s, 0x13c18c)
    done = [0] * n
    for step in range(most):
        st = i32(s, 0x145c94) + 2
        w32(s, 0x145c94, st)
        P.var(0)[0] = st
        w32(s, 0x12f1bc, 1)
        for k, v in enumerate(speeds[:racers]):
            a = s.u32(0x13c24c + 4 * k)
            raw = bytearray(s.raw(a, 0x800))
            node, lap = struct.unpack_from("<i", raw, 0x48)[0], struct.unpack_from("<i", raw, 0x50)[0]
            past = lap > laps if circuit else node > finish
            if past or (v < 0 and node < 12):
                done[k] += 1
            move(raw, k, v, step, circuit, finish, laps, done[k])
            s.mu.mem_write(a, bytes(raw))
            P.set_car(k, raw)
        s.call(0x519d8)
        L.timing_step()
        L.timing_sort()
        L.timing_sort()
        L.timing_splits()
        if not compare("step %d" % step):
            return False
        # the place the status bar works out, for the viewed car
        s.call(0x2e368, eax=0x121, edx=1, ebx=7)
        viewed = i32(s, 0x12dd54)
        P.var(24)[0] = L.timing_place(viewed)
        if not compare("the place at step %d" % step):
            return False
        if i32(s, 0x13c178) and all(done[:racers]):
            break
    print("  %s: equal, %d steps, the result %d, finish %s, places %s, split %d" % (
        name, step + 1, i32(s, 0x13c178), [i32(s, 0x14ec3c + REC * p) for p in range(racers)],
        [i32(s, 0x14ec4a + REC * p) for p in range(racers)], i32(s, 0x14ea6c)))
    return True


def main():
    L = lib()
    L.files_init(b"NFSSE", None)
    if not L.exe_load():
        sys.exit("no NFS_DOS.EXE")
    bad = 0
    for path in sys.argv[1:]:
        print(os.path.basename(path))
        for name, setup, speeds, most in SCENARIOS:
            bad += not run(L, path, name, setup, speeds, most)
    print("all equal" if not bad else "%d differ" % bad)


if __name__ == "__main__":
    main()
