#!/usr/bin/env python3
"""The port's track loader (port/src/track.c) against the original's memory in a snapshot:
the head of the TRI as loaded, and the ring of decoded sections round the camera
(tri_keep_sections 0x4ee30, tri_decode_section 0x4eb00).

    build/venv/bin/python tools/track_emu.py cy1 build/snap/d2.bin [...]
"""
import ctypes, os, struct, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from snap_emu import Snap


def lib():
    out = "build/track_test.so"
    subprocess.check_call(["cc", "-O1", "-shared", "-fPIC", "-o", out, "port/src/track.c",
                           "port/src/files.c", "-Iport/src"])
    return ctypes.CDLL(os.path.abspath(out))


def main():
    name, snaps = sys.argv[1], sys.argv[2:]
    L = lib()
    L.files_init(b"NFSSE", None)
    L.track_section.restype = ctypes.c_void_p
    for p in snaps:
        if not L.track_load(name.encode()):
            sys.exit("no track")
        s = Snap(p)
        node = struct.unpack("<i", s.raw(s.u32(0x143df4) + 0x38, 4))[0]
        L.track_keep_sections(node // 4 if node >= 0 else -((-node) // 4))
        head = s.i32(0x1365ec)
        ring = s.raw(s.u32(0x1365e8), 0x8700)
        bad = []
        for sec in range(head - 35, head):
            if sec < 0:
                continue
            at = (sec * 0x21c) % 0x8700
            got = ctypes.string_at(L.track_section(sec), 0x21c)
            if got != ring[at:at + 0x21c]:
                bad.append(sec)
        print("%s: camera node %d, ring up to section %d: %s" % (
            os.path.basename(p), node, head,
            "35 sections equal" if not bad else "sections %s differ" % bad))


if __name__ == "__main__":
    main()
