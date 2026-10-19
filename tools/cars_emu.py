#!/usr/bin/env python3
"""The cars drawn into the road, pixel by pixel: the original's sky 0x4a9d0, 0x597e4 and
0x5a4f0 with the cars in (cars_at_node 0x6837c and all under it), run on a snapshot,
against the port's sky_draw and road_draw with cars_at_node (port/src/cardraw.c,
model.c) given the same road, view and cars. The cars are the port's own (car_load from
the CD), placed and turned as in the snapshot; the order the original's sorts left the
polygon lists in is copied over, as are the wheels' pictures. Left out on both sides: the
walls 0x59294, the 3D roadside objects 0x66738 and the racers' markers 0x66b98 (the
shadows 0x8a094 are in). Writes build/shots/cars_<snapshot>_{orig,port,diff}.png.

    build/venv/bin/python tools/cars_emu.py cy1 build/snap/e3.bin [...]
"""
import ctypes, os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP
from snap_emu import Snap, game
from poly_emu import Vec3
from camera_emu import Car, Camera
import roadpic_emu as R
import racepal_emu
from roaddraw_emu import Painters, CARS
from setup_check import Fam

I, U8, P = ctypes.c_int, ctypes.c_ubyte, ctypes.c_void_p
SOURCES = ["cardraw.c", "model.c", "polyfill.c", "camera.c", "car.c", "dash.c", "hud.c", "config.c", "view.c", "sky.c", "roaddraw.c",
           "trackinfo.c", "road.c", "poly.c", "raster.c", "track.c", "files.c", "exe.c", "trackfam.c",
           "racepal.c", "font.c", "video.c", "fsh.c", "unpack.c"]
LEFT_OUT = {0x59294: 3, 0x66738: 0}


class Model(ctypes.Structure):
    _fields_ = [("orip", P), ("lo", P), ("hi", P), ("npoints", I), ("flags", ctypes.c_uint),
                ("m", I * 9), ("pos", Vec3), ("view", ctypes.POINTER(Vec3)), ("screen", P),
                ("count", I), ("list", ctypes.POINTER(ctypes.c_uint)), ("tex", ctypes.POINTER(P)),
                ("ntex", I)]


class Group(ctypes.Structure):
    _fields_ = [("index", I), ("count", I), ("list", ctypes.POINTER(ctypes.c_uint))]


class RaceCar(ctypes.Structure):
    _fields_ = Car._fields_ + [
        ("police", I), ("cfm", P), ("cfm_len", ctypes.c_long), ("colours", P),
        ("pics", P * 2), ("pics_pal", P * 2),
        ("state", I), ("slot", I), ("draw_node", I), ("own_turn", I), ("turn", I * 9),
        ("yaw_drawn", I), ("half_len", I), ("half_wid", I), ("braking", I), ("bits", U8 * 3),
        ("seen", Vec3), ("far", I), ("shadow", Vec3), ("along", Vec3), ("across", Vec3),
        ("near_m", Model), ("far_m", Model), ("groups", Group * 4)]


def lib():
    import subprocess
    out = "build/cars_test.so"
    subprocess.check_call(["cc", "-O1", "-g", "-shared", "-fPIC", "-o", out] +
                          ["port/src/" + f for f in SOURCES] + ["-Iport/src"] +
                          subprocess.check_output(["sdl2-config", "--cflags", "--libs"]).decode().split())
    return ctypes.CDLL(os.path.abspath(out))


def original(s, n, node):
    mu = s.mu
    W, H = R.W, R.H
    mu.mem_map(R.SCREEN, 0x80000)
    mu.mem_write(R.ROWS, struct.pack("<%dI" % H, *[R.SCREEN + W * y for y in range(H)]))
    s.write(0xf88c4, struct.pack("<II", R.ROWS, 0))
    s.write(0xf88a4, struct.pack("<4i", *R.CLIP))

    def hook(mu, addr, size, k):
        sp = mu.reg_read(UC_X86_REG_ESP)
        ret = struct.unpack("<I", bytes(mu.mem_read(sp, 4)))[0]
        mu.reg_write(UC_X86_REG_ESP, sp + 4 + 4 * k)
        mu.reg_write(UC_X86_REG_EIP, ret)
    for at, k in LEFT_OUT.items():
        mu.hook_add(UC_HOOK_CODE, hook, k, begin=game(at), end=game(at))
    s.call(0x4a9d0, eax=s.u32(0x143df4))
    s.call(0x597e4)
    s.call(0x5a4f0, eax=n, edx=node)
    return bytes(mu.mem_read(R.SCREEN, W * H))


