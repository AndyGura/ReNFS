#!/usr/bin/env python3
"""Cars meeting cars, the original's code against the port's (port/src/collide.c), on
snapshots of a race: two cars' bodies (cars 0 and 1, their weights and sizes as the
snapshot has them) put at random places, turns and speeds near each other, then

  - box_separate 0x2b864 (with the box test 0x26324): whether they touched, where, which
    way, and both bodies' places after being pushed apart;
  - after a touch, box_impulse 0x2afb8: whether they were closing, the hit (0xea668), both
    bodies' speeds and spins;
  - EA's random numbers 0x91e04 from the snapshot's state, 1000 of them.

    build/venv/bin/python tools/collide_emu.py [--n=2000] [--coverage] [snapshot ..]   (default: all d*, e*, r*)
"""
import ctypes, glob, math, os, random, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from snap_emu import Snap, STACK, flat
from unicorn.x86_const import UC_X86_REG_EIP
import drive_emu as D

CAR = D.CAR
BODY = 0xc4
BUF = STACK - 0x200            # the point and the normal (game addresses, above the stack)


def lib():
    import subprocess
    out = "build/collide_test.so"
    subprocess.check_call(["cc", "-O1", "-g", "-fwrapv", "-shared", "-fPIC", "-o", out] +
                          ["port/src/" + f for f in D.SOURCES] +
                          ["tools/drive_test.c", "tools/collide_test.c", "tools/replay_stub.c", "-Iport/src"] +
                          subprocess.check_output(["sdl2-config", "--cflags", "--libs"]).decode().split())
    return ctypes.CDLL(os.path.abspath(out))


def fx(v):
    return int(round(v * 65536))


def turn(rng):
    """rows across, up, along of a turn by random yaw, pitch, roll (or a plain one)"""
    k = rng.random()
    if k < 0.15:
        return [0x10000, 0, 0, 0, 0x10000, 0, 0, 0, 0x10000]
    y = rng.uniform(-math.pi, math.pi)
    p = rng.uniform(-0.3, 0.3) if k < 0.8 else rng.uniform(-math.pi, math.pi)
    r = rng.uniform(-0.3, 0.3) if k < 0.8 else rng.uniform(-math.pi, math.pi)
    if k < 0.3:
        p = r = 0.0
    cy, sy, cp, sp, cr, sr = math.cos(y), math.sin(y), math.cos(p), math.sin(p), math.cos(r), math.sin(r)
    # yaw about up, then pitch about across, then roll about along
    Ry = [[cy, 0, -sy], [0, 1, 0], [sy, 0, cy]]
    Rp = [[1, 0, 0], [0, cp, sp], [0, -sp, cp]]
    Rr = [[cr, sr, 0], [-sr, cr, 0], [0, 0, 1]]
    mm = lambda a, b: [[sum(a[i][t] * b[t][j] for t in range(3)) for j in range(3)] for i in range(3)]
    m = mm(Rr, mm(Rp, Ry))
    return [fx(m[i][j]) for i in range(3) for j in range(3)]


