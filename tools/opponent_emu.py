#!/usr/bin/env python3
"""The opponent's driving, the original's code against the port's (port/src/opponent.c),
step by step: on a snapshot of a race cut down to the player's car and the opponent's
(0x12da38 = 2, the orders re-sorted), the original's chain of a race step (for each car
by the road's order its kind's routine: drive_player 0x382d0 for the player's, its
controls 0x10400 every second step, drive_opponent 0x7b34c for the opponent's; then
car_road_offset 0x51598; the orders 0x518c0, cars_meets_world 0x1c878 with the whole
traffic_police_tick 0x7a040 every fourth step, the orders again) run in the emulator, and
the port's drive_controls, drive_step, timing_sort and crash_world from the same cars and
globals. After every step both cars' fields, the opponent's globals, the stretches of
road, the random numbers and the orders are compared. The traffic's coming and going
(0x79500), the sounds and the replay's highlights are left out on the original's side.

    build/venv/bin/python tools/opponent_emu.py [--coverage] [--only=<variant>] [snapshot ..]
    (default: all d*, e*, r*)
"""
import ctypes, glob, os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from snap_emu import Snap, game, flat
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP
import drive_emu as D

SOURCES = [f for f in D.SOURCES if f != "opponent.c"]
LEFT_OUT = dict(D.LEFT_OUT)
# the traffic's coming and going; sounds; the police's lanes; EXPLODE's push
LEFT_OUT.update({0x79500: 0, 0x80a60: 0, 0x79728: 0, 0x78370: 0})
KIND = {2: 0x382d0, 3: 0x7b34c, 4: 0x1cfe0}
CAR = 0x800

# opponent_test_var's order
VARS = [(0x15d470, "brake"), (0x15d474, "lead"), (0x15d4b8, "ticks"), (0x15d4bc, "lane width"),
        (0x15d488, "lane rule"), (0xf74ac, "off"), (0xf74a4, "explode"), (0x12da3c, "traffic"),
        (0xf74f0, "wander"), (0x15d47c, "bend drop"), (0x15d478, "bend slow"),
        (0x15d484, "behind"), (0xeab70, "random"), (0xeab78, "multiplier"), (0xeab74, "last"),
        (0x12da20, "race type"), (0x12da34, "police")]
ZONE, CFG, RECS, LEAD, RIVAL, GRID = 0x15d29c, 0x15d2bc, 0x15d490, 0x15d48c, 0x13c270, 0x12dca0
BY_ROAD, BY_RACE, TOP = 0x13c298, 0x13c274, 0x14ec44
TOP_G = D.GLOBALS.index((0x14ea80 + 0x1c4, 4))     # top_speed[0] in drive_test_global

W = D.word
GAS, BRAKE, COAST = D.GAS, D.BRAKE, D.COAST


def pieces(name):
    if name == "gas":
        return [(600, W(pedal=GAS))]
    if name == "weave":
        return [(80, W(pedal=GAS)), (20, W(0, GAS)), (25, W(63, GAS)), (20, W(0, GAS)),
                (60, W(pedal=COAST)), (15, W(63, COAST)), (80, W(pedal=GAS)),
                (40, W(pedal=BRAKE)), (40, W(63, GAS)), (30, W(0, GAS)), (100, W(pedal=GAS))]
    if name == "wait":
        return [(250, W(pedal=BRAKE)), (350, W(pedal=GAS))]
    raise ValueError(name)


# the settings tried on top of each snapshot's: name, globals, the opponent's fields
VARIANTS = [("as is", {}, {}),
            ("single race", {0x12da20: 2}, {}),
            ("no traffic option", {0x12da28: 4}, {}),
            ("lane rule off", {0x15d488: 0}, {}),
            ("fast", {}, {0x15c: 0x380000}),
            ("steering", {}, {0x18: 0x60000, 0x1c: -0x40000}),
            ("steering hard", {}, {0x18: 0x180000, 0x74: 0x200000}),
            ("off its lane", {}, {0x164: -0x60000, 0x33c: 0x20000}),
            ("off the road", {}, {0x80: 0x1c0000, 0x164: 0x1c0000}),
            ("in the air", {}, {0x3fd: 0, 0x58: 0x60000}),
            ("changing gear", {}, {0x150: 5}),
            ("past the finish", {0x13c164: 40}, {}),
            ("before the start", {0x145c94: 250}, {}),
            # the player catching a slow opponent and passing it
            ("caught up", {0x145c94: 250}, {}, {0x170: 0x4000}),
            ("caught up later", {0x145c94: 250}, {0x80: -0x30000}, {0x170: 0x8000})]
