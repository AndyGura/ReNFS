#!/usr/bin/env python3
"""NFS SE car physics: SIMDATA/CARSPECS/<car>.PBS (8 player cars + TRAFFC) and
SIMDATA/CARFAMS/<car>.PDN (every car, traffic and police too). Both are packed (30FB,
tools/unpack.py): a .PBS unpacks to 1912 bytes, a .PDN to 460. Every value is an i32,
most 16.16. What is known (NOTES §3.9):

    python3 -I tools/carspec.py pbs <file.pbs>...   the known fields, the torque table
    python3 -I tools/carspec.py pdn <file.pdn>...   the known fields, the 93-value curve
    python3 -I tools/carspec.py png <NFSSE> <out.png>  every car's torque and gears
"""
import glob, os, struct, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import unpack

FX = 65536.0


class Pbs:
    """load_car_pbs 0x50c24: the car's +0x479 points at it, used in place"""
    def __init__(self, path):
        d = self.d = unpack.unpack(open(path, "rb").read())
        assert len(d) == 1912, len(d)
        i = lambda o: struct.unpack_from("<i", d, o)[0]
        self.sum_ok = sum(d[:0x758]) == struct.unpack_from("<I", d, 0x774)[0]
        self.x14 = i(0x14)
        self.power = i(0x3C) / FX        # x 248/256 with the automatic (0x18990); 0 if edited
        n = i(0x88)
        self.torque = [struct.unpack_from("<2i", d, 0xB4 + 8 * k) for k in range(n)]
        self.x2e4, self.x2e8 = i(0x2E4), i(0x2E8)          # copied to the car's +0x78, +0x7c
        self.x334, self.x338 = i(0x334), i(0x338)          # > 0: two features the car has


class Pdn:
    """0x1e9cc copies it into the car record (car + 0xc0: its body)"""
    def __init__(self, path):
        d = self.d = unpack.unpack(open(path, "rb").read())
        assert len(d) == 460, len(d)
        v = self.v = struct.unpack("<115i", d)
        self.head = [x / FX for x in v[0:5]]               # +0xc, +0x10 to 0x2c920
        self.x14 = v[5] / FX                               # car +0x168, its inverse +0x16c
        self.x18 = v[6] / FX                               # car +0x170
        self.curve = [x / FX for x in v[7:100]]            # car +0x178 .. (93 values)
        self.gear_speed = [x / FX for x in v[107:113]]     # car +0x308 .., inverses +0x320 ..
        self.redline = v[113] / FX                         # car +0x338
        self.gears = v[114]                                # car +0x469


def main():
    what = sys.argv[1]
    if what == "pbs":
        for p in sys.argv[2:]:
            c = Pbs(p)
            peak = max(c.torque, key=lambda t: t[1])
            print("%s: sum %s, power factor %.3f, +0x14 %d, +0x334 %d, +0x338 %d; torque %d points "
                  "%d..%d rpm, peak %d at %d rpm" % (
                      p, "right" if c.sum_ok else "WRONG", c.power, c.x14, c.x334, c.x338,
                      len(c.torque), c.torque[0][0], c.torque[-1][0], peak[1], peak[0]))
    elif what == "pdn":
        for p in sys.argv[2:]:
            c = Pdn(p)
            print("%s: head %s, +0x14 %.2f, +0x18 %.2f, redline %.0f, gears %d, top speeds %s m/s" % (
                p, " ".join("%.3f" % x for x in c.head), c.x14, c.x18, c.redline, c.gears,
                " ".join("%.1f" % x for x in c.gear_speed)))
            print("   curve %s" % " ".join("%.3f" % x for x in c.curve[:12]), "..")
    elif what == "png":
        from PIL import Image, ImageDraw
        root, out = sys.argv[2], sys.argv[3]
        W, H = 900, 700
        im = Image.new("RGB", (W, H), (20, 22, 30)); dr = ImageDraw.Draw(im)
        cols = [(255, 90, 90), (90, 200, 255), (255, 210, 60), (120, 230, 120), (230, 120, 255),
                (255, 150, 60), (200, 200, 200), (60, 220, 200), (150, 150, 255)]
        files = sorted(glob.glob(os.path.join(root, "SIMDATA", "CARSPECS", "*.PBS")))
        dr.text((10, 8), "torque (N m?) against rpm, from the .PBS; the gears' top speeds "
                         "(km/h) from the .PDN", fill=(220, 220, 220))
        X = lambda r: 50 + r * (W - 80) / 11000
        Y = lambda t: 480 - t * 440 / 800
        for k in range(0, 801, 100):
            dr.line([(50, Y(k)), (W - 30, Y(k))], fill=(45, 48, 60)); dr.text((10, Y(k) - 6), str(k), fill=(140, 140, 140))
        for r in range(0, 11001, 1000):
            dr.text((X(r) - 10, 484), str(r), fill=(140, 140, 140))
        for n, f in enumerate(files):
            name = os.path.basename(f)[:-4]
            c = Pbs(f)
            dr.line([(X(r), Y(t)) for r, t in c.torque], fill=cols[n], width=2)
            pdn = os.path.join(root, "SIMDATA", "CARFAMS", name + ".PDN")
            g = ""
            if os.path.exists(pdn):
                p = Pdn(pdn)
                g = "  " + " ".join("%.0f" % (s * 3.6) for s in p.gear_speed if s) + " km/h, redline %.0f" % p.redline
            dr.text((60, 510 + 18 * n), "%-7s peak %d at %d%s" % (
                name, max(t for _, t in c.torque), max(c.torque, key=lambda x: x[1])[0], g), fill=cols[n])
        im.save(out)
        print(out)
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
