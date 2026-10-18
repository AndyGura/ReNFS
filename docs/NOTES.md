# ReNFS — the full record

Everything read out of `NFS_DOS.EXE` and the data, with addresses and proofs. Sections
are numbered; `CLAUDE.md` has the one-line summaries. Addresses are flat image addresses
(`build/nfs_flat.bin`, made by `tools/unle.py --fixups`).

---

## 1. The program

- Watcom C/C++32 runtime banner at `0x94446`; entry `0x94444` jumps to `0x944bc`, which
  calls `0xa9b62` (Watcom's `__CMain` equivalent), which calls **`main` = `0x4716c`**.
- `main` (first look): sets a dozen globals, `0x89897(0x12dd30, 1, argv?, 9)`,
  `0x94010`, copies 7 bytes from `0xe2a1c` somewhere, **`0x42054`** (start-up: calls
  `initgraphics` twice, fonts, sound), `addtimer` (`0x80f58`), `0x1f07c`, **`0x47000`**
  (the front end's loop, to be confirmed), `0x4d6c8`, `0x80f90`, then a function pointer
  at `0xf8a84` (exit).
- `initgraphics` (`0x84d74`, EA library, stack arguments): `(320, 200, 0)` and
  `(640, 480, 8)` from `0x341a4`; also called from `0x1a5d8` and `0x1b6a0` (two big
  routines that call nearly the same things: race set-up for the two detail levels?),
  `0x3770c`, `0x3c0c8`, `0x42054`, `0x56338`.
- EA's libraries name themselves in their error messages: memory manager (`initmemman`,
  `getmemblock`, `purgememblock`, ...), shapes (`loadshape`, `convertshapetype`), file
  handles (`openhandle`, `loadfile`, `openblockhandle`), packed files (`unpack`
  `0xa7020`, `unpacksize` `0xa711c`: RefPack), CD-ROM (`cdromread`, `cdromseek`), streams
  (`setstreamspeed`, `delstream`, `streamreader`: the ADPCM music and video), network
  (`newnetgame`, `sendpacket`, ...), windows/text (`createwindow`, `setfont`,
  `textcolour`). Banner "(c)1994-95, EAC Inc. by DM, AS, PC, IM, KH, BP, et al." at
  `0xe6219`. Names: `tools/names.txt`.

## 2. The data

- The CD (`NFSSE/`) has everything: `FRONTEND/` (ART, SHOW, SPEECH, MOVIE, MUSIC and the
  German G-versions), `SIMDATA/` (CARFAMS, CARSPECS, DASH, ETRACKFM, NTRACKFM, MISC,
  SLIDES, SOUNDBNK and German G-versions), `GAMEDATA/` (CONFIG/CONFIG.DAT, MODEM,
  REPLAY/REPLAY1-4.RPL, SAVEGAME).
- The installation copies a subset of the CD unchanged (every file byte for byte), puts
  the eleven position words (`FIRST.EAS` .. `FINALLAP.EAS`, from `FRONTEND/SPEECH/`) in
  its root, and adds `NFS.CFG` ("SB16 VIDEO ENGLISH NOREMOTE ") and
  `GAMEDATA/CONFIG/PATHS.DAT` (fixed 80-byte strings: `gamedata/config/`,
  `gamedata/savegame/`, ...). The program finds the rest on the CD.
- File names in the program (strings at `0xe0104`..): `%s%s%s.qfs`, `%s%s16.eas`,
  `%s.tgv`, `%sattract%d.tgv`, `%stitle.tgv`, `win#%d.tgv`, `cop%d.tgv`, `%s%s.SAV`,
  `%s%s.rpl`, `%s%s.tri`, `%sjoystick.cfg`, `%s.vis`, the car sound banks
  (`Supra_sw.bnk`, `OSupmb.bnk`, `traffc.bnk`, `COLLSWWT.BNK`, ...), `nfs.cfg`,
  `%sconfig.dat`, `maskhi.fsh` / `masklo.fsh` (the cockpit for 640 / 320).

## 3. Formats

### 3.1 FSH / QFS (pictures) — read

`tools/fsh.py list|png`. "SHPI", u32 size, u32 count, 4-byte id (`LN32`, `GIMX`), then
`count` × (4-char name, u32 offset). A shape: u8 type, u24 offset to its next block
(0 = last), u16 width, height, x centre, y centre, x, y; pixels follow. Types: `0x7B`
8-bit indexed, `0x7D` 32-bit, `0x78` 565, `0x22` palette 6 bits a channel, `0x24`
palette 8 bits, `0x2A` 32-bit palette, `0x7C` an attached block of unknown use (comes
after most menu pictures, w×h often 0×0: a hot spot or a link?). A file's palette is a
shape named `!pal` / `!PAL`, or a palette block attached to the picture.
QFS = RefPack: `10FB` + big-endian u24 unpacked size (`11FB`: a compressed size first),
then the LZ77 commands (2/3/4-byte copies, literal runs, `FC..FF` end). The game's
unpacker is `unpack` (`0xa7020`).
Menu pictures are all 640x480 (TITLE.QFS `bgnd`, the slides `SCRN`).

### 3.2 `wwww` containers — read (`tools/orip.py list`)

"wwww", u32 count, `count` u32 offsets from the container's start (0 = an empty slot).
Entries nest. The reader is `wwww_resolve` `0x60134`: it turns every offset into a pointer
in place, nested containers too, and returns the address of the table (`+8`);
`wwww_count` `0x602a4` reads the count before it, `wwww_entry` `0x60028` one slot.
- `.CFM` (a car, `SIMDATA/CARFAMS`): [0] `ORIP` + [1] its `SHPI` textures (high detail),
  [2] `ORIP` + [3] `SHPI` (low detail: ANSX 16 points and 6 polygons against 103 / 56).
- `.FAM` (a track family, `<track>_M01` / `_R01` / `_T01`, read by `load_track_fam`
  `0x60078`, names `%s/%s_T%.2d.fam` / `_M` / `_R` / `%s_%.3d.fam`): [0] 17 `SHPI` texture
  banks (names `A000`, `A004`, `B000`, `C004` ..: the road's?), [1] 32 `SHPI` of one picture
  `0000` each (the flat roadside objects?), [2] one `SHPI` `horz` (the horizon), [3] a
  `wwww` of `wwww` (`ORIP`, `SHPI`) pairs = the track's 3D models (`load_track_models`
  `0x66118` → `load_track_model` `0x65224` each): 1-2 on the open roads, 4-8 on the
  circuits (TR1: a water tower, a pylon, grandstands, rock arches, a bridge). What T / M / R
  choose: to find.
- `.FMM` (a dashboard, `<car>DH` / `DL` = 640 / 320): 4 `BAMF`. `.MMB`: 12 `BAMF`.

### 3.3 TRI (track) — read (`tools/tri.py`, 2026-10-19)

`SIMDATA/MISC/<track><n>.TRI`: AL1-3 (alpine), CL1-3 (coast), CY1-3 (city) are the open
roads, in three segments each; TR1-7 the circuits; AL22 = AL2 + 12 bytes. Read by
`load_tri` `0x4f010` (from `race_load_track` `0x5ba1c`) in three parts, each in memory
under the name the loader gives it, and **the file stays open**: the road's 3D points are
read from it section by section as the car goes (`0x4ee30` → `0x4ed74` → `0x4eb00`).

1. **"RoadSection"**, 0x1621c bytes, at `0x136608`:

   | at | what | proof |
   |---|---|---|
   | `+0` | u32 0x11 | every file |
   | `+4` | u16 sections in the loop, 0 = an open road | `load_tri`: loop → node mask `0x13bb88` = 4 × n − 1, `0x13bb80` = 1 |
   | `+6` | u16 sections (4 road nodes each) | the speed loop runs to `+6` × 4 nodes |
   | `+8` | i32 node spacing, 6.0 (16.16) on every track | the nodes are 6.00 apart |
   | `+0xc` | 3 × i32 (?), small, different per track | |
   | `+0x18`, `+0x20` | copied into node 0's `+0x14` / `+0x18` | `load_tri` |
   | `+0x24` | u32 the size of part 3 | `allocmem("TrackSlices", +0x24)` |
   | `+0x28` | u32 33..63 (?) | |
   | `+0x2c` | 600 × u32: each section's offset in part 3 | `0x4ed74`, `0x4eb00` |
   | `+0x98c` | **2,400 road nodes** × 0x24 → `0x137430` | |
   | `+0x15b0c` | 600 × 3 bytes, one a section → `0xf74a0` | below |
   | `+0x16214` | u32 object kinds (64) | |
   | `+0x16218` | u32 the most placements (1000; `0x1c630` keeps 2 bytes of state each) | |

   **A road node** (0x24 bytes): `+0..3` four bytes (widths? `+1` / 8 picks a lane width in
   `cars_meet_world`), `+4` the lanes, a nibble a side (high = the left / negative side, low
   = the right), `+5..7` bytes (`+7` picks a row of `0xed284` in `0x4f7d4`),
   `+8` **x, y (up), z** as i32 16.16, `+0x14` u16 (the slope?), **`+0x18` u16 the heading**
   (0x4000 = a full turn; the code takes it × 4 for its 65,536: `car_road_offset`
   `0x51598` reads the dword at `+0x16` >> 16 << 10 >> 8), `+0x1a..0x23` (?).
   Proof of the heading: on all 17 tracks it is atan2(dx, dz) towards the next node within
   1-6 parts in 16,384 (one node of AL1 off by 43).
   **Left and right**: in the original (DOSBox, `nfsdos.py car:`), at the start of a race on
   CY1 the player (right-hand lane on the screen) is at x = +2.5 and the opponent at −2.5,
   both heading +z: +x is the driver's right when the heading is 0, so a picture with x to
   the right and z up is not mirrored.
   **The section bytes**: `[0]` scaled at load by a factor from `0xed12c` / `0xed1d4`
   (picked by two settings, `0x47b58` / `0x47b50`: the difficulty?), read by the
   opponents (`0x7cf98`): their speed; `[1]` **the speed limit** (`police_pull_over`: the
   chase starts when the player is faster than it × 65,536): 27 on the coast and the
   mountains, 33 in the city, 9 / 16 / 18 on the circuits, 2 or 9 or 10 in a few places;
   `[2]` read by the traffic (`0x786d4`, `0x7e64c`), about 0.6 × `[0]`, 5 on most circuits.
2. **"RoadObjects"** (`0x13bb84`): "SJBO", u32 size (with this 12-byte head), u32 0. Then
   64 **kinds** × 16 bytes (read by `0x59490` drawing, `tri_section_objects` `0x4f7d4`):
   `+0` flags (4 = animated: `+8` frames, `+9` steps a frame on the step counter), **`+1`
   the type = the 3D points it takes**: 1 = a 3D model (`+2` = its number among the
   family's models), 4 = one flat picture, 6 = two flat faces at a right angle (the second
   `+8` wide, going back from corner 3); `+2` the picture (`0x141ed8[+2 + variant]`, the
   variant 0..3 from `0xf4604` / `0xf458c` by distance; `/ 4` = the family's entry-1
   `SHPI`), `+3` the second face's picture, `+4` i32 width, `+0xc` i32 height (16.16). Then the **placements**
   (`0x13bb94`) × 16 bytes up to an i32 −1, in node order: `+0` i32 node, `+4` u8 kind,
   `+5` u8 angle (256 a turn, added to the node's heading), `+0xa` 3 × i16 x, y, z from the
   node's position (8.8, the world's axes). Read by `0x4f7d4` and `car_node_in_list`
   (walks them by node). A flat object's corners (`0x4f7d4`): 0 = (x − c, y + height,
   z − s), 1 = (x + c, y + height, z + s), 2 = (x + c, y, z + s), 3 = (x − c, y, z − s), with
   (c, s) = width / 2 × (cos, sin) of −(`+5` / 256 + heading / 16384) turns: at angle 0
   along the road's right, the picture facing the driver. **The picture's right edge goes
   on corner 0**: the other way the original's "MALL" sign on CY1 reads "LLAM" (§3.6). 584..998 placements on the open roads, 138..595 on the circuits.
