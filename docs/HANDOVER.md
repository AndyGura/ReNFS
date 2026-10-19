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

## Done (2026-10-19, second session)

- The project rebuilt after it was lost (see the top). `tools/rebuild.sh` is now general:
  `--decomp` re-applies `tools/names.txt` and decompiles again in minutes, and it takes the
  entry, the code range and the BSS from the EXE's layout.
- **The skill `reverse-engineer-dos-game`** (`~/.claude/skills/`): the method of
  OpenScreamer and ReNFS, the toolkit, the decompiler's traps, checking against DOSBox.
  What ReNFS learns that holds for any game goes there too.
- **The program mapped, first pass** (NOTES §4): 85 of the game's routines named (start-up,
  the front end's screens, the race and its loading, HUD, speech, replays, saves). The race
  loop is `race_main` `0x6348c` steps + `race_draw_frame` `0x5bc40`. The race clock (the
  physics' rate) is 100 Hz divided in a 3-cycle: 33.3 steps a second by default, 30 to 60
  by a setting (NOTES §4.3). `tools/strmap.py` lists the strings each routine uses.

- (continued) The libraries the game uses, its fixed-point arithmetic (16.16, a rounding
  multiply, 65,536 a turn: the port can match it to the bit), the cars' six driving kinds,
  the frame's drawing (NOTES §4.4-4.8).
- **The original runs here, unseen** (`nfs-dos`, `tools/nfsdos.py`, NOTES §5): into a race
  with keys, its memory read. **The race's 33.3 steps a second is measured**, not only read.
- (third part) The race's rules mapped: what runs on which step (every step / 2nd / 4th,
  measured in the original with the new `peek` step; an earlier note had it one place off),
  how a race ends (finished, arrested, everyone turned back, quit), each racer's results
  record, cars against walls and each other, the traffic's driving, and the police: stop
  quickly when caught and it is a warning, after a long chase a ticket, the second ticket an
  arrest (NOTES §4.3, §4.8). 22 more routines named (215 names in all).

- **The track format is read** (`tools/tri.py`, NOTES §3.3): the road's nodes, lanes,
  heading, the speed limits and the opponents' and traffic's speeds a section, the roadside
  objects, and the road's 3D surface, all from the loader's code. A picture of every track
  from above (`build/tri/`). Left and right checked against the original running in DOSBox
  (the game's pointers now read through DOS/4GW's paging: NOTES §5).

- **The 3D models are read** (`tools/orip.py`, NOTES §3.2, §3.5): the `wwww` containers,
  the `ORIP` models (points, polygons, textures, the named parts: wheels, lights,
  steering), drawn textured from eight sides: the cars and the circuits' grandstands, water
  tower, arches look right. Units are 1/128 m (the NSX model is the real car's size).

- **A track drawn from its own files matches the original** (`tools/trackview.py`, NOTES
  §3.6): the view at the start of the City race, road, textures, roadside pictures and
  buildings, next to the DOSBox screenshot of the same spot (`build/tri/compare_cy1.png`).
  The track, model and texture formats are proved together.

- **Fonts and sound are read** (`tools/ffn.py`, `tools/eacs.py`, NOTES §3.7, §3.8): every
  font draws text; every speech, music and sound-bank file converts to WAV
  (`build/snd/`). The music's compression is standard IMA ADPCM, found in the mixer's own
  decoder; ROCK1 matches ffmpeg's EA decoder sample for sample. The music loops are
  understood (a marker block points back at the block the loop starts with).

- **EA's packers are read and the car physics files opened** (`tools/unpack.py`,
  `tools/carspec.py`, NOTES §3.9, §3.10): all 527 packed files on the CD unpack exactly as
  the game's own routine does (run in an emulator to check), which also opens 23 menu
  pictures the picture tool couldn't (German records screens, rally maps, police screens).
  The car files give the torque curves, the gears' top speeds (the real cars' figures),
  the rev limits and which cars have ABS and traction control (`build/cars/torque.png`).

- **The movies play** (`tools/tgv.py`, NOTES §3.14): all 26 (EA logo, title, attract
  loops, car showcases, police and winners' clips) decode frame for frame the same as
  ffmpeg's own decoder, sound included (`build/tgv/title.png`).

- **The replays are read** (`tools/rpl.py`, NOTES §3.15): a replay is the race's settings
  plus the driver's keys every second step and a snapshot of every car every 27 s; the
  game drives the race again from them. The CD's four (alpine, Burnt Sienna,
  Transtropolis, coast) drawn on their tracks (`build/rpl/*.png`). For the port this
  means the race must run exactly as the original's for a replay to play right.

- **CONFIG.DAT is read** (`tools/config.py`, NOTES §3.16): the player's name, the menus'
  last choices, the keys and the records of all nine tracks (best segment times or laps,
  top speeds, three tables of race times). The CD's records are not the ones built into
  the program (a revised set), so the port should start from the CD's file.

