#!/usr/bin/env python3
"""NFS SE replays: GAMEDATA/REPLAY/*.RPL (the CD's four: REPLAY.RPL and REPLAY1-3.RPL, which
the attract mode plays; a player's own are written beside them).

    python3 -I tools/rpl.py list <file.rpl>...
    python3 -I tools/rpl.py png  <file.rpl> <out.png>   the cars on the track, the controls

A replay is five blocks of the race's memory, written by replay_file 0x5c3b0 at the race's
end and read back by replay_settings 0x5c340 (the first) and replay_load 0x5c4a4 (all):
  0x00000  0x348   the race record 0x12da0c: +6 the track (0x41b68's order), +0xa its
                   files' stem, +0x20 racers, +0x24 drivers, +0x2c cars in all, +0x30
                   traffic, +0x38 the time (seconds from 1970), +0x3c two 0x4b-byte entries
                   (+2 name, +0xb car: 0xf6e40's order), ... +0x300 the clock setting
  0x00348  0x9a4   0x144ba8 (the highlights 0x144da8 among it)
  0x00cec  0x15e00 the recording ("UsrCtrlBuff", replay_start 0x5dd68), never cleared, so
                   what the race did not reach is old memory:
           0x0000  the drivers' controls, one slot every second step (replay_record_controls
                   0x5cac4): driver k's low bytes at k x 7200 + t, high bytes at 14400 +
                   k x 7200 + t, t = (step counter mod 28800) / 4: a ring of 7.2 minutes
           0x7080  a driver's car every 900 steps (16 x 0x1a4 a driver, 0x1a40 apart)
           0xa500  the other cars at the same moments (16 x 0xb90, 8 x 0x172 in each)
  0x16aec  0xcc8   0x143ed8: +4 the step counter (2 a step) at the last step recorded,
                   +0x14 + s x 0xa2 what else the snapshot s needs (the random seed..)
  0x177b4  9 x 0x1d2 from 0x14ea80: the racers' results (records from 0x14ea7c: so the
                   file's copy starts 4 bytes into the first and ends 4 into a tenth)
A control word (0x49248 makes it, 0x483e4 / 0x48990 use it): bits 0-5 steering (32 the
middle), 6-11 the pedals (30 neither, below 15 the throttle, 45 and above the brake), 12
gear down, 13 gear up, 14 and 15 two switches (car +0x4ba, +0x3f5).
"""
import os, struct, sys

TRACKS = ["city", "coast", "alpine", "springs", "avalley", "bsienna", "vridge", "tpolis",
          "lvegas", "moon"]                                  # 0x41b68
CARS = ["TSupra", "LDiabl", "P911", "CZR1", "F512TR", "DVIPER", "ANSX", "MRX7", "MRX7",
        "MRX7", "MRX7", "Traffc"]                            # 0xf6e40

SETTINGS, MISC, REC, SNAPS, RESULTS = 0, 0x348, 0xcec, 0x16aec, 0x177b4
SIZE = RESULTS + 9 * 0x1d2
COUNT = 28800          # the step counter's ring (0x7080)
SNAP_EVERY = 1800      # 0x708


