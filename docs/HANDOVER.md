# Handover — where ReNFS stands

Read this first in a new session, then `CLAUDE.md` (the summary and the rules) and the
sections of `docs/NOTES.md` for the area you work on. Keep this file current: what is done,
what is next, what is waiting for Danik.

Start a new session in `~/renfs` (the game) or `~/MiSTer-ReNFS` (the core).

**2026-10-19: `~/renfs` was lost and rebuilt.** Deleting the old link `~/nfsse` (which
pointed to `~/renfs`) emptied the project. Restored from Claude Code's file backups (notes,
handover, `CLAUDE.md`, our own tools), `~/openscreamer/tools` (the copied tools), the game's CD image (→ `NFSSE/`) and its installed copy (→ `NFSSE-Install/`), and `tools/rebuild.sh` (the
Ghidra project and `build/decomp`, one command). The git history was started again.

## Done (2026-10-18, first session)

- Names agreed with Danik: engine **ReNFS**, game `SE|Need for Speed SE`, hand-written
  reimplementation, local repositories only.
- `~/renfs`: git repository, `.gitignore` keeping out the game
  (`NFSSE/`, `NFSSE-Install/`) and `build/`. Tools copied from OpenScreamer (`unle.py`, the
  Ghidra scripts, `ghidra-ext/`) and new ones: `tools/fsh.py` (FSH/QFS pictures to PNG,
  RefPack in Python), `tools/errnames.py` + `tools/RenameFuncs.java` (function names from
  EA's error messages, `tools/names.txt`).
- Ghidra project `build/ghidra/renfs` (`nfs_flat.bin`), analysed, seeded from the
  relocation records, 1,953 functions; every function decompiled into `build/decomp/`
  (`f_<addr>.c`, `INDEX.txt` with callers and callees). Rebuild steps: `CLAUDE.md`.
- `main` = `0x4716c` found; the start-up and the two video modes (320x200, 640x480) seen.
  First map of the program (NOTES §4: the front end's state machine `0x42c30`), the
  `wwww` containers' layout (§3.2), the TRI loader `0x4f010` and what it reads (§3.3).
  `tools/calltree.py` prints the call tree under a routine.
- `~/MiSTer-ReNFS`: stamped with `hybrid/new_core.sh ReNFS "Need for Speed SE"` (hybrid
  at `68b4bd4`, host version 14), `package/games.txt` = `SE|Need for Speed SE`; the core
  builds as stamped (`./core/build_core.sh`, timing met). The game
  submodule is not added yet (there is no repository to point at: add `~/renfs` as the
  submodule `renfs/` on its branch `mister` once Danik agrees on GitHub, or from the local
  path meanwhile).
- The launcher's catalogue needs no change: it is made from `package/games.txt` by
  `distribution.py` (the skill's step 3 predates that).

## Waiting for Danik

Nothing. Decided 2026-10-18: menus 640x480 on the MiSTer (interlaced at 15 kHz),
races 320x200 / 320x240; commit locally as the work goes.

## Next, in order

1. **Map the program.** Name the top-level routines: the front end's loop (`0x47000`?),
   the race (`0x3770c` → `0x1a5d8` / `0x1b6a0`?), the frame loop, the renderer, the car
   physics, opponents / traffic / police, sound, network (skip), replay. Put each name into
   `tools/names.txt` and the findings into `docs/NOTES.md`.
2. **Formats**, each from the code that reads it: `wwww`, TRI + INFO, CFM (car models),
   FAM (track textures and objects), PBS/PDN (`30FB`), FFN fonts, INV, BAMF, EA ADPCM
   (`EACS`/`1SNh`), BNK, TGV, RPL, CONFIG.DAT. A Python reader and a picture for each
   (`tools/`).
3. **The port's skeleton** (`port/`, CMake, SDL2, C): an 8-bit framebuffer at 640x480 and
   320x200, palette, the file layer (case-insensitive names in the CD folder; the data
   directory from `-data`, `$RENFS_DATA`, `./NFSSE`), the title picture on the screen.
   OpenScreamer's `port/` layout is the model (one process, `--shot` for headless
   pictures, a home directory for settings).
4. Front end (menus, car and track choice), then the race (track, cars, physics, HUD,
   sound), then the rest (replays, championship/tournament, cops, the showcase).
5. MiSTer (`~/MiSTer-ReNFS`): the skill's steps 5–8 — `MISTER_HYBRID`, `mister_port.[ch]`,
   `CONF_STR`, the cheats (look for them in the program: NFS SE has typed codes on the
   menus, e.g. the hidden Warrior car and the Lost Vegas track: find the code that checks
   them), 240 lines and HUD margins, `tools/test_on_mister.sh`.
