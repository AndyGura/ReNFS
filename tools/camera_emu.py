#!/usr/bin/env python3
"""The starting grid (0x7c938), the road node a position is on (0x4f534) and the driver's
camera (camera_update 0x136b0, modes 0 and 1) run on snapshots in Unicorn, against the
port's car_grid_place, track_follow and camera_update (port/src/car.c, track.c,
camera.c) given the same car and camera.

    build/venv/bin/python tools/camera_emu.py cy1 build/snap/r1a.bin [...]
"""
import ctypes, os, struct, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from snap_emu import Snap
from poly_emu import Vec3

SOURCES = ["camera.c", "car.c", "model.c", "polyfill.c", "dash.c", "hud.c", "config.c", "view.c", "sky.c", "roaddraw.c", "trackinfo.c", "road.c", "poly.c",
           "raster.c", "track.c", "files.c", "exe.c", "trackfam.c", "racepal.c", "font.c", "video.c",
           "fsh.c", "unpack.c", "crash.c", "drive.c", "cardraw.c", "timing.c"]
I = ctypes.c_int
U8 = ctypes.c_ubyte


def lib():
    out = "build/camera_test.so"
    subprocess.check_call(["cc", "-O1", "-g", "-shared", "-fPIC", "-o", out] +
                          ["port/src/" + f for f in SOURCES] + ["-Iport/src"] +
                          subprocess.check_output(["sdl2-config", "--cflags", "--libs"]).decode().split())
    return ctypes.CDLL(os.path.abspath(out))


class Car(ctypes.Structure):
    _fields_ = [("pos", Vec3), ("pitch", I), ("yaw", I), ("roll", I), ("node", I), ("body_roll", I),
                ("body_pitch", I), ("dist", I), ("roll_phase", U8), ("pitch_phase", U8),
                ("roll_shake", I), ("pitch_shake", I), ("player", I), ("number", I)]


class Camera(ctypes.Structure):
    _fields_ = [("mode", I), ("pos", Vec3), ("off", Vec3), ("node", I), ("flags", I),
                ("pitch", I), ("yaw", I), ("roll", I), ("tilt", I * 3), ("eye", Vec3),
                ("mirror_on", I), ("yaw_follow", I), ("ahead", ctypes.c_void_p),
                ("mirror", ctypes.c_void_p), ("car", ctypes.POINTER(Car)), ("m", I * 9), ("mm", I * 9)]


def car_of(s, a):
    g = lambda o: struct.unpack("<i", s.raw(a + o, 4))[0]
    b = lambda o: s.raw(a + o, 1)[0]
    c = Car()
    c.pos = Vec3(g(0), g(4), g(8))
    c.pitch, c.yaw, c.roll, c.node = g(0xc), g(0x10), g(0x14), g(0x48)
    c.body_roll, c.body_pitch, c.dist = g(0x365), g(0x369), g(0x39d)
    c.roll_phase, c.pitch_phase, c.roll_shake, c.pitch_shake = b(0x427), b(0x428), g(0x429), g(0x42d)
    c.player, c.number = g(0x47d), g(0x48a)
    return c