- **The port runs: the title picture on the screen** (`port/`, NOTES and `CLAUDE.md` "The
  port"). One command builds it; it finds the CD's folder, reads the game's own
  `TITLE.QFS`, fades it in and out as the original does and shows it for its 5 seconds
  (`build/shots/title_120.png`; `RENFS_LANG=de` the German one). The picture equals the
  reader's pixel for pixel; the port's unpacker equals the game's on all 527 packed files.

- **The port plays the movies with sound, as the attract mode does**: the EA logo, one of
  the three attract movies (chosen at random the first time, then in turn), the title
  movie, then the title picture; Space, Enter or Esc pass each one. The pictures equal the
  reader's at the right moments; the sound heard equals the movie's own (recorded with
  `RENFS_SNDWAV`, the same to within a sample all the way through); a whole run takes
  79 s, the movies' own length. Still to come in the attract mode: the replay race from
  the second round on (needs the race).

- **The port's game mode screen and exit dialog equal the original's** (screenshots
  compared by palette entry: 9 pixels, and 1 near-black pixel). Single player / Modem /
  Exit with the mouse and the keys, the move and choice sounds, the music chosen as the
  original does it, 45 s idle goes back to the movies, Esc asks "Exit to system ?".
  Single player asks for the name as the original does (typed, Backspace, Del, Esc;
  equal to the original's pictures), then the main menu. Modem is left out (no
  network). Under the hood: the menus' font and letter blending, the mouse pointer, the
  inverse colour table, CONFIG.DAT read from the CD.

- **The port's main menu ("Control Central") equals the original's** (five runs of
  keys compared screen by screen: 1 to 24 pixels each, DOSBox's colour noise). The
  player's car, the opponent, the race type and the track change with the arrows, the
  keys or the mouse; the mirrored and rally tracks, Lost Vegas and the Warrior unlock by
  their typed codes (SKCART checked against the original), End held on head to head shows
  "No mercy", Esc / Cancel go back, the right button asks "Exit game ?", 2 minutes untouched
  shows the title. Race, the cars' showcase and the track records do nothing yet. The
  menu's choices are kept between runs (`~/.config/renfs/config.dat`).
- **The options screen equals the original's** (gearbox, ABS, traction control, Done,
  Cancel, Esc; six screens compared). Audio, graphics, controls, load / save and credits
  behind it are not written yet.

- **The race, under way (2026-10-19, third session)**. Ported and equal to the original
  run on snapshots of a City 1 race (Unicorn, the original's own code against the port's):
  the track file and its ring of sections, the sky settings and tint, the family's
  textures and their re-colouring (`trackinfo.c`, `trackfam.c`, `racepal.c`), the fixed
  tables read from the CD's `NFS_DOS.EXE` (`exe.c`), the frame's road points and their
  move into the view (`road.c`), the quad and triangle painters and the cutting of near
  pieces (`raster.c`, `poly.c`), and **the road painted far to near with its roadside
  signs (`roaddraw.c`): the same picture pixel for pixel on all 6 snapshots**
  and **the open roads' sky (`sky.c`, with EA's sin, cos and atan2 in `exe.c`)**:
  City 1's view, sky and road together, the same picture pixel for pixel on 5 snapshots
  (`tools/roadpic_emu.py`; `build/shots/road_d2_port.png` next to `road_d2_orig.png`).
  Not yet: the circuits' sky ring (`0x4ace8`), the roadside walls (`0x59294`, other
  tracks than the cities), 3D roadside models, the cars, the camera, the race palette's
  composition.

**Progress page for Danik**: https://claude.ai/artifact/VZdxTpYC3oLXYeBVJpoDoM (every step,
its percent done and weight). Update its steps file after each finished step.

## Waiting for Danik

- **Which settings a first start uses.** The CD carries a CONFIG.DAT (head to head,
  Ferrari 512TR against the Diablo, Coastal); an installed copy has none, so the original
  installed starts from its built-in defaults (City). The port reads the CD's for now.
  Either is easy; it only changes what the first main menu shows.

Decided 2026-10-18: menus 640x480 on the MiSTer (interlaced at 15 kHz),
races 320x200 / 320x240; commit locally as the work goes.

## Next, in order

1. **Finish the map** (NOTES §4.8, "Still to name"): the opponents' routines under
   `drive_opponent`, `0x5efb0` (the replay?), the second drawing pass `0x5a4f0`, the
   widgets `0x71208`.., the Watcom runtime. Small enough now to leave for when the port
   reaches them; the formats are the better next step.
2. **Formats: done.** Every file type on the CD has a reader in `tools/` (TRI, `wwww`,
   `ORIP`, textures, FFN, EA sound, the packers, PBS/PDN first reading, INFO, INV, BAMF,
   TGV, RPL, CONFIG.DAT). What is still open inside them is listed in NOTES §3.4.
3. **The port's skeleton: done** (movies, title, game mode screen, exit dialog). Name entry
   the main menu, the settings kept and the options screen done too. Next: the screens
   behind the options (credits, audio, graphics, controls, load / save), Danik chose
   (2026-10-19): **the race next**, the remaining menus later. In the race, next: the
   race's loading in `race_main` `0x6356c`'s order (cars' files and palettes `0x64e14`,
   the dashboard `0x2e8bc`, traffic `0x7de84`, then `0x55f58` the palette pieces and
   `0x2e74c`: the allowed entries `0xf77c8` (0..254 less 0x47, 0x5f, 0xfa..0xff and the
   list at `0x14e5bc`), the inverse table `0x8227c` from the DAC, the fade table
   `0x8a390`), the cars' placing on the grid (`0x506dc`), then the camera
   (`camera_update` `0x136b0`, 2.7 KB; `0x1486c` makes the view current: matrix camera
   +0x94, mirror +0xb8), so the port makes a first frame of its own. The walls `0x59294`
   and the circuits' sky ring `0x4ace8` can be checked on CY1 memory with made-up input,
   or on snapshots of another track (`tools/nfsdos.py`).
4. Front end (menus, car and track choice), then the race (track, cars, physics, HUD,
   sound), then the rest (replays, championship/tournament, cops, the showcase).
5. MiSTer (`~/MiSTer-ReNFS`): the skill's steps 5–8 — `MISTER_HYBRID`, `mister_port.[ch]`,
   `CONF_STR`, the cheats (look for them in the program: NFS SE has typed codes on the
   menus, e.g. the hidden Warrior car and the Lost Vegas track: find the code that checks
   them), 240 lines and HUD margins, `tools/test_on_mister.sh`.
