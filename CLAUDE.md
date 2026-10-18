# ReNFS — *The Need for Speed SE* (1996, MS-DOS) for Linux and the MiSTer

Reverse engineering and a hand-written reimplementation of `NFS_DOS.EXE`, the DOS program
of *The Need for Speed Special Edition*. A native SDL2 port (Linux x86, then ARM), then a
MiSTer hybrid core (game on the ARM, the FPGA for 15 kHz video, audio and input).

**This file is the summary. `docs/NOTES.md` is the full record**: addresses, proofs,
formats, dead ends. New findings go there in detail and here as one line.
**`docs/HANDOVER.md` is where the work stands**: read it first in a new session.

---

## Working agreement — read this first

**Danik does not write code or decompile.** He knows the MiSTer and these ports, and he
does the verification and judges whether it plays like the original. Don't hand him code
to review, patches to apply or compiler errors to sort out.

- **Claude:** all code, tooling, decompilation, builds, debugging. This machine has
  Ghidra 12.1.2 (`/opt/ghidra`), python3, wine, objdump, docker. The `Read` tool opens
  PNGs: look at the pictures you make yourself.
- **Danik:** real hardware, whether it looks and feels like the original, and decisions
  about scope, licences and targets.

"Here is a build and a screenshot" beats "please check this function". Everything builds
with one command. Plain language, not jargon.

