#!/usr/bin/env python3
"""The original's own code run on a snapshot of its memory (tools/nfsdos.py snap:<f>), in
a CPU emulator: a routine called as the game would, then its results read back. Needs
unicorn (build/venv, see tools/unpack_emu.py).

    from snap_emu import Snap
    s = Snap("build/snap/r1a.bin")
    s.call(0x5bc40)                      # race_draw_frame, by its flat image address
    s.i32(0xf88c8)                       # a word, by its flat address

    build/venv/bin/python tools/snap_emu.py <snapshot> <flat address> [args..]   one call, traced

The snapshot holds the game's memory by its own addresses (physical ones under DOS/4GW,
from 0). Each of the program's objects was loaded at its own place, so a flat image
address moves by its object's offset, read from the relocated pointers: the code object
+0x1e9000, the data object and its BSS +0x1c4000 (snapshot r1a; checked on every one of
the 27,294 pointer sites). The routines' pointers are the game's own; only the flat
addresses used to call them and read their globals are translated here.
"""
import struct, sys
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_INTR, UC_HOOK_INSN, UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_FPCW, UC_X86_REG_ESP, UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX,
                               UC_X86_REG_EDX, UC_X86_REG_EIP, UC_X86_INS_IN, UC_X86_INS_OUT,
                               UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP)

OBJECTS = [(0x10000, 0xc0000, 0x1e9000), (0xc0000, 0xd0000, 0xdd000), (0xd0000, 0xe0000, 0xce000),
           (0xe0000, 0x200000, 0x1c4000)]
STACK, DONE = 0x3000000, 0x3f00000


def game(flat):
    for lo, hi, off in OBJECTS:
        if lo <= flat < hi:
            return flat + off
    raise ValueError("0x%x is in no object" % flat)


def flat(addr):
    for lo, hi, off in OBJECTS:
        if lo + off <= addr < hi + off:
            return addr - off
    return None


class Snap:
    def __init__(self, path, trace_io=False):
        self.mu = mu = Uc(UC_ARCH_X86, UC_MODE_32)
        mu.mem_map(0, 32 << 20)
        mu.mem_write(0, open(path, "rb").read())
        mu.mem_map(STACK - 0x100000, 0x100000)
        mu.mem_map(DONE, 0x1000)
        # the x87 as the game sets it (Watcom's start-up, 0xa8c95: 0x128b08 = 0x127f,
        # double precision, rounding to nearest); Unicorn's own 0 is single precision
        mu.reg_write(UC_X86_REG_FPCW, 0x127f)
        self.io, self.ints = [], []

        def on_int(mu, n, _):
            self.ints.append((n, flat(mu.reg_read(UC_X86_REG_EIP))))

        def on_in(mu, port, size, _):
            self.io.append(("in", port))
            return 0

        def on_out(mu, port, size, value, _):
            self.io.append(("out", port, value))
        mu.hook_add(UC_HOOK_INTR, on_int)
        mu.hook_add(UC_HOOK_INSN, on_in, None, 1, 0, UC_X86_INS_IN)
        mu.hook_add(UC_HOOK_INSN, on_out, None, 1, 0, UC_X86_INS_OUT)

    def call(self, at, *stack, eax=0, edx=0, ebx=0, ecx=0, limit=0):
        """the routine at the flat address `at`; register arguments (Watcom) and stack ones
        (EA's libraries); returns EAX"""
        mu = self.mu
        sp = STACK - 0x1000 - 4 * len(stack)
        mu.mem_write(sp - 4, struct.pack("<I", DONE) + struct.pack("<%dI" % len(stack),
                                                                  *[v & 0xffffffff for v in stack]))
        mu.reg_write(UC_X86_REG_ESP, sp - 4)
        for r, v in ((UC_X86_REG_EAX, eax), (UC_X86_REG_EDX, edx), (UC_X86_REG_EBX, ebx),
                     (UC_X86_REG_ECX, ecx)):
            mu.reg_write(r, v & 0xffffffff)
        mu.emu_start(game(at), DONE, count=limit)
        return mu.reg_read(UC_X86_REG_EAX)

    def read(self, flat_addr, n):
        return bytes(self.mu.mem_read(game(flat_addr), n))

    def raw(self, addr, n):
        """n bytes at one of the game's own addresses (a pointer read from its memory)"""
        return bytes(self.mu.mem_read(addr, n))

    def i32(self, flat_addr):
        return struct.unpack("<i", self.read(flat_addr, 4))[0]

    def u32(self, flat_addr):
        return struct.unpack("<I", self.read(flat_addr, 4))[0]

    def write(self, flat_addr, data):
        self.mu.mem_write(game(flat_addr), data)


if __name__ == "__main__":
    s = Snap(sys.argv[1])
    calls = {}

    def on_code(mu, addr, size, _):
        f = flat(addr)
        if f is not None:
            calls[f] = calls.get(f, 0) + 1
    s.mu.hook_add(UC_HOOK_CODE, on_code)
    try:
        r = s.call(int(sys.argv[2], 16), *[int(a, 0) for a in sys.argv[3:]])
        print("returned 0x%x" % r)
    except Exception as e:
        print("stopped: %s at 0x%x (flat 0x%x)" % (e, s.mu.reg_read(UC_X86_REG_EIP),
                                                    flat(s.mu.reg_read(UC_X86_REG_EIP)) or 0))
    print("%d instructions, %d places; interrupts %s; ports %d" % (
        sum(calls.values()), len(calls), sorted(set(s.ints))[:20], len(s.io)))
    ports = sorted(set(p[1] for p in s.io))
    print("ports used:", [hex(p) for p in ports])