def model_state(s, inst, mine, cfm_game, cfm_port, groups):
    """the snapshot's instance at inst onto the port's: the lists' order, the pictures"""
    g = lambda a, o: struct.unpack("<i", s.raw(a + o, 4))[0]
    u = lambda a, o: struct.unpack("<I", s.raw(a + o, 4))[0]
    orip = u(inst, 0)
    ix = u(orip, 0x50)
    for grp in groups:
        if grp.index < 0 or grp.index >= 8:
            continue
        rec = u(orip, 0x4c) + 28 * grp.index
        first, count = u(rec, 0x10), u(rec, 0x14)
        for k in range(count):
            grp.list[k] = u(ix + 4 * (first + k), 0)
    for k in range(g(inst, 8)):
        mine.list[k] = u(u(inst, 0x84) + 4 * k, 0)
    tex = u(inst, 0x14)
    for k in range(mine.ntex):
        sh = u(tex + 20 * k, 0xc)
        mine.tex[k] = cfm_port + (sh - cfm_game) if sh else None
    mine.flags = u(inst, 0x10)


def port(s, name, L, n, node, cars):
    paint = Painters.in_dll(L, "g_road_paint")
    near, win = R.setup(s, name, L, n, node)
    v = R.View.in_dll(L, "g_view")
    v.cx, v.cy, v.min_z = s.i32(0xf7a28), s.i32(0xf7a2c), s.i32(0xf7938)
    for i, r in enumerate(struct.unpack("<1024i", s.raw(s.u32(0xf7a14), 4096))):
        v.recip[i] = r
    v.far_shift, v.max_level = s.read(0xf6aa4, 1)[0], s.i32(0xeaad8)
    v.clip_x, v.clip_y, v.clip_z = s.i32(0x143e54), s.i32(0x143e58), s.i32(0x143e5c)
    paint.roadside = paint.roadside.__class__()
    for f in ("model", "car_row"):
        setattr(paint, f, getattr(paint, f).__class__())
    for f, sym in (("quad", "quad_view"), ("whole", "paint_whole"), ("picture", "paint_picture"),
                   ("cars", "cars_at_node")):
        ctypes.memmove(ctypes.addressof(paint) + getattr(Painters, f).offset,
                       ctypes.byref(ctypes.c_void_p(ctypes.cast(getattr(L, sym), ctypes.c_void_p).value)), 8)
    ctypes.c_void_p.in_dll(L, "g_paint").value = ctypes.cast(L.raster_quad, ctypes.c_void_p).value
    fbuf = (U8 * (R.W * R.H))()
    L.raster_window(fbuf, R.W, *R.CLIP)
    R.sky_setup(s, L)
    R.sky(s, L, win)
    L.road_objects_start()
    L.road_draw(n, node, near, ctypes.byref(win))
    return bytes(fbuf)


def load_cars(s, name, L):
    """the port's cars as the snapshot's, their pictures in the race's colours"""
    addrs = [s.u32(0x13c24c + 4 * k) for k in range(s.i32(0x12da38))]
    racepal_emu.race_track(s, name, L)
    cars = []
    g = lambda a, o: struct.unpack("<i", s.raw(a + o, 4))[0]
    for k, a in enumerate(addrs):
        c = RaceCar()
        c.police = 1 if s.raw(a + 0x174, 1)[0] & 8 else 0
        cid = 400 if c.police else s.i32(0x12dce4 + 4 * k)
        if not L.car_load(ctypes.byref(c), cid):
            sys.exit("no car %d" % cid)
        c.pos = Vec3(g(a, 0), g(a, 4), g(a, 8))
        c.pitch, c.yaw, c.roll, c.node = g(a, 0xc), g(a, 0x10), g(a, 0x14), g(a, 0x48)
        c.body_roll, c.body_pitch, c.dist = g(a, 0x365), g(a, 0x369), g(a, 0x39d)
        c.roll_phase, c.pitch_phase = s.raw(a + 0x427, 1)[0], s.raw(a + 0x428, 1)[0]
        c.roll_shake, c.pitch_shake = g(a, 0x429), g(a, 0x42d)
        c.player, c.number = g(a, 0x47d), g(a, 0x48a)
        c.state, c.slot, c.draw_node, c.own_turn = g(a, 0x512), g(a, 0x516), g(a, 0x341), g(a, 0x20)
        for i in range(9):
            c.turn[i] = g(a, 0x24 + 4 * i)
        c.yaw_drawn, c.half_len, c.half_wid, c.braking = g(a, 0x351), g(a, 0x78), g(a, 0x7c), g(a, 0x3b9)
        for i in range(3):
            c.bits[i] = s.raw(a + 0x174 + i, 1)[0]
        cfm_game = struct.unpack("<I", s.raw(a + 0x792, 4))[0]
        model_state(s, struct.unpack("<I", s.raw(a + 0x52e, 4))[0], c.near_m, cfm_game, c.cfm, c.groups)
        model_state(s, struct.unpack("<I", s.raw(a + 0x666, 4))[0], c.far_m, cfm_game, c.cfm, [])
        cars.append(c)
    k, marks = racepal_emu.race_palette(s, name, L, [ctypes.addressof(c) for c in cars])
    racepal_emu.race_pictures(s, L, k, marks)
    keep = ctypes.POINTER(RaceCar) * 12
    allc = keep.in_dll(L, "g_cars")
    for k, c in enumerate(cars):
        allc[k] = ctypes.pointer(c)
    I.in_dll(L, "g_ncars").value = len(cars)
    order = keep.in_dll(L, "g_car_order")
    n = s.i32(0x13c220)
    for i in range(n):
        order[i] = ctypes.pointer(cars[addrs.index(s.u32(0x13c224 + 4 * i))])
    I.in_dll(L, "g_car_order_n").value = n
    I.in_dll(L, "g_player").value = s.i32(0x12dd58)
    I.in_dll(L, "g_racers").value = s.i32(0x13c18c)
    for sym, at in (("g_race_type", 0x12da20), ("g_field", 0x12da2c), ("g_humans", 0x12da30),
                    ("g_view_res", 0x12c308)):
        I.in_dll(L, sym).value = s.i32(at)
    ctypes.memmove(ctypes.addressof((U8 * 12).in_dll(L, "g_car_marks")), s.read(0x14e5bc, 12), 12)
    ctypes.memmove(ctypes.addressof((U8 * 35).in_dll(L, "g_cars_seen")), s.read(0x14e5d0, 35), 35)
    return cars, addrs