3. **"TrackSlices"** (`0x136604`), one record a section: "TRKD", u32, u32, 12 bytes
   (copied as they are: the strips' textures?), then **4 rows (one a node) of 11 points ×
   3 × i16** in 1/128 units: point 0 from the node's position, points 1 and 6 from point 0,
   every other point from the one before it (1..5 one side, 6..10 the other). 288 bytes a
   section. `0x4eb00` turns one into 12 + 528 bytes (i32 points) in a ring of 64 at
   `0x1365e8` (0x8700 bytes); `0x4ee30` keeps the 30 sections round the car there.

Units: a node is 6 units from the next; TR1 is 3,066 units round; the speed limits 27 / 33
would be 60 / 74 mph in metres a second. **Probably metres**, not yet proved (the HUD shows
"5.2 MILES" at the start of CY1, 11,514 units long: the race may not run the whole file).
`tools/tri.py info|nodes|png`: the picture (`build/tri/`) shows the road strips (grey the
two inner ones a side, then green, then brown), the centre line, the start, and the
objects (red: pictures, blue: points).

### 3.5 ORIP (3D models) — read (`tools/orip.py`, 2026-10-19)

EA's 3D library (`0x94000`..`0x9a000`, every routine opening with `push n; call __chk`)
takes a model in place: `orip_fix` `0x978a8` turns its offsets into pointers,
`orip_instance` `0x95522` makes a drawable copy (room for the turned points, their screen
places, the sort list), `draw_model` `0x666b0` → `orip_transform` `0x98137`, `orip_sort`
`0x982e1` (by the z of each polygon's first two corners), `orip_draw` `0x98735` →
`orip_draw_list` `0x9868a` → `orip_draw_poly` `0x98482`. Offsets from the "ORIP":

| at | what | proof |
|---|---|---|
| `+4` | u32 size; `+8` 0x2bc; `+0xc` 0 (a use count at run time) | `orip_instance` adds 1 to `+0xc` |
| `+0x10` / `+0x18` | points / their offset: 3 × i32 x, y (up), z (forward), **1/128 units** | NSX 234 × 151 × 562 = the real car's 1.81 × 1.17 × 4.43 m at 7.8 mm a unit |
| `+0x14` | points of the body (ANSX 87 of 103; the rest the wheels?) | |
| `+0x1c` / `+0x20` | texture positions: u32 u, v in pixels (0 on most track models) | `0xab105` reads them through `0x1616cc`, u << 16 |
| `+0x24` / `+0x28` | polygons, 12 bytes: u32 flags, u32 corners, u32 texture positions (both counted in u32s into the index table) | `orip_fix`, `orip_sort`, `orip_draw_poly` |
| `+0x2c` | 12 bytes: the model's name (`_NSX`, `_TR1`..) | |
| `+0x38` / `+0x3c` | textures, 20 bytes: 8 bytes a part's name or 0, the `SHPI` picture's name (4), 8 bytes | names match the `SHPI` beside the model |
| `+0x40` / `+0x44` | named point groups, 20 bytes (`left_tur`, `right_tu`: count 8, first, ..) | |
| `+0x48` / `+0x4c` | named polygon groups, 28 bytes (`NON-SORT`, `inside`, `surface`, `outside`) | |
| `+0x50` | the index table (u32 each) | `orip_fix` |
| `+0x54` / `+0x58`, `+0x5c` / `+0x60` | named points, 12 bytes: 8-char name, u32 point (`smok`, the wheels `FL0`.. `RR1`, the lights `bklr` `bkll` `lt_frnt` ..) | |
| `+0x64` / `+0x68` | a count and a table (0 on the cars) | `orip_fix` |

**The polygon flags** (`orip_draw_poly`): bits 0-2 the corners (3 or 4); `0x80`
textured (else one colour, `0xaa990`); bits 3-6 pick the drawing routine from tables at
`0x1265d8` (triangles) and `0x1265e4` / `0x1265f0` / `0x1265fc` / `0x126608` (quads, by
`0x1000` / `0x2000` and the model's flag); `0x100` or `0x400` drawn from both sides, `0x200`
the other side faces out (`0xaa95b` decides); `0x800` the next polygon is drawn with it;
**`0x1000` the polygon has its own texture positions** (`0xab105` reads `+8`); without it
the whole picture is put on corners 0-3 = top left, top right, bottom right, bottom left
(`map_quad_whole` `0x91b90` gives them (0,0), (w,0), (w,h), (0,h)); bits 16-23 the
texture. Colour 255 is see-through in every palette seen (magenta, 206 28 198), and covers
the open parts of the grandstands. A texture with a part's name and no picture
(`frntires`, `rertires`) is the wheels: the game fills it in (`tyr1`..`tyr4`, turning).
Car tail lights are drawn in colours the second palette (`!xxx`) changes (to check).
`tools/orip.py png` draws a model from eight sides (`build/orip/`): the NSX and TR1's
models look right from all of them.

### 3.6 A track drawn from its files, against the original (2026-10-19)

`tools/trackview.py <NFSSE> <track> <node> <out> [across eye fov]` draws the view along a
track from the TRI (road points, strips, objects) and the FAM (road textures, roadside
pictures, models), textures in perspective. **At the start of CY1 it shows what the
original shows** (`build/tri/compare_cy1.png`: the DOSBox screenshot above, the drawing
below): the road and its lines, the bank and curved barrier on the right, the "MALL" sign
and building on the hill, the lamp posts, the trees and buildings on the left. What that
proves:
- **The road strips.** `draw_road_sections` `0x5aa84` draws from the quad lists at
  `0xf4880` (11 bytes: 4 point numbers, the strip, a drawing record, a flag), one list a
  detail level (`0xf61c0`: 32, 18, 10 quads a section). At full detail the inner strips
  (0-2 and 5-7: between points 0/1, 1/2, 2/3 and 0/6, 6/7, 7/8) go row by row and the outer
  ones (3, 4, 8, 9) over two rows. Strip s's texture is the section header's byte `2 + s`
  (bytes 0, 1: 0 so far).
- **The road textures.** The table `0x140cd8` (144 textures × 8 slots, filled by `0x5b34c`
  from the family's entry 0): texture t = bank t / 3, picture `A`/`B`/`C` by t % 3, slot
  digit 0..7 (`A000` .. `A007`; a missing slot repeats the one before, the families have 0
  = 128 × 256 near and 4 = 32 × 96 far). The picture's width runs along the road over two
  rows (the second drawing record, `[1]` = 0 / 1, picks the half), its height across the
  strip, top = the outer side: CY1's lane picture has its solid line at the top (the edge)
  and dashes at the bottom (the middle of the road).
- **The roadside pictures**: `0x141ed8`, 4 slots a picture, filled from entry 1 (one
  `SHPI` a picture named `0000`, or one `SHPI` of `0000`, `0100` ..).
- **Left and right** a second time: the bank and barrier are on the right in both.
The camera here is a guess (1.3 above the road, 70° wide); the game's own is still to
read, and the horizon (`horz`, entry 2) and the sky are not drawn yet.

### 3.7 FFN (fonts) — read (`tools/ffn.py`, 2026-10-19)

Read by `setfont` `0x91ed0`. "FNTF" (setfont also takes `FNT3` `FNTP` `FNTS` `FNTU`),
u32 size, u16 100, u16 characters (224), u16 `+0xc`, u16 `+0xe` (`& 3 == 2` picks the other
drawer), bytes `+0x10`..`+0x13` (`+0x12` + `+0x13` = the line's height), u32 `+0x14` the
character table, u32 `+0x1c` the bitmap. A character, 11 bytes: u16 code, u8 width, u8
height, u16 x, u16 y (in the bitmap), u8 advance, i8 x offset, i8 y offset (from the line's
top). The bitmap is an FSH shape (16-byte header, type `0x7a` = 4 bits a pixel, high nibble
first, rows of (4 × width + 7) / 8 bytes); a pixel is a level 0..15 (the edges' smoothing),
mixed with the text's colour when drawn. Checked by drawing a line of text with each font
(`build/ffn/`).

### 3.4 Still to read

`BAMF`, `30FB`-headed
`.PBS` / `.PDN` (RefPack with flag bits `0x20`?), `.BNK` (EA sound bank), `EACS` / `1SNh`
(EA ADPCM: `.EAS` speech, `.ASF` / `.AS4` music), `kVGT` (TGV video, 320x192?), `.INV` (32 KB), `BAMF` (`MASKHI.B00` ..), `.RPL`, `CONFIG.DAT` (player name, replay
names).

## 4. Map of the program (2026-10-19)

Tools: `tools/strmap.py` (the strings each function uses; the full lists
`build/strs_by_site.txt`, `build/strs_by_func.txt`), `tools/calltree.py`. Names are in
`tools/names.txt` (prefixes: `fe_` front end, `race_`, `load_`, `snd_`, `cfg_`, `modem_` /
`net_`, `hud_`). The evidence for a name is the files and texts the routine uses (listed
here) unless said otherwise. Layout of the code object: game `0x10000`..`0x80000`, EA's
libraries `0x80000`..`0x94000` and `0x95000`..`0xb0000`, Watcom's runtime `0x94000`.. and
`0xa9000`.. (mixed; not separated yet).

### 4.1 Start-up and the outer loop

- `main` `0x4716c`: `0x89897` (memset-like, `0x12dd30`), `0x94010`, copies "NFS SE"
  (`0xe2a1c`), `startup` `0x42054`, `addtimer(race_timer_tick 0x4c454)`, `0x1f07c`,
  `fe_outer_loop` `0x47000`, `0x4d6c8`, `0x80f90(0x4c454)` (removes the timer), exit
  through the pointer at `0xf8a84`.
- `startup` `0x42054`: `cfg_read_nfs_cfg` `0x41d74` (`nfs.cfg`: SBPRO SB16 GUS AWE WSS,
  YESSOUND NOSOUND, NOVIDEO LOWVIDEO, GERMAN, NOREMOTE), the XMS / Windows 256-colour /
  "NFS SE CD not present..." errors, `cfg_init_paths` `0x39db8` (`gamedata\config\paths.dat`
  or the built-in list `gamedata/config/` .. `simdata/ntrackFm/`, English and G-versions),
  `initgraphics`, `fonts/graver18.ffn`, `%stitle.tgv`, the stream speed (`setstreamspeed`).
  `0x93090` (called from it) sets the PC timer: `inittimer` `0x81078`, divisor
  `0x1234f0 / TIMERHZ`; **TIMERHZ = 100** (`0xf7940`, initialised data).
- `fe_outer_loop` `0x47000`: `cfg_load_config_dat` `0x39c40`, `fe_title_screen` `0x46f20`
  (`title.qfs`), `fe_main_menu` `0x42c30`, `cfg_save_config_dat` `0x3a134`, `0x550a0`
  (network shutdown?), `fe_attract_mode` `0x34284` (`%sea.tgv`, `%sattract%d.tgv`,
  `%stitle.tgv`, `eatitle.qfs`, and a race replay of `gamedata\replay\REPLAY%d.RPL`),
  `fe_game_mode_screen` `0x47274` (`control/gamemod2.qfs`: single / modem / exit),
  "Exit to system ?", "No network services found.".

### 4.2 The front end

- `fe_main_menu` `0x42c30` (17 KB, `control/central.qfs`, `control/ani.qfs`,
  `control/tourny1..3`, `showcase.asf`, "Quit tournament ?", "Track already completed"):
  the central screen and its state machine. It calls the race (`race_from_menu`), the
  options, the showcase, the chat, the track selection, the standings.
- Screens (one routine each): `fe_audio_options` `0x105dc` (`option/audio.fsh`; "Cannot
  change audio mode in Multi-player modes."), `fe_graphics_options` `0x32d08`
  (`option/graphics.qfs`, `check/graphics.fsh` = the same screen during a race),
  `fe_options_screen` `0x53a30` (`option/options.qfs`, `credits.asf`), `fe_controls_setup`
  `0x22bd0` (`option/cntrlset.qfs`, `check/chconset.qfs`), `fe_joystick_calibrate`
  `0x4c6b0` (`joychk.qfs`, `joycal.qfs`), `fe_credits` `0x25600` (`credits.fsh`, `bgn%d`),
  `fe_showcase` `0x17724` (`%s.tgv`, `showcase.asf`, pages vid / his / gen / per / eng)
  with `fe_showcase_specs` `0x15008` (`mec`) and `fe_showcase_history` `0x16290` (`his`,
  `gen`, `f1s%d` / `f2s%d`), `fe_chat_screen` `0x195d8` (`control/chat.qfs`),
  `fe_track_select` `0x75288` and `fe_track_select_tournament` `0x76380`
  (`tracksel/%s.qfs`, the lengths "16.0 miles" .. and the music names `city.eas` ..
  `loopx.eas`), the load / save screens `0x3aacc` `0x3c0c8` `0x3d4c4` (`*.SAV`, `*.RPL`,
  "Load replay ?", the four system replays REPLAY.RPL, REPLAY1-3.RPL may not be deleted:
  `is_system_replay` `0x3a9c4`), `fe_yes_no_dialog` `0x73b04`, `fe_enter_name` `0x743b4`,
  the standings `0x69f8c` .. `0x6f070` (`stats/standbst` / `standseg` / `standtrk` /
  `standlo1` / `standlo2` / `sgmtstnd` / `finstnd` / `trmtstnd`, the commentary
  `con<n>_<x>16.eas`), `fe_winner_movie` `0x37ff0` (`win#%d.tgv`, `winner.qfs`),
  `fe_cop_ticket_movie` `0x3823c` (`cop%d.tgv`, `ticket.qfs`).
- Modem / network: `modem_config_screen` `0x213b0`, `modem_settings_screen` `0x62314`,
  `modem_connect` `0x605e0`, `modem_phonebook` `0x61338` (`*.num`), `net_wait_players`
  `0x55104`. Not to be ported (Danik, 2026-10-18: network skipped).
- The block `0x71208`..`0x74dcc` (small routines called from every screen): the widgets
  (pictures, text, buttons, the mouse `%smouse.fsh`), key names (`0x73068` German,
  `0x733a0` English), `fmt_speed` `0x74854`, `fmt_time` `0x74914` (`%d:%02d.%d`),
  `screenshot_tga` `0x72ac4` (`nfs%02d.tga`). The opponents' names "Drifter Burnout Psycho
  Phreak Hacker Razor Mr. Ice" are at `0x74748`.

### 4.3 The race

```
race_from_menu 0x3770c (from fe_main_menu)
  race_run 0x37228 (also from fe_attract_mode, the load/save screens, the race menus)
    race_setup_record 0x36c5c   the race record (0x12da..), clock rate setting 0x12dd0c = 1
    race_clock_start 0x4c520, race_clock_rate 0x4c5f8
    race_main 0x6356c
      loading: load_slide 0x64d28 (the loading picture), snd_load_car_banks 0x52774,
        load_track_info 0x49dc0 (%s%sInfo), load_car_cfm 0x65874 (via 0x51388 "carObj"),
        load_smoke 0x645bc, race_load_track 0x5ba1c -> load_tri 0x4f010, load_vis 0x58bc4,
        load_traffic_cfg 0x7de84, load_dash 0x2e8d4, load_cockpit_mask 0x5f7c8
      the loop:  while steps done 0xf6d60 < steps due 0xf6d64:  race_step 0x6348c
                 then race_draw_frame 0x5bc40
  race_menu_checkpoint 0x1a5d8 (check/chkmenu.qfs, REPLAY.RPL, "End game ?")
  race_menu_finish 0x1b6a0 (check/finmenu.qfs)  -- both call race_menu_options 0x12760
  race_pause_menu 0x56474 (check/pause.fsh, pause.asf)
```

- **The race clock.** `race_timer_tick` `0x4c454` runs on every timer interrupt (100 a
  second). A countdown `0x12f19c` is reloaded with `0x12f1a8` twice and `0x12f1a4` once in
  a cycle of three (`0x12f1a0`). Each time it reaches 0 the race clock `0xed0f8` goes on by
  one (not while state `0xf6b74` = 3, paused), and on every second tick in state 1
  `0x48f6c(clock/2, 0, 2, ...)` is called. `race_clock_rate` `0x4c5f8` sets the pair from
  `0x12dd0c`:

  | `0x12dd0c` | reloads | interrupts per 3 ticks | clock rate |
  |---|---|---|---|
  | 0 | 3 3 4 | 10 | 30 Hz |
  | 1 | 3 3 3 | 9 | 33.3 Hz |
  | 2 | 3 3 2 | 8 | 37.5 Hz |
  | 3 | 2 2 3 | 7 | 42.9 Hz |
  | 4 | 2 2 2 | 6 | 50 Hz |
  | 5 | 2 2 1 | 5 | 60 Hz |

  `race_setup_record` sets 1; `race_run` sets 3 when bit 8 of `0x12da28` is set (what that
  bit is: to find). Single player: steps due `0xf6d64` = the clock, so **one `race_step`
  per clock tick**, then one frame drawn: the physics' rate is the clock's.
  **Measured in the original (DOSBox, §5, 2026-10-19): 33.31 race-clock ticks per 100
  system ticks** in a single race on City (setting 1, reloads 3 and 3), the steps done
  equal to the steps due all along, and the race time on the screen = clock / 33.33
  (1,449 ticks, "00:43.4"). The system clock runs at 100 a second. In the modes `0x15d134` = 1..3 (network, replay?)
  the steps due come from elsewhere (`0x12dd18`, `0xf6d68`).
- `race_step` `0x6348c`, in this order: the controls (`0x71cec` or `runsystemtasks`
  `0x83908`); `race_timed_calls` `0x1fa1c` (two timers at `0x129fb0` that call
  `race_pause_menu` `0x56474` / `0x63110` once their time comes); `race_check_pause`
  `0x63304`; `race_keys` `0x47c88`; then, unless the race is held (state 4, or 2 before the
  start), `race_update_cars` `0x519d8` with the step flags `0x12f1bc`, and the step counter
  `0x145c94` goes on by 2.
  - **The step counter and the flags.** `0x145c94` counts in twos (2 a step: it is
    always even and twice the race clock). The flags are 1 if `(n & 1) == 0`, | 4 if
    `(n & 3) == 0`, | 0x10 if `(n & 7) == 0`: **1 on every step, 4 on every second step,
    0x10 on every fourth**. Measured (§5, 2026-10-19, `peek`): over 10 s of a race the
    counter was never odd and ran at 2 × the clock; of 1,914 samples the flags were 1
    (910), 5 (508), 21 (496). (An earlier note here said every 2nd / 4th / 8th: wrong.)
  - The keyboard is a table of keys down by scan code at `0xf7ac8` (`0xf7ac8 + code`).
    `race_keys`: keys 1..8 (codes 2..9) put the camera on car 0..7 (`camera_set_car(0x129ecc,
    car, 4)`, `0x12dd54` = the car watched). `race_check_pause`: Esc (code 1) or P (`0x19`)
    after the first 60 steps opens the pause menu (`0x1f988`); in a network race
    (`0x15d134`) with a Shift (`0x2a` / `0x36`) held it ends the race instead (`0x15d138` = 1,
    `0x13c17c` = 2).
- **`race_update_cars` `0x519d8`** (the step's work on the cars): on flag 4 each car's
  sound routine (`+0x50e` → `0x13c1e0`); on flag 1 (every step), in state 1,
  `car_check_finish` `0x51760` for every racer, then the end-of-race test (below), then
  **every car's driving routine** (`+0x512` → `0x13c1b8`, in road order: the list
  `0x13c298`) and `car_road_offset`; then `cars_meet_world` `0x1c878`; then the camera
  (`camera_update`), and the HUD's sums.
- **Where a car is in its race:** `car_race_progress` `0x5162c` returns 1 before the start
  and 2 past the finish, 0 between. A point-to-point track (`0x13bb80` = 0) ends at road node
  `0x13c164` (`+0x48` is the car's node; below node 12 it is before the start); a circuit
  (`0x13bb80` ≠ 0) at lap `0x13c160` (`+0x50` is the car's lap). `car_past_finish_by`
  `0x51724`: how many nodes past.
- **The racers' results**, one record of `0x1d2` bytes a racer (car `+0x516` is its index)
  from `0x14ea7c`: lap times at `+0` / `+8` + lap × 4, the tickets `+0x1b4` (`0x14ec30`),
  the warnings `+0x1b8` (`0x14ec34`), **the finish time** `+0x1c0` (`0x14ec3c`: 0 = still
  racing, the step counter − 300 when past the finish, −999 = turned back past the start),
  `+0x1ce` (`0x14ec4a`) the place (counted from the times already in).
- **The end of a race: `0x13c178`** (a copy in `0x13c17c`), 0 while it runs, cleared by
  `race_init_car_kinds` and `0x506dc`. `race_update_cars` sets **1** once every racer has a
  finish time and is down to half a unit of speed (`+0x70` ≤ `0x8000`), **0x10** if none of
  those times is positive (all turned back); `police_pull_over` sets **4** (arrested,
  below). `race_run` copies it to `0x15d122` (and makes it 2 when `0x15d136` = 0, `0x12da24`
  = 1 and `0x156cc3` = 0: when that is, not yet known), and `race_from_menu` reads it: 4 →
  `fe_cop_ticket_movie` (`ticket.qfs`, `cop%d.tgv`), 1 → the winner's film and the
  standings. `0x68dcc` (the track records?) also tests it for 1 and 0x10.
- The cars' records: `0x12da30` cars, `0x18c` bytes each from `0x12cccc`.
- `race_draw_frame` `0x5bc40` ("sub:%4d cam:%c", a debug line): calls the 3D and HUD
  routines `0x59a74` `0x5a4f0` `0x5aa84` `0x5adbc` (`horz`), `0x31cb4` (`mpbd`: the map?),
  `0x3081c` (`b00t` ..: cockpit), `0x314e4`, `0x31684`.
- HUD / dashboard: `load_dash` `0x2e8d4` (`DH.fsh` / `DL.fsh`: `dash`, `tach`, `sped`, ...,
  and `%s%sDH.fmm` / `DL.fmm`), `0x2e100` (`lap#`), `0x2e1dc` (`splt`), `0x2df78` (`mile`),
  `0x30424` (the police: `ticketDH` / `tickegDH` / `warnDH` ..., the G = German),
  `0x30b64` (`tach`).
- Speech in the race: `snd_say_position` `0x501a0` (`first.eas` .. `eighth.eas`),
  `snd_say_best_time` `0x5056c` (`besttime.eas`, `bestlast.eas`, `finallap.eas`). The car
  sound banks `Supra_sw.bnk` .. (the player, by car), `O<car>mb.bnk` (opponents),
  `traffc.bnk`, `COLLSWWT.BNK` .. (collisions), `NFS_FMmb.bnk`: the table at `0x524b4`.
- Physics file: `load_car_pbs` `0x50c24` (`.PBS`, "By_R&T"); `.PDN` at `0x660bc` in
  `load_car_cfm`'s neighbour `0x65874`; the car's parts by name (`frntires`, `rertires`,
  `left_turn`, `right_turn`, `inside`, `outside`, `surface`, `convex`).
- Replays: `save_replay` `0x3a628`, `load_replay` `0x3a218`, `replay_file` `0x5c3b0`
  (`Replay.rpl`), `0x5dd68` (`UsrCtrlBuff`: a replay records the controls?).
- Track names: `0x41b68` (`city coast alpine springs avalley bsienna vridge tpolis lvegas`
  and their `m` / `r` variants = mirrored? / reversed?), `0x68dcc` (four-letter: city coas
  alpi rust autu burt mont trop neod). `0x5636e`: the music loops `loop1` .. `loopx`.

### 4.4 The libraries the game leans on (read from their code)

- Memory: `allocmem` `0x828a8` (`reservemem` under a lock), `freemem` `0x82ebc`
  (`findmemblock` + `purgememblock`), `__chk` `0xa5ff4` (Watcom's stack check: `push <bytes>; call` opens every library routine; named `free` until 2026-10-19, wrongly), `memmove` `0x8ecc4`
  (stack arguments). `fatalerror` `0x92484`: prints, restores the system, exits through
  `0xf8a84`. `sprintf` `0x8198b` (stack, `__prtf`-like core `0xa23f3`).
- Time: the system clock `0xf7980` (TIMERHZ ticks). `ticksince` `0x81350`, `settimeout`
  `0x8136b`, `timedout` `0x81389`. `runsystemtasks` `0x83908`: runs every task given to
  `addsystemtask` whose time has come (`0x15db6c` routine, `+4` period, `+8` next time) and
  returns their results OR-ed. It is called before nearly everything; the keyboard, the
  sound and the music streams are such tasks (to confirm).
- Pictures: `findshape` `0x80e1d` (a shape in a SHPI by its 4-letter name, `fatalerror`
  if asked to and missing), `blitshape` `0x9d8d0` (shape, x, y, window), `drawshape`
  `0x81280`, `drawshapeatpos` `0x8129c` (at the shape's own x / y, `+0xa` / `+0xc` >> 16),
  `drawshapemove` `0x84248`. Windows: the current window is 13 dwords at `0xf889c`;
  `setwindow` `0x8118c` copies one in, `setscreenwindow` `0xba93c` puts the screen's
  (`0xf88d4`) back; `newwindow` `0x80c4c`. `waitretrace` `0xba95f` (port `0x3da` bit 8).
  The video driver's routines are pointers set by `initgraphics` (`0xf795c` show the page,
  `0xf7960`, `0xf7964`).
- Palette: `setpalette` `0x84080` (first, count, colours; out `0x3c8`, 6-bit DAC),
  `nearestcolour` `0x80c80`, `pal_clear` `0x7188c` (768 bytes), `pal_from_shape` `0x718a4`
  (palette shape type `0x24` = 8 bits a channel, else 6), `pal_fade_down` `0x7198c` /
  `pal_fade_up` `0x71918` (steps of `0x98abc` / `0x98aec`; which is in and which is out: to
  read).
- The front end's helpers: the mouse at `0xf78e0` / `0xf78e4` (`mouse_set` `0x71ad4`,
  `mouse_in_rect` `0x71a1c`, `mouse_moved` `0x71b24`), `fe_show_if_changed` `0x71afc`
  (shows the page when something changed), `fe_service_music` `0x71c20` (keeps the menu's
  music stream fed, called in every wait loop), `fe_play_sound` `0x72510`.

### 4.5 Numbers: fixed point, no floating point

- **The game computes in whole numbers.** The game code (`0x10000`..`0x80000`) holds 18
  x87 instructions in all; the physics is 16.16 fixed point, inline: `imul` then `shrd`
  by 16. **The multiply rounds**: `(p >> 16) + bit 15 of p` (`rotate2d` `0x3e918`,
  `car_road_offset` `0x51598`). So the port can match the original to the bit with plain
  integer C (64-bit products, the same rounding).
- Angles: **65,536 = a full turn**. `fixsin` `0x80b00` / `fixcos` `0x80b70`: a quarter-wave
  table of 256 entries at `0xf8448` (index = angle >> 6, the two bits above it the
  quadrant), plus a straight-line step between entries: `(cos(i) >> 2) * (((a & 0x3f) *
  0x6487e) >> 9) >> 21` (`0x6487e` = 2π × 65,536). The result is 16.16.
- `rotate2d` `0x3e918` (x, y, angle, &x', &y'): x' = x·cos − y·sin, y' = x·sin + y·cos.
- A random number generator: `0xeab70 = (0xeab70 × 0xeab78) & 0xffff`, the bits 8.. of the
  product used (in `drive_opponent_wait` `0x7e0c8`).

### 4.6 The cars

- The cars' objects: pointers at `0x13c24c` (`0x12da38` of them; the racers first,
  `0x12da30`; then traffic and police?). A second table, `0x12da30` records of `0x1d2`
  bytes from `0x14ec3c` (the racers' standings?), and the controls / players `0x18c` bytes
  from `0x12cccc`.
- A car's fields seen so far: `+0x0` `+0x4` `+0x8` a position (16.16; `+0x4` the height?),
  `+0xc` `+0x10` `+0x14` angles (`+0x10` the heading, kept in 24 bits), `+0x48` the road
  node it is on (`& 0x13bb88`, the track's node mask), `+0x60` / `+0x68` the speeds
  sideways / forwards?, `+0x70` / `+0x74` (speed; `0x960000` = 150 the cap), `+0x88` a
  pointer to the record that holds the kind, `+0x341`.. the last step's values, `+0x47d`
  the player number, `+0x50e` the sound kind, `+0x512` **the driving kind**, `+0x51a` flags
  (4 = active).
- **The driving kind** `+0x512` picks the routine called every step through the table
  `0x13c1b8` (filled by `race_init_car_kinds` `0x504c4`); `+0x50e` picks one called every
  second step (flag 4) through `0x13c1e0` (1 = `car_sound_player` `0x10400`, 2 = nothing):

  | kind | routine | what | set by |
  |---|---|---|---|
  | 1 | `drive_player_wait` `0x7ea20` | counts down, then kind 2 | `police_pull_over` `0x79728`, `0x7e64c` |
  | 2 | `drive_player` `0x382d0` → `car_physics_player` `0x408e0` (4.4 KB) | the player | `load_car_pbs` |
  | 3 | `drive_opponent` `0x7b34c` (3 KB) | a computer's car | `load_car_pbs`, `0x51200`, kinds 5, 6 |
  | 4 | `drive_crashed` `0x1cfe0` | after a crash? | `0x1ca9c`, `0x1cb14`, `car_car_impact` `0x1e314` |
  | 5 | `drive_kind5` `0x7d9e4` | slows by `0x13333` a step (`0x23333` with flag 4), runs the opponent's routine | `car_set_drive_kind` `0x1cd40` |
  | 6 | `drive_opponent_wait` `0x7e0c8` | held at (8, 200 + 10n, 8), counts down, then kind 3 | `0x7ccc0` |

  After its routine, every car goes through `car_road_offset` `0x51598` (its place
  across the road: the road node's position `0x137430 + node × 0x24`, turned by the node's
  angle).
- `car_physics_player` `0x408e0`: `0x3ea98` / `0x3e9c4` every second step (flag 4; the gearbox?),
  `0x3ed68` (the engine: reads `Carspecs` through `0x18a04`), `0x3f260` twice (front 0 and
  rear 1, from `+0x60` / `+0x68`), `0x3f7b8` twice (the axles' forces?), `0x4071c`,
  `0x40230`, `0x4008c`, `replay_mark_highlight` `0x5c6d4` (events 0x28, 0x57, 0x78),
  `race_stats_max` `0x2d71c` (the race's greatest values, capped at 1200).
- `replay_mark_highlight` `0x5c6d4`: for each slot of 60 half-steps (`0x145c94` counts 2
  a step) keeps the greatest event and its time (`0x144da8`): the replay's highlights
  ("Load highlight ?"). 60 half-steps = 0.9 s at the measured 33.3 steps a second (1 s at
  setting 0's 30: perhaps what it was made for).
- `car_check_lap` `0x5056c` (called by the opponents too; speaks `besttime` / `bestlast` /
  `finallap` for the player).

### 4.7 Drawing a frame

- `camera_update` `0x136b0` (also in the step), `camera_set_car` `0x13530`
  (`0x129ecc` = the camera, a car, a mode: 3 seen).
- `draw_road_sections` `0x5aa84`: the track **from far to near** (a painter's order), a
  section = 4 road nodes (`0x13bb90` the sections' mask); per section 11 points, 3D at
  `0x13b728` (12 bytes each, `0x84` a section) and on the screen at `0x13b9bc` (8 bytes,
  `0x58` a section); its faces (11 bytes each: four point numbers, a texture number,
  flags) drawn by `draw_quad` `0x35b04` with the texture from `0x140cd8`; then the objects
  and cars at that slice: `draw_slice_objects` `0x68430` (→ `0x66d08`). The section's kind
  is `0x136621 + section × 6` → a record of `0x4c` bytes at `0xf61c0`.
- `0x5a4f0` (1.1 KB, reaches 18 KB) is a second pass of the same shape with `0x35348`
  (another polygon routine), `0x59d3c`, `0x5a0a8`, `0x679ec`: the walls and roadside, or
  the near part. To read.
- `draw_horizon` `0x5adbc` (`horz`), the HUD and dashboard `0x314e4` / `0x31684` /
  `0x3081c` (`b00t`.., the cockpit) / `0x31cb4` (`mpbd`, the map), `0x59a74`, `0x5868c`.

### 4.8 Cars meeting cars, walls, traffic and the police

- **`cars_meet_world` `0x1c878`**, every step after the driving: every 4th call
  (`0x129fac` counts down from 4) `traffic_police_tick` `0x7a040`. Then for every racer
  `0x1c800`, and `car_wall_collide` `0x1ebf4` when the car is further across the road than
  the node's lanes reach (node byte 4: lanes on each side, a nibble each; lane widths from
  `0x15d15a`). Then each car (not kind 6, flags `+0x51a` & 6 = 6) against the next ones in
  road order, as long as they are within two nodes: if both are within the sum of their
  sizes (`+0xc0` → `+0x70`) in x and in z, `car_car_collide` `0x1c6a4` →
  `car_car_impact` `0x1e314` (951 bytes: the push, `0x2afb8`, `0x2b864` (6 KB: the bodies'
  dents?), the replay's highlight `0x5c6d4`, and kind 4 through `car_set_drive_kind`).
- `car_set_drive_kind` `0x1cd40` (car, kind): sets `+0x512` and puts the car's position,
  angles and speeds back from its body record (`+0xc0`). Kind 4 = crashed; if the car is
  the one watched, `snd_play_event29` `0x53214` (sound 0x29 through `0x95a7a`, the sound
  library's play).
- **`traffic_police_tick` `0x7a040`** (3.7 KB, every 4th step = 8.3 times a second): every
  car that is not a racer (`+0x516` < 0 or ≥ `0x12da30`) and not crashed drives in its lane:
  `0x786d4` (939 b), the lane choices on the node's lanes (byte 4), `0x78c88` (2.2 KB, the
  steering), `0x78a80`, `0x7850c` (a lane change), `0x78370` (slow down to 25 near the
  player). Cars with `+0x174` & 8 are **police**: `police_pull_over` `0x79728`. Cars with
  `+0x174` & 4: `ai_car_engine` `0x79e28` (past the finish they pull to the side; the gear
  from the speed against the table at `+0x308`, the engine's revs `+0x3c1` eased by 1/8).
- **`police_pull_over` `0x79728`**: (for a police car chasing, `+0x175` & 4) once it has
  caught the player (the player's `+0x176` & 1, set when the police car is more than two
  nodes ahead, or level and both slower than 3) and both are stopped (speeds `+0x15c` /
  `+0x70` under 1), the player gets **a warning** (`0x14ec34` + 1, `hud_warning_on` `0x2d2c8`) if the chase's
  counter `0x15d4b8` (`traffic_police_tick` adds to it) is still under the limit
  `0x15d2d8`, else **a ticket** (`0x14ec30` + 1, `hud_ticket_on` `0x2d290`). Either way
  the player's car goes to kind 1 (wait, then drive again), the chase ends (`0x15d4b8` =
  `0xf74a8` = 0) and the police car is put back (`0x7c030`, `0x7aeb8`). **On the second
  ticket the race ends: `0x13c178` = 4, arrested**, and the player's finish time is 0.
  The limits `0x15d2d8` / `0x15d2e0` / `0x15d2e4` are set by `drive_opponent_wait`.
- The message: `hud_ticket_on` sets `0xea81c`, `hud_warning_on` `0xea820`, both with a
  time of −1000 (`0xea824`); `hud_ticket_or_warning` `0x2d2ac` tells if one is up (the
  pause waits for it); `load_ticket_picture` `0x30424` loads `ticketDH` / `warnDH` ..
  (`tickeg..` / `gwarn..` German, `..DL` the low-detail screen) from a `.qfs`.

Still to name: the opponents' routines under `drive_opponent` (`0x7cf98` 2.6 KB,
`0x7b178`, `0x7dbd8`, `0x77c..`), `0x5efb0` (first in `race_update_cars` when the state is
not 0: the replay?), `0x2a918`, the second drawing pass `0x5a4f0`, the widget block
`0x71208`..`0x74dcc`, Watcom's runtime.

## 5. The original in DOSBox (2026-10-19)

- The game for DOSBox is a COPY of the installed game in `~/dosgames/nfs` (`NFSSE/` +
  `UNIVBE.EXE`, from the download's `TNFSSE/`); the CD image is read where it was
  downloaded (`.../TNFSSE/cd/NFSSE.iso`, `NFS_ISO` overrides). `nfs-dos` (`~/.local/bin`)
  starts it in DOSBox Staging (flatpak) with `~/dosgames/nfs.conf` (Screamer's settings:
  S3, 60000 cycles, Pentium, SB16): `mount C ~/dosgames/nfs`, `imgmount D <iso> -t iso`,
  `C:\NFSSE\NFS`. UNIVBE is not needed (DOSBox's VESA is enough).
- **The game's pointers are its own addresses, not DOSBox's.** DOS/4GW runs with paging:
  the data object is at its own address 0x2a4000 but physically at 0xea7000. Its own
  address comes from any relocated pointer in it (`0xea828` holds image `0xe0afc`), and the
  game's memory is one run, so host = where the data object was found + (pointer − its own
  address). Checked: the road nodes' pointer `0x137430` read that way gives CY1's node 0
  byte for byte. `nfsdos.py` does this (`dos()`, the `car:` step).
- **`tools/nfsdos.py <prefix> "<plan>"`** runs it unseen on `Xvfb :77` as its own child,
  presses keys (XTEST), takes pictures, finds the data object in DOSBox's memory by the
  car sound banks' names (`0xe2d14`) and reads the clocks (the header lists what).
  `peek:<s>:<a>+<a>..` samples any words (flat addresses) every 5 ms and prints the
  values each took: the way to check a rate or a set of flags in the running original.
- The way into a single race with the defaults (Ferrari 512TR against the Diablo,
  head to head, City), in seconds from the start: `wait:20` (the EA logo, Pioneer, the
  intro, the title), `key:Return:5` ×3 (title, game mode SINGLE PLAYER, the name
  "Player"), `key:Return:7` (into CONTROL CENTRAL), `key:Down:1.5` ×3 (to RACE),
  `key:Return`. The race's clock starts about 2 s later.
- The race in 640x480: the cockpit view, the HUD line on top (best lap? 02:02.2, the race
  time, the gear, the distance 5.2 miles, the lap 2/2), a mirror, the map at the bottom
  right.