def check(path, L, bad, stats):
    s = Snap(path)
    name = os.path.basename(path)
    cars = [s.u32(0x13c24c + 4 * k) for k in range(s.i32(0x12da38))]
    racers, kind = s.i32(0x12da2c), s.i32(0x12da12)

    # the grid: every racer placed by both
    for a in cars[:racers]:
        o = Snap(path)
        o.call(0x7c938, eax=a)
        want = struct.unpack("<3i", o.raw(a, 12)) + struct.unpack("<i", o.raw(a + 0x10, 4)) + \
            struct.unpack("<i", o.raw(a + 0x48, 4)) + struct.unpack("<i", o.raw(a + 0x39d, 4))
        c = car_of(s, a)
        L.car_grid_place(ctypes.byref(c), s.i32(0x12dca0 + 4 * c.player), racers, kind)
        got = (c.pos.x, c.pos.y, c.pos.z, c.yaw, c.node, c.dist)
        if want != got:
            bad.append("%s grid, car %d: the original %s, the port %s" % (name, c.player, want, got))

    # the node of every car's position, from 12 nodes behind to 12 ahead
    for a in cars:
        c = car_of(s, a)
        for d in range(-12, 13):
            start = c.node + d
            if start < 0 and not s.i32(0x13bb80):
                continue
            o = Snap(path)
            o.mu.mem_write(a + 0x48, struct.pack("<i", start))
            r = o.call(0x4f534, eax=a, edx=a + 0x48)
            want = (struct.unpack("<i", o.raw(a + 0x48, 4))[0], r & 0xffffffff)
            n = I(start)
            r = L.track_follow(ctypes.byref(c.pos), ctypes.byref(n))
            got = (n.value, r & 0xffffffff)
            stats["follow"] += 1
            stats["moved"] += want[0] != start
            if want != got:
                bad.append("%s follow, car %d from %d: the original %s, the port %s" % (
                    name, c.player, start, want, got))

    # the camera
    cam = s.u32(0x143df4)
    g = lambda o: struct.unpack("<i", s.raw(cam + o, 4))[0]
    if g(0) > 1:
        bad.append("%s: camera mode %d not ported" % (name, g(0)))
        return
    car_at = struct.unpack("<I", s.raw(cam + 0x90, 4))[0]
    if car_at != cars[s.i32(0x12dd58)]:
        bad.append("%s: the camera on another car" % name)
    c = car_of(s, car_at)
    k = Camera()
    k.mode, k.node, k.flags = g(0), g(0x38), g(0x3c)
    k.tilt[1] = g(0x60)
    k.eye = Vec3(g(0x6c), g(0x70), g(0x74))
    k.car = ctypes.pointer(c)
    L.camera_init(struct.unpack("<i", s.raw(struct.unpack("<I", s.raw(car_at + 0x479, 4))[0]
                                            + 0x36c, 4))[0])
    ctypes.c_int.in_dll(L, "g_cam_pitch_bias").value = s.i32(0x12ba20)
    o = Snap(path)
    o.call(0x136b0, eax=cam)
    h = lambda o_: struct.unpack("<i", o.raw(cam + o_, 4))[0]
    L.camera_update(ctypes.byref(k))
    fields = [("pos", (0x10, 0x14, 0x18), (k.pos.x, k.pos.y, k.pos.z)),
              ("off", (0x2c, 0x30, 0x34), (k.off.x, k.off.y, k.off.z)),
              ("node, flags", (0x38, 0x3c), (k.node, k.flags)),
              ("angles", (0x40, 0x44, 0x48), (k.pitch, k.yaw, k.roll)),
              ("tilt", (0x5c, 0x60, 0x64), tuple(k.tilt)),
              ("eye", (0x6c, 0x70, 0x74), (k.eye.x, k.eye.y, k.eye.z)),
              ("yaw follow", (0x78,), (k.yaw_follow,))]
    for what, offs, got in fields:
        want = tuple(h(x) for x in offs)
        if want != got:
            bad.append("%s camera %s: the original %s, the port %s" % (name, what, want, got))


def main():
    name, snaps = sys.argv[1], sys.argv[2:]
    L = lib()
    L.files_init(b"NFSSE", None)
    if not L.exe_load():
        sys.exit("no NFS_DOS.EXE")
    if not L.track_load(name.encode()):
        sys.exit("no track")
    for p in snaps:
        bad, stats = [], {"follow": 0, "moved": 0}
        check(p, L, bad, stats)
        print("%s: the grid, %d node searches (%d moved), the camera: %s" % (
            os.path.basename(p), stats["follow"], stats["moved"],
            "all equal" if not bad else "%d differ" % len(bad)))
        for b in bad[:12]:
            print("  " + b)


if __name__ == "__main__":
    main()
