#!/usr/bin/env python3
"""The tyres' marks, the original's code against the port's (port/src/skid.c), on
snapshots of a race (their camera, view, track and tables), with random rings of marks:

  - the recorder 0x671dc: random slides, tyres, nodes, surfaces and places of the player's
    car (near the last record, a little way on, a long way on), the ring's place anywhere
    and at its two ends: the whole ring after (with the record before it), its place, the
    last slides and centre, and the lines its painting drew (0x679ec by record);
  - the rows' marks 0x67784, then the road's rows 0x679ec(-999) one by one with random
    edges: the table, the lines and every record's points seen and on the screen;
  - the line 0x989a0 itself: random lines and widths painted on the snapshot's screen;
  - the dust 0x664bc behind and in front of the car: random slides, tyres, distances,
    angles round the car, records' ages and places: the puffs (which picture, size,
    place) and EA's random numbers after (the port's dust keeps its own: seeded equal);
  - the dirt's stretch 0x973d0 (see-through) with the snapshot's own dir pictures.

The original's lines are caught at 0x989a0 (its arguments read, the call stepped over),
its puffs at 0x96f70 and 0x97500; the port's through g_skid_line and g_skid_puff.

    build/venv/bin/python tools/skid_emu.py [--n=300] [snapshot ..]   (default: h7, e3)
"""
import ctypes, os, random, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from snap_emu import Snap, game, STACK
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP
import cars_emu as C
import roadpic_emu as R

I, U8 = ctypes.c_int, ctypes.c_ubyte
RING, REC = 0x149574, 0xa0
BEFORE = RING - REC            # the record before the ring (0x1494d4)
RING_BYTES = 127 * REC         # it, the ring's 125 and the one after
BUF = STACK - 0x400            # the row's edge points (game addresses, above the stack)


def lib():
    import subprocess
    out = "build/skid_test_%d.so" % os.getpid()
    subprocess.check_call(["cc", "-O1", "-g", "-fwrapv", "-shared", "-fPIC", "-o", out] +
                          ["port/src/" + f for f in C.SOURCES] +
                          ["tools/timing_test.c", "tools/collide_test.c", "tools/replay_stub.c",
                           "tools/cars_test.c", "tools/skid_test.c", "-Iport/src"] +
                          subprocess.check_output(["sdl2-config", "--cflags", "--libs"]).decode().split())
    L = ctypes.CDLL(os.path.abspath(out))
    os.unlink(out)
    return L


def ints(*v):
    return (I * len(v))(*v)


