#!/usr/bin/env python3
"""Names for functions from the EA libraries' error messages.

    python3 tools/errnames.py build/nfs_layout.json build/nfs_flat.bin build/decomp/INDEX.txt > build/names.txt

EA's code prints "<function> - <what went wrong>" ("openhandle - %s FILE ERROR",
"initgraphics - UNABLE TO ..."). The relocation records list every pointer to such a
string; the function that holds the pointer is given the name, when only one name points
into it.  Output: "0x<entry> <name>" a line, for tools/RenameFuncs.java.
"""
import bisect, json, re, struct, sys

L = json.load(open(sys.argv[1]))
img = open(sys.argv[2], "rb").read()
entries = sorted(int(l.split()[0], 16) for l in open(sys.argv[3]))
CODE_END = 0x10000 + L["objects"][0]["vsize"]

byval = {}
for s in L["fixup_sites"]:
    byval.setdefault(struct.unpack_from("<I", img, s)[0], []).append(s)

pat = re.compile(rb"([a-z_][a-z_0-9]{3,})( ?\(\))? -+ ")
names = {}
for t, sites in byval.items():
    if t < 0xD0000 or t >= len(img):
        continue
    m = pat.match(img, t)
    if not m:
        continue
    for s in sites:
        if s >= CODE_END:
            continue
        i = bisect.bisect_right(entries, s) - 1
        if i >= 0:
            names.setdefault(entries[i], set()).add(m.group(1).decode())

for ea in sorted(names):
    if len(names[ea]) == 1:
        print("0x%x %s" % (ea, names[ea].pop()))
    else:
        print("# 0x%x several: %s" % (ea, " ".join(sorted(names[ea]))))
