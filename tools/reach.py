#!/usr/bin/env python3
"""Which of the given roots reach each function (through the call graph of INDEX.txt).

    python3 -I tools/reach.py build/decomp/INDEX.txt tools/names.txt race_step=0x6348c draw=0x5bc40 ...

Prints, per function in address order: address, size, the roots that reach it, its name.
A function reached by one root only belongs to that root's subsystem; the shared ones are
the libraries. Roots are not followed through each other (a root stops the walk).
"""
import re, sys

calls, size = {}, {}
for l in open(sys.argv[1]):
    a = int(l.split()[0], 16)
    size[a] = int(l.split()[1])
    m = re.search(r"calls \[([^\]]*)\]", l)
    calls[a] = [int(x, 16) for x in m.group(1).split(", ") if x] if m else []
names = {}
for l in open(sys.argv[2]):
    if l.strip() and not l.startswith("#"):
        names[int(l.split()[0], 16)] = l.split()[1]
roots = [(r.split("=")[0], int(r.split("=")[1], 16)) for r in sys.argv[3:]]
rootset = {a for _, a in roots}
reach = {a: [] for a in size}
for tag, r in roots:
    seen, todo = set(), [r]
    while todo:
        a = todo.pop()
        if a in seen:
            continue
        seen.add(a)
        reach.setdefault(a, []).append(tag)
        todo += [c for c in calls.get(a, []) if c not in rootset]
for a in sorted(size):
    print("%6x %6d  %-30s %s" % (a, size[a], ",".join(reach[a]) or "-", names.get(a, "")))
