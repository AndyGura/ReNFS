#!/usr/bin/env python3
"""A track's sky and horizon settings: SIMDATA/MISC/<track>INFO (text), read by
load_track_info 0x49dc0 at the race's start.

    python3 -I tools/trackinfo.py <file>...

Read as the game reads it (0x38c50): a line at a time, lines starting with '#', or empty,
skipped; one value a line, the two colours three on one line. The first eight through
0x41b68 (a decimal number into 16.16), the rest through atoi (so CY3INFO's stray "o" is 0).
When the file is missing (or 0x12c340 is 1) the game takes a table built into the program
instead (33 values a track from 0xec5dc, cl1 cl2 cl3 al1 al2 al3 cy1 cy2 cy3 tr1; older
values than the files').
"""
import re, sys

FIELDS = [  # name, the global it goes to (NOTES §3.11)
    ("vert offset near", 0x12EF44), ("vert offset far", 0x12EF48),
    ("zoffset near", 0x12EF4C), ("zoffset far", 0x12EF50),
    ("height near", 0x12EF58), ("height far", 0x12EF60),
    ("radius near", 0x12EF54), ("radius far", 0x12EF5C),
    ("colour top", 0x12EF64), ("colour bottom", 0x12EF68),
    ("ring type", 0x12EF6C), ("ring y offset", 0x12EF70), ("ring height", 0x12EF74),
]
# then "RGB TIME OF DAY VALUES": three groups of five, to 0x12ef78 + 4 g + 12 k (k = 0..4,
# the fifth x 256), and one more to 0x12efb4


def atoi(s):
    m = re.match(r"\s*([-+]?\d+)", s)
    return int(m.group(1)) if m else 0


def fixed(s):
    s = s.strip()
    try:
        return float(s.split()[0]) if s else 0.0
    except ValueError:
        return float(atoi(s))


def read(path):
    lines = [l for l in open(path, "rb").read().decode("latin1").replace("\r", "").split("\n")
             if l and l[0] not in "#\n"]
    out, i = {}, 0
    for name, _ in FIELDS[:8]:
        out[name] = fixed(lines[i]); i += 1
    for name, _ in FIELDS[8:10]:
        out[name] = tuple(atoi(t) for t in lines[i].split()[:3]); i += 1
    for name, _ in FIELDS[10:]:
        out[name] = atoi(lines[i]); i += 1
    out["time of day"] = [[atoi(lines[i + 5 * g + k]) for k in range(5)] for g in range(3)]
    out["last"] = atoi(lines[i + 15]) if i + 15 < len(lines) else 0
    return out


if __name__ == "__main__":
    for p in sys.argv[1:]:
        v = read(p)
        print("%s:" % p)
        for k, x in v.items():
            print("   %-17s %s" % (k, x))
