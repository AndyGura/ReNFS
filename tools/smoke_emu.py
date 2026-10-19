#!/usr/bin/env python3
"""The crash's smoke painted, pixel by pixel: the original's 0x64a74 (the puffs into the
view, sorted far first, each the picture "smok" stretched by 0x96e40 / 0x96aa8 and added
to the screen through the table [0x1464cc]) run on a snapshot, against the port's
smoke_draw (port/src/smoke.c) with the same camera, view, puffs and screen. The puffs are
made up: placed round the camera's view at random distances (some too near, some behind),
ages and order, a few dead; the screen a pattern. The picture and the table are the
snapshot's (racepal_emu.py checks the port's own). Also compares the order left behind.
Writes build/shots/smoke_<snapshot>_<k>_{orig,port}.png.

    build/venv/bin/python tools/smoke_emu.py build/snap/e3.bin [...]
"""
import ctypes, os, random, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from snap_emu import Snap, game
from poly_emu import Vec3, View
from camera_emu import Camera
import roadpic_emu as R
import cars_emu

I, U8, P = ctypes.c_int, ctypes.c_ubyte, ctypes.c_void_p
SMOKE, SMOKE_LEN = 0x1464d0, 0x96c
ROUNDS = 8
CLIPS = [(0, 0, 320, 127), (0, 17, 320, 177), (13, 30, 251, 140)]


class Shape(ctypes.Structure):
    _fields_ = [("head", P), ("type", I), ("w", I), ("h", I), ("cx", I), ("cy", I),
                ("x", I), ("y", I), ("pixels", P)]


class SmokeArt(ctypes.Structure):
    _fields_ = [("d", P), ("len", ctypes.c_long), ("puff", Shape), ("add", U8 * 0x10000)]


