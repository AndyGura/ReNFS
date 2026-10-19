#!/usr/bin/env python3
"""The game's textured-quad painters (0x91360, and 0x90b30 that leaves colour 255 out) run
in a CPU emulator against the port's port/src/raster.c, on random quads. Needs unicorn
(build/venv, see tools/unpack_emu.py) and a C compiler.

    build/venv/bin/python tools/raster_emu.py [count] [seed]

Prints the quads whose pixels differ, and the totals.
"""
import ctypes, os, random, struct, subprocess, sys
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP

IMAGE = "build/nfs_flat.bin"
PLAIN, SEE_THROUGH, FATAL = 0x91360, 0x90b30, 0x92484
X0, Y0, X1, Y1, BITS, ROWTAB, BASE = 0xf88a4, 0xf88a8, 0xf88ac, 0xf88b0, 0xf88b4, 0xf88c4, 0xf88c8
MEM, MEMSIZE = 0x400000, 0x100000         # the texture in the middle, its neighbours around
TEX = MEM + 0x80000
FB, ROWS, ARGS, STACK, DONE = 0x800000, 0x900000, 0xa00000, 0x1000000, 0x2000000
W, H = 640, 480


class Original:
    def __init__(self, mem):
        self.mu = mu = Uc(UC_ARCH_X86, UC_MODE_32)
        mu.mem_map(0, 0x200000)
        mu.mem_write(0, open(IMAGE, "rb").read())
        mu.mem_map(MEM, MEMSIZE)
        mu.mem_write(MEM, mem)
        mu.mem_map(FB, W * H + 0x1000)
        mu.mem_map(ROWS, 0x10000)
        mu.mem_write(ROWS, struct.pack("<%dI" % H, *[FB + y * W for y in range(H)]))
        mu.mem_map(ARGS, 0x1000)
        mu.mem_map(STACK - 0x100000, 0x101000)
        mu.mem_map(DONE, 0x1000)
        mu.mem_write(ROWTAB, struct.pack("<II", ROWS, 0))
        mu.mem_write(BITS, b"\x08")
        self.fatal = False

        def hook(mu, addr, size, _):
            self.fatal = True
            mu.emu_stop()
        mu.hook_add(UC_HOOK_CODE, hook, begin=FATAL, end=FATAL)

    def quad(self, fb, win, uv, xy, see_through):
        mu = self.mu
        mu.mem_write(FB, bytes(fb))
        mu.mem_write(X0, struct.pack("<4i", win[0], win[1], win[2], win[3]))
        mu.mem_write(ARGS, struct.pack("<8i", *uv) + struct.pack("<8i", *xy))
        sp = STACK - 0x100
        mu.mem_write(sp, struct.pack("<4I", DONE, TEX, ARGS, ARGS + 32))
        mu.reg_write(UC_X86_REG_ESP, sp)
        mu.emu_start(SEE_THROUGH if see_through else PLAIN, DONE)
        return bytes(mu.mem_read(FB, W * H))


def port_lib():
    out = "build/raster_test.so"
    subprocess.check_call(["cc", "-O1", "-shared", "-fPIC", "-o", out, "port/src/raster.c"])
    return ctypes.CDLL(os.path.abspath(out))


class Tex(ctypes.Structure):
    _fields_ = [("pixels", ctypes.c_void_p), ("lo", ctypes.c_void_p), ("hi", ctypes.c_void_p),
                ("rowbytes", ctypes.c_int), ("w", ctypes.c_int), ("h", ctypes.c_int),
                ("see_through", ctypes.c_int)]


def main():
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 2000
    rnd = random.Random(int(sys.argv[2]) if len(sys.argv) > 2 else 1)
    tw, th = 64, 64
    mem = bytearray(rnd.randrange(256) for _ in range(MEMSIZE))
    for i in range(0, MEMSIZE, 7):                 # plenty of see-through pixels
        mem[i] = 255
    mem[TEX - MEM:TEX - MEM + 16] = struct.pack("<I6H", 0x7b | 0x100000, tw, th, 0, 0, 0, 0)
    orig = Original(bytes(mem))
    lib = port_lib()
    cmem = (ctypes.c_ubyte * MEMSIZE).from_buffer(mem)
    base = ctypes.addressof(cmem)
    tex = Tex(base + TEX - MEM + 16, base, base + MEMSIZE, tw, tw, th, 0)
    cfb = (ctypes.c_ubyte * (W * H))()
    bad = fatal = painted = 0
    for n in range(count):
        kind = n % 4
        win = [0, 0, 320, 200] if kind < 2 else \
              [rnd.randrange(0, 100), rnd.randrange(0, 100), rnd.randrange(200, 641), rnd.randrange(150, 481)]
        span = 40 if kind == 0 else 400
        cx, cy = rnd.randrange(-50, 400), rnd.randrange(-50, 300)
        xy = [v for _ in range(4) for v in (cx + rnd.randrange(-span, span), cy + rnd.randrange(-span, span))]
        if kind == 3:                              # a proper convex quad, corners round in order
            r = rnd.randrange(2, 200)
            xy = [cx - r, cy - r, cx + r + rnd.randrange(-r, r), cy - r, cx + r, cy + r, cx - r, cy + r + rnd.randrange(-r, r)]
            if rnd.random() < 0.5:
                xy = xy[6:] + xy[4:6] + xy[2:4] + xy[0:2]
        uv = [rnd.randrange(0, tw << 16) if i % 2 == 0 else rnd.randrange(0, th << 16) for i in range(8)]
        see = rnd.random() < 0.3
        fb = bytes([n & 0xff]) * (W * H)
        want = orig.quad(fb, win, uv, xy, see)
        if orig.fatal:
            orig.fatal = False
            fatal += 1
            continue
        ctypes.memmove(cfb, fb, W * H)
        lib.raster_window(cfb, W, *win)
        tex.see_through = see
        lib.raster_quad(ctypes.byref(tex), (ctypes.c_int * 8)(*uv), (ctypes.c_int * 8)(*xy))
        got = bytes(cfb)
        if want != fb:
            painted += 1
        if got != want:
            bad += 1
            diff = sum(a != b for a, b in zip(got, want))
            if bad <= 10:
                print("quad %d: %d pixels differ  win %s xy %s uv %s see %d" % (n, diff, win, xy, uv, see))
    print("%d quads (%d painted something, %d stopped by the original's fatalerror), %d differ"
          % (count, painted, fatal, bad))


if __name__ == "__main__":
    main()