def check(path, name, L):
    s = Snap(path)
    tag = os.path.splitext(os.path.basename(path))[0]
    if s.u32(0x143dfc) != struct.unpack("<I", s.raw(s.u32(0x143df4) + 0x88, 4))[0]:
        print("%s: skipped (taken while the mirror's view was current)" % tag)
        return
    cam_a = s.u32(0x143df4)
    n = s.i32(0x136618)
    node = struct.unpack("<i", s.raw(cam_a + 0x38, 4))[0]
    cars, addrs = load_cars(s, name, L)
    cam = Camera()
    cam.mode, cam.node = struct.unpack("<i", s.raw(cam_a, 4))[0], node
    cam.pos = Vec3(*struct.unpack("<3i", s.raw(cam_a + 0x10, 12)))
    camm = (I * 9)(*struct.unpack("<9i", s.raw(s.u32(0x143df8), 36)))
    ctypes.c_void_p.in_dll(L, "g_camera").value = ctypes.addressof(cam)
    ctypes.c_void_p.in_dll(L, "g_camera_m").value = ctypes.addressof(camm)
    s.write(0xf88a4, struct.pack("<4i", *R.CLIP))
    # the view current (0x143dfc): its near plane for the markers
    rec = (I * 8)(*struct.unpack("<2i", s.raw(s.u32(0x143dfc), 8)))
    ctypes.c_void_p.in_dll(L, "g_view_now").value = ctypes.addressof(rec)
    got = port(s, name, L, n, node, cars)
    # a snapshot taken inside a painter may hold one see-through picture's mark switched
    mine, base = ctypes.c_void_p.in_dll(L, "g_fam").value, s.u32(0x145c50)
    f = Fam.in_dll(L, "g_fam")
    for q in set(f.texture[t][k] for t in range(144) for k in range(8)) | \
            set(f.picture[t][k] for t in range(64) for k in range(4)):
        if q and ctypes.string_at(q, 1) != s.raw(base + q - mine, 1):
            s.mu.mem_write(base + q - mine, ctypes.string_at(q, 1))
    want = original(s, n, node)
    # the cars' places in the view and their points, against the original's
    bad = []
    for c, a in zip(cars, addrs):
        if c.state == 6 or (c.slot == s.i32(0x12dd58) and cam.mode == 0):
            continue           # (the player's own, from the driver's seat: not placed)
        w = struct.unpack("<3i", s.raw(a + 0x391, 12))
        if (c.seen.x, c.seen.y, c.seen.z) != w and c.draw_node >= 0:
            bad.append("car %d seen: original %s, port %s" % (c.slot, w, (c.seen.x, c.seen.y, c.seen.z)))
    raw = (U8 * 768)()
    L.video_get_palette(raw, 0, 256)
    pal = [tuple(raw[3 * i:3 * i + 3]) for i in range(256)]
    R.png("build/shots/cars_%s_orig.png" % tag, want, pal)
    R.png("build/shots/cars_%s_port.png" % tag, got, pal)
    diff = [255 if a != b else 0 for a, b in zip(want, got)]
    R.png("build/shots/cars_%s_diff.png" % tag, diff, [(0, 0, 0)] * 255 + [(255, 255, 255)])
    d = sum(1 for x in diff if x)
    print("%s: %d pixels differ; %s" % (tag, d, "; ".join(bad) if bad else "the cars' places equal"))
    for c in cars:
        L.car_free(ctypes.byref(c))


def main():
    name, snaps = sys.argv[1], sys.argv[2:]
    L = lib()
    L.files_init(b"NFSSE", None)
    if not L.exe_load():
        sys.exit("no NFS_DOS.EXE")
    for p in snaps:
        check(p, name, L)


if __name__ == "__main__":
    main()
