#!/usr/bin/env python3
"""The strings each function uses.

    python3 tools/strmap.py build/nfs_layout.json build/nfs_flat.bin build/decomp/INDEX.txt [addr ...]

For every pointer from the code into a printable string in the data (the relocation
records list them all), the function holding the pointer and the string. With addresses
given, only those functions. Without INDEX.txt ("-"), the pointer's own address.
"""
import bisect, json, struct, sys

L = json.load(open(sys.argv[1]))
img = open(sys.argv[2], "rb").read()
entries = [] if sys.argv[3] == "-" else sorted(int(l.split()[0], 16) for l in open(sys.argv[3]))
want = {int(a, 16) for a in sys.argv[4:]}
# the code objects (LE object flag 4: executable); everything else is data
CODE = [(o["base"], o["base"] + o["vsize"]) for o in L["objects"] if o["flags"] & 4]


def in_code(a):
    return any(lo <= a < hi for lo, hi in CODE)


def string_at(a):
    e = a
    while e < len(img) and (32 <= img[e] < 127 or img[e] in (9, 10, 13)):
        e += 1
    if e - a < 3 or e >= len(img) or img[e] != 0:
        return None
    return img[a:e].decode("latin-1")


out = {}
for s in L["fixup_sites"]:
    if not in_code(s):
        continue
    t = struct.unpack_from("<I", img, s)[0]
    if in_code(t) or t >= len(img):
        continue
    st = string_at(t)
    if st is None:
        continue
    if entries:
        i = bisect.bisect_right(entries, s) - 1
        f = entries[i] if i >= 0 else 0
    else:
        f = s
    if want and f not in want:
        continue
    out.setdefault(f, []).append((s, t, st))

for f in sorted(out):
    print("%x" % f)
    for s, t, st in sorted(out[f]):
        print("   %x -> %x  %r" % (s, t, st[:100]))
