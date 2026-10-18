#!/usr/bin/env python3
"""The call tree under a routine, from build/decomp/INDEX.txt.

    python3 -I tools/calltree.py build/decomp/INDEX.txt 0x4716c [depth] [names.txt]

A routine already shown is printed again as "^" and not expanded; sizes are in bytes.
"""
import sys

idx = {}
for line in open(sys.argv[1]):
    p = line.split()
    ea = int(p[0], 16)
    calls = line.split("calls [")[1].rstrip().rstrip("]")
    idx[ea] = (int(p[1]), [int(c, 16) for c in calls.split(", ") if c])
root = int(sys.argv[2], 16)
depth = int(sys.argv[3]) if len(sys.argv) > 3 else 3
names = {}
if len(sys.argv) > 4:
    for line in open(sys.argv[4]):
        if line.strip() and not line.startswith("#"):
            a, n = line.split()[:2]
            names[int(a, 16)] = n
seen = set()


def show(ea, d):
    size = idx.get(ea, (0, []))[0]
    tag = names.get(ea, "")
    if ea in seen:
        print("  " * d + "%x %s ^" % (ea, tag))
        return
    seen.add(ea)
    print("  " * d + "%x %s(%d)" % (ea, tag + " " if tag else "", size))
    if d < depth:
        for c in idx.get(ea, (0, []))[1]:
            show(c, d + 1)


show(root, 0)
