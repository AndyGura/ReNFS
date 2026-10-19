#!/usr/bin/env python3
"""Run the ORIGINAL Need for Speed SE in DOSBox, unseen, and read its memory while it runs.

    python3 tools/nfsdos.py <out prefix> "<plan>"

The plan is a comma-separated list of steps:
    wait:<s>            wait so many seconds
    key:<name>[:<s>]    press and release a key (X keysym name: Return, Escape, Up, Down,
                        Left, Right, space, a ...), then wait <s> seconds (default 1)
    hold:<name>:<s>     hold a key down for <s> seconds
    shot:<name>         a picture of the screen: <prefix>_<name>.png
    watch:<s>           print the game's clocks every 0.25 s for <s> seconds
    peek:<s>:<a>[+<a>..] sample these 32-bit words (flat image addresses, hex) every 5 ms
                        for <s> seconds: a row every 0.5 s, then each word's values seen
    dump:<ptr>:<n>:<f>  n bytes (hex count) at the pointer held in the word at flat address
                        <ptr> (hex), written to the file f: a table on the game's heap
    mem:<a>:<n>:<f>     n bytes (hex count) at the flat address a (hex) in the data object
    car:<n>             car n's record (through the pointers at 0x13c24c): position +0 +4 +8
                        (16.16), road node +0x48, across the road +0x80, speed +0x70
    snap:<f>            the game paused and its whole memory (32 MB, by the game's own
                        addresses) written to f, its offset to the flat image in f.json:
                        for running the game's code on it (tools/snap_emu.py)
    find                find the game's data object in DOSBox's memory (needed before watch;
                        done by the first watch if not done before)

Needs Xvfb running on :77 (`Xvfb :77 -screen 0 1280x1024x24 &`), the launcher
~/.local/bin/nfs-dos, python-xlib, and build/nfs_flat.bin (for the signature that finds
the data object). DOSBox is started as this script's child, so that the script may read
/proc/<pid>/mem (ptrace_scope 1). The log goes to <prefix>.log.

What it reads (flat image addresses, NOTES §4): the system clock 0xf7980 (100 Hz), the race
clock 0xed0f8, the race state 0xf6b74, the steps done 0xf6d60 / due 0xf6d64, the clock
setting 0x12dd0c, the reloads 0x12f1a4 / 0x12f1a8.
"""
import os, re, struct, subprocess, sys, time
from Xlib import display, X, XK
from Xlib.ext import xtest

out, plan = sys.argv[1], sys.argv[2]
here = os.path.dirname(os.path.abspath(__file__))
IMG = open(os.path.join(here, "..", "build", "nfs_flat.bin"), "rb").read()
DATA = 0xE0000                      # the data object's flat address
SIG_AT = 0xE2D14                    # the car sound banks' names: only the data object holds them
SIG = IMG[SIG_AT:SIG_AT + 64]

env = dict(os.environ, DISPLAY=":77", NFS_DOS_FLATPAK="--nosocket=wayland --socket=x11 "
           "--env=DISPLAY=:77 --env=SDL_AUDIODRIVER=dummy --env=SDL_VIDEODRIVER=x11")
env.pop("WAYLAND_DISPLAY", None)
proc = subprocess.Popen([os.path.expanduser("~/.local/bin/nfs-dos")], env=env,
                        stdout=open(out + ".log", "w"), stderr=subprocess.STDOUT)
d = display.Display(":77")
code = lambda n: d.keysym_to_keycode(XK.string_to_keysym(n))


def key(n, down):
    xtest.fake_input(d, X.KeyPress if down else X.KeyRelease, code(n))
    d.sync()


def shot(name):
    subprocess.run(["import", "-display", ":77", "-window", "root", "%s_%s.png" % (out, name)])


mem, base, prog = None, None, None


def find():
    """the host address of the data object: DOSBox's emulated memory holds it unmoved"""
    global mem, base, prog
    pid = None
    for l in subprocess.run(["pgrep", "-a", "dosbox"], capture_output=True, text=True).stdout.splitlines():
        if l.split()[1].endswith("dosbox"):
            pid = int(l.split()[0])
    if pid is None:
        sys.exit("no dosbox process")
    mem = open("/proc/%d/mem" % pid, "rb", 0)
    globals()["pid"] = pid
    for l in open("/proc/%d/maps" % pid):
        m = re.match(r"([0-9a-f]+)-([0-9a-f]+) rw", l)
        if not m:
            continue
        a, b = int(m.group(1), 16), int(m.group(2), 16)
        if not 0x100000 <= b - a <= 0x10000000:
            continue
        try:
            buf = os.pread(mem.fileno(), b - a, a)
        except OSError:
            continue
        i = buf.find(SIG)
        if i >= 0:
            base = a + i - (SIG_AT - DATA)
            # The game's pointers are its own (paged) addresses, not DOSBox's physical ones.
            # A relocated pointer in the data object (0xea828 -> image 0xe0afc) gives the
            # object's own address; the game's memory is one run, so one offset holds.
            prog = (struct.unpack("<I", os.pread(mem.fileno(), 4, base + 0xEA828 - DATA))[0]
                    - (0xE0AFC - DATA))
            print("data object at host 0x%x, its own address 0x%x (dosbox pid %d)" % (base, prog, pid))
            return
    sys.exit("the data object was not found (the game not loaded yet?)")


