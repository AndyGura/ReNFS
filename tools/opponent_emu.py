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
road, both random numbers (the opponent's and EA's), the last hit between the cars and
the orders are compared. The cars meet as in the original (collide.c); some settings put
the opponent beside, ahead of or behind the player, or start it thrown about (kind 4),
the player's car thrown at a wall too, and the result counts how often they came near,
touched and hit hard. The sounds and the replay's highlights are left out on the
original's side (and the police's coming: settings +0x34 made 0).

With --full the race is kept whole (the snapshot's 9 cars: the traffic waiting off the
road (kind 6, drive_opponent_wait 0x7e0c8), placed and placed again (0x7cac8), the police
car parked until it comes out ahead (0x79500), its chase and pull-over (0x79728), the
player held after it (kind 1, 0x7ea20)) and every car is compared, with the police's
counts, the tickets and warnings and the race's end. --only=<variant> or
--only="<drive> <variant>" picks runs.

    build/venv/bin/python tools/opponent_emu.py [--full] [--coverage] [--only=<variant>] [snapshot ..]
    (default: all d*, e*, r*)
"""
import ctypes, glob, os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from snap_emu import Snap, game, flat
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP
import drive_emu as D

SOURCES = [f for f in D.SOURCES if f not in ("opponent.c", "racesnd.c")]    # (sound_test.c has it)
LEFT_OUT = dict(D.LEFT_OUT)
# EXPLODE's push (a computer's horn wanted, 0x80a60, is the game's: its random number)
LEFT_OUT.update({0x78370: 0})
# with --sound the sounds are the original's too: the hits', the engine's on and off, where
# a car is heard from
SOUNDS = (0x52d2c, 0x52e40, 0x52ea8, 0x53214, 0x531dc, 0x7fb58)
# the race's sounds' globals, in sound_test_var's order
SOUND_VARS = [(0xeded0, "muted"), (0xeded4, "engine"), (0xee358, "beep on"), (0xee35c, "beep wait"),
              (0xee360, "road pitch"), (0xee364, "revs"), (0xee368, "load"), (0xee36c, "road up"),
              (0xee370, "under vol"), (0xee374, "under pan"), (0xf7768, "skid 0"), (0xf776c, "skid 1"),
              (0xf7770, "echo now"), (0xf7774, "horn kind"), (0xf7778, "horn time"), (0xf777c, "horn car"),
              (0xf7780, "siren on"), (0xf7784, "echo wait"), (0xf7788, "siren node"),
              (0xf778c, "siren bend"), (0xea814, "radar"), (0x12b9f8, "radar flash"), (0xf973c, "next voice")]
SOUND_VARS += [(0xeded8 + 4 * c, "bend %d" % c) for c in range(16)]
for c in range(16):
    SOUND_VARS += [(0x13c2dc + 8 * c, "channel %d voice" % c), (0x13c2e0 + 8 * c, "channel %d sound" % c)]
VOICES, SLOTS, SND_SETTINGS = 0x160fa4, 0x161224, 0x12da53
# the voice records' fields compared (the rest is the mixer's)
VOICE_FIELDS = [(0, 4), (4, 4), (0xa, 1), (0xb, 1), (0xc, 4), (0x10, 1), (0x11, 1), (0x12, 1), (0x13, 1),
                (0x16, 1), (0x17, 1), (0x18, 4), (0x24, 1), (0x25, 1), (0x26, 1), (0x27, 1)]
KIND = {1: 0x7ea20, 2: 0x382d0, 3: 0x7b34c, 4: 0x1cfe0, 5: 0x7d9e4, 6: 0x7e0c8}
CAR = 0x800

# opponent_test_var's order
VARS = [(0x15d470, "brake"), (0x15d474, "lead"), (0x15d4b8, "ticks"), (0x15d4bc, "lane width"),
        (0x15d488, "lane rule"), (0xf74ac, "off"), (0xf74a4, "explode"), (0x12da3c, "traffic"),
        (0xf74f0, "wander"), (0x15d47c, "bend drop"), (0x15d478, "bend slow"),
        (0x15d484, "behind"), (0xeab70, "random"), (0xeab78, "multiplier"), (0xeab74, "last"),
        (0x12da20, "race type"), (0x12da34, "police"), (0x15d480, "waiting"),
        (0x15d154, "side 0"), (0x15d158, "side 1"), (0x12da9e, "rival car"), (0xf74a8, "chasing"),
        (0xf7510, "police looked"), (0x15d468, "long chases"), (0x12da24, "linked"),
        (0xea81c, "ticket up"), (0xea820, "warning up"), (0xea824, "picture's time"),
        (0x14ec30, "tickets"), (0x14ec34, "warnings"), (0x14ec3c, "finish"), (0x13c178, "result")]
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
    # past the police at speed, then stopped for it; a longer chase; stopped twice
    if name == "chase":
        return [(850, W(pedal=GAS)), (150, W(pedal=BRAKE)), (250, W(pedal=COAST))]
    if name == "long":
        return [(1100, W(pedal=GAS)), (150, W(pedal=BRAKE)), (300, W(pedal=COAST))]
    if name == "again":
        return [(850, W(pedal=GAS)), (150, W(pedal=BRAKE)), (150, W(pedal=COAST)),
                (1200, W(pedal=GAS)), (150, W(pedal=BRAKE)), (300, W(pedal=COAST))]
    if name == "twice":
        return [(850, W(pedal=GAS)), (150, W(pedal=BRAKE)), (150, W(pedal=COAST)),
                (700, W(pedal=GAS)), (150, W(pedal=BRAKE)), (250, W(pedal=COAST))]
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
            # the road given two lanes the other way (CY1 has none): the traffic placed
            # coming too (both sides' node bytes +4 patched)
            ("two-way", {}, {}),
            # every pull-over a ticket (the warning's time 0): the second ends the race
            ("tickets only", {0x15d2d8: 0}, {}),
            # one ticket already: the next ends the race (arrested)
            ("arrest", {0x15d2d8: 0, 0x14ec30: 1}, {}),
            ("before the start", {0x145c94: 250}, {}),
            # the player catching a slow opponent and passing it
            ("caught up", {0x145c94: 250}, {}, {0x170: 0x4000}),
            ("caught up later", {0x145c94: 250}, {0x80: -0x30000}, {0x170: 0x8000}),
            # the opponent put in the player's way (metres along, across, its speed):
            # beside it, touching; just ahead, standing; just behind, faster
            ("beside", {}, {}, {}, (0.5, -1.7, 0)),
            ("beside right", {}, {}, {}, (-1.0, 1.6, 0x40000)),
            ("ahead", {}, {}, {}, (6.0, 0.3, 0)),
            ("behind", {}, {}, {}, (-6.0, -0.4, 0x180000)),
            ("behind fast", {}, {}, {}, (-9.0, 0.0, 0x400000)),
            # the opponent thrown about where it is: tumbling, on its roof, lying still
            ("thrown", {}, {}, {}, None, ((3.0, 6.0, -8.0), (0.4, 0.1, 0.7), 40, 0)),
            ("on its roof", {}, {}, {}, None, ((0.0, 0.5, 0.0), (0.0, 0.0, 0.0), 0, 1)),
            ("rolling", {}, {}, {}, None, ((6.0, 2.0, 0.0), (0.0, 0.0, 1.5), 90, 0)),
            # both: the player put back while the opponent still lies about
            ("both thrown", {}, {}, {}, None, ((0.0, 3.0, 0.0), (0.0, 0.0, 0.0), 700, 0),
             {0x54: 0x300000, 0x5c: 0x200000}),
            ("both thrown roof", {}, {}, {}, None, ((0.0, 0.5, 0.0), (0.0, 0.0, 0.0), 700, 1),
             {0x54: -0x300000, 0x5c: 0x280000})]
V = {v[0]: k for k, v in enumerate(VARIANTS)}
RUNS = [("gas", "as is"), ("weave", "as is"), ("wait", "as is"), ("gas", "single race"),
        ("weave", "no traffic option"), ("weave", "lane rule off"), ("gas", "fast"),
        ("gas", "steering"), ("weave", "steering hard"), ("gas", "off its lane"),
        ("gas", "off the road"), ("gas", "in the air"), ("gas", "changing gear"),
        ("gas", "past the finish"), ("gas", "before the start"), ("weave", "before the start"),
        ("gas", "caught up"), ("weave", "caught up"), ("gas", "caught up later"),
        ("gas", "beside"), ("weave", "beside"), ("gas", "beside right"), ("gas", "ahead"),
        ("weave", "ahead"), ("wait", "behind"), ("gas", "behind"), ("wait", "behind fast"),
        ("gas", "thrown"), ("wait", "thrown"), ("gas", "on its roof"), ("wait", "rolling"),
        ("wait", "both thrown"), ("wait", "both thrown roof"),
        ("weave", "caught up later"), ("gas", "two-way"), ("weave", "two-way"), ("wait", "two-way"),
        ("chase", "as is"), ("long", "as is"), ("twice", "as is"), ("twice", "tickets only"),
        ("chase", "two-way"), ("again", "tickets only"), ("long", "arrest"), ("chase", "arrest")]


def lib():
    import subprocess
    out = "build/opponent_test_%d.so" % os.getpid()    # (several runs at once)
    subprocess.check_call(["cc", "-O1", "-g", "-fwrapv", "-shared", "-fPIC", "-o", out] +
                          ["port/src/" + f for f in SOURCES] +
                          ["tools/drive_test.c", "tools/timing_test.c", "tools/opponent_test.c", "tools/collide_test.c",
                           "tools/sound_test.c",
                           "-Iport/src"] +
                          subprocess.check_output(["sdl2-config", "--cflags", "--libs"]).decode().split())
    L = ctypes.CDLL(os.path.abspath(out))
    os.unlink(out)
    P = ctypes.POINTER
    L.opp_test_var.restype = P(ctypes.c_int)
    L.opp_test_zone.restype = P(ctypes.c_short)
    L.opp_test_cfg.restype = P(ctypes.c_int)
    L.opp_test_grid.restype = P(ctypes.c_int)
    L.opp_test_drivers.restype = P(ctypes.c_int)
    L.timing_test_array.restype = P(ctypes.c_int)
    L.sound_test_var.restype = P(ctypes.c_int)
    L.timing_test_var.restype = P(ctypes.c_int)
    L.sound_test_freq.restype = ctypes.c_uint
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
# the race's sounds (--sound): the game's (racesnd.c) and the library's (sndlib.c)
SOUND_PORTED = [(a, a + n) for a, n in (
    (0x52ac8, 70), (0x7fb2c, 44), (0x52ecc, 439), (0x53084, 41), (0x52b10, 293), (0x52c38, 184),
    (0x52cf0, 57), (0x52d2c, 240), (0x52e1c, 36), (0x52e40, 61), (0x52e80, 37), (0x52ea8, 34),
    (0x530b0, 186), (0x5316c, 54), (0x531a4, 8), (0x531ac, 48), (0x531dc, 54), (0x53214, 139),
    (0x53338, 1559), (0x7fb58, 212), (0x7fc2c, 142), (0x7fcbc, 460), (0x7fe88, 110), (0x7fef8, 141),
    (0x7ff88, 148), (0x8001c, 132), (0x800a0, 2257), (0x80974, 234), (0x80a60, 77),
    (0x95a7a, 212), (0x95b4e, 398), (0x95cfb, 25), (0x98bf5, 138), (0x8169b, 128), (0x8171b, 131),
    (0x8179e, 118), (0x81814, 109), (0x8193c, 60), (0x9ea7d, 49), (0x9eaae, 61), (0xa1fbb, 127),
    (0xa062c, 121), (0xa20a8, 217))]
PORTED = [(0x7b34c, 0x7bf35), (0x7b178, 0x7b34a), (0x7cf04, 0x7cf68), (0x7cf98, 0x7d9e2),
          (0x7a040, 0x7aeb6), (0x78c88, 0x794fe), (0x78a80, 0x78c30), (0x7850c, 0x7865e),
          (0x79b60, 0x79e27), (0x79e28, 0x79ffc), (0x786d4, 0x78a7f), (0x78660, 0x786d4),
          (0x7e548, 0x7e59a), (0x7e5e4, 0x7e649), (0x7e64c, 0x7e996), (0x7dd33, 0x7ddb7),
          (0x77ca4, 0x77d3b), (0x77e4c, 0x78149), (0x7bf38, 0x7bfd7), (0x51724, 0x5175d),
          # a computer's car thrown about, stood up, put back
          (0x1de8a, 0x1e101), (0x7d9e4, 0x7daeb), (0x7daec, 0x7dbd5), (0x7c030, 0x7c292),
          # the traffic waiting, placed and placed again
          (0x7e0c8, 0x7e209), (0x77d3c, 0x77de8), (0x77de8, 0x77e4c), (0x77c78, 0x77ca3),
          (0x77c20, 0x77c76), (0x7cac8, 0x7ccbf), (0x7c5b4, 0x7c7e5), (0x7c7e8, 0x7c938),
          (0x7ccc0, 0x7cf01), (0x7c294, 0x7c3f6), (0x7c3f8, 0x7c4d3), (0x7c4d4, 0x7c5b4),
          # the police: its coming, the chase, the pull-over, the player's wait
          (0x79500, 0x79727), (0x79728, 0x79b5f), (0x7aeb8, 0x7b12f), (0x1eb54, 0x1ebf0),
          (0x7ea20, 0x7ea86)]


def run(L, F, OF, path, scr, var, coverage=False, full=False, sound=False):
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
        if not (sound and at in SOUNDS):
            mu.hook_add(UC_HOOK_CODE, hook, k, begin=game(at), end=game(at))
    met = {0x1c6a4: 0, 0x2afb8: 0, 0x1e591: 0}      # the cars near, touching, a hard hit

    def count(mu, addr, size, u):
        met[flat(addr)] += 1
    for at in met:
        mu.hook_add(UC_HOOK_CODE, count, None, begin=game(at), end=game(at))
    player = s.i32(0x12dd58)
    if player != 0:
        return "the player is not car 0"
    n_cars = s.i32(0x12da38) if full else 2
    cars = [s.u32(0x13c24c + 4 * k) for k in range(n_cars)]
    opp = cars[1]
    kind1 = struct.unpack("<i", bytes(mu.mem_read(opp + 0x512, 4)))[0]
    if kind1 != 3:
        return "car 1 is not driven by the computer (kind %d)" % kind1
    # the race cut down to the two (or the whole field: the police car stays parked,
    # its coming and going left out on both sides)
    if not full:
        wr(s, 0x15d2f0, 0)          # (no police coming: settings +0x34)
        wr(s, 0x12da38, 2)
        s.write(BY_ROAD, struct.pack("<2i", 0, 1))
        s.write(BY_RACE, struct.pack("<2i", 0, 1))
    s.call(0x518c0)
    placed = [0]
    pol = {"came": None, "chase": 0, "caught": 0}
    f = lambda a, o: struct.unpack("<i", bytes(mu.mem_read(a + o, 4)))[0]

    def count_placed(mu, addr, size, u):
        placed[0] += 1
    mu.hook_add(UC_HOOK_CODE, count_placed, None, begin=game(0x7ccc0), end=game(0x7ccc0))
    vname, gset, cset = VARIANTS[var][:3]
    # (and fields set after 0x3f364, which works the car's top speed out again)
    late = VARIANTS[var][3] if len(VARIANTS[var]) > 3 else {}
    for a, v in gset.items():
        wr(s, a, v)
    L.track_load(b"cy1")
    if vname == "two-way":
        base, nodes = s.u32(0x137430), (0x15b0c - 0x98c) // 0x24
        for k in range(nodes):
            b4 = bytes(mu.mem_read(base + 36 * k + 4, 1))[0]
            mu.mem_write(base + 36 * k + 4, bytes([b4 & 15 | 0x20]))
            L.opp_test_node_byte(k, 4, b4 & 15 | 0x20)
    for o, v in cset.items():
        if o == 0x4c:
            v += struct.unpack("<i", bytes(mu.mem_read(opp + o, 4)))[0]
        mu.mem_write(opp + o, struct.pack("<i", v))
    s.call(0x3f364)
    for o, v in late.items():
        mu.mem_write(opp + o, struct.pack("<i", v))
    # the opponent put beside the player: metres along and across its road, its speed
    if len(VARIANTS[var]) > 4 and VARIANTS[var][4]:
        along, across, speed = VARIANTS[var][4]
        pl = lambda o: struct.unpack("<i", bytes(mu.mem_read(cars[0] + o, 4)))[0]
        pv = [pl(o) for o in (0, 4, 8)]
        side = [pl(0x8c + 4 * j) for j in range(3)]
        fwd = [pl(0xa4 + 4 * j) for j in range(3)]
        at = [pv[j] + int(along * fwd[j]) + int(across * side[j]) for j in range(3)]
        mu.mem_write(opp, struct.pack("<3i", *at))
        for o in (0x10, 0x48, 0x8c, 0x90, 0x94, 0x98, 0x9c, 0xa0, 0xa4, 0xa8, 0xac, 0xb0, 0xb4, 0xb8):
            mu.mem_write(opp + o, struct.pack("<i", pl(o)))
        off = pl(0x80) + int(across * 0x10000)
        for o in (0x80, 0x164):
            mu.mem_write(opp + o, struct.pack("<i", off))
        for o, v in ((0x15c, speed), (0x68, speed), (0x160, speed)):
            mu.mem_write(opp + o, struct.pack("<i", v))
    # the opponent thrown about (kind 4): its body from the car (0x1cbb0), given a speed
    # and a spin (m/s, radians a second), a wait, and turned over (on its roof) or not
    if len(VARIANTS[var]) > 5:
        vel, spin, wait, roof = VARIANTS[var][5]
        s.call(0x1cbb0, eax=opp)
        body = opp + 0xc4
        if roof:
            m = list(struct.unpack("<9i", bytes(mu.mem_read(body, 36))))
            mu.mem_write(body, struct.pack("<9i", *(m[:3] + [-v for v in m[3:]])))
        mu.mem_write(body + 0x30, struct.pack("<3i", *[int(v * 65536) for v in vel]))
        mu.mem_write(body + 0x3c, struct.pack("<3i", *[int(v * 65536) for v in spin]))
        mu.mem_write(body + 0x80, struct.pack("<i", wait))
        mu.mem_write(opp + 0x512, struct.pack("<i", 4))
    # the player's car thrown at a wall as well (drive_emu's thrown runs)
    if len(VARIANTS[var]) > 6:
        for o, v in VARIANTS[var][6].items():
            mu.mem_write(cars[0] + o, struct.pack("<i", v))
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
        spec = ctypes.create_string_buffer(bytes(mu.mem_read(spec_at, D.SPEC)) if spec_at else
                                           bytes(D.SPEC), D.SPEC)
        c = ctypes.create_string_buffer(CAR * 2)
        L.drive_test_in(c, raw, spec)
        L.drive_test_car(k, c)
        rc.append(c)
        specs.append(spec)
    # the opponent's globals
    for k, (a, nm) in enumerate(VARS):
        L.opp_test_var(k)[0] = rd(s, a)
    L.opp_test_odds()
    z = L.opp_test_zone()
    for i, v in enumerate(struct.unpack("<12h", s.read(ZONE, 24))):
        z[i] = v
    cfg = L.opp_test_cfg()
    for i, v in enumerate(struct.unpack("<107i", s.read(CFG, 0x1ac))):
        cfg[i] = v
    g = L.opp_test_grid()
    dv = L.opp_test_drivers()
    for i in range(9):
        dv[i] = rd(s, GRID + 0x20 + 4 * i)
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
        for i in range(n_cars):
            arr[i] = rd(s, a + 4 * i)
    D.set_collide(L, s)
    L.timing_test_var(21)[0] = rd(s, 0x145c90)
    # the race's sounds: the settings (silent without --sound), the banks, the globals,
    # the voices and the camera they are heard from
    sset = SND_SETTINGS + 0x4b * player
    sfx = rd(s, sset + 0x14) if sound else 0
    L.sound_test_start(sfx, rd(s, sset + 0x18), 1 if rd(s, sset + 0x1c) == 2 else 0,
                       f(cars[0], 0x48a), f(cars[1], 0x48a), rd(s, 0x12da12))
    slot_of = {s.u32(SLOTS + 4 * n): n for n in range(128) if s.u32(SLOTS + 4 * n)}
    if sound:
        rec, head = ctypes.create_string_buffer(0x48), ctypes.create_string_buffer(0x1c)
        for n in range(128):
            p = s.u32(SLOTS + 4 * n)
            if bool(p) != bool(L.sound_test_slot(n, rec, head)):
                return "sound %d: in the bank on one side only" % n
            # (+4 and the header's +0x18 are pointers there, offsets here; +0x1c on is the mixer's)
            if p and bytes(mu.mem_read(p + 8, 0x38)) != rec.raw[8:0x40]:
                return "sound %d: its record differs" % n

    def voice_raw(v):
        raw = bytearray(s.read(VOICES + 0x28 * v, 0x28))
        p = struct.unpack_from("<I", raw, 0)[0]
        struct.pack_into("<i", raw, 0, -1 if p == 0 else slot_of.get(p, -2))
        return bytes(raw)
    for k, (a, nm) in enumerate(SOUND_VARS):
        L.sound_test_var(k)[0] = rd(s, a)
    for v in range(16):
        L.sound_test_voice(v, ctypes.create_string_buffer(voice_raw(v), 0x28), 1)
    for k in range(2):
        p = s.u32(0xf7790 + 4 * k)
        L.sound_test_near(k, cars.index(p) if p in cars else -1, 1)
    CAM = 0x129ecc
    cc = s.u32(CAM + 0x90)
    L.sound_test_camera(rd(s, CAM), rd(s, CAM + 0x10), rd(s, CAM + 0x14), rd(s, CAM + 0x18),
                        rd(s, CAM + 0x38), rd(s, CAM + 0x44), cars.index(cc) if cc in cars else 0)
    heard = [0]
    vb = ctypes.create_string_buffer(0x28)
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
            if os.environ.get("OPP_TRACE") and n == int(os.environ["OPP_TRACE"]):
                from unicorn import UC_HOOK_MEM_WRITE

                def wtrace(mu, acc, addr, size, val, u):
                    sp = mu.reg_read(UC_X86_REG_ESP); print("write +%#x = %d at %#x (stack %s)" % (addr - tc, val, flat(mu.reg_read(UC_X86_REG_EIP)), " ".join("%x" % v for v in struct.unpack("<12I", bytes(mu.mem_read(sp, 48))))))
                tc = cars[int(os.environ.get("OPP_TRACE_CAR", "0"))]
                to = int(os.environ.get("OPP_TRACE_OFF", "0x58"), 0)
                mu.hook_add(UC_HOOK_MEM_WRITE, wtrace, None, begin=tc + to, end=tc + to + 3)
            if fl & 4:
                s.call(0x10400, eax=cars[0])
            order = struct.unpack("<%di" % n_cars, s.read(BY_ROAD, 4 * n_cars))
            kinds = []
            for idx in order:
                a = cars[idx]
                kind = struct.unpack("<i", bytes(mu.mem_read(a + 0x512, 4)))[0]
                part = bytes(mu.mem_read(a + 0x51a, 1))[0]
                kinds.append(kind)
                if not (part & 4) or not kind:
                    continue
                if kind not in KIND or (idx and kind == 2) or (not idx and kind in (3, 5, 6)):
                    return "step %d: car %d of kind %d, not in the port" % (n, idx, kind)
                s.call(KIND[kind], eax=a)
                s.call(0x51598, eax=a)
            s.call(0x518c0)
            s.call(0x1c878)
            s.call(0x518c0)
            # (race_update_cars after the camera: the sounds, but at the pause)
            if sound and rd(s, 0xf6b74) != 3 and sfx:
                for a in (0x80974, 0x7fef8, 0x7fcbc) + ((0x7fe88,) if rd(s, 0x12da30) > 1 else ()) + (0x800a0,):
                    s.call(a)
            # the port's
            if fl & 4:
                L.drive_controls(rc[0])
            arr = L.timing_test_array(7)
            for idx in [arr[i] for i in range(n_cars)]:
                c = rc[idx]
                L.drive_test_out(c, out)
                part, kind = out.raw[0x51a], struct.unpack("<i", out.raw[0x512:0x516])[0]
                if not (part & 4) or not kind:
                    continue
                L.drive_step(c)
            L.timing_sort()
            L.crash_world()
            L.timing_sort()
            if sound:
                L.rsnd_step()
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
                if [rd(s, a + 4 * i) for i in range(n_cars)] != [arr[i] for i in range(n_cars)]:
                    diff.append("order %d" % k)
            for p in range(2):
                if rd(s, TOP + 0x1d2 * p) != L.drive_test_global(TOP_G + p, 0, 0):
                    diff.append("top speed %d %d/%d" % (p, rd(s, TOP + 0x1d2 * p), L.drive_test_global(TOP_G + p, 0, 0)))
            for k, (a, nm) in enumerate(D.CRASH_GLOBALS):
                if rd(s, a, 4) != L.crash_test_global(k, 0, 0):
                    diff.append("%s %d/%d" % (nm, rd(s, a, 4), L.crash_test_global(k, 0, 0)))
            for k, (a, nm) in enumerate(SOUND_VARS):
                x, y = rd(s, a), L.sound_test_var(k)[0]
                if x != y and (sound or nm.startswith("horn")):
                    diff.append("%s %d/%d" % (nm, x, y))
            if sound:
                for v in range(16):
                    t = voice_raw(v)
                    L.sound_test_voice(v, vb, 0)
                    for o, nb in VOICE_FIELDS:
                        x = int.from_bytes(t[o:o + nb], "little", signed=True)
                        y = int.from_bytes(vb.raw[o:o + nb], "little", signed=True)
                        if x != y:
                            diff.append("voice %d +%#x %d/%d" % (v, o, x, y))
                for k in range(2):
                    p = s.u32(0xf7790 + 4 * k)
                    x = cars.index(p) if p in cars else -1 if not p else -2
                    if x != L.sound_test_near(k, 0, 0):
                        diff.append("near %d %d/%d" % (k, x, L.sound_test_near(k, 0, 0)))
                if rd(s, CAM + 0x44) != L.sound_test_cam_yaw():
                    diff.append("camera yaw")
                heard[0] += sum(1 for v in range(16) if s.read(VOICES + 0x28 * v + 0x16, 1)[0])
            ea = (ctypes.c_uint * 6)()
            L.collide_test_random(ea, 0)
            if list(ea) != list(struct.unpack("<6I", s.read(0xf8a8c, 24))):
                diff.append("EA's random numbers")
            if rd(s, 0xea668) != L.collide_test_impulse():
                diff.append("hit %#x/%#x" % (rd(s, 0xea668), L.collide_test_impulse()))
            fd = D.fallen_differ(L, s)
            if fd:
                diff.append(fd)
            if diff and os.environ.get("OPP_DEBUG"):
                for k, a in enumerate(cars):
                    t = bytes(mu.mem_read(a, CAR))
                    out.raw = t
                    L.drive_test_out(rc[k], out)
                    g = lambda b, o: struct.unpack("<i", b[o:o + 4])[0]
                    print("car %d kind %d node %d pos %s / %s vel %s / %s ground %d/%d" % (
                        k, g(t, 0x512), g(t, 0x48), [g(t, o) >> 8 for o in (0, 4, 8)],
                        [g(out.raw, o) >> 8 for o in (0, 4, 8)], [g(t, o) >> 8 for o in (0x54, 0x58, 0x5c)],
                        [g(out.raw, o) >> 8 for o in (0x54, 0x58, 0x5c)], g(t, 0x3fd), g(out.raw, 0x3fd)))
                print(diff)
                for k, (a, nb) in enumerate(D.GLOBALS):
                    if a and rd(s, a, nb) != L.drive_test_global(k, 0, 0):
                        print("global %#x %d/%d" % (a, rd(s, a, nb), L.drive_test_global(k, 0, 0)))
            if diff:
                return "step %d (word %#x, steps %d, kinds %s, met %d, touched %d): %s" % (
                    n, w, st, kinds, met[0x1c6a4], met[0x2afb8], ", ".join(diff[:12]))
            if full and n_cars > 2:
                pc = bytes(mu.mem_read(cars[2] + 0x174, 4))
                if pc[0] & 8 and bytes(mu.mem_read(cars[2] + 0x51a, 1))[0] & 4 and pol["came"] is None:
                    pol["came"] = (n, f(cars[2], 0x48))
                pol["chase"] += 1 if rd(s, 0xf74a8) else 0
                pol["caught"] += 1 if bytes(mu.mem_read(cars[0] + 0x176, 1))[0] & 1 else 0
            wr(s, 0x145c94, st + 2)
            clock = s.i32(0xed100) + 3
            wr(s, 0xed100, clock)
            L.drive_test_global(13, 1, clock)
            n += 1
    touched = ", %d times near, %d touching, %d hard" % (met[0x1c6a4], met[0x2afb8], met[0x1e591]) \
        if met[0x1c6a4] else ""
    if full:
        on = sum(1 for a in cars[3:] if f(a, 0x512) != 6)
        touched += ", %d placings, %d traffic cars on the road" % (placed[0], on)
        if pol["came"]:
            touched += "; the police out at step %d, node %d, %d steps chasing, %d caught, " \
                "%d warnings, %d tickets, result %d" % (pol["came"] + (pol["chase"], pol["caught"],
                rd(s, 0x14ec34), rd(s, 0x14ec30), rd(s, 0x13c178)))
    if sound:
        touched += "; %d voices playing a step on average" % (heard[0] // max(n, 1))
    return "equal, %d steps (the player at node %d, the opponent at %d, %.1f m/s, lane place %.2f m%s)" % (
        n, f(cars[0], 0x48), f(opp, 0x48), f(opp, 0x15c) / 65536, f(opp, 0x80) / 65536, touched)


def main():
    args = sys.argv[1:]
    coverage = "--coverage" in args
    full = "--full" in args
    sound = "--sound" in args
    if sound and coverage:
        PORTED[:] = SOUND_PORTED
    only = [a[7:] for a in args if a.startswith("--only=")]
    args = [a for a in args if a not in ("--coverage", "--full", "--sound") and not a.startswith("--only=")]
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
            if only and var not in only and "%s %s" % (scr, var) not in only:
                continue
            r = run(L, F, OF, p, scr, V[var], coverage, full, sound)
            bad += not r.startswith("equal")
            print("%s %-6s %-18s %s" % (os.path.basename(p), scr, var, r), flush=True)
    print("all equal" if not bad else "%d runs differ" % bad)
    if coverage:
        D.PORTED[:] = PORTED
        D.report_coverage()


if __name__ == "__main__":
    main()
