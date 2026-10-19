#!/usr/bin/env python3
"""The rear-view mirror, the original's against the port's, on snapshots of a 320x200
race: the original's sequence from race_draw_frame (0x1486c(camera, 1), 0x5a950, the
mirror's picture 0xf457c as the window, the horizon 0x5adbc, the road behind 0x5aa84 with
its cars 0x68430, the flip 0x96240, 0x1486c(camera, 0)) run on the snapshot, and the
port's road_gather_behind, road_transform_behind, sky_behind, road_draw_behind with
cars_behind_at, and the flip, on the same view. Also compares the road points behind the
port gathers with the snapshot's (0x13b494). Writes build/shots/mirror_<snap>_{orig,port}.png
(enlarged).

    build/venv/bin/python tools/mirror_emu.py cy1 build/snap/e1.bin [...]

With BEHIND=k (nodes) the opponent (car 1) is first put on the road k nodes behind the
camera, in the camera's lane and on that node's heading, so that the mirror has a car in it.
"""
import ctypes, os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from snap_emu import Snap
import cars_emu
import roadpic_emu as R
from roaddraw_emu import setup, Painters, CARS
from road_emu import Road
from poly_emu import View, Vec3
from camera_emu import Camera

I = ctypes.c_int
FILL = 0xfe


class Window(ctypes.Structure):
    _fields_ = [("x", I), ("y", I), ("w", I), ("h", I)]


def picture(s):
    rec = s.u32(0xf457c)
    w, h = struct.unpack("<2I", s.raw(rec, 8))
    return w, h, struct.unpack("<I", s.raw(rec + 0x2c, 4))[0] + 0x10


def original(s, cam):
    w, h, pix = picture(s)
    s.mu.mem_write(pix, bytes([FILL]) * (w * h))
    s.call(0x1486c, eax=cam, edx=1)
    s.call(0x5a950)
    s.call(0x8118c, s.u32(0xf457c))
    s.call(0x5adbc)
    s.call(0x5aa84, eax=s.i32(0x13661c))
    s.call(0x96240, struct.unpack("<I", s.raw(s.u32(0xf457c) + 0x2c, 4))[0])
    s.call(0x1486c, eax=cam, edx=0)
    return bytes(s.raw(pix, w * h))


def put_behind(s, k):
    """car 1 k nodes behind the camera, marked for the mirror (as 0x677f8 would)"""
    cam_a = s.u32(0x143df4)
    node = struct.unpack("<i", s.raw(cam_a + 0x38, 4))[0]
    at = node - k
    rec = s.u32(0x137430) + (at & s.u32(0x13bb88)) * 0x24
    own = s.u32(0x137430) + (node & s.u32(0x13bb88)) * 0x24
    x, y, z = struct.unpack("<3i", s.raw(rec + 8, 12))
    cx, cy, cz = struct.unpack("<3i", s.raw(own + 8, 12))
    px, py, pz = struct.unpack("<3i", s.raw(cam_a + 0x10, 12))
    x, z = x + px - cx, z + pz - cz        # in the camera's lane
    heading = struct.unpack("<h", s.raw(rec + 0x18, 2))[0]
    car = s.u32(0x13c24c + 4)
    s.mu.mem_write(car, struct.pack("<3i", x, y, z))
    s.mu.mem_write(car + 0x10, struct.pack("<i", (heading << 10) & 0xffffff))
    s.mu.mem_write(car + 0x341, struct.pack("<ii", at, at))
    s.mu.mem_write(car + 0x39d, struct.pack("<i", 0x10000))
    s.mu.mem_write(car + 0x20, struct.pack("<i", 0))
    marks = bytearray(s.read(0x14e5c5, 11))
    marks[k] = 1
    s.write(0x14e5c5, bytes(marks))


