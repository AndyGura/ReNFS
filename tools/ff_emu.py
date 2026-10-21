#!/usr/bin/env python3
"""The force feedback (NOTES §4.26): the original's game side (0x4d6ec .. 0x4dc40) and
its I-Force driver's commands against port/src/ff.c, on random cars, globals and states,
in random sequences (the shakes', the steady push's and the hits' states carried from
one call to the next). Every command the original's driver would send (its packet
sender 0x4c258: the command and its bytes; the serial port's 0x60b8c, 0x60c10, 0x60c30
stepped over) is compared with the port's (ffdev.c's record), and the state after.

    build/venv/bin/python tools/ff_emu.py [--n=4000] [snapshot]
"""
import ctypes, os, random, struct, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from snap_emu import Snap, game
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EAX, UC_X86_REG_EDX, UC_X86_REG_EBX

I = ctypes.c_int
# g_ff's fields and the original's places for them
STATE = [("on", 0x15d144, 2), ("last_hit", 0xed114, 4), ("vib0", 0xed10c, 4), ("vib1", 0xed110, 4),
         ("vib_rate", 0xed120, 4), ("vib_size", 0xed124, 4), ("steady0", 0xed118, 4),
         ("steady1", 0xed11c, 4), ("rumble", 0x12f1e0, 4)]
CAR = [("speed", 0x68), ("road_speed", 0x15c), ("gear", 0x3ed), ("gear_now", 0x419), ("rpm", 0x3c1),
       ("side", 0x411), ("fwd", 0x415), ("surface", 0x4a2), ("player", 0x47d)]


class State(ctypes.Structure):
    _fields_ = [(n, I) for n, a, w in STATE]


def lib():
    out = "build/ff_test_%d.so" % os.getpid()
    sdl = subprocess.check_output(["sdl2-config", "--cflags", "--libs"]).decode().split()
    subprocess.check_call(["cc", "-shared", "-O1", "-g", "-fwrapv", "-fPIC", "-Iport/src", "-o", out] +
                          ["port/src/" + f for f in ("ff.c", "ffdev.c", "exe.c", "files.c")] +
                          ["tools/ff_test.c"] + sdl)
    L = ctypes.CDLL(os.path.abspath(out))
    os.unlink(out)
    L.ff_test_car.restype = ctypes.c_void_p
    L.ff_test_car.argtypes = [I] * 10
    for f in ("ff_object", "ff_cars", "ff_gear", "ff_step"):
        getattr(L, f).argtypes = [ctypes.c_void_p] + [I] * {"ff_object": 1, "ff_cars": 2}.get(f, 0)
    return L


def packets(b):
    out, i = [], 0
    while i < len(b):
        cmd, n = b[i], b[i + 1]
        out.append((cmd,) + tuple(b[i + 2:i + 2 + n]))
        i += 2 + n
    return out


class Original:
    def __init__(self, path):
        self.s = s = Snap(path)
        self.sent = []
        mu = s.mu

        def ret(mu, value):
            sp = mu.reg_read(UC_X86_REG_ESP)
            mu.reg_write(UC_X86_REG_EAX, value)
            mu.reg_write(UC_X86_REG_EIP, struct.unpack("<I", bytes(mu.mem_read(sp, 4)))[0])
            mu.reg_write(UC_X86_REG_ESP, sp + 4)

        def packet(mu, addr, size, _):
            cmd = mu.reg_read(UC_X86_REG_EAX) & 0xff
            n = mu.reg_read(UC_X86_REG_EBX) & 0xff
            b = bytes(mu.mem_read(mu.reg_read(UC_X86_REG_EDX), n)) if n else b""
            self.sent.append((cmd,) + tuple(b))
            ret(mu, 1)
        mu.hook_add(UC_HOOK_CODE, packet, None, begin=game(0x4c258), end=game(0x4c258))
        for at in (0x60b8c, 0x60c10, 0x60c30):
            mu.hook_add(UC_HOOK_CODE, lambda mu, a, sz, _: ret(mu, 0), None, begin=game(at), end=game(at))
        s.write(0x145c6c, b"\x01")     # the driver's device ready
        self.cars = [s.u32(0x13c24c + 4 * k) for k in range(2)]


