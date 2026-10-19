#!/usr/bin/env python3
"""The built-in settings and records, the original's against the port's: cfg_defaults
(0x38ea8) run on a snapshot from a cleared 0x156c14 (the speed test 0x74ca0 answering as
the port does, the CPU check 0x8ee80 left out), English and German, and the port's
config_load with no copy in its home folder. Prints the bytes that differ.

    build/venv/bin/python tools/config_emu.py build/snap/e1.bin
"""
import ctypes, os, struct, sys, tempfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from snap_emu import Snap, game
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EAX
import cars_emu

SIZE, AT = 0x5f52, 0x156c14
SPEED = 500                    # the port's answer to the speed test (0x1a4..599)


def original(path, german):
    s = Snap(path)

    def ret(value):
        def hook(mu, addr, size, k):
            sp = mu.reg_read(UC_X86_REG_ESP)
            mu.reg_write(UC_X86_REG_EIP, struct.unpack("<I", bytes(mu.mem_read(sp, 4)))[0])
            mu.reg_write(UC_X86_REG_ESP, sp + 4)
            if value is not None:
                mu.reg_write(UC_X86_REG_EAX, value)
        return hook
    s.mu.hook_add(UC_HOOK_CODE, ret(None), 0, begin=game(0x8ee80), end=game(0x8ee80))
    s.mu.hook_add(UC_HOOK_CODE, ret(SPEED), 0, begin=game(0x74ca0), end=game(0x74ca0))
    def sprintf(mu, addr, size, k):    # (its segment pops are beyond the emulator; the
        sp = mu.reg_read(UC_X86_REG_ESP)  # texts are plain, no formats)
        dst, fmt = struct.unpack("<II", bytes(mu.mem_read(sp + 4, 8)))
        t = bytes(mu.mem_read(fmt, 64)).split(b"\0")[0]
        assert b"%" not in t, t
        mu.mem_write(dst, t + b"\0")
        ret(len(t))(mu, addr, size, k)
    s.mu.hook_add(UC_HOOK_CODE, sprintf, 0, begin=game(0x8198b), end=game(0x8198b))
    s.write(AT, bytes(SIZE))
    s.write(0x15d140, struct.pack("<h", german))
    s.call(0x38ea8)
    return s.read(AT, SIZE)


def main():
    path = sys.argv[1]
    L = cars_emu.lib()
    L.files_init(b"NFSSE", None)
    if not L.exe_load():
        sys.exit("no NFS_DOS.EXE")
    os.environ["RENFS_HOME"] = tempfile.mkdtemp()
    os.environ.pop("RENFS_CONFIG", None)
    for german in (0, 1):
        ctypes.c_int.in_dll(L, "g_german").value = german
        L.files_init(b"NFSSE", None)
        made = L.config_load()
        got = bytes((ctypes.c_ubyte * SIZE).in_dll(L, "g_config"))
        want = original(path, german)
        d = [i for i in range(SIZE) if got[i] != want[i]]
        print("%s: config_load %s; %s" % ("German" if german else "English",
              "made the defaults" if made == 0 else "read a copy (wrong)",
              "all %d bytes equal" % SIZE if not d else "%d bytes differ, first +0x%x (port %s, original %s)" % (
                  len(d), d[0], got[d[0]:d[0] + 8].hex(), want[d[0]:d[0] + 8].hex())))


if __name__ == "__main__":
    main()
