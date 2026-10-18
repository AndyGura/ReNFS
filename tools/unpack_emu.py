#!/usr/bin/env python3
"""The game's own unpacker (0xa7020: 10FB, 30FB/32FB/34FB, 46FB, 4AFB) run in a CPU emulator,
to check tools/unpack.py against. Needs unicorn (build/venv: python3 -m venv
--system-site-packages build/venv && build/venv/bin/pip install unicorn).

    build/venv/bin/python tools/unpack_emu.py <packed file> <out>
"""
import struct, sys
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EAX

IMAGE = "build/nfs_flat.bin"
UNPACK = 0xa7020                # (src, dst, complain) on the stack, returns the size
SRC, DST, STACK, DONE = 0x400000, 0x800000, 0x1000000, 0x2000000


def unpack(data):
    img = open(IMAGE, "rb").read()
    mu = Uc(UC_ARCH_X86, UC_MODE_32)
    mu.mem_map(0, 0x200000)                     # the program, its data and BSS
    mu.mem_write(0, img)
    mu.mem_map(SRC, 0x400000)
    mu.mem_write(SRC, data)
    mu.mem_map(DST, 0x400000)
    mu.mem_map(STACK - 0x100000, 0x101000)
    mu.mem_map(DONE, 0x1000)
    sp = STACK - 0x100
    mu.mem_write(sp, struct.pack("<4I", DONE, SRC, DST, 0))
    mu.reg_write(UC_X86_REG_ESP, sp)
    mu.emu_start(UNPACK, DONE)
    n = mu.reg_read(UC_X86_REG_EAX)
    return bytes(mu.mem_read(DST, n))


if __name__ == "__main__":
    out = unpack(open(sys.argv[1], "rb").read())
    open(sys.argv[2], "wb").write(out)
    print("%s: %d bytes" % (sys.argv[2], len(out)))