def vec(rng, big):
    k = rng.random()
    if k < 0.2:
        return [0, 0, 0]
    if k < 0.5:
        return [rng.randint(-0x8000, 0x8000) for _ in range(3)]
    return [rng.randint(-big, big) if j != 1 else rng.randint(-big // 4, big // 4) for j in range(3)]


def put_body(s, car, m, pos, vel, spin):
    s.mu.mem_write(car + BODY, struct.pack("<9i3i3i3i", *(m + pos + vel + spin)))


def body_of(raw):
    """m, pos, vel, spin of a car record's body"""
    v = struct.unpack("<18i", raw[BODY:BODY + 72])
    return list(v[:9]), list(v[9:12]), list(v[12:15]), list(v[15:18])


def rd(s, a):
    return struct.unpack("<i", s.read(a, 4))[0]


# for --coverage: the box test, the pushing apart, the hit's push (first, last + 1)
PORTED = [(0x26324, 0x2a332), (0x2afb8, 0x2bf9c), (0x2bf9c, 0x2c0f2), (0x2c1c0, 0x2c25a)]
HIT = set()


def run(L, path, n, seed, coverage=False):
    s = Snap(path)
    mu = s.mu
    if coverage:
        from unicorn import UC_HOOK_CODE
        from snap_emu import game
        for lo, hi in PORTED:
            mu.hook_add(UC_HOOK_CODE, lambda mu, a, sz, u: HIT.add(flat(a)), None, begin=game(lo), end=game(hi) - 1)
    cars = [s.u32(0x13c24c + 4 * k) for k in range(2)]
    if s.i32(0x12da38) < 2:
        return "fewer than two cars"
    for c in cars:
        if struct.unpack("<I", bytes(mu.mem_read(c + 0xc0, 4)))[0] != c + BODY:
            return "a body not at +0xc4"
    s.write(0x15d144, b"\0\0\0\0")           # (no force feedback)
    spec = ctypes.create_string_buffer(D.SPEC)
    rc = [ctypes.create_string_buffer(CAR * 2) for _ in cars]
    out = ctypes.create_string_buffer(CAR)
    radius = [struct.unpack("<i", bytes(mu.mem_read(c + 0x134, 4)))[0] for c in cars]
    rng = random.Random(seed)
    hits = pushes = 0
    for t in range(n):
        reach = radius[0] + radius[1]
        ma, mb = turn(rng), turn(rng)
        pa = [rng.randint(-0x1000000, 0x1000000) for _ in range(3)]
        d = [rng.randint(-reach, reach) for _ in range(3)]
        d[1] //= rng.choice([1, 4, 16])
        if rng.random() < 0.2:            # side by side, nearly touching
            d = [rng.choice([-1, 1]) * rng.randint(reach // 3, reach // 2), 0, rng.randint(-reach // 2, reach // 2)]
        pb = [pa[j] + d[j] for j in range(3)]
        big = rng.choice([0x40000, 0x200000, 0x300000])   # (beyond, the original's length overflows)
        put_body(s, cars[0], ma, pa, vec(rng, big), vec(rng, 0x20000))
        put_body(s, cars[1], mb, pb, vec(rng, big), vec(rng, 0x20000))
        raws = [bytes(mu.mem_read(c, CAR)) for c in cars]
        for k in range(2):
            L.drive_test_in(rc[k], raws[k], spec)
        mu.mem_write(BUF, b"\0" * 24)
        theirs = s.call(0x2b864, eax=cars[0] + BODY, edx=cars[1] + BODY, ebx=BUF, ecx=BUF + 12,
                        limit=2000000)
        if flat(mu.reg_read(UC_X86_REG_EIP)) is not None:
            return "case %d: the original did not come back (at %#x)" % (t, flat(mu.reg_read(UC_X86_REG_EIP)))
        res = (ctypes.c_int * 6)()
        mine = L.collide_test_separate(rc[0], rc[1], res)
        diff = []
        if theirs != mine:
            diff.append("touched %d/%d" % (theirs, mine))
        tp = struct.unpack("<6i", bytes(mu.mem_read(BUF, 24)))
        if theirs and mine and list(tp) != list(res):
            diff.append("point, normal %s / %s" % (tp, list(res)))
        for k in range(2):
            L.drive_test_out(rc[k], out)
            a, b = body_of(bytes(mu.mem_read(cars[k], CAR))), body_of(out.raw)
            if a[1] != b[1]:
                diff.append("car %d at %s / %s" % (k, a[1], b[1]))
        if diff:
            return "case %d (d %s): %s" % (t, d, ", ".join(diff))
        if not theirs:
            continue
        hits += 1
        theirs = s.call(0x2afb8, eax=cars[0], edx=cars[1], ebx=BUF, ecx=BUF + 12)
        pt = (ctypes.c_int * 3)(*tp[:3])
        nn = (ctypes.c_int * 3)(*tp[3:])
        mine = L.collide_test_push(rc[0], rc[1], pt, nn)
        if theirs != mine:
            diff.append("closing %d/%d" % (theirs, mine))
        if theirs and rd(s, 0xea668) != L.collide_test_impulse():
            diff.append("hit %#x/%#x" % (rd(s, 0xea668), L.collide_test_impulse()))
        for k in range(2):
            L.drive_test_out(rc[k], out)
            a, b = body_of(bytes(mu.mem_read(cars[k], CAR))), body_of(out.raw)
            if a[2:] != b[2:]:
                diff.append("car %d speed, spin %s / %s" % (k, a[2:], b[2:]))
        if diff:
            return "case %d push (d %s): %s" % (t, d, ", ".join(diff))
        pushes += theirs
    # the random numbers
    st = (ctypes.c_uint * 6)(*struct.unpack("<6I", s.read(0xf8a8c, 24)))
    L.collide_test_random(st, 1)
    for k in range(1000):
        a = s.call(0x91e04) & 0xffffffff
        b = L.collide_test_rand()
        if a != b:
            return "random %d: %#x / %#x" % (k, a, b)
    L.collide_test_random(st, 0)
    if list(st) != list(struct.unpack("<6I", s.read(0xf8a8c, 24))):
        return "the random numbers' state differs"
    return "equal: %d cases, %d touching, %d pushed; 1000 random numbers" % (n, hits, pushes)


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    n = next((int(a[4:]) for a in sys.argv[1:] if a.startswith("--n=")), 2000)
    L = lib()
    L.collide_test_rand.restype = ctypes.c_uint
    L.files_init(b"NFSSE", None)
    if not L.exe_load():
        sys.exit("the program's tables were not read")
    snaps = args or sorted(glob.glob("build/snap/[der]*.bin"))
    bad = 0
    for i, p in enumerate(snaps):
        r = run(L, p, n, 1000 + i, "--coverage" in sys.argv)
        bad += not r.startswith("equal")
        print("%s: %s" % (os.path.basename(p), r), flush=True)
    if "--coverage" in sys.argv:
        D.HIT.clear()
        D.HIT.update(HIT)
        D.PORTED[:] = PORTED
        D.report_coverage()
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