def dos(p, n):
    """n bytes at the game's own address p (a pointer read from its memory)"""
    return os.pread(mem.fileno(), n, base + p - prog)


def i32(a):
    return struct.unpack("<i", os.pread(mem.fileno(), 4, base + a - DATA))[0]


def car(n):
    if base is None:
        find()
    p = i32(0x13C24C + 4 * n)
    rd = lambda o: struct.unpack("<i", dos(p + o, 4))[0]
    print("car %d at 0x%x: x %.3f y %.3f z %.3f  node %d  across %.3f  speed %.3f" % (
        n, p, rd(0) / 65536, rd(4) / 65536, rd(8) / 65536, rd(0x48), rd(0x80) / 65536,
        rd(0x70) / 65536))


def watch(secs):
    if base is None:
        find()
    print("   t     sys100  raceclk  state  done   due  set  r_a4 r_a8")
    t0 = time.time()
    first = None
    while time.time() - t0 < secs:
        row = (time.time() - t0, i32(0xF7980), i32(0xED0F8), i32(0xF6B74), i32(0xF6D60),
               i32(0xF6D64), i32(0x12DD0C), i32(0x12F1A4), i32(0x12F1A8))
        print("%6.2f %9d %8d %6d %5d %5d %4d %5d %4d" % row)
        if first is None and row[3] == 1:
            first = row
        time.sleep(0.25)
    if first and row[3] == 1 and row[1] > first[1]:
        print("race clock ticks per 100 system ticks: %.2f" % (100.0 * (row[2] - first[2]) / (row[1] - first[1])))


def peek(secs, addrs):
    if base is None:
        find()
    addrs = [int(a, 16) for a in addrs.split("+")]
    print("   t  " + " ".join("%10x" % a for a in addrs))
    seen = [dict() for a in addrs]
    t0 = last = time.time()
    while time.time() - t0 < secs:
        row = [i32(a) for a in addrs]
        for s_, v in zip(seen, row):
            s_[v] = s_.get(v, 0) + 1
        if time.time() - last >= 0.5:
            last = time.time()
            print("%6.2f" % (last - t0) + " ".join("%10d" % v for v in row))
        time.sleep(0.005)
    for a, s_ in zip(addrs, seen):
        vals = sorted(s_)
        odd = sum(1 for v in vals if v & 1)
        print("0x%x: %d values, %d odd, from %d to %d%s" % (a, len(vals), odd, vals[0], vals[-1],
              (": " + " ".join("%d(x%d)" % (v, s_[v]) for v in vals)) if len(vals) <= 12 else ""))


try:
    for step in plan.split(","):
        p = step.split(":")
        if p[0] == "wait":
            time.sleep(float(p[1]))
        elif p[0] == "key":
            key(p[1], 1); time.sleep(0.1); key(p[1], 0)
            time.sleep(float(p[2]) if len(p) > 2 else 1.0)
        elif p[0] == "hold":
            key(p[1], 1); time.sleep(float(p[2])); key(p[1], 0)
        elif p[0] == "shot":
            shot(p[1])
        elif p[0] == "watch":
            watch(float(p[1]))
        elif p[0] == "peek":
            peek(float(p[1]), p[2])
        elif p[0] == "car":
            car(int(p[1]))
        elif p[0] == "dump":
            if base is None:
                find()
            at = i32(int(p[1], 16))
            open(p[3], "wb").write(dos(at, int(p[2], 16)))
            print("0x%x -> 0x%x: 0x%x bytes to %s" % (int(p[1], 16), at, int(p[2], 16), p[3]))
        elif p[0] == "mem":
            if base is None:
                find()
            open(p[3], "wb").write(os.pread(mem.fileno(), int(p[2], 16), base + int(p[1], 16) - DATA))
            print("0x%s: 0x%s bytes to %s" % (p[1], p[2], p[3]))
        elif p[0] == "snap":
            if base is None:
                find()
            import json, signal
            os.kill(pid, signal.SIGSTOP)
            try:
                # the game's memory is one run under DOS/4GW's paging: written from where
                # its address 0 would be, so the file is read by the game's own addresses
                # (what lies below its program is other memory, 0xa0000 is not the screen)
                open(p[1], "wb").write(os.pread(mem.fileno(), 32 << 20, base - prog))
            finally:
                os.kill(pid, signal.SIGCONT)
            json.dump({"flat_to_game": prog - DATA}, open(p[1] + ".json", "w"))
            print("memory to %s (the game's addresses = flat + 0x%x)" % (p[1], prog - DATA))
        elif p[0] == "find":
            find()
        sys.stdout.flush()
finally:
    subprocess.run(["flatpak", "kill", "io.github.dosbox-staging"])
    proc.wait()