V = {v[0]: k for k, v in enumerate(VARIANTS)}
RUNS = [("gas", "as is"), ("weave", "as is"), ("wait", "as is"), ("gas", "single race"),
        ("weave", "no traffic option"), ("weave", "lane rule off"), ("gas", "fast"),
        ("gas", "steering"), ("weave", "steering hard"), ("gas", "off its lane"),
        ("gas", "off the road"), ("gas", "in the air"), ("gas", "changing gear"),
        ("gas", "past the finish"), ("gas", "before the start"), ("weave", "before the start"),
        ("gas", "caught up"), ("weave", "caught up"), ("gas", "caught up later"),
        ("weave", "caught up later")]


def lib():
    import subprocess
    out = "build/opponent_test.so"
    subprocess.check_call(["cc", "-O1", "-g", "-fwrapv", "-shared", "-fPIC", "-o", out] +
                          ["port/src/" + f for f in SOURCES] +
                          ["tools/drive_test.c", "tools/timing_test.c", "tools/opponent_test.c",
                           "-Iport/src"] +
                          subprocess.check_output(["sdl2-config", "--cflags", "--libs"]).decode().split())
    L = ctypes.CDLL(os.path.abspath(out))
    P = ctypes.POINTER
    L.opp_test_var.restype = P(ctypes.c_int)
    L.opp_test_zone.restype = P(ctypes.c_short)
    L.opp_test_cfg.restype = P(ctypes.c_int)
    L.opp_test_grid.restype = P(ctypes.c_int)
    L.timing_test_array.restype = P(ctypes.c_int)
    return L


def opp_fields(L):
    out, i = [], 0
    at, size = ctypes.c_int(), ctypes.c_int()
    while L.opp_test_field(i, ctypes.byref(at), ctypes.byref(size)):
        out.append((at.value, size.value))
        i += 1
    return out


def rd(s, a, n=4):
    return int.from_bytes(s.read(a, n), "little", signed=n == 4)


def wr(s, a, v, n=4):
    s.write(a, (v & ((1 << 8 * n) - 1)).to_bytes(n, "little"))


