#!/usr/bin/env python3
"""Seeds for Ghidra: code addresses that pointers take (relocation records).

    python3 tools/seeds.py build/nfs_layout.json build/nfs_flat.bin build/decomp/INDEX.txt > build/seeds.txt

A pointer from the data to the code, or a code pointer pushed or loaded into a register
(push / mov reg / mov [mem], imm32), is a function Ghidra may not have found. Runs of
pointers in the code are jump tables and are left out. Only the new entries are printed.
"""
import json, struct, sys

L = json.load(open(sys.argv[1]))
img = open(sys.argv[2], "rb").read()
starts = {int(l.split()[0], 16) for l in open(sys.argv[3])}
CODE_END = 0x10000 + L["objects"][0]["vsize"]
sites = set(L["fixup_sites"])

seeds = set()
for s in sites:
    t = struct.unpack_from("<I", img, s)[0]
    if not 0x10000 <= t < CODE_END:
        continue
    if s >= 0xD0000:
        seeds.add(t)
    elif s < CODE_END:
        if s - 4 in sites or s + 4 in sites:
            continue
        op = img[s - 1]
        if op == 0x68 or 0xB8 <= op <= 0xBF or img[s - 2] == 0xC7:
            seeds.add(t)

new = sorted(t for t in seeds if t not in starts)
print("%d seeds, %d new" % (len(seeds), len(new)), file=sys.stderr)
sys.stdout.write("".join("0x%x\n" % t for t in new))
