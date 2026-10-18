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


mem, base = None, None


def find():
    """the host address of the data object: DOSBox's emulated memory holds it unmoved"""
    global mem, base
    pid = None
    for l in subprocess.run(["pgrep", "-a", "dosbox"], capture_output=True, text=True).stdout.splitlines():
        if l.split()[1].endswith("dosbox"):
            pid = int(l.split()[0])
    if pid is None:
        sys.exit("no dosbox process")
    mem = open("/proc/%d/mem" % pid, "rb", 0)
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
            print("data object at host 0x%x (dosbox pid %d)" % (base, pid))
            return
    sys.exit("the data object was not found (the game not loaded yet?)")


def i32(a):
    return struct.unpack("<i", os.pread(mem.fileno(), 4, base + a - DATA))[0]


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
        elif p[0] == "find":
            find()
        sys.stdout.flush()
finally:
    subprocess.run(["flatpak", "kill", "io.github.dosbox-staging"])
    proc.wait()