def check_settings(L, s):
    """the port's settings (0x63f50 read from the program) against the snapshot's block"""
    out = (ctypes.c_int * 107)()
    L.opp_test_settings(out)
    theirs = struct.unpack("<107i", s.read(CFG, 0x1ac))
    # (+0x34 is cleared for a time trial or a circuit's race: load_traffic_cfg)
    bad = [k for k in range(107) if out[k] != theirs[k] and k != 0x34 // 4]
    return "settings equal" if not bad else "settings differ at +%#x: %#x / %#x" % (
        4 * bad[0], theirs[bad[0]], out[bad[0]])


# the ported routines, for --coverage: (first, last instruction + 1)
PORTED = [(0x7b34c, 0x7bf35), (0x7b178, 0x7b34a), (0x7cf04, 0x7cf68), (0x7cf98, 0x7d9e2),
          (0x7a040, 0x7aeb6), (0x78c88, 0x794fe), (0x78a80, 0x78c30), (0x7850c, 0x7865e),
          (0x79b60, 0x79e27), (0x79e28, 0x79ffc), (0x786d4, 0x78a7f), (0x78660, 0x786d4),
          (0x7e548, 0x7e59a), (0x7e5e4, 0x7e649), (0x7e64c, 0x7e996), (0x7dd33, 0x7ddb7),
          (0x77ca4, 0x77d3b), (0x77e4c, 0x78149), (0x7bf38, 0x7bfd7), (0x51724, 0x5175d)]


def run(L, F, OF, path, scr, var, coverage=False):
    s = Snap(path)
    mu = s.mu
    if coverage:
        for lo, hi in PORTED:
            mu.hook_add(UC_HOOK_CODE, lambda mu, a, n, u: D.HIT.add(flat(a)), None, begin=game(lo), end=game(hi) - 1)

    def hook(mu, addr, size, k):
        sp = mu.reg_read(UC_X86_REG_ESP)
        ret = struct.unpack("<I", bytes(mu.mem_read(sp, 4)))[0]
        mu.reg_write(UC_X86_REG_ESP, sp + 4 + 4 * k)
        mu.reg_write(UC_X86_REG_EIP, ret)
    for at, k in LEFT_OUT.items():
        mu.hook_add(UC_HOOK_CODE, hook, k, begin=game(at), end=game(at))
    player = s.i32(0x12dd58)
    if player != 0:
        return "the player is not car 0"
    cars = [s.u32(0x13c24c + 4 * k) for k in range(2)]
    opp = cars[1]
    kind1 = struct.unpack("<i", bytes(mu.mem_read(opp + 0x512, 4)))[0]
    if kind1 != 3:
        return "car 1 is not driven by the computer (kind %d)" % kind1
    # the race cut down to the two
    wr(s, 0x12da38, 2)
    s.write(BY_ROAD, struct.pack("<2i", 0, 1))
    s.write(BY_RACE, struct.pack("<2i", 0, 1))
    s.call(0x518c0)
    vname, gset, cset = VARIANTS[var][:3]
    # (and fields set after 0x3f364, which works the car's top speed out again)
    late = VARIANTS[var][3] if len(VARIANTS[var]) > 3 else {}
    for a, v in gset.items():
        wr(s, a, v)
    for o, v in cset.items():
        if o == 0x4c:
            v += struct.unpack("<i", bytes(mu.mem_read(opp + o, 4)))[0]
        mu.mem_write(opp + o, struct.pack("<i", v))
    s.call(0x3f364)
    for o, v in late.items():
        mu.mem_write(opp + o, struct.pack("<i", v))
    for k, (a, n) in enumerate(D.GLOBALS):
        L.drive_test_global(k, 1, rd(s, a, n) if a else 0)
    L.drive_test_global(12, 1, 1 if s.i32(0xf74f8) == 4 else 0)
    L.drive_test_track(s.read(0x12da16, 8).split(b"\0")[0])
    L.opp_test_humans_only(0)
    L.drive_test_global(23, 1, 0)
    L.drive_surfaces()
    for k, (a, nm) in enumerate(D.CRASH_GLOBALS):
        L.crash_test_global(k, 1, rd(s, a, 4))
    for k, a in enumerate(D.CRASH_IN + D.CRASHES):
        L.crash_test_global(len(D.CRASH_GLOBALS) + k, 1, rd(s, a, 4))
    smoke = ctypes.create_string_buffer(bytes(s.read(D.SMOKE, D.SMOKE_LEN)), D.SMOKE_LEN)
    L.crash_test_smoke(smoke, 1)
    # the cars
    rc, specs = [], []
    for k, a in enumerate(cars):
        raw = bytes(mu.mem_read(a, CAR))
        if struct.unpack("<I", raw[0xc0:0xc4])[0] != a + 0xc4:
            return "car %d's body is not at +0xc4" % k
        if struct.unpack("<I", raw[0x88:0x8c])[0] != a:
            return "car %d's +0x88 is not itself" % k
        spec_at = struct.unpack("<I", raw[0x479:0x47d])[0]
        spec = ctypes.create_string_buffer(bytes(mu.mem_read(spec_at, D.SPEC)), D.SPEC)
        c = ctypes.create_string_buffer(CAR * 2)
        L.drive_test_in(c, raw, spec)
        L.drive_test_car(k, c)
        rc.append(c)
        specs.append(spec)
    # the opponent's globals
    for k, (a, nm) in enumerate(VARS):
        L.opp_test_var(k)[0] = rd(s, a)
    z = L.opp_test_zone()
    for i, v in enumerate(struct.unpack("<12h", s.read(ZONE, 24))):
        z[i] = v
    cfg = L.opp_test_cfg()
    for i, v in enumerate(struct.unpack("<107i", s.read(CFG, 0x1ac))):
        cfg[i] = v
    g = L.opp_test_grid()
    for i in range(8):
        g[i] = rd(s, GRID + 4 * i)
        p = s.u32(RECS + 4 * i)
        L.opp_test_rec(i, flat(p) if p else 0)
    lead, rival = s.u32(LEAD), s.u32(RIVAL)
    if lead not in cars or rival not in cars:
        return "the lead or the rival is not one of the two"
    L.opp_test_cars(cars.index(lead), cars.index(rival))
    for k, a in ((7, BY_ROAD), (8, BY_RACE)):
        arr = L.timing_test_array(k)
        for i in range(2):
            arr[i] = rd(s, a + 4 * i)
    out = ctypes.create_string_buffer(CAR)
    n = 0
    for steps, w in D.START + pieces(scr):
        for _ in range(steps):
            st = s.i32(0x145c94)
            fl = (1 if st & 1 == 0 else 0) | (4 if st & 3 == 0 else 0) | (0x10 if st & 7 == 0 else 0)
            wr(s, 0x12f1bc, fl, 1)
            wr(s, 0x12d954, w)
            L.drive_test_global(0, 1, st)
            L.drive_test_global(1, 1, fl)
            L.drive_test_global(24, 1, w)
            # the original's step
            if fl & 4:
                s.call(0x10400, eax=cars[0])
            order = struct.unpack("<2i", s.read(BY_ROAD, 8))
            kinds = []
            for idx in order:
                a = cars[idx]
                kind = struct.unpack("<i", bytes(mu.mem_read(a + 0x512, 4)))[0]
                part = bytes(mu.mem_read(a + 0x51a, 1))[0]
                kinds.append(kind)
                if not (part & 4) or not kind:
                    continue
                if kind not in KIND or (idx and kind != 3) or (not idx and kind == 3):
                    return "step %d: car %d of kind %d, not in the port" % (n, idx, kind)
                s.call(KIND[kind], eax=a)
                s.call(0x51598, eax=a)
            s.call(0x518c0)
            s.call(0x1c878)
            s.call(0x518c0)
            # the port's
            if fl & 4:
                L.drive_controls(rc[0])
            arr = L.timing_test_array(7)
            for idx in [arr[0], arr[1]]:
                c = rc[idx]
                L.drive_test_out(c, out)
                part, kind = out.raw[0x51a], struct.unpack("<i", out.raw[0x512:0x516])[0]
                if not (part & 4) or not kind:
                    continue
                L.drive_step(c)
            L.timing_sort()
            L.crash_world()
            L.timing_sort()
            # compared
            diff = []
            for k, a in enumerate(cars):
                theirs = bytes(mu.mem_read(a, CAR))
                out.raw = theirs
                L.drive_test_out(rc[k], out)
                for at, size in F + OF:
                    x = int.from_bytes(theirs[at:at + size], "little", signed=True)
                    y = int.from_bytes(out.raw[at:at + size], "little", signed=True)
                    if x != y:
                        diff.append("car %d +%#x %d/%d" % (k, at, x, y))
            for k, (a, nm) in enumerate(VARS):
                x, y = rd(s, a), L.opp_test_var(k)[0]
                if x != y:
                    diff.append("%s %#x/%#x" % (nm, x & 0xffffffff, y & 0xffffffff))
            if s.u32(LEAD) != cars[L.opp_test_lead()]:
                diff.append("lead car")
            zt = struct.unpack("<12h", s.read(ZONE, 24))
            zp = [z[i] for i in range(12)]
            if list(zt) != zp:
                diff.append("zones %s / %s" % (zt, zp))
            for k, a in ((7, BY_ROAD), (8, BY_RACE)):
                arr = L.timing_test_array(k)
                if [rd(s, a), rd(s, a + 4)] != [arr[0], arr[1]]:
                    diff.append("order %d" % k)
            for p in range(2):
                if rd(s, TOP + 0x1d2 * p) != L.drive_test_global(TOP_G + p, 0, 0):
                    diff.append("top speed %d %d/%d" % (p, rd(s, TOP + 0x1d2 * p), L.drive_test_global(TOP_G + p, 0, 0)))
            for k, (a, nm) in enumerate(D.CRASH_GLOBALS):
                if rd(s, a, 4) != L.crash_test_global(k, 0, 0):
                    diff.append("%s %d/%d" % (nm, rd(s, a, 4), L.crash_test_global(k, 0, 0)))
            if diff:
                return "step %d (word %#x, steps %d, kinds %s): %s" % (n, w, st, kinds, ", ".join(diff[:12]))
            wr(s, 0x145c94, st + 2)
            clock = s.i32(0xed100) + 3
            wr(s, 0xed100, clock)
            L.drive_test_global(13, 1, clock)
            n += 1
    f = lambda a, o: struct.unpack("<i", bytes(mu.mem_read(a + o, 4)))[0]
    return "equal, %d steps (the player at node %d, the opponent at %d, %.1f m/s, lane place %.2f m)" % (
        n, f(cars[0], 0x48), f(opp, 0x48), f(opp, 0x15c) / 65536, f(opp, 0x80) / 65536)


def main():
    args = sys.argv[1:]
    coverage = "--coverage" in args
    only = [a[7:] for a in args if a.startswith("--only=")]
    args = [a for a in args if a != "--coverage" and not a.startswith("--only=")]
    paths = args or sorted(glob.glob("build/snap/[der]*.bin"))
    L = lib()
    L.files_init(b"NFSSE", None)
    if not L.exe_load():
        sys.exit("no NFS_DOS.EXE")
    if not L.track_load(b"cy1"):
        sys.exit("no track")
    F, OF = D.fields(L), opp_fields(L)
    bad = 0
    for p in paths:
        r = check_settings(L, Snap(p))
        bad += r != "settings equal"
        print("%s %s" % (os.path.basename(p), r))
    for p in paths:
        for scr, var in RUNS:
            if only and var not in only:
                continue
            r = run(L, F, OF, p, scr, V[var], coverage)
            bad += not r.startswith("equal")
            print("%s %-6s %-18s %s" % (os.path.basename(p), scr, var, r), flush=True)
    print("all equal" if not bad else "%d runs differ" % bad)
    if coverage:
        D.PORTED[:] = PORTED
        D.report_coverage()


if __name__ == "__main__":
    main()