**The method is the skill `reverse-engineer-dos-game`** (`~/.claude/skills/`: the steps,
the toolkit, the decompiler's traps, checking against the original). What ReNFS learns that
holds for any game goes into the skill too.

The model project is **OpenScreamer** (`~/openscreamer`, its `CLAUDE.md` and
`docs/NOTES.md`): the same kind of program (Watcom C, DOS/4GW, LE), the same tools, the
same rules. Its "Hard-won rules" hold here too, above all: **when a format question
stalls, find the code that reads the field.**

---

## Decisions (Danik, 2026-10-18)

- Engine name **ReNFS** (`~/renfs`, the core `~/MiSTer-ReNFS`); game id
  `SE|Need for Speed SE` (menu entry `_Other/dnk-Need for Speed SE.mgl`).
- **Hand-written from the start.** The decompiled code is read, and clean C is written that
  does the same thing. Machine-converted code may exist only as an unpublished local
  reference for checking, under `build/`. OpenScreamer had to write every converted routine
  again by hand later; don't repeat that.
- **The player provides only the CD's files** (the folder `NFSSE/`, the ISO extracted).
  The installed copy (`NFSSE-Install/`) holds nothing the CD lacks, apart from `NFS.CFG`,
  `GAMEDATA/CONFIG/PATHS.DAT` and `NFS.EXE` (= `NFS_DOS.EXE`): the port makes its own
  settings and never needs an installation.
- **The menus on the MiSTer are 640x480** (interlaced on a 15 kHz screen, progressive with
  *HDMI Only* / VGA); races at 320x200 / 320x240.
- **Commit locally as the work goes** (each finished step, in `~/renfs` and
  `~/MiSTer-ReNFS`), never game data or decompiled code; nothing is pushed.
- GitHub: **local only** until Danik says otherwise. Ask before creating
  `ItsDanik/renfs` (branch `mister`) or `ItsDanik/ReNFS_MiSTer`.

## Legal

The game is still EA's. **Never publish `NFSSE/`, `NFSSE-Install/` or anything made from
them**: `build/` (the flat image, the Ghidra project, `build/decomp`, PNGs, unpacked files)
is ignored. Decompiled text is never copied into `port/`. Check `git status` before every
commit.

---

## The binary and how to decompile it

`NFS_DOS.EXE`, 1,254,591 bytes, Watcom C/C++32 (1988–1995 runtime), DOS/4GW stub (11,392
bytes) + **LE** image. Objects: 1 code `0x10000` (698,877 bytes), 2 code `0xC0000` (60),
3 data `0xD0000` (24), 4 data + BSS + stack `0xE0000` (563,728). Entry `0x94444` (a jump
over the Watcom banner to `0x944bc`); **`main` = `0x4716c`**. No symbols.

**One command: `tools/rebuild.sh`** (about an hour, logs in `build/logs/`;
`tools/rebuild.sh --decomp` = names from `tools/names.txt` + decompile again, minutes).
What it does:

```bash
tools/ghidra-ext/install.sh   # once per Ghidra install: x86:LE:32:watcom (already done on this machine)
python3 tools/unle.py NFSSE/NFS_DOS.EXE -o build/nfs_flat.bin --map build/nfs_layout.json --fixups
G=/opt/ghidra/support/analyzeHeadless
$G build/ghidra renfs -import build/nfs_flat.bin -processor x86:LE:32:watcom -loader BinaryLoader \
   -loader-baseAddr 0x0 -noanalysis -scriptPath tools -postScript CreateEntry.java 0x94444
$G build/ghidra renfs -process nfs_flat.bin -analysisTimeoutPerFile 3600
$G build/ghidra renfs -process nfs_flat.bin -noanalysis -scriptPath tools \
   -postScript DecompRange.java build/decomp 0x10000 0xC0100      # a first INDEX.txt
# seeds: code addresses that pointers take (relocation records), then analyse again
python3 tools/seeds.py build/nfs_layout.json build/nfs_flat.bin build/decomp/INDEX.txt > build/seeds.txt
$G build/ghidra renfs -process nfs_flat.bin -scriptPath tools -postScript SeedFuncs.java build/seeds.txt
$G build/ghidra renfs -process nfs_flat.bin -analysisTimeoutPerFile 3600
$G build/ghidra renfs -process nfs_flat.bin -noanalysis -scriptPath tools -postScript RenameFuncs.java tools/names.txt
$G build/ghidra renfs -process nfs_flat.bin -noanalysis -scriptPath tools \
   -postScript DecompRange.java build/decomp 0x10000 0xC0100      # one file a function + INDEX.txt
# (with DECOMP_BSS=0x129000: globals from there up that hold addresses are shown as ints)
```

- `--fixups` applies the LE relocation records, so every pointer in the image is a real
  address (no `RelocateRefs.java` needed). `build/nfs_layout.json` lists every fixup site.
- 1,953 functions, 89% of the code object (the rest is data in code).
- Calling conventions are mixed. The game and the Watcom runtime use `__watcall` (EAX, EDX,
  EBX, ECX, then the stack). **EA's libraries take their arguments on the stack**
  (`initgraphics(640, 480, 8)` is `push 8; push 0x1e0; push 0x280`): Ghidra drops them,
  so read call sites with `objdump -D -b binary -m i386 -M intel --start-address=... build/nfs_flat.bin`.
- `tools/strmap.py build/nfs_layout.json build/nfs_flat.bin build/decomp/INDEX.txt [addr..]`:
  the strings each function uses (the map's first draft; the full list `build/strs_by_site.txt`).
- `tools/errnames.py` names functions after EA's error messages ("openhandle - %s FILE
  ERROR") → `tools/names.txt` → `RenameFuncs.java`. Names given by hand go into
  `tools/names.txt` too, so a rebuilt project gets them back.
- Pitfalls carried over from OpenScreamer: never import the EXE itself (Ghidra has no LE
  loader); headless scripts are Java and need `-scriptPath tools`; log headless output to a
  file, never pipe it through `tail`.

---

## The program (details: NOTES §4)

`main` `0x4716c` → `startup` `0x42054` → `fe_outer_loop` `0x47000` → `fe_main_menu`
`0x42c30` → `race_from_menu` `0x3770c` → `race_run` `0x37228` → `race_main` `0x6356c`:
`race_step` `0x6348c` once per race-clock tick, then `race_draw_frame` `0x5bc40`. The timer
runs at 100 Hz; the race clock divides it in a 3-cycle (`race_timer_tick` `0x4c454`):
33.3 Hz by default (measured in DOSBox), 30–60 Hz by `0x12dd0c`. Names: `tools/names.txt`.
Arithmetic: 16.16 fixed point, the multiply rounds, 65,536 = a full turn (NOTES §4.5).
A step: the cars' driving every step, their sounds and the gearbox every 2nd, traffic and
police every 4th (flags `0x12f1bc`, measured). A race ends through `0x13c178`: 1 finished,
4 arrested (the second police ticket), 0x10 everyone turned back (NOTES §4.3, §4.8).
The original in DOSBox: `nfs-dos`, `tools/nfsdos.py` (unseen, keys, memory; NOTES §5).

## File formats (details in `docs/NOTES.md`, "Formats")

| Ext | What | Status |
|---|---|---|
| `.FSH` | EA shapes, "SHPI": directory of 4-char names, 8-bit pictures + palettes (`!pal`) | read (`tools/fsh.py`) |
| `.QFS` | packed FSH (RefPack `10FB`, 23 Huffman or pairs) | read (`tools/fsh.py`) |
| `.CFM` `.FAM` | `wwww` containers of `ORIP` 3D models (1/128 units) + `SHPI` textures: cars, track families | read (`tools/orip.py`) |
| `.TRI` | track: road nodes (x y z, heading, lanes), speeds a section, objects, the road's 3D points | read (`tools/tri.py`) |
| `xxxINFO` | a track's sky and horizon settings, text | read (`tools/trackinfo.py`) |
| `.PBS` `.PDN` | car physics (torque, gears, ABS/TCS), Huffman-packed `30FB` | first reading (`tools/carspec.py`) |
| `xxFB` | EA's packers: RefPack `10FB`, Huffman `30FB`/`32FB`/`34FB`, pairs `46FB` | read (`tools/unpack.py`, equal to the game's own on all 527) |
| `.EAS` `.ASF` `.AS4` | EA sound/music (`EACS`, `1SNh` streams): PCM or IMA ADPCM | read (`tools/eacs.py`) |
| `.BNK` | EA sound bank (engines, collisions): 128 offsets, `EACS` records | read (`tools/eacs.py`) |
| `.TGV` | EA video: `kVGT` key frames (RefPack), `fVGT` 4x4 blocks + motion vectors, 320x240 at 15 a second | read (`tools/tgv.py`, equal to ffmpeg) |
| `.FFN` | fonts (`FNTF`): 4-bit smoothed bitmap + character table | read (`tools/ffn.py`) |
| `.INV` | the menus' inverse palettes (15-bit colour → entry), 32 KB | read (`tools/inv.py`) |
| `.B00`.. `.MMB` `.FMM` | `BAMF` blit lists: which spans of the 3D view reach the screen (HUD, dashboards) | read (`tools/bamf.py`) |
| `.RPL` | replays: the race record, every driver's controls (a word every 2nd step), car snapshots every 27 s | read (`tools/rpl.py`) |
| `CONFIG.DAT` | the front end's state and the records (9 tracks, times in 1/60 s, speeds 16.16 m/s), copied whole to `0x156c14` | read (`tools/config.py`) |

Language: English art is `ART`, `SHOW`, `SLIDES`, `DASH`, `SPEECH`, `NTRACKFM`/`ETRACKFM`;
German has a `G` in front (`GART`, `GSHOW`, ...).

---

## Video

The game uses two modes: **320x200** (low detail, `initgraphics(320, 200, 0)`) and
**640x480x8** (VESA, `initgraphics(640, 480, 8)`). **The front end exists only at 640x480**
(every menu picture is 640x480, there is no low-resolution set). On the MiSTer the menus
stay 640x480 (Danik, 2026-10-18).