def main():
    args = sys.argv[1:]
    n = int(next((a[4:] for a in args if a.startswith("--n=")), 4000))
    paths = [a for a in args if not a.startswith("--")] or ["build/snap/e1.bin"]
    L = lib()
    L.files_init(b"NFSSE", None)
    if not L.exe_load():
        sys.exit("no NFS_DOS.EXE")
    g = State.in_dll(L, "g_ff")
    buf = (ctypes.c_ubyte * 4096)()
    L.ff_test_take(buf)
    rng = random.Random(1)
    bad = 0
    counts = {}
    for path in paths:
        O = Original(path)
        s = O.s
        for run in range(n // 40):
            # a fresh state, the same on both sides
            st = {"on": 1, "last_hit": rng.choice([0, rng.randrange(0, 5000)]),
                  "vib0": rng.randrange(2), "vib1": rng.randrange(2),
                  "vib_rate": rng.choice([1, rng.randrange(5, 14)]), "vib_size": rng.randrange(15, 25),
                  "steady0": rng.randrange(-100, 101), "steady1": rng.randrange(-100, 101),
                  "rumble": rng.randrange(2)}
            for name, at, w in STATE:
                setattr(g, name, st[name])
                s.write(at, struct.pack("<i" if w == 4 else "<h", st[name]))
            clock = rng.randrange(0, 3000)
            for k in range(40):
                # the cars, the globals
                speed = rng.choice([0, rng.randrange(-0x20000, 0x20000), rng.randrange(-0x800000, 0x800000)])
                f = {"speed": speed, "road_speed": rng.choice([speed, rng.randrange(-0x800000, 0x800000)]),
                     "gear": rng.randrange(-2, 7), "gear_now": rng.randrange(-2, 7),
                     "rpm": rng.randrange(0x600, 0x2000),
                     "side": rng.choice([rng.randrange(-0x40000, 0x40000), rng.randrange(-0x2000000, 0x2000000)]),
                     "fwd": rng.choice([rng.randrange(-0x40000, 0x40000), rng.randrange(-0x2000000, 0x2000000)]),
                     "surface": rng.choice([0, 1, 2, 3, 3, rng.randrange(4, 7)]), "player": rng.randrange(3)}
                if rng.randrange(4) == 0:      # the same car again: the shakes kept
                    f.update({x: last[x] for x in ("speed", "road_speed", "surface")}) if k else None
                last = f
                viewed, humans = rng.randrange(3), rng.randrange(1, 3)
                clock += rng.choice([0, 1, 5, 20, 30, 100, -50])
                cp = L.ff_test_car(0, *[f[x] for x, o in CAR])
                L.ff_test_car(1, 0, 0, 0, 0, 0, 0, 0, 0, 1)
                L.ff_test_globals(viewed, humans, clock)
                for x, o in CAR:
                    s.mu.mem_write(O.cars[0] + o, struct.pack("<i", f[x]))
                s.write(0x12dd54, struct.pack("<i", viewed))
                s.write(0x12da30, struct.pack("<i", humans))
                s.write(0xed100, struct.pack("<i", clock))
                what = rng.choice(["object", "cars", "wall", "gear", "step", "step", "step", "crash",
                                   "stop", "forces"])
                O.sent = []
                a = O.cars[0]
                if what == "object":
                    across = rng.randrange(-0x20000, 0x20000)
                    s.call(0x4d730, eax=a, edx=across)
                    L.ff_object(cp, across)
                elif what == "cars":
                    px = rng.randrange(-0x40000, 0x40000)
                    j = rng.choice([rng.randrange(0, 0x2000), rng.randrange(0, 0x20000),
                                    rng.randrange(0, 0x400000)])
                    s.call(0x4d7a4, px, 0, 0, j, eax=a, edx=O.cars[1])
                    L.ff_cars(cp, px, j)
                elif what == "wall":
                    side = rng.randrange(2)
                    s.call(0x4d878, eax=a, edx=side)
                    L.ff_wall(side)
                elif what == "gear":
                    s.call(0x4d8c0, eax=a)
                    L.ff_gear(cp)
                elif what == "step":
                    s.call(0x4d9a8, eax=a)
                    s.call(0x4d928, eax=a)
                    L.ff_step(cp)
                elif what == "crash":
                    on = rng.randrange(2)
                    s.call(0x4dc40, eax=on)
                    L.ff_crash(on)
                elif what == "stop":
                    s.call(0x4d714)
                    L.ff_stop()
                else:
                    on = rng.randrange(2)
                    s.call(0x4d700 if on else 0x4d6ec)
                    L.ff_forces(on)
                mine = packets(bytes(buf[:L.ff_test_take(buf)]))
                theirs = O.sent
                after_o = [struct.unpack("<i" if w == 4 else "<h", bytes(s.read(at, w)))[0] for _, at, w in STATE]
                after_p = [getattr(g, x) for x, _, _ in STATE]
                counts[what] = counts.get(what, 0) + 1
                for p in theirs:
                    counts["cmd %02x" % p[0]] = counts.get("cmd %02x" % p[0], 0) + 1
                if mine != theirs or after_o != after_p:
                    bad += 1
                    if bad <= 10:
                        print("%s run %d call %d %s: original %s %s, port %s %s; car %s viewed %d humans %d clock %d"
                              % (os.path.basename(path), run, k, what, theirs, after_o, mine, after_p, f,
                                 viewed, humans, clock))
    print("calls:", ", ".join("%s %d" % kv for kv in sorted(counts.items())))
    print("%d of %d calls differ" % (bad, sum(v for x, v in counts.items() if not x.startswith("cmd"))))
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
