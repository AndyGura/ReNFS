#!/usr/bin/env python3
"""Stack-frame accesses made while arguments are pushed: where [esp+N] is not the frame's N.

    python3 tools/espdepth.py <flat.bin> <start> <end>

Watcom keeps a function's locals at fixed [esp+N] once its prologue's `sub esp,K` is done,
but each `push` (an argument for a library that takes them on the stack) moves esp, so
between a push and the call's `add esp` the same local is [esp+N+4]. A store such as
`push ecx; mov [esp+0x730],1` therefore writes the frame's 0x72c, not 0x730: read as 0x730
it looks like a variable nobody reads (ReNFS: the load / save screen's full redraw, the
file list's first choice). Ghidra gets these right; reading objdump by eye does not.

Prints each access to [esp+..] made with something pushed: its address, the bytes pushed,
the instruction, and the frame offset it really means. The depth is followed in order of
the addresses: it is set to 0 after a jmp or a ret, and after a call that no `add esp`
follows (a Watcom callee that takes its stack arguments off itself), so a branch target
reached with pushes pending can be missed or reported wrongly: a list to check by eye.
"""
import re
import subprocess
import sys


def main():
    flat, start, end = sys.argv[1], int(sys.argv[2], 16), int(sys.argv[3], 16)
    out = subprocess.run(["objdump", "-D", "-b", "binary", "-m", "i386", "-M", "intel",
                          "--start-address=%#x" % start, "--stop-address=%#x" % end, flat],
                         capture_output=True, text=True).stdout
    ins = []
    for line in out.splitlines():
        m = re.match(r"\s*([0-9a-f]+):\s+(?:[0-9a-f]{2} )+\s*(\w+)\s*(.*)", line)
        if m:
            ins.append((int(m.group(1), 16), m.group(2), m.group(3).strip()))
    depth = None                       # unknown until the prologue's sub esp
    for i, (a, op, args) in enumerate(ins):
        if depth is None:
            if op == "sub" and args.startswith("esp,"):
                depth = 0
            continue
        m = re.search(r"\[esp\+(0x[0-9a-f]+|\d+)\]", args)
        if m and depth > 0:
            n = int(m.group(1), 0)
            print("%x  +%-3d %-5s %-40s frame %#x" % (a, depth, op, args, n - depth))
        if op == "push":
            depth += 4
        elif op == "pop":
            depth -= 4
        elif op in ("add", "sub") and args.startswith("esp,"):
            n = int(args[4:], 0)
            depth += -n if op == "add" else n
        elif op in ("jmp", "ret"):
            depth = 0
        elif op == "call":
            nxt = ins[i + 1] if i + 1 < len(ins) else None
            if not (nxt and nxt[1] == "add" and nxt[2].startswith("esp,")):
                depth = 0
        if depth < 0:
            depth = 0


main()