def puffs(s, rnd, dense):
    """100 puffs round what the camera sees: (pos, age, size, order) as the block holds them"""
    cam = s.u32(0x143df4)
    c = struct.unpack("<3i", s.raw(cam + 0x10, 12))
    m = struct.unpack("<9i", s.raw(s.u32(0x143df8), 36))
    pos, age, size = [], [], []
    for i in range(100):
        r = rnd.random()
        z = (rnd.randint(-40, 8) if r < 0.05 else rnd.randint(9, 200) if r < 0.08 + 0.3 * dense
             else rnd.randint(200, 900) if r < 0.4 else rnd.randint(900, 8000))
        x, y = rnd.randint(-abs(z) - 20, abs(z) + 20), rnd.randint(-abs(z) // 2 - 10, abs(z) // 2 + 10)
        # the view's (x, y, z) back into the world: the matrix's rows (r = p M)
        w = [x * m[3 * k] + y * m[3 * k + 1] + z * m[3 * k + 2] for k in range(3)]
        pos.append(tuple(c[k] + (w[k] >> 5) + rnd.randint(0, 2047) for k in range(3)))
        a = rnd.choice([-1, 0xffff0000 - (1 << 32), -1, -1, rnd.randint(0, 0xffff),
                        rnd.randint(0, 0x4000), rnd.randint(0x4000, 0xffff), 0])
        age.append(a)
        size.append(rnd.randint(0, 0xff) << 7)
    order = list(range(100))
    rnd.shuffle(order)
    if rnd.random() < 0.3:
        order = list(range(100))
    raw = b"".join(struct.pack("<3i", *p) for p in pos) + struct.pack("<100i", *age) + \
        struct.pack("<100i", *order) + struct.pack("<100i", *size) + struct.pack("<3i", 0, 0, 0)
    return raw


def screen(k):
    return bytes((x * 5 + y * 3 + k * 17) % 251 for y in range(R.H) for x in range(R.W))


def original(s, raw, clip, off, k):
    mu = s.mu
    s.write(SMOKE, raw)
    mu.mem_write(R.SCREEN, screen(k))
    s.write(0xf88a4, struct.pack("<4i", *clip))
    at = R.SCREEN + 0x70000
    mu.mem_write(at, struct.pack("<2i", *off))
    s.call(0x64a74, eax=game(SMOKE), edx=at)
    return bytes(mu.mem_read(R.SCREEN, R.W * R.H)), s.read(SMOKE + 0x640, 400)


def setup_port(s, L):
    v = View.in_dll(L, "g_view")
    v.cx, v.cy, v.min_z, v.focal = s.i32(0xf7a28), s.i32(0xf7a2c), s.i32(0xf7938), s.i32(0xf7a30)
    for i, r in enumerate(struct.unpack("<1024i", s.raw(s.u32(0xf7a14), 4096))):
        v.recip[i] = r
    v.far_shift, v.max_level = s.read(0xf6aa4, 1)[0], s.i32(0xeaad8)
    v.clip_x, v.clip_y, v.clip_z = s.i32(0x143e54), s.i32(0x143e58), s.i32(0x143e5c)
    cam_a = s.u32(0x143df4)
    cam = Camera()
    cam.pos = Vec3(*struct.unpack("<3i", s.raw(cam_a + 0x10, 12)))
    camm = (I * 9)(*struct.unpack("<9i", s.raw(s.u32(0x143df8), 36)))
    ctypes.c_void_p.in_dll(L, "g_camera").value = ctypes.addressof(cam)
    ctypes.c_void_p.in_dll(L, "g_camera_m").value = ctypes.addressof(camm)
    art = SmokeArt.in_dll(L, "g_smoke")
    sh = s.u32(0xf6db0)
    head = s.raw(sh, 16)
    w, h, cx, cy, x, y = struct.unpack("<6h", head[4:16])
    pix = ctypes.create_string_buffer(s.raw(sh, 16 + w * h), 16 + w * h)
    art.puff = Shape(ctypes.addressof(pix), head[0], w, h, cx, cy, x, y, ctypes.addressof(pix) + 16)
    ctypes.memmove(art.add, s.raw(s.u32(0x1464cc), 0x10000), 0x10000)
    return cam, camm, pix


def port(L, raw, clip, off, k):
    block = ctypes.create_string_buffer(raw, SMOKE_LEN)
    fb = (U8 * (R.W * R.H)).from_buffer_copy(screen(k))
    L.raster_window(fb, R.W, *clip)
    L.smoke_draw(block, *off)
    return bytes(fb), block.raw[0x640:0x640 + 400]


def check(path, L):
    s = Snap(path)
    tag = os.path.splitext(os.path.basename(path))[0]
    # the original's screen: R.W x R.H at R.SCREEN, its rows' table
    s.mu.mem_map(R.SCREEN, 0x80000)
    s.mu.mem_write(R.ROWS, struct.pack("<%dI" % R.H, *[R.SCREEN + R.W * y for y in range(R.H)]))
    # (and as an 8-bit screen R.W wide: a snapshot may have another picture current)
    cols = R.SCREEN + 0x78000
    s.mu.mem_write(cols, struct.pack("<%di" % R.W, *range(R.W)))
    s.write(0xf88c4, struct.pack("<III", R.ROWS, 0, cols))
    s.write(0xf88bc, struct.pack("<i", R.W))
    s.write(0xf88c0, struct.pack("<i", 0))
    s.write(0xf88b5, bytes([0x7b]))
    keep = setup_port(s, L)
    rnd = random.Random(tag)
    pal = [tuple(4 * c for c in s.read(0x13cb48 + 3 * i, 3)) for i in range(256)]
    results = []
    for k in range(ROUNDS):
        raw = puffs(s, rnd, k == ROUNDS - 1)
        clip = CLIPS[k % len(CLIPS)]
        off = (rnd.randint(-9, 9), rnd.randint(-9, 9)) if k % 2 else (0, 0)
        want, want_order = original(s, raw, clip, off, k)
        got, got_order = port(L, raw, clip, off, k)
        d = [i for i in range(R.W * R.H) if want[i] != got[i]]
        changed = sum(1 for a, b in zip(want, screen(k)) if a != b)
        if d or k == 0:
            R.png("build/shots/smoke_%s_%d_orig.png" % (tag, k), want, pal)
            R.png("build/shots/smoke_%s_%d_port.png" % (tag, k), got, pal)
        results.append("%d%s" % (len(d), "" if want_order == got_order else " (order differs)"))
        print("%s round %d (window %s, moved %s): %d pixels painted, %d differ%s%s" % (
            tag, k, clip, off, changed, len(d), " (first %d, %d)" % (d[0] % R.W, d[0] // R.W) if d else "",
            "" if want_order == got_order else "; the order differs"))
    del keep
    return all(r == "0" for r in results)


def main():
    L = cars_emu.lib()
    L.files_init(b"NFSSE", None)
    if not L.exe_load():
        sys.exit("no NFS_DOS.EXE")
    os.makedirs("build/shots", exist_ok=True)
    ok = all([check(p, L) for p in sys.argv[1:]])
    print("all equal" if ok else "DIFFERENCES")


if __name__ == "__main__":
    main()