def check(path, name, L):
    s = Snap(path)
    tag = os.path.splitext(os.path.basename(path))[0]
    behind = int(os.environ.get("BEHIND", "0"))
    if behind:
        put_behind(s, behind)
        tag += "_behind%d" % behind
    cam_a = s.u32(0x143df4)
    node = struct.unpack("<i", s.raw(cam_a + 0x38, 4))[0]
    cars, addrs = cars_emu.load_cars(s, name, L)
    setup(s, name, L, s.i32(0x136618), node)
    road = Road.in_dll(L, "g_road")

    # the road behind, gathered by the port, against the snapshot's
    L.road_gather_behind(node)
    n = s.i32(0x13bb7c)
    theirs = struct.unpack("<%di" % (3 * n), s.read(0x13b494, 12 * n))
    mine = [v for i in range(road.bcount) for v in (road.bworld[i].x, road.bworld[i].y, road.bworld[i].z)]
    print("%s: the road behind, %d points: %s" % (tag, n, "equal" if road.bcount == n and tuple(mine) == theirs
                                                    else "differ (port %d points)" % road.bcount))

    # the mirror's view as the original makes it current
    view = Snap(path)
    view.call(0x1486c, eax=cam_a, edx=1)
    v = View.in_dll(L, "g_view")
    v.cx, v.cy, v.min_z = view.i32(0xf7a28), view.i32(0xf7a2c), view.i32(0xf7938)
    for i, r in enumerate(struct.unpack("<1024i", view.raw(view.u32(0xf7a14), 4096))):
        v.recip[i] = r
    v.far_shift, v.max_level = view.read(0xf6aa4, 1)[0], view.i32(0xeaad8)
    v.clip_x, v.clip_y, v.clip_z = view.i32(0x143e54), view.i32(0x143e58), view.i32(0x143e5c)
    mm = (I * 9)(*struct.unpack("<9i", view.raw(view.u32(0x143df8), 36)))
    rec_a = view.u32(0x143dfc)
    rec = (I * 8)(*struct.unpack("<2i", view.raw(rec_a, 8)))
    ctypes.c_void_p.in_dll(L, "g_view_now").value = ctypes.addressof(rec)
    layout = struct.unpack("<I", view.raw(rec_a + 0x18, 4))[0]
    win_a = struct.unpack("<I", view.raw(rec_a + 0x1c, 4))[0]
    lens_h = struct.unpack("<i", view.raw(layout + 0x20, 4))[0]
    dy = struct.unpack("<i", view.raw(win_a + 0xc, 4))[0]
    win = Window(*struct.unpack("<4i", view.raw(win_a + 0x18, 16)))

    cam = Camera()
    g = lambda o: struct.unpack("<i", s.raw(cam_a + o, 4))[0]
    cam.mode, cam.node, cam.pitch, cam.yaw = g(0), node, g(0x40), g(0x44)
    cam.pos = Vec3(g(0x10), g(0x14), g(0x18))
    ctypes.c_void_p.in_dll(L, "g_camera").value = ctypes.addressof(cam)
    ctypes.c_void_p.in_dll(L, "g_camera_m").value = ctypes.addressof(mm)
    ctypes.memmove(ctypes.addressof((ctypes.c_ubyte * 11).in_dll(L, "g_cars_behind")), s.read(0x14e5c5, 11), 11)
    paint = Painters.in_dll(L, "g_road_paint")
    ctypes.memmove(ctypes.addressof(paint) + Painters.cars_behind.offset,
                   ctypes.byref(ctypes.c_void_p(ctypes.cast(L.cars_behind_at, ctypes.c_void_p).value)), 8)
    ctypes.c_void_p.in_dll(L, "g_paint").value = ctypes.cast(L.raster_quad, ctypes.c_void_p).value

    w, h, _ = picture(s)
    buf = (ctypes.c_ubyte * (w * h))(*([FILL] * (w * h)))
    L.raster_window(buf, w, 0, 0, w, h)
    near = L.road_transform_behind(ctypes.byref(cam.pos), mm)
    L.sky_behind(cam.yaw, cam.pitch, node, lens_h, dy, w, h)
    L.road_draw_behind(s.i32(0x13661c), node, near, ctypes.byref(win))
    got = bytes(buf)
    got = b"".join(got[y * w:(y + 1) * w][::-1] for y in range(h))

    if behind:
        s = Snap(path)
        put_behind(s, behind)
    want = original(s, cam_a)
    d = [i for i in range(w * h) if got[i] != want[i]]
    pal = [tuple(4 * c for c in s.read(0x13cb48 + 3 * i, 3)) for i in range(256)]
    for which, pix in (("orig", want), ("port", got)):
        big = bytes(pix[(y // 8) * w + x // 8] for y in range(h * 8) for x in range(w * 8))
        R.png("build/shots/mirror_%s_%s.png" % (tag, which), big, pal, w * 8, h * 8)
    print("%s: the mirror %dx%d, %d pixels differ%s" % (tag, w, h, len(d),
          " (first %d, %d)" % (d[0] % w, d[0] // w) if d else ""))
    for c in cars:
        L.car_free(ctypes.byref(c))


def main():
    name, snaps = sys.argv[1], sys.argv[2:]
    L = cars_emu.lib()
    L.files_init(b"NFSSE", None)
    if not L.exe_load():
        sys.exit("no NFS_DOS.EXE")
    os.makedirs("build/shots", exist_ok=True)
    for p in snaps:
        check(p, name, L)


if __name__ == "__main__":
    main()