class Check:
    def __init__(self, path, L, rng):
        self.s = s = Snap(path)
        self.L, self.rng = L, rng
        self.tag = os.path.splitext(os.path.basename(path))[0]
        self.cam_a = s.u32(0x143df4)
        self.cam_node = struct.unpack("<i", s.raw(self.cam_a + 0x38, 4))[0]
        self.pos = list(struct.unpack("<3i", s.raw(self.cam_a + 0x10, 12)))
        self.m = list(struct.unpack("<9i", s.raw(s.u32(0x143df8), 36)))
        self.player = s.u32(0x13c24c + 4 * s.i32(0x12dd58))
        s.write(0x12d9d7, b"\0")
        # the original's lines caught
        self.lines, self.catching, self.nlines, self.npuffs = [], True, 0, 0

        def on_line(mu, addr, size, _):
            if not self.catching:
                return
            sp = mu.reg_read(UC_X86_REG_ESP)
            v = struct.unpack("<I5i", bytes(mu.mem_read(sp, 24)))
            self.lines.append(tuple(v[1:]))
            mu.reg_write(UC_X86_REG_EIP, v[0])
            mu.reg_write(UC_X86_REG_ESP, sp + 4)
        s.mu.hook_add(UC_HOOK_CODE, on_line, None, begin=game(0x989a0), end=game(0x989a0))
        # .. and its puffs: (dirt, which picture, size, x, y)
        self.puffs = []
        dust = [s.u32(0x1464bc + 4 * k) for k in range(4)]
        dirt = [s.u32(0x1460ac + 4 * k) for k in range(4)]

        def on_puff(mu, addr, size, dirty):
            if not self.catching:
                return
            sp = mu.reg_read(UC_X86_REG_ESP)
            v = struct.unpack("<IiIii", bytes(mu.mem_read(sp, 20)))
            pics = dirt if dirty else dust
            self.puffs.append((dirty, pics.index(v[2]) if v[2] in pics else -1, v[1], v[3], v[4]))
            mu.reg_write(UC_X86_REG_EIP, v[0])
            mu.reg_write(UC_X86_REG_ESP, sp + 4)
        s.mu.hook_add(UC_HOOK_CODE, on_puff, 0, begin=game(0x96f70), end=game(0x96f70))
        s.mu.hook_add(UC_HOOK_CODE, on_puff, 1, begin=game(0x97500), end=game(0x97500))
        self.dirt_shapes = dirt
        recip = struct.unpack("<1024i", s.raw(s.u32(0xf7a14), 4096))
        L.skid_test_view(ints(*self.pos), ints(*self.m), s.i32(0xf7a28), s.i32(0xf7a2c),
                         s.i32(0xf7938), (I * 1024)(*recip))
        self.slide = rng.randrange(-1, 7)
        self.moved = {}

    def world(self):
        """the snapshot's numbers into the port (and the loading picture's flags into both)"""
        s, L = self.s, self.L
        used = bytearray(22)
        if self.slide >= 0:
            used[self.slide] = 1
        s.write(0x14e602, bytes(used))
        L.skid_test_world(s.u32(0x13bb88), s.i32(0x13bb8c), s.i32(0x12c308), s.read(0x12d9d9, 1)[0],
                          s.i32(0x145c94), self.slide, (I * 16)(*struct.unpack("<16i", s.read(0x12cc00, 64))),
                          (U8 * 256)(*s.read(0xf8118, 256)))

    # --- the ring, both sides

    def seen_to_world(self, v):
        """a point seen from the camera (1/512 units) back into the world"""
        m = self.m
        d = [(v[0] * m[3 * j] + v[1] * m[3 * j + 1] + v[2] * m[3 * j + 2]) >> 16 for j in range(3)]
        return [self.pos[j] + (d[j] << 9) for j in range(3)]

    def random_record(self, node0):
        rng = self.rng
        z = rng.choice([rng.randint(-0x200, 0x80), rng.randint(0x20, 0x400), rng.randint(0x100, 0x1800)])
        v = [rng.randint(-abs(z) - 0x100, abs(z) + 0x100), rng.randint(-0x200, 0x200), z]
        ctr = self.seen_to_world(v)
        corners = [rng.randint(-0x120, 0x120) if j % 3 != 1 else rng.randint(-0x30, 0x30)
                   for j in range(12)]
        flag = lambda: rng.choice([0, 0, rng.randint(1, 0x1000000), rng.randint(0x3c0000, 0x2000000)])
        node = node0 + rng.randint(-3, 3) if rng.random() < 0.9 else rng.randint(-50, 5000)
        view = [rng.randint(-0x1000, 0x1000) for _ in range(12)]
        scr = [rng.randint(-100, 800) for _ in range(8)]
        return struct.pack("<12i3i12i8i5i", *(corners + ctr + view + scr +
                                              [node, flag(), flag(), rng.randint(0, 5000), rng.randint(1, 2)]))

    def random_ring(self):
        rng = self.rng
        recs = []
        node = self.cam_node + rng.randint(-2, 16)
        for i in range(125):
            if rng.random() < 0.15:
                node = self.cam_node + rng.randint(-4, 20)
            recs.append(self.random_record(node))
        # the record before the ring: zeros but for its points seen
        before = bytes(0x3c) + bytes(self.rng.getrandbits(8) for _ in range(0x50)) + bytes(0x14)
        ring = before + b"".join(recs) + bytes(REC)
        at = rng.choice([0, 1, 124, 123, rng.randrange(125)])
        last = self.seen_to_world([rng.randint(-0x400, 0x400), 0, rng.randint(0, 0x1000)])
        st = struct.pack("<i2i3i", at, rng.choice([0, rng.randint(0x3c0000, 0x900000)]),
                         rng.choice([0, rng.randint(0x3c0000, 0x900000)]), *last)
        rows = bytes(rng.choice([0, 0, 1]) for _ in range(15))
        self.put(ring, st, rows)

    def put(self, ring, st, rows):
        s, L = self.s, self.L
        s.write(BEFORE, ring)
        at, f, r, x, y, z = struct.unpack("<6i", st)
        s.write(0xf6e2c, struct.pack("<i", at))
        s.write(0xf6ff4, struct.pack("<2i", f, r))
        s.write(0x14e620, struct.pack("<3i", x, y, z))
        s.write(0x14e5f3, rows)
        g = ctypes.addressof(ctypes.c_char.in_dll(L, "g_skid"))
        ctypes.memmove(g, ring, RING_BYTES)
        ctypes.memmove(g + RING_BYTES, st, len(st))
        ctypes.memmove(g + RING_BYTES + len(st), rows, 15)

    def compare(self, what, port_lines):
        """the ring and its state, and the lines, both sides"""
        s, L = self.s, self.L
        g = ctypes.addressof(ctypes.c_char.in_dll(L, "g_skid"))
        mine = ctypes.string_at(g, RING_BYTES + 24 + 15)
        theirs = s.read(BEFORE, RING_BYTES) + s.read(0xf6e2c, 4) + s.read(0xf6ff4, 8) + \
            s.read(0x14e620, 12) + s.read(0x14e5f3, 15)
        bad = []
        if mine != theirs:
            for k in range(0, len(mine), 4):
                if mine[k:k + 4] != theirs[k:k + 4]:
                    if k < RING_BYTES:
                        where = "record %d +0x%x" % (k // REC - 1, k % REC)
                    else:
                        where = "state +0x%x" % (k - RING_BYTES)
                    bad.append("%s: original %s, port %s" % (
                        where, struct.unpack("<i", theirs[k:k + 4].ljust(4, b"\0"))[0],
                        struct.unpack("<i", mine[k:k + 4].ljust(4, b"\0"))[0]))
        self.nlines += len(port_lines)
        if self.lines != port_lines:
            bad.append("lines: original %d %s, port %d %s" % (len(self.lines), self.lines[:4],
                                                              len(port_lines), port_lines[:4]))
        if bad:
            print("%s %s: %d differ: %s" % (self.tag, what, len(bad), "; ".join(bad[:6])))
        return not bad

    def port_lines(self):
        out = (I * (5 * 4096))()
        n = self.L.skid_test_lines(out, 4096)
        return [tuple(out[5 * k:5 * k + 5]) for k in range(min(n, 4096))]

    def port_puffs(self):
        out = (I * (5 * 256))()
        n = self.L.skid_test_puffs(out, 256)
        return [tuple(out[5 * k:5 * k + 5]) for k in range(min(n, 256))]

    # --- the cases

    def dust(self):
        rng, s, L = self.rng, self.s, self.L
        self.random_ring()
        self.world()
        steps = s.i32(0x145c94)
        # the newest records young, their screen points on the screen or not projected
        for i in range(125):
            b = RING + REC * i
            age = rng.choice([0, rng.randint(1, 0x2f), 0x30, rng.randint(-5, 200)])
            s.write(b + 0x98, struct.pack("<i", steps - age))
            pts = [rng.randint(-50, R.W + 50) if j % 2 == 0 else rng.randint(-50, R.H + 50) for j in range(8)]
            if rng.random() < 0.1:
                pts[2 * rng.randrange(4)] = -0x80000000
            s.write(b + 0x6c, struct.pack("<8i", *pts))
        g = ctypes.addressof(ctypes.c_char.in_dll(L, "g_skid"))
        ctypes.memmove(g, s.read(BEFORE, RING_BYTES), RING_BYTES)
        seed = [rng.getrandbits(32) for _ in range(6)]
        s.write(0xf8a8c, struct.pack("<6I", *seed))
        L.skid_test_random((ctypes.c_uint * 6)(*seed))
        car = self.player
        slide = lambda: rng.choice([0, rng.randint(1, 0x3c0000), rng.randint(0x3c0000, 0x4000000), -rng.randint(1, 0x400000)])
        sf, sr = slide(), slide()
        tf, tr = rng.choice([0, 0, 1, 2, 3]), rng.choice([0, 0, 1, 2, 3])
        far = 1 if rng.random() < 0.1 else 0
        z = rng.choice([0x20, rng.randint(0x20, 0x200), rng.randint(0x200, 0x1900)])
        yaw = rng.randint(0, 0xffffff)
        pos = [self.pos[0] + rng.randint(-0x200000, 0x200000), self.pos[1], self.pos[2] + rng.randint(-0x200000, 0x200000)]
        behind = rng.randrange(2)
        s.mu.mem_write(car, struct.pack("<3i", *pos))
        s.mu.mem_write(car + 0x351, struct.pack("<i", yaw))
        s.mu.mem_write(car + 0x399, struct.pack("<i", z))
        s.mu.mem_write(car + 0x405, struct.pack("<2i", sf, sr))
        s.mu.mem_write(car + 0x425, bytes([tf, tr]))
        s.mu.mem_write(car + 0x78e, struct.pack("<i", far))
        self.puffs = []
        L.skid_test_catch(1)
        s.call(0x664bc, eax=car, edx=behind)
        L.skid_test_dust(sf, sr, tf, tr, far, z, ints(*pos), yaw, behind)
        mine = self.port_puffs()
        rnd = (ctypes.c_uint * 6)()
        L.skid_test_random_get(rnd)
        bad = []
        self.npuffs += len(mine)
        if mine != self.puffs:
            bad.append("puffs: original %d %s, port %d %s" % (len(self.puffs), self.puffs[:3], len(mine), mine[:3]))
        if list(rnd) != list(struct.unpack("<6I", s.read(0xf8a8c, 24))):
            bad.append("random numbers")
        if bad:
            print("%s dust: %s" % (self.tag, "; ".join(bad)))
        return not bad

    def dirt(self):
        """0x973d0 painting a dir picture, the screen compared"""
        rng, s, L = self.rng, self.s, self.L
        W, H = R.W, R.H
        pix = bytes(rng.getrandbits(8) for _ in range(W * H))
        s.mu.mem_write(R.SCREEN, pix)
        fbuf = (U8 * (W * H)).from_buffer_copy(pix)
        L.raster_window(fbuf, W, *R.CLIP)
        a = self.dirt_shapes[rng.randrange(4)]
        w, h, cx, cy = struct.unpack("<4h", s.raw(a + 4, 8))
        data = (U8 * (w * h)).from_buffer_copy(s.raw(a + 0x10, w * h))
        scale = rng.choice([rng.randint(0x1000, 0x10000), rng.randint(0x10000, 0x40000), -rng.randint(0x1000, 0x20000)])
        x, y = rng.randint(-60, W + 60), rng.randint(-60, H + 60)
        self.catching = False
        s.call(0x973d0, scale, a, x, y)
        self.catching = True
        L.skid_test_solid(scale, w, h, cx, cy, data, x, y)
        want = bytes(s.mu.mem_read(R.SCREEN, W * H))
        d = sum(1 for p, q in zip(want, bytes(fbuf)) if p != q)
        if d:
            print("%s dirt %dx%d scale %x at (%d,%d): %d pixels differ" % (self.tag, w, h, scale, x, y, d))
        return not d

    def record(self):
        rng, s, L = self.rng, self.s, self.L
        self.random_ring()
        self.world()
        slide = lambda: rng.choice([0, rng.randint(0, 0x3c0000), 0x3c0000, rng.randint(0x3c0000, 0x1000000)])
        sf, sr = slide(), slide()
        tf, tr = rng.choice([0, 1, 2, 3, 4, 7]), rng.choice([0, 1, 2, 3, 4, 7])
        node = self.cam_node + rng.randint(-3, 18)
        surface = rng.randrange(4)
        along = [rng.randint(-0x100, 0x100), rng.randint(-0x20, 0x20), rng.randint(-0x100, 0x100)]
        across = [rng.randint(-0x80, 0x80), rng.randint(-0x20, 0x20), rng.randint(-0x80, 0x80)]
        last = struct.unpack("<3i", s.read(0x14e620, 12))
        k = rng.random()
        far = rng.randint(0, 0x8000) if k < 0.3 else rng.randint(0xa000, 0x30000) if k < 0.6 \
            else rng.randint(0x2f000, 0x200000)
        a = rng.random()
        centre = [last[0] + int(far * a) * rng.choice([1, -1]), last[1] + rng.randint(-0x8000, 0x8000),
                  last[2] + int(far * (1 - a)) * rng.choice([1, -1])]
        car = self.player
        s.mu.mem_write(car + 0x405, struct.pack("<2i", sf, sr))
        s.mu.mem_write(car + 0x425, bytes([tf, tr]))
        s.mu.mem_write(car + 0x48, struct.pack("<i", node))
        s.mu.mem_write(car + 0x4a2, struct.pack("<i", surface))
        self.lines = []
        L.skid_test_catch(1)
        before = s.i32(0xf6e2c)
        s.call(0x671dc, *(along + across + centre), eax=car)
        moved = (s.i32(0xf6e2c) - before) % 125
        self.moved[moved] = self.moved.get(moved, 0) + 1
        L.skid_test_car(sf, sr, tf, tr, node, surface, ints(*along), ints(*across), ints(*centre))
        return self.compare("record", self.port_lines())

    def rows(self):
        rng, s, L = self.rng, self.s, self.L
        self.random_ring()
        self.world()
        self.lines = []
        L.skid_test_catch(1)
        s.call(0x67784, eax=self.cam_node)
        L.skid_rows(self.cam_node)
        ok = self.compare("rows", [])
        mask, length = s.u32(0x13bb88), s.i32(0x13bb8c)
        for d in range(18, -2, -1):
            row = (self.cam_node - 2 + d) & mask
            if row & 0x80000000:
                row -= 1 << 32
            w = R.W
            a = [rng.randint(-50, w + 50), rng.randint(-50, 500)]
            b = [rng.randint(-50, w + 50), rng.randint(-50, 500)]
            s.mu.mem_write(BUF, struct.pack("<4i", *(a + b)))
            s.call(0x679ec, BUF, BUF + 8, eax=-999, edx=self.cam_node, ebx=row, ecx=0)
            L.skid_test_row(self.cam_node, row, ints(*a), ints(*b))
            ok = self.compare("row %d" % d, self.port_lines()) and ok
            self.lines = []
        return ok

    def line(self):
        """0x989a0 painting, the screen compared"""
        rng, s, L = self.rng, self.s, self.L
        W, H = R.W, R.H
        L.skid_test_catch(0)
        pix = bytes(rng.getrandbits(8) for _ in range(W * H))
        s.mu.mem_write(R.SCREEN, pix)
        fbuf = (U8 * (W * H)).from_buffer_copy(pix)
        L.raster_window(fbuf, W, *R.CLIP)
        cx, cy = W // 2, H // 2
        p = [rng.randint(-W // 2, W + W // 2) for _ in range(2)] + [rng.randint(-H // 2, H + H // 2) for _ in range(2)]
        x0, x1, y0, y1 = p
        if rng.random() < 0.3:
            x1, y1 = x0 + rng.randint(-3, 3), y0 + rng.randint(-3, 3)
        w = rng.randint(1, 15)
        self.catching = False
        s.call(0x989a0, x0, y0, x1, y1, w)
        self.catching = True
        L.skid_line(x0, y0, x1, y1, w)
        want = bytes(s.mu.mem_read(R.SCREEN, W * H))
        d = sum(1 for a, b in zip(want, bytes(fbuf)) if a != b)
        if d:
            print("%s line (%d,%d)-(%d,%d) w %d: %d pixels differ" % (self.tag, x0, y0, x1, y1, w, d))
        return not d


def main():
    n = 300
    args = []
    for a in sys.argv[1:]:
        if a.startswith("--n="):
            n = int(a[4:])
        else:
            args.append(a)
    snaps = args or ["build/snap/h7.bin", "build/snap/e3.bin"]
    L = lib()
    L.files_init(b"NFSSE", None)
    if not L.exe_load():
        sys.exit("no NFS_DOS.EXE")
    total = {"record": [0, 0], "rows": [0, 0], "line": [0, 0], "dust": [0, 0], "dirt": [0, 0]}
    for p in snaps:
        rng = random.Random(p)
        c = Check(p, L, rng)
        R.screen_for(c.s)
        c.s.mu.mem_map(R.SCREEN, 0x80000)
        c.s.mu.mem_write(R.ROWS, struct.pack("<%dI" % R.H, *[R.SCREEN + R.W * y for y in range(R.H)]))
        c.s.write(0xf88c4, struct.pack("<II", R.ROWS, 0))
        c.s.write(0xf88a4, struct.pack("<4i", *R.CLIP))
        for k in range(n):
            for what in ("record", "rows", "line", "dust", "dirt"):
                if what == "rows" and k % 5:
                    continue
                c.slide = rng.randrange(-1, 7)
                ok = getattr(c, what)()
                total[what][0] += 1
                total[what][1] += ok
        print("%s: %d lines and %d puffs compared; the ring moved on by %s" % (c.tag, c.nlines, c.npuffs,
                                                                              sorted(c.moved.items())))
    print("; ".join("%s %d of %d equal" % (w, t[1], t[0]) for w, t in total.items()))


if __name__ == "__main__":
    main()
