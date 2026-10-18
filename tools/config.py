#!/usr/bin/env python3
"""NFS SE's GAMEDATA/CONFIG/CONFIG.DAT: the front end's state and the records, 24,402 bytes
(0x5f52) copied as they are to 0x156c14 by cfg_load_config_dat 0x39c40 (another size, or
no file: cfg_defaults 0x38ea8 builds the same from tables at 0xeab80..), written back by
cfg_save_config_dat 0x3a134. The CD's copy is the defaults.

    python3 -I tools/config.py <CONFIG.DAT> [track]

    +0     the player's name ("Player" / "Spieler")
    +0x3b  the replay to play or save (0x156c4f, "REPLAY%d.RPL" from fe_attract_mode)
    +0x6b.. the menus' choices (words, race_setup_record 0x36c5c reads them): +0x93 the
           segment (0x12da0e), +0x95 the track (0x12da12), +0xb3 the race length (which
           table of race times), ..
    +0xb5  9 tracks x 0xa6b bytes of records (0x41b68's order), 0x27 bytes a record:
           +0 name (15), +0xf car (the menu's order), +0x13 0/1, +0x17 1 = filled,
           +0x1b time (1/60 s, fmt_time 0x74914), +0x1f speed (16.16 m/s: x 2.24 mph,
           x 3.6 km/h, 0x74854), +0x23 the kind of race (0 time trial, 1 head to head,
           2 a race)
           records 0-2: city/coast/alpine the best time of segment 1-3 (0x69608), a
               circuit the best lap in 0 (0x69438); 3-5 the best top speed of segment 1-3
           records 6-35: three tables of ten race times (+0xb3 picks one)
           record 36 (+0x57c): the track's top speed
    +0x5f36 the keys (0x15cb4a..)
"""
import struct, sys

TRACKS = ["city", "coast", "alpine", "springs", "avalley", "bsienna", "vridge", "tpolis",
          "lvegas"]
CARS = ["RX-7", "NSX", "SUPRA", "911", "ZR-1", "VIPER", "512TR", "DIABLO", "WARRIOR"]  # 0xf7464
KINDS = ["time trial", "head to head", "race"]
SIZE, RECORDS, BLOCK, REC = 0x5f52, 0xb5, 0xa6b, 0x27


def time(t):
    if t <= 0 or t in (99999, 999999):
        return "N/A"
    return "%d:%02d.%d" % (t // 3600, t % 3600 // 60, t % 60 // 6) if t >= 3600 else \
        "%d.%d" % (t % 3600 // 60, t % 60 // 6)


def speed(v):
    return "%.1f mph" % (v / 65536 * 2.24) if v > 0x9fff6 else "N/A"


def record(d, track, k):
    o = RECORDS + track * BLOCK + k * REC
    name = d[o:o + 15].split(b"\0")[0].decode("latin1").rstrip()
    car, x13, filled, t, v, kind = struct.unpack_from("<6i", d, o + 0xf)
    return dict(name=name, car=CARS[car] if 0 <= car < len(CARS) else car, x13=x13,
                filled=filled, time=t, speed=v, kind=KINDS[kind] if 0 <= kind < 3 else kind)


def main():
    d = open(sys.argv[1], "rb").read()
    assert len(d) == SIZE, "%d bytes, not %d: the game would use its defaults" % (len(d), SIZE)
    w = lambda o: struct.unpack_from("<h", d, o)[0]
    print("player %r, replay %r, track %d, segment %d, race length %d" % (
        d[:d.index(b"\0")].decode("latin1"), d[0x3b:d.index(b"\0", 0x3b)].decode("latin1"),
        w(0x95), w(0x93), w(0xb3)))
    tracks = [int(sys.argv[2])] if len(sys.argv) > 2 else range(9)
    for t in tracks:
        print("%s:" % TRACKS[t])
        line = lambda what, r, v: print("   %-26s %-15s %-7s %-12s %s" % (
            what, r["name"], r["car"], r["kind"], v))
        for k in range(3):
            r = record(d, t, k)
            if r["filled"]:
                line("best segment %d" % (k + 1) if t < 3 else "best lap", r,
                     "%s  (%s)" % (time(r["time"]), speed(r["speed"])))
        for k in range(3, 6):
            r = record(d, t, k)
            if r["filled"]:
                line("top speed, segment %d" % (k - 2), r, speed(r["speed"]))
        for table in range(3):
            for k in range(10):
                r = record(d, t, 6 + table * 10 + k)
                if r["filled"]:
                    line("race times %d, %d." % (table, k + 1), r, time(r["time"]))
        r = record(d, t, 36)
        if r["filled"]:
            line("top speed", r, speed(r["speed"]))


if __name__ == "__main__":
    main()