class Replay:
    def __init__(self, path):
        d = self.d = open(path, "rb").read()
        assert len(d) == SIZE, len(d)
        i = lambda o: struct.unpack_from("<i", d, o)[0]
        self.x12da0e = i(2)
        self.track = i(6)                                    # 0x12da12
        self.stem = d[0xa:d.index(b"\0", 0xa)].decode("latin1")
        self.x12da20, self.network, self.flags = i(0x14), i(0x18), i(0x1c)
        self.racers, self.drivers, self.x12da34 = i(0x20), i(0x24), i(0x28)
        self.cars, self.traffic, self.x12da40, self.time = i(0x2c), i(0x30), i(0x34), i(0x38)
        self.entries = []      # 0x4b bytes from 0x12da48: the player, the head-to-head rival
        for k in range(2):
            o = 0x3c + k * 0x4b
            self.entries.append(dict(name=d[o + 2:d.index(b"\0", o + 2)].decode("latin1"),
                                     car=i(o + 0xb)))
        self.end = i(SNAPS + 4)

    def steps(self):
        return self.end // 2

    def controls(self, k):
        """driver k's control words in the order recorded (up to the end)"""
        d, base = self.d, REC + k * 7200
        n = min(self.end, COUNT) // 4
        first = (self.end // 4 - n) if self.end > COUNT else 0
        out = []
        for j in range(first, first + n):
            t = j % 7200
            out.append(d[base + t] | d[base + 14400 + t] << 8)
        return out

    def snapshots(self):
        """(snapshot, car index, x, y, z, road node) of every car in every snapshot made"""
        d, out = self.d, []
        made = min(self.end // SNAP_EVERY + 1, 16)
        for s in range(made):
            for k in range(self.drivers):
                out.append((s,) + self.car(REC + 0x7080 + s * 0x1a4 + k * 0x1a40))
            others = struct.unpack_from("<i", d, SNAPS + 0x64 + s * 0xa2)[0]   # 0x143f3c
            for j in range(max(0, min(others, 8))):
                out.append((s,) + self.car(REC + 0xa500 + s * 0xb90 + j * 0x172))
        return out

    def car(self, o):
        """a car as 0x5cbd0 packs it: +8 its index << 24, +0xc.. car +0.. (the position)"""
        d = self.d
        idx = struct.unpack_from("<I", d, o + 8)[0] >> 24
        x, y, z = (v / 65536 for v in struct.unpack_from("<3i", d, o + 0xc))
        return idx, x, y, z, struct.unpack_from("<i", d, o + 0x54)[0]

    def result(self, k, field):
        return struct.unpack_from("<i", self.d, RESULTS + k * 0x1d2 + field - 4)[0]


def clock(n):
    """a step counter value (2 a step, 33.3 steps a second) as m:ss.s"""
    s = n / 2 / (100 / 3)
    return "%d:%04.1f" % (s // 60, s % 60)


def listing(path):
    import time
    r = Replay(path)
    name = TRACKS[r.track] if 0 <= r.track < len(TRACKS) else "?"
    print("%s: track %d %s (%s), %d racers, %d drivers, %d cars in all (%d traffic), "
          "recorded %s, %d steps (%s)" % (
              path, r.track, name, r.stem, r.racers, r.drivers, r.cars, r.traffic,
              time.strftime("%Y-%m-%d %H:%M", time.gmtime(r.time)), r.steps(), clock(r.end)))
    print("   0x12da0e %d, 0x12da20 %d, network %d, flags %#x, 0x12da34 %d, 0x12da40 %d" % (
        r.x12da0e, r.x12da20, r.network, r.flags, r.x12da34, r.x12da40))
    for k, e in enumerate(r.entries):
        fin = r.result(k, 0x1c0)
        print("   entry %d: %-8s car %d %-7s %s" % (
            k, e["name"], e["car"], CARS[e["car"]] if e["car"] < len(CARS) else "?",
            "finished %s, place %d" % (clock(fin + 300), r.d[RESULTS + k * 0x1d2 + 0x1ce - 4])
            if fin > 0 else "not finished"))
    for k in range(r.drivers):
        w = r.controls(k)
        thr = sum(1 for c in w if (c >> 6 & 63) < 15)
        brk = sum(1 for c in w if (c >> 6 & 63) >= 45)
        shifts = sum(1 for c in w if c >> 12 & 3)
        print("   driver %d: %d control slots, throttle in %d, brake in %d, gear keys in %d" % (
            k, len(w), thr, brk, shifts))


def png(path, out):
    from PIL import Image, ImageDraw
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import tri
    r = Replay(path)
    root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(path))))
    misc = os.path.join(root, "SIMDATA", "MISC")
    name = next(f for f in os.listdir(misc) if f.upper() == r.stem.upper() + ".TRI")
    t = tri.Track(os.path.join(misc, name))
    W, H, CH = 900, 900, 260
    im = Image.new("RGB", (W, H + CH), (22, 22, 28)); dr = ImageDraw.Draw(im)
    xs = [n["pos"][0] for n in t.nodes]; zs = [n["pos"][2] for n in t.nodes]
    span = max(max(xs) - min(xs), max(zs) - min(zs)) or 1
    k = (W - 60) / span
    f = lambda x, z: (30 + (x - min(xs)) * k, H - 30 - (z - min(zs)) * k)
    c = [f(n["pos"][0], n["pos"][2]) for n in t.nodes]
    dr.line(c + ([c[0]] if t.loop else []), fill=(110, 110, 120), width=5)
    cols = [(255, 80, 80), (90, 200, 255), (255, 210, 60), (120, 230, 120), (230, 120, 255),
            (255, 150, 60), (220, 220, 220), (60, 220, 200), (150, 150, 255)] + [(150, 150, 150)] * 8
    for s, idx, x, y, z, node in r.snapshots():
        px, py = f(x, z)
        col = cols[idx] if idx < len(cols) else (120, 120, 120)
        dr.ellipse([px - 5, py - 5, px + 5, py + 5], fill=col)
        dr.text((px + 7, py - 6), "%d" % s, fill=col)
    dr.text((10, 8), "%s: %s (%s), %d racers; cars where each snapshot (every 27 s) put them" % (
        os.path.basename(path), TRACKS[r.track], r.stem, r.racers), fill=(230, 230, 230))
    for j, e in enumerate(r.entries[:r.racers]):
        dr.text((10, 26 + 14 * j), "car %d: %s%s" % (j, CARS[e["car"]] if e["car"] < len(CARS) else "?",
                                                     " (driver)" if j < r.drivers else ""), fill=cols[j])
    # driver 0's controls: steering line, throttle (green) and brake (red) underneath
    w = r.controls(0)
    y0 = H + 20
    dr.text((10, y0 - 16), "driver 0's controls, %d slots (%s): steering, throttle, brake, gear keys"
            % (len(w), clock(r.end)), fill=(230, 230, 230))
    n = max(1, len(w))
    pts = []
    for j, cw in enumerate(w):
        x = 10 + j * (W - 20) / n
        pts.append((x, y0 + 80 - ((cw & 63) - 32) * 2.4))
        p = cw >> 6 & 63
        if p < 15:
            dr.line([(x, y0 + 170), (x, y0 + 190)], fill=(80, 200, 80))
        elif p >= 45:
            dr.line([(x, y0 + 195), (x, y0 + 215)], fill=(230, 70, 70))
        if cw >> 12 & 3:
            dr.line([(x, y0 + 220), (x, y0 + 235)], fill=(255, 220, 60) if cw >> 13 & 1 else (90, 160, 255))
    dr.line([(10, y0 + 80), (W - 10, y0 + 80)], fill=(60, 60, 70))
    dr.line(pts, fill=(230, 230, 230))
    im.save(out)
    print(out)


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    if sys.argv[1] == "list":
        for p in sys.argv[2:]:
            listing(p)
    elif sys.argv[1] == "png":
        png(sys.argv[2], sys.argv[3])
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
