#!/bin/bash
# Every check of the port against the original's own code (Unicorn on the snapshots in
# build/snap): the driving, the timing, the opponent and the field, cars meeting, the
# cameras, the dashboard, the cars drawn, the road, the tyres' marks, the watching bar; at 320x200 and 640x480; the force feedback. Logs in build/logs/chk; a line each.
cd "$(dirname "$0")/.." || exit 1
P=build/venv/bin/python; L=build/logs/chk; mkdir -p $L
ALL="build/snap/d1.bin build/snap/d2.bin build/snap/d3.bin build/snap/d4.bin build/snap/e1.bin build/snap/e2.bin build/snap/e3.bin build/snap/e4.bin build/snap/e5.bin build/snap/r1a.bin build/snap/r1b.bin"
for t in drive_emu timing_emu opponent_emu collide_emu; do
    timeout 3000 $P tools/$t.py > $L/$t.log 2>&1; echo "$t exit $?: $(tail -1 $L/$t.log | cut -c1-150)"
done
timeout 3000 $P tools/camera_emu.py cy1 $ALL > $L/camera_emu.log 2>&1; echo "camera exit $?: $(tail -1 $L/camera_emu.log | cut -c1-150)"
timeout 3000 $P tools/dash_emu.py cy1 build/snap/e1.bin build/snap/e2.bin build/snap/e3.bin build/snap/e4.bin > $L/dash_emu.log 2>&1; echo "dash exit $?: $(tail -1 $L/dash_emu.log | cut -c1-150)"
timeout 3000 $P tools/cars_emu.py cy1 build/snap/e3.bin > $L/cars_emu.log 2>&1; echo "cars exit $?: $(tail -1 $L/cars_emu.log | cut -c1-150)"
# 640x480 races (h1..h7: the original's default resolution; h7 from outside)
timeout 3000 $P tools/dash_emu.py cy1 build/snap/h1.bin build/snap/h6.bin build/snap/h7.bin > $L/dash_hi.log 2>&1; echo "dash 640 exit $?: $(tail -1 $L/dash_hi.log | cut -c1-150)"
timeout 3000 $P tools/roadpic_emu.py --walls --models cy1 build/snap/e3.bin build/snap/h6.bin build/snap/h7.bin > $L/roadpic.log 2>&1; echo "road exit $?: $(tail -1 $L/roadpic.log | cut -c1-150)"
timeout 3000 $P tools/cars_emu.py cy1 build/snap/h1.bin build/snap/h6.bin build/snap/h7.bin > $L/cars_hi.log 2>&1; echo "cars 640 exit $?: $(tail -1 $L/cars_hi.log | cut -c1-150)"
# the tyres' marks: random rings and cars on a 640x480 snapshot from outside and a 320x200 one
timeout 3000 $P tools/skid_emu.py build/snap/h7.bin build/snap/e3.bin > $L/skid_emu.log 2>&1; echo "skid exit $?: $(tail -1 $L/skid_emu.log | cut -c1-150)"
# the watching bar (another car watched): random steps and drawings, both sizes, two tracks
( timeout 3000 $P tools/watch_emu.py cy1 build/snap/e3.bin build/snap/h7.bin && timeout 3000 $P tools/watch_emu.py tr1 build/snap/t1.bin ) > $L/watch_emu.log 2>&1; echo "watch exit $?: $(grep -h "equal$" $L/watch_emu.log | tr '\n' ' ' | cut -c1-150)"
# the force feedback: the original's game side and driver commands against ff.c, random sequences
timeout 3000 $P tools/ff_emu.py --n=4000 build/snap/e1.bin > $L/ff_emu.log 2>&1; echo "ff exit $?: $(tail -1 $L/ff_emu.log | cut -c1-150)"
