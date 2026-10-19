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

**In memory** (snapshot of CY1, 2026-10-19): the head is the file's first 0x1621c bytes
unchanged (node 0's `+0x14` / `+0x18` from the head's `+0x18` / `+0x20` are the same values
on CY1; the speed scaling's factor was 1.0); `0x1365e0` = the objects after the 12-byte
"SJBO" head, the placements `0x13bb94` = that + kinds × 16; the stored sections follow the
objects in the file. `tri_keep_sections` `0x4ee30(sec)`: L = `0x13bb8c` (sections round a
circuit, 0x8000 on an open road), the head H `0x1365ec`, ahead A `0x1365f8` = 30, span
T `0x1365f4` = A + 5; fold(d) = L − d if d > L/2 (sic), d + L if d < −L/2. d = fold(H −
sec); e = fold(|sec − (H − A)|); e > T → H = (A + sec − T) & mask, d = A − T. While d < A:
load H, H + 1; while d > A: load H − 1 − T, H − 1 (d folded again each time). A section
goes to the ring slot (sec × 0x21c) % 0x8700. **Ported (`port/src/track.c`: the whole file
in memory, the same ring), equal to the original's ring in 3 snapshots
(`tools/track_emu.py`).**

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

### 3.8 EA sound: EAS, ASF / AS4, BNK — read (`tools/eacs.py`, 2026-10-19)

One header, **`EACS`**, everywhere (read by `snd_stream_header` `0x8c8f3`, `snd_play_eacs`
`0xa1f11`, `0x95b4e`, `0xad887`): +4 u32 samples a second (16000, 16247, 22050), +8 u8 bytes
a sample, +9 u8 channels, +0xa u8 compression (0 PCM, 2 IMA ADPCM), +0xb i8 the voice a
stream plays on (0..15, records of 0x28 at `0x160fa4`; -1 in EAS / BNK), +0xc u32 samples,
+0x10 i32 loop start (-1 none), +0x14 u32 loop length, +0x18 u32 where the data starts.
- **`.EAS`** (193: speech, `SPEECH/`, `GSPEECH/`): the header at 0, the data at 0x20. 170 PCM
  (8-bit signed: the mixer's volume table `0xa06a5` maps byte 0x80 to -128), 23 IMA.
- **`.BNK`** (38: engines, collisions, sirens): 128 u32 offsets (0 = none) of 0x48-byte
  records; a record's +0x28 is the header, its +4 the header's file position, its data
  offset a file position too; +0, +0xc, +0x14..+0x27 (volumes, pitch?) still to read.
  All PCM.
- **`.ASF` / `.AS4`** (3 PCM 16 kHz, 12 IMA 22 kHz music): blocks of u32 tag + u32 size
  (the 8 included), handled by `snd_stream_block` `0x8cbf5` and queued for the mixer
  (`0x8c9fe`, the list at `0x15ece4`/`0x15ece8`): `1SNh` (the header at +8, data from +0x28),
  `1SNd` data, `1SNl` at the loop's end (u32: the file position of the block the loop starts
  with: checked on all 14, every loop starts on a block), `1SNe` the end (CREDITS.ASF has
  1.7 MB after it that is not part of the sound). The headers' sample counts equal the
  blocks' sums.
- **The IMA ADPCM** (`snd_voice_start` `0x9ff4c` sets the voice's mode 2 and its decoder
  `[voice+0x58]`: `ima_decode_mono` `0xadfdc` or `ima_decode_stereo` `0xae084`): standard
  IMA, the tables pre-multiplied in the data (`0x12669c`: 89 steps × 16 codes of ready
  differences, computed bit by bit: step/8 + step·b2 + step/2·b1 + step/4·b0; `0x127cdc`:
  the 8 index changes ×64; the index is kept ×64, clamped to 0..0x1600). That is why a search
  for the step table found nothing. High nibble first; stereo is one byte a sample, high
  nibble the first channel. A stream's data block (and the 1SNh's data) starts with 0x14
  bytes: u32 samples, u32 step index channel 0, 1, u32 value channel 0, 1 (the voice packs
  them as `+0x30` = index0·64 | index1·64 << 16, `+0x34` = value0 | value1 << 16). An IMA
  `.EAS` starts from 0, 0. **Checked against ffmpeg's EA decoder** (`adpcm_ima_ea_eacs`):
  with its rounding ((2n+1)·step/8) every sample of ROCK1.AS4 is equal; the tool keeps the
  game's bit-by-bit rounding (differences up to 72 of 32768).
- Not sound: `0x8cd60`, `0x8cdf0`, `0x8cf00` are the TGV video's (§3.14).

### 3.9 EA's packers: 10FB, 30FB/32FB/34FB, 46FB — read (`tools/unpack.py`, 2026-10-19)

`unpack` `0xa7020` (src, dst, complain; returns the size; `unpack_size` `0xa711c`) picks by
the first two bytes, `& 0xfe` on the first (an odd first byte = a header 3 bytes longer):
`10FB` RefPack (`0xaf2c8`; 473 `.QFS`), `30FB`/`32FB`/`34FB` Huffman (`0xaf440`; the `.PBS`,
`.PDN`, 15 `.QFS`), `46FB` byte pairs (`0xb0674`; 8 `.QFS`), `4AFB` (`0xb0890`; no file uses
it). **All 527 packed files on the CD unpack to the byte as the game's own routine does**:
`tools/unpack_emu.py` runs `0xa7020` itself in Unicorn (`build/venv`) and the two are
compared file by file. Before this, `fsh.py` could not open 23 menu pictures (the German
track records, the rally maps, the police screens); it now uses `unpack.py`.
- **Huffman** (`0xaf440`, bits read from the top of each byte): 16 bits the kind (bit 8:
  24 more bits to skip), 24 bits the size, 8 bits the escape byte. A "number": `1xx` = 4..7,
  or z zeros, a one, z + 2 bits b = 2^(z+2) + b. Per code length 1, 2, ..: a number − 4 =
  the codes of that length (canonical), until the code space is full. The symbols in code
  order: each a number − 3 = how many unused byte values to step on from the last (from
  255, wrapping). Then codes: a symbol that is not the escape is a byte; the escape is
  followed by a number − 4 = n: n > 0 repeats the last byte n times, n = 0 then 1 bit: 1 the
  end, 0 then 8 bits a byte as it is. `32FB` then sums the bytes (deltas), `34FB` twice.
  (The routine decodes codes up to 8 bits through two 256-byte tables, longer ones through
  left-aligned 16-bit limits.)
- **Pairs** (`0xb0674`, `47FB` has 3 bytes more): 24-bit size, the escape byte, the number
  of pairs, then (code, first, second) each; a code stands for its two bytes, each maybe a
  code again (`0xb0630`, recursive). A byte that is no code is itself; escape + 0 ends,
  escape + b is b.

### 3.10 Car physics: PBS and PDN — first reading (`tools/carspec.py`, 2026-10-19)

Both Huffman-packed, all i32, mostly 16.16; the fields' meaning settles when the physics
is written (`car_physics_player`, `0x3fb90`, `0x3ed68` ..). `build/cars/torque.png`.
- **`.PBS`** (CARSPECS: the 8 player cars and TRAFFC; 1912 bytes), `load_car_pbs` `0x50c24`:
  used in place (the car's `+0x479`). **A checksum**: the sum of bytes 0..0x757 = u32
  `+0x774`, otherwise `+0x3c` is set to 0 (an edited file gives a car without power).
  `+0x3c` a power factor (0.5..0.84; ×248/256 with the automatic gearbox, `0x18990`;
  TRAFFC 2.0), `+0x88` the torque points, from `+0xb4` (rpm, torque) pairs every 200 rpm
  from 1000 (in N·m it seems: the NSX peaks at 286 at 5600; `0x18990` makes them 16.16).
  `+0x334` ABS, `+0x338` traction control (1.0 = the car has it: the NSX, ZR-1, Supra both,
  512TR, RX-7, 911 ABS only, Viper and Diablo neither; on only with the player's own
  setting, `0x12da5f` / `0x12da5b` in the player's 0x4b-byte record → car `+0x492` /
  `+0x48e`). `+0x2e4`, `+0x2e8` → car `+0x78`, `+0x7c`. When `0x12dd2c` is set the loader
  halves `+0x320`, `+0x328` (and stores 1/x at `+0x324`, `+0x32c`), `+0x2dc`, `+0x2e0`,
  `+0x28`, `+0x2c`, multiplies `+0x24` by 1.5, copies `+0x374..` 0x200 bytes to `+0x574..`,
  and scales `+0x3c` by 0x14c/256 (`0x12da12` = 3) or 0x133/256: a game mode still to name.
  (`By_R&T`, a 4-byte file beside them, is opened for writing in a debug case: not needed.)
- **`.PDN`** (CARFAMS: all 22 cars; 460 bytes), loaded in `load_car_cfm` `0x65874` into
  `0x1493a4[car]`, applied by `0x1e9cc`: `+0xc`, `+0x10` → `0x2c920` with the car's body
  (`+0xc0`); `+0x14` → car `+0x168` and 1/x `+0x16c` (10, 15, 16; 255 for traffic);
  `+0x18` → `+0x170`; 93 values from `+0x1c` → car `+0x178..` (a falling curve, a value a
  speed step?); six from `+0x1ac` → `+0x308..` with 1/x at `+0x320..`: the gears' top
  speeds in m/s (NSX 20, 35.6, 50.2, 63.5, 74.7 = 269 km/h; Diablo 323, 512TR 307: the real
  cars'); `+0x1c4` the rev limit (NSX 8000) → `+0x338`; `+0x1c8` → `+0x469` (5 for the
  player cars: the gears?). Traffic cars have 0 in the head, gears and rev limit.

### 3.11 Track INFO (sky and horizon) — read (`tools/trackinfo.py`, 2026-10-19)

`SIMDATA/MISC/<track>INFO`, text with its own labels, read by `load_track_info` `0x49dc0`:
a line at a time (`0x38c50`: lines starting `#` or empty skipped), one value a line. Eight
decimals into 16.16 (`0x41b68`): vert offset near/far → `0x12ef44`/`48`, zoffset near/far →
`4c`/`50`, height near/far → `58`/`60`, radius near/far → `54`/`5c`; colour top and bottom,
three numbers on a line packed a<<10 | b<<5 | c → `0x12ef64`/`68` (all tracks: 0 1 2 and
3 4 5); then `atoi`: ring type (1 on CL3, AL3) → `6c`, ring y offset ("bigger places the
horizon further down", -60 / -15 / -63) → `70`, ring height (112) → `74`; "RGB TIME OF DAY
VALUES": three groups of five → `0x12ef78 + 4g + 12k` (the fifth ×256; CY1: 400 202 80 2572
216 / 255 255 255 0 255 / 296 52 52 0 255: colours above 255 and an angle?, no direct reader
found), and one last value → `0x12efb4` (1 on CL3, AL3). Users: `load_horizon`, `0x4a9d0`,
`0x4ace8`, `race_menu_options`. CY3INFO has a stray "o" line that `atoi` reads as 0.
**The files win**: the flag `0xec5d8` is 1; only when a file is missing (or `0x12c340` = 1)
does the game take the built-in table (33 values a track from `0xec5dc`, 0x84 apart, picked
by the names cl1 .. tr1), which holds older values than the files.

### 3.12 INV (the menus' inverse palettes) — read (`tools/inv.py`, 2026-10-19)

32768 bytes beside a menu picture, loaded with it (e.g. the audio screen `0x10853`:
`option/audio.fsh`, its `!pal` through `pal_from_shape` `0x718a4`, `option/audio.inv` into
`0xf77c4`). Read by EA's `nearestcolour` `0x80c80` in 8-bit modes: a colour `0xRRGGBB` →
the palette entry at `(R>>3)<<10 | (G>>3)<<5 | B>>3`; with no table it searches the palette
at `0x1607a4` itself (only the entries flagged at `0xf77c8`); in 15/16/24/32-bit modes it
packs the colour instead. 30 routines call it (menus, fonts, the 3D library).
**36 of the 51 tables are their screen's nearest-colour table** (90–93 % the nearest entry,
the rest ties); **15 fit no palette on the CD** (all the installer's `IART` screens, `AUDIO`,
`OPTIONS`, `MODEMSET`, `FINSTND`, and the bonus tracks' `BRUN`/`TROP` screens that share
City's tables): made for older art, so the original picks odd colours through them. The port
reads the files as they are, and that stays faithful.

### 3.13 BAMF (what of the 3D view reaches the screen) — read (`tools/bamf.py`, 2026-10-19)

EA's blit lists, copied by `0x8b8f0` (the off-screen view `0xf4578`, the list) by VESA
64 KB bank (`0x9d7f8` sets the bank): "BAMF", u32 size, u16 banks, u16 the longest span,
u32 `+0xc` and `+0x10` (0x10000, or 0x8000 = the source read at half the rate across /
down: pixels doubled), then per bank a u16 count of spans, each u32 offset in the source,
u16 offset in the bank, u16 length. Every file's spans end exactly at its size.
- **The HUD** (`load_cockpit_mask` `0x5f7c8`, drawn by `0x30b64`): `MASKHI.B<s><n>` /
  `MASKLO..` into `0x145a58`, and `MASKHI.MMB` (a `wwww` of 12) into `0x145a24`; s = the
  view's size (full screen, medium, small window), n = how it is copied (1:1; doubled
  across; doubled both ways; doubled with every other line: the detail settings). The
  spans leave out the status bar on top, the tachometer bottom left and (the `.B` set
  only) the map's box bottom right; `0x12b988` picks the set. `MASKHI.FSH` / `MASKLO.FSH`
  (`MASKHIG` German) hold the HUD's pieces (digits, bars, the tachometer, labels).
- **The dashboards** (`SIMDATA/DASH/<car>DH.FMM`, `DL.FMM`: a `wwww` of 4, the same four
  ways of copying): the windscreen and side window, A-pillar and mirror cut out
  (`build/bamf/ansxdh.png`); the dashboard picture is `<car>DH.FSH`.

### 3.14 TGV (the movies) — read (`tools/tgv.py`, 2026-10-19)

The 26 `FRONTEND/MOVIE/*.TGV`: the EA logo, the title, three attract loops, a showcase a
car, three police clips (and German `GCOP`), seven winners' clips. Played by `movie_play`
`0x34ba0` → `movie_loop` `0x346dc` (from `fe_attract_mode` `0x34284`, `fe_winner_movie`,
`fe_cop_ticket_movie`); a frame decoded by `tgv_frame` `0x8d4ac`. All 320x240, 15 frames a
second, the sound an EA stream (16 kHz 8-bit stereo PCM in all checked).
- Blocks of u32 tag + u32 size (the 8 included): `1SNh`/`1SNd` the sound (§3.8), `kVGT` a
  key frame, `fVGT` a delta frame.
- **`kVGT`** (`tgv_key` `0x8d190`): `+8` u16 width, `+0xa` height, `+0xc` u16 (0 in all),
  `+0xe` u16 colours (256), `+0x10` u32 flags (bit 0 = the picture unpacked; 0 in all
  1,236), `+0x14` the palette, 3 bytes a colour, **8-bit** (values to 255), copied as it is
  to the decoder's `+0x44`; then the picture, RefPack (`10FB`, §3.9). A new size frees and
  allocates the two picture buffers (`+0x28` current, `+0x2c` previous).
- **`fVGT`** (`tgv_delta` `0x8cf00`): swaps the two buffers, then `+8` u16 vectors, `+0xa`
  raw blocks, `+0xc` packed blocks, `+0xe` bits an index. From `+0x14`: the vectors (x, y:
  pairs of 10-bit signed fields, least significant bit first, `bits_signed` `0x8cd60`; the
  run rounded up to 32 bits); the raw 4x4 blocks (16 bytes, row by row); the packed blocks
  (8 bytes: 4 colours, then a u32 LE of sixteen 2-bit codes, the top two bits first,
  `tgv_packed_blocks` `0x8cdf0`); then one index a 4x4 block of the picture
  (`bits_unsigned` `0xa6ec8`, least significant bit first). `tgv_build_frame` `0xa6f6c`,
  left to right, top to bottom: an index below the vectors' number copies the 4x4 at
  (this place + that vector) from the previous picture (a row table makes dy·width);
  any other is block (index − vectors) of the raw blocks followed by the packed ones.
- **Checked against ffmpeg's `tgv` decoder**: all 26 movies, 11,833 frames, equal pixel for
  pixel; TITLE's sound equal sample for sample.

### 3.15 RPL (replays) — read (`tools/rpl.py`, 2026-10-19)

A replay is not a film: it is the race's settings, every driver's controls and snapshots of
every car, from which the race is driven again. 100,374 bytes, five blocks of memory
written by `replay_file` `0x5c3b0` (`fwrite`, `<path>Replay.rpl`, when `0x12dd5c` is set and
the mode `0x15d136` is not 1) and read back in the same order: `replay_settings` `0x5c340`
(from `race_run`: the first block only, from the name at `0x145558`; no file → `0x14554c`
= 6, `0x13c17c` = 0x20) and `replay_load` `0x5c4a4` (all of it, keeping the player's own
three settings in its entry). The attract mode (`fe_attract_mode` `0x34284`) puts
`REPLAY%d.RPL` in `0x156c4f`, sets `0x156cc3` = 3 and runs a race.
- `0x00000` 0x348 bytes: **the race record `0x12da0c`** (filled by `race_setup_record`
  `0x36c5c`): `+2` (0 in all), `+6` the track (`0x41b68`'s order: city coast alpine
  springs avalley bsienna vridge tpolis lvegas moon), `+0xa` its files' stem (`al1`,
  `tr6`, ..), `+0x14` 1 / 2 (head to head / a race of 8?), `+0x18` network, `+0x1c` flags
  (`0x15d12a`), `+0x20` racers, `+0x24` drivers (people), `+0x28` (1 with traffic), `+0x2c`
  cars in all, `+0x30` traffic cars, `+0x38` **the time** (seconds from 1970: the CD's were
  made 15–16 June 1996), from `+0x3c` two 0x4b-byte entries (the player, the head-to-head
  rival: `+2` the name, `+0xb` the car in `0xf6e40`'s order: TSupra LDiabl P911 CZR1
  F512TR DVIPER ANSX MRX7 .. Traffc, the police and traffic cars; `+0x13` / `+0x17` the
  two driving aids), .. `+0x300` the clock setting `0x12dd0c`, `+0x310` the player
  `0x12dd1c`.
- `0x00348` 0x9a4 bytes from `0x144ba8` (the highlights `0x144da8`, §4.8, among them).
- `0x00cec` 0x15e00 bytes, **the recording** (`UsrCtrlBuff`, allocated by `replay_start`
  `0x5dd68` and **never cleared**: what the race did not reach is whatever the memory held,
  so a reader must stop at the end below). `replay_record_step` `0x5d888` runs on every
  second step (flag 4, §4.3); `0x144ba4` = step counter `0x145c94` mod 28,800:
  - `+0`: the drivers' control words, one slot a call (`replay_record_controls` `0x5cac4`,
    played back by `replay_play_controls` `0x5cb58`): driver k's low bytes at k·7200 + t,
    its high bytes at 14,400 + k·7200 + t, t = `0x144ba4` / 4. Two drivers at most; a
    ring of 7,200 slots = 14,400 steps = 7.2 minutes. During the first 120 counts every
    word is `0x7a0` (no steering, no pedals).
  - `+0x7080`: every 450 calls (900 steps, 27 s; `0x143ee8` counts down) a snapshot
    s = `0x144ba4` / 1800 of 16: each driver's car, 0x1a4 bytes at `+0x7080 + s·0x1a4 +
    k·0x1a40` (`replay_pack_car` `0x5cbd0` + `replay_record_car` `0x5d0e4`; back by
    `replay_restore_car` `0x5ceb0`);
  - `+0xa500`: the other cars at the same moment, 0x172 bytes at `+0xa500 + s·0xb90 +
    j·0x172` (8 at most; the count at `0x143f3c + s·0xa2`).
  - A car's snapshot: `+0` / `+4` car `+0x50e` / `+0x512`, `+8` its index (`+0x516`) << 24
    | `+0x51a` << 8, `+0xc..` car `+0..+0x20` (the position, the angles), `+0x54` car
    `+0x48` (the road node), .. The positions lie on their road nodes (checked on all four).
- `0x16aec` 0xcc8 bytes from `0x143ed8`: `+4` (`0x143edc`) **the step counter at the last
  step recorded** (2 a step: REPLAY.RPL 11,632 = 5,816 steps = 2:54.5), from `+0x14` 16 ×
  0xa2 bytes, what each snapshot needs besides the cars (the cars' order `0x13c298` /
  `0x13c274`, the random seed `0xeab70`, ..).
- `0x177b4` 9 × 0x1d2 bytes from **`0x14ea80`**: the racers' results (§4.3), whose records
  start at `0x14ea7c`: the game's copy is 4 bytes late (each record's first 4 bytes are
  missing, the next one's follow). REPLAY.RPL: the Diablo finishes at 11,198 (+300 =
  2:52.5).
- **The control word** (`controls_read` `0x49248` builds it from the keys or joystick;
  `car_steer` `0x483e4` / `car_steer_2` `0x4853c` and `car_pedals` `0x48990` /
  `car_pedals_2` `0x48a80` use it, chosen by the entry's `0x12d974` / `0x12d978`): bits
  0–5 the steering (32 = straight; 1 to 15 either way a slope, beyond it full lock),
  6–11 the pedals (30 = neither; below 15 `car_throttle` `0x48600` ramps `+0x3b1` to 255,
  45 and above `car_brake` `0x48658` the brake `+0x3b9`), 12 a gear down, 13 a gear up
  (`car_shift` `0x48714`), 14 car `+0x4ba`, 15 car `+0x3f5`.
- The CD's four: REPLAY alpine (al1) Diablo against a 911 with traffic, 2:54; REPLAY1
  Burnt Sienna (tr6) a 512TR in a race of 8, 1:53; REPLAY2 Transtropolis (tr7) a Diablo in
  a race of 8, 2:03; REPLAY3 coast (cl1) Viper against a Supra with traffic, 1:44. All
  one driver at the keyboard (`build/rpl/*.png`).

### 3.16 CONFIG.DAT (the front end's state, the records) — read (`tools/config.py`, 2026-10-19)

`GAMEDATA/CONFIG/CONFIG.DAT`, exactly 24,402 bytes (0x5f52), is copied as it is to
`0x156c14` by `cfg_load_config_dat` `0x39c40` (another size or no file: `cfg_defaults`
`0x38ea8` builds it) and written back by `cfg_save_config_dat` `0x3a134`. Offsets from
`0x156c14`:
- `+0` the player's name ("Player" / "Spieler"); `+0x3b` (`0x156c4f`) the replay's name;
  from `+0x6b` the menus' choices as words, read by `race_setup_record` `0x36c5c`: `+0x93`
  the segment (→ `0x12da0e`), `+0x95` the track (→ `0x12da12`), `+0xb3` the race length
  (which table of race times), the car, the opponents, the detail settings (`0x156c93`..
  set from a speed test `0x74ca0` in the defaults), ..; `+0x5f36` (`0x15cb4a`..) the keys.
- From `+0xb5`, **the records**: 9 tracks (`0x41b68`'s order) × 0xa6b bytes, 0x27 bytes a
  record: `+0` the name (15), `+0xf` the car (the menu's order, `0xf7464`: RX-7 NSX SUPRA
  911 ZR-1 VIPER 512TR DIABLO), `+0x13` 0/1 (?), `+0x17` 1 = filled, `+0x1b` the time in
  **1/60 s** (`fmt_time` `0x74914`: m:ss.t; 99,999 / 999,999 = "N/A"), `+0x1f` a speed
  (16.16 m/s; `fmt_speed` `0x74854`: × 2.24 "mph", German × 3.6 + 1 km/h; below
  0x9fff6 "N/A"), `+0x23` the kind of race (0 time trial "t.t." / German "z", 1 head to
  head "h.h." / "d", 2 a race: the track select screen `0x75288`).
  - records 0–2: city, coast, alpine: the best time of segments 1–3 (`records_segment`
    `0x69608`, from the result's `0x15cbc1`, its top speed `0x15cbc9`); a circuit: the best
    lap in record 0 (`records_best_lap` `0x69438`, from the 20 lap times at `0x15cb71`);
    records 3–5: the best top speed of segments 1–3.
  - records 6–35: three tables of ten race times (`+0xb3` picks one; on a circuit each
    length twice the one before: Rusty Springs 2:43, 5:23, 10:34).
  - record 36 (`+0x57c`): the track's top speed.
  The players' results they come from: 0x95 bytes a player from `0x15cb68` (`+0` the name,
  `+9` 20 lap times, `+0x59` the time, `+0x61` the top speed, `+0x6d` ..).
- **The CD's copy is not the built-in defaults**: `cfg_defaults` takes its records from
  text tables (`0xeab80` .., six strings a record: name, time, car, speed, `+0x13`, kind);
  the first five race times on City agree, the slower five are faster on the CD (Slayer
  7:54.5 against 8:09.5 built in, ..). The CD's city segment times add up to its best
  race time (7,332 + 7,596 + 11,706 = 26,634). The port should seed its records from the
  CD's file.

### 3.4 Still to read

Every file type on the CD has a reader now. Open within them: the PBS/PDN fields the
physics will name, the 0x9a4-byte block of a replay, the menus' choices word by word.

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

- **The menus' common routines, as the port has them (2026-10-19; checked against the
  original: the game mode screen to 9 pixels, the exit dialog to 1 near-black pixel of
  DOSBox's colour adjustment)**:
  - Keys `fe_read_keys` `0x727a0`: the key *held*, first of Ctrl+F10 / F5 (both 0), Esc
    0x1b, Space 0x20, Enter 0x0d, 0x4800 up, 0x5000 down, 0x4b00 left, 0x4d00 right,
    0x5200 Ins, 0x5300 Del, F1-F4 (only when `0x12bd3c`), then the raw scan codes 0x2a,
    0x36, 0x1d, 0x38, 0x15 (Y), 0x31 (N), 0x24 (J); Tab 0; the joystick after.
    `key_down` `0x84742` (by scan code), `key_scancode` `0x91d60`. **The menus read held
    keys**: a DOSBox test must hold a key (`hold:Return:0.2`), a tap is missed.
    A screen acts on a key once, then again only when it is let go or held over 25
    hundredths (not Esc, Space, Enter). `key_flush` `0x84358` (empties the typed-key queue; `key_get` `0x84304`).
  - Mouse: the driver `0x8498e` sets the poll `[0xf7964]` = `0x848af` (relative mickeys,
    x at half speed; buttons 1 left, 2 right), hide `[0xf795c]`, show `[0xf7960]`; place
    `0xf78e0` / `0xf78e4`. `0x156604` the mouse has been used, `0x156606` its buttons.
    `mouse_load` `0x52020` (`MOUSE.FSH`: ptr0, ptr1 pressed, ptr2; 15x15, 255
    see-through), `mouse_draw` `0x52288` (also sets 0x156604), `mouse_buttons` `0x522e8`,
    `mouse_press` `0x52304` (ptr1 for 0.2 s), `mouse_save` `0x52348`, `0x71ad4` place +
    save, `0x71b24` moved.
  - Sound: `fe_sound_bank_load` `0x724c4` (`SIMDATA/SOUNDBNK/FRONT_MW.BNK`),
    `fe_play_sound` `0x72510` (sound n, pan 0x40, volume CONFIG.DAT word +0x79),
    `fe_sound_bank_free` `0x725ac`. 0x47 a move, 0x48 a choice.
  - Music: `music_choose` `0x63ed0`: mode CONFIG.DAT +0x73 (0 none, 1 any of 12 in turn,
    3 and 10-15 techno, else rock), counter `0xed0fc`; pick 5 of a set means none, so
    ROCK6 never plays (kept). `music_start` `0x72010` (looping AS4), the feeding
    `fe_service_stream` `0x71b6c` / `fe_service_music` `0x71c20`, `music_fade_out`
    `0x720d8`, `music_wait_end` `0x72150`.
  - Text: `setfont` `0x91ed0` (FFN +0xa count, +0x10/+0x11 origin, +0x12/+0x13 line,
    +0x14 characters, +0x1c the 4-bit bitmap), `printstring` `0x94234` (characters
    <= 0x20 move by the space's advance, newline: x 0, y + line), `text_width` `0x84794`
    (through `0xa3e30`, at most 256 characters). `textcolour` `0x95e10` with `0xa87ec`
    makes a ramp of 16 ARGB: a = k*255/15, the colour * k / 15. `0xa857c` also makes a
    16x256 table of nearest entries, but the letter routine `0xa8080` uses it only when
    the window has **7** bits a pixel (`0xf88b4` = window +0x18); 8-bit menus always take
    the slow path: `0xb1430` for each pixel of level 1..15 (level 0 is left alone):
    channel = round(under * (65536 - 256a) / 65536) + the ramp's, at most 255, then
    `nearestcolour`; level 15 is the colour's nearest entry. (The table path would also
    skip letters one byte wide.) Styles `fe_styles_init` `0x71208` (white ffffff, beige
    e8d3b7, yellow ffce06, red ff0606, the chosen one, black for the shadow, saved at
    0x152f04 0x153f44 0x154fd4 0x150b84 0x14fb44 0x151bc4), `fe_style` `0x7135c` (3 white,
    2 red, 1 yellow, else beige), `fe_text` `0x71774` (shadow black at +2,+2, then the
    text), `fe_text_centre` `0x716fc`, `fe_button_text` `0x713d0` (the same into a window
    of width + 10 by 45 holding the screen there, pasted back: text drawn over text).
  - Colours: `nearestcolour` `0x80c80`: the inverse table `0xf77c4` when there is one, else
    the closest of entries 0..253 in the palette copy `0x1607a4` (dg^2+dr^2+db^2, strict <
    from 99999). `make_inverse_table` `0x8227c` reads the DAC back (`read_dac` `0xa3184`,
    6 bits x 4) and builds the exact 32x32x32 inverse map (`inverse_colormap` `0x819c0`:
    cell centres x*8+4, entries 0..253, the lowest wins a tie). **`pal_fade_up` `0x71918`
    calls it at its end**; checked: the original's table on the game mode screen equals
    this computation in all 32768 cells.
  - `fe_yes_no_dialog` `0x73b04`: two windows of 340x100 holding the screen at (150,190);
    the box darkened through `remap_table` `0x89f3c` (0x9000 a channel, 6-bit cap, weights
    2R 3G 1B) by `polygon` `0x8a0cf`, two red frames `frame` `0x8a0f0`, the question red
    centred at (170,20), "Yes" at (60,60), "No" at (250,60). **"changed" starts at 1**: on
    its first pass it plays sound 0x47 and draws both buttons again with `0x713d0` (so
    they come out brighter than drawn once). Mouse placed at x 225 / 420, y 260 only if
    the mouse has not been used. Hit boxes yes x 0xbe..0x104, no 0x17c..0x1cc, y
    0xf5..0x118. Esc / N no, Y (J in German) yes, left / right swap, Enter / Space /
    a click the one chosen (the right button: no).
  - `fe_text_field` `0x701cc` (text, max length, centre x, y, names flag on the stack):
    a window of max/2*30 + 5 each side by 35 holding the screen; typed keys from the
    queue (`key_get` `0x84304`); names (flag 1) let in space . * + [ ] : ; " < > , ?,
    file names (0) none of them and letters become capitals; % \ / | never; German adds
    0x81 0x84 0x94 0xe1 0xe6 0x8e 0x9a 0x99 0xf8 0x15 (and installs its own keyboard
    handler `0x8447c` / `0x843bc`). Appends while length < max and the width < 440.
    Backspace, Del clears all, Enter (refused when only spaces, `0x98aac` isspace)
    copies back, Esc not. Any key redraws without the cursor; "_" blinks every 15
    hundredths. The mouse buttons are read once before the loop (after they are let go),
    so a click in it does nothing. `0x4dfe0` reads the joystick (`0x12f25c`: 0x20/0x0d).
  - `fe_enter_name` `0x743b4` (the name, CONFIG.DAT +0 = 0x156c14): the screen at
    (80,190) 480x100 darkened by `remap_table` 0x6000 (darker than the dialog's), red
    frames, "Please Enter Name" / "Bitte Namen Eingeben" red centred at (240,10), the
    field (8, 320, 230, names); sound 0x48 either way; a new name clears 0x15d12a and
    0x15d12e (bits the main menu keeps). The pointer is hidden and stays hidden. Port
    against the original: equal (only DOSBox's near-black noise), typing too.
  - `fe_game_mode_screen` `0x47274`: `CONTROL/GAMEMOD2.QFS` (bgnd, sinl/modl/exil lit,
    sind/modd/exid dark), the mouse picks by y (<= 0xcd single, <= 0x118 modem, else
    exit), button centres (320, 167/242/315); 45 s without a choice: back to the movies
    (result 2); single → `fe_enter_name` → 0, exit → 1, Esc and the right button ask
    "Exit to system ?".
  - `fe_outer_loop` `0x47000`: state 2; when going round again: music fade and wait, the
    attract mode, the first time CONFIG.DAT (state 3 if it fails), `music_choose`; the
    title when state is 2 or 3; state = the game mode screen; 0: `0x7469c`, the main menu,
    the settings saved; until 1. The main menu's result is the next state: 0 the game mode
    screen, 2 (2 minutes untouched) the title then the game mode screen, 1 quit.
    `fe_new_session` `0x7469c` copies the name into the tournament's records and clears
    them (`tourn_clear_records` `0x745f4`, 0x15d0d8 / 0x15d0e8 / 0x15d0fa), 0x15d134 = 0.

- **`fe_main_menu` `0x42c30`, "Control Central" (2026-10-19; the port's
  `port/src/mainmenu.c` equals the original in every screen tried: the start, car, track
  and race type changes, SKCART, the opponent, the buttons, Esc — 1 to 24 pixels, all
  DOSBox's colour noise)**. Network parts (0x15d136) left out.
  - Art `CONTROL/CENTRAL.QFS` (bgnd, cham = Chat greyed, the frames Trb3 opponent, Tlb1
    player, Brb4 location, Blb2 race type, arrows Aro1..Aro8, buttons Ra1l optl chal canl,
    Coml/Netl/Modl network; `!pal`), its inverse table `CENTRAL.INV` (`fe_load_inv`
    `0x747b8`) but `pal_fade_up` rebuilds it anyway. Each panel is its own file
    `CONTROL/<name>.QFS`, shape "0000", loaded again only when its key changes (0xec500/4/6
    track, 0xec502 player, 0xec4fc opponent, 0xec4fe race type; 0x14 = none;
    `mm_pictures_free` `0x424c0`). Names: tracks 0x41c80 (city coast alpine springs
    avalley bsienna vridge tpolis lvegas moon), mirrored with `m` 0x41ca8, rally with `r`
    0x41ccc; player 0x41cf0 (rx71 nsx1 sup1 9111 vet1 vip1 5121 dia1 war1); opponent
    0x41d20 (rx72 .. dia2, slew = traffic, watch2 = the clock, tourny, war2); tournament:
    tourny3 / tourny2 / tourny1 by the player's car < 3 / < 6 / else; race type 0x41d50
    (watch1 headhead single tourny, nomercy).
  - Drawing, into the off-screen window 0x156658 and shown (`screen_show_begin` `0x8378c`,
    `drawshapeatpos`, `screen_show_end` `0x83740`), then the pointer (`mouse_draw`, always):
    bgnd, cham, track, `flad` (tournament track done), player, opponent, race type, then
    the chosen one: a panel's frame and its arrow under the mouse with colour 255
    see-through (`drawshape_see_through` `0x815e4`), a button opaque. The first time it
    draws twice, fades up, builds the inverse table, the styles, and starts the 2-minute
    timeout. A change of choice or arrow plays 0x47 (not for "none") and draws all again
    (the buttons it draws first are covered by that).
  - The choice (8 = none): 0 opponent, 1 player, 2 location, 3 race type, 4 Race,
    5 Options, 6 Chat, 7 Cancel, 9 network. The keys put the mouse at 0xec418 / 0xec42c
    (x 482 175 444 211 45 228 400 570, y 158 197 318 349 454 454 454 454), since the
    mouse's hit test runs every pass. Hit boxes (inclusive, `mouse_in_rect` `0x71a1c`),
    in this order: opponent 0x153..0x271 x 0x61..0xda (race types 1 and 2 only; arrows x
    0x155..0x16a and 0x25d..0x26f, y 0x91..0xa8), player 0x13..0x14a x 0x70..0x11a
    (arrows 0x15..0x28, 0x136..0x148, y 0xba..0xd1), location 0x153..0x225 x
    0xfc..0x17f (arrows 0x155..0x168, 0x20f..0x223, y 0x131..0x14e), race type
    0x5a..0x14c x 0x13a..0x180 (arrows 0x5c..0x6b, 0x136..0x145, y 0x154..0x16d), Options
    0xa5..0x125, Cancel 0x1f8..0x27f, Chat 0x16b..0x1b5 (network), Race 0..0x70, all y
    0x1ba..0x1d1. A click tests the right arrows one pixel wider.
  - Keys (the held-key rule of the menus; any key hides the pointer, `fe_mouse_hide`
    `0x71afc`, until the next drawing): Up 0→4, 1→4 (time trial, tournament) else 0,
    2..4 → one less, 5..7 → 3, 8 → 1. Down 0→1, 1→2, 2→3, 3→4, 4..7 → 1 (time trial,
    tournament) else 0, 8 → 1. Left / Right on a panel change its value (sound 0x48),
    on the buttons Race → Options → Cancel → Race. Enter / Space / a click: on an arrow
    the value, on a car picture the showcase (`fe_showcase` `0x17724`), the location the
    track's records (`fe_track_select_tournament` `0x76380`), Race the race (or "Track
    already completed" in a tournament), Options `0x53a30`, Cancel back (0). Esc back (0),
    the right button "Exit game ?" / "Spiel verlassen ?". 2 minutes untouched (not in a
    tournament): result 2.
  - Values (CONFIG.DAT): +0x6d race type (0 time trial, 1 head to head, 2 single race,
    3 tournament), +0x91 opponent (0-7 cars, 8 traffic, 9 clock, 10 tournament, 11
    Warrior), +0x95 track (8 Lost Vegas, 9 never offered), +0x9b player car (11
    Warrior), +0xb3 race length 0/1/2 → +0xb1 laps 4/8/16, or 2/6/12 on tracks 4..7.
    `car_setup` `0x73a80`: +0x99 = 0 or 2 and +0x8f = 1 or 2 by the car (tables in the
    port). The player's car steps 0..7 (11 after 7 once the Warrior is unlocked); in a
    tournament only within the track's class (tracks 0,3: 0-2; 1,4,5: 3-5; else 6-7) and
    the track sets the car (0xec440 = 0 3 6 0 3 3 6 6 7 3). The opponent steps 0..7, or
    0..8 (traffic) in a single race. `mm_race_type_changed` `0x425b0`: time trial opp 9,
    head to head the player's car if the opponent was 8..10, single race opp 8,
    tournament: no mirror / rally, track 9 or a locked 8 → 0, its car, opp 10, length 1,
    8 laps (6 on 4..7). `mm_race_type_step` `0x4235c` (a tournament running asks "Quit
    tournament ?" first). Tracks: `mm_track_next` `0x4287c` goes normal → mirrored (if
    unlocked, not a tournament) → rally (likewise) → `mm_track_plus_one` `0x42910` (the
    next, of 9 with Lost Vegas else 8); `mm_track_previous` `0x42984` the other way.
  - Unlocks: +0x5f50 (kept in CONFIG.DAT) ≥ 1 mirrored tracks, the Warrior and Lost Vegas;
    ≥ 2 rally tracks too. **The typed codes** (`cheat_code_step` `0x46e94`, one letter
    each time a letter key goes down, 0x12ccaa; the strings stored as letter + 99 from
    0xec48c, their positions 0xec4d8..): SKCART mirrored tracks (and shows one), EARALLYE
    rally tracks, EXPLODE 0xf74a4, CASINOVILLE Lost Vegas (silent), CONCEPTCAR the
    Warrior as the car, TURBO 0x15d12a |= 8, NITRO 0x12cca0, ROCKYROAD +0x5f50 between 1
    and 2 with its unlocks (silent). Not in a tournament; TURBO and NITRO not in a network
    game. **End held** on head to head: "nomercy" picture and 0x15d12a |= 4, cleared on
    release.
  - The CD's CONFIG.DAT has type 1, opponent 7, track 1, car 6, 8 laps, length 1; the
    DOSBox install has none, so the original there starts from `cfg_defaults` `0x38ea8`
    (track 0). The port tests take the original's live state (`nfsdos.py
    mem:156c14:5f52:...`, `RENFS_CONFIG=`).
  - Options (sel 5): fade down, pictures freed, `fe_options_screen`; back: the race type
    checked again (a loaded game may change it), `CENTRAL.QFS` and its palette loaded
    again, the mouse put on Options, drawn and faded up as the first time (0x4577a).

- **`fe_options_screen` `0x53a30` (2026-10-19; the port's `port/src/options.c` equals the
  original on six screens: 6 to 24 pixels of DOSBox's colour noise)**. `OPTION/OPTIONS.QFS`
  (`OPTIONS.INV`, rebuilt by the fade anyway). Rows 0 Transmission, 1 ABS, 2 Traction
  control, 3 Audio (`fe_audio_options` `0x105dc`), 4 Graphics detail (`0x32d08`), 5
  Controls setup (`0x22bd0`), 6 Load and save (`0x3c0c8`; not in a network game), 7
  Credits (`fe_credits` `0x25600` with `credits.asf`), 8 Done, 9 Cancel. Shapes: `bgnd`,
  the names lit `Tral Absl Trcl Audl Gral Conl Loal Crel Donl Canl` (dark ones are in
  bgnd), values `Autd/Mand`, `On1d/Of1d/N/1d`, `On2d/Of2d/N/2d` and their lit `..l`
  (tables 0x53978..). The values: CONFIG.DAT **+0x97 gearbox (0 automatic, 1 manual; +0x8d
  = 1 when automatic, set with it), +0x99 ABS, +0x8f traction control (0 on, 1 off, 2 the
  car has none: not switchable)**. Mouse by height: y <= 0x85, 0xa7, 0xcb, 0xec, 0x10e,
  0x132, 0x155, 0x184 the rows; below, x <= 0x141 Done, x >= 0x143 Cancel; elsewhere the
  choice stays. Keys put the mouse at x 240 (Done 40, Cancel 565), y 118 151 185 220 253
  287 324 363 450 450. Up: one less (Cancel → Credits), from the top to Done; Down: one
  more, from Done / Cancel to the top; Left / Right swap Done and Cancel. A choice plays
  0x48 then, as the screen is drawn again, 0x47. Esc and Cancel put the four words back
  (and `0x721ec` re-applies the volume +0x77); Done, the right button, 2 minutes or
  `0x13c3ca` leave keeping them, fitting the opponent to the race type when a game was
  loaded. Every key acts once a quarter second (Esc included here). Leaving: fade down,
  `0x15d13e` = 1 (the main menu draws again), result 0.

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
- Replays (§3.15): `save_replay` `0x3a628` ("Save replay as") copies the last race's
  `replay.rpl` under a new name through `replay_copy_as` `0x3a218` (named `load_replay`
  until 2026-10-19: it reads, checks the disk's free space, writes); `replay_file`
  `0x5c3b0` writes it at the race's end, `replay_start` `0x5dd68` allocates `UsrCtrlBuff`.
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

#### Painting a textured quad (2026-10-19; ported, `port/src/raster.c`)

- The filler pointer `0x12c0dc` = `0x34ed8` (set by `load_vis` `0x58bc4`; the other
  setters of `0x12c0d8..e4` are a debug `.vis` recorder). It calls `0x91360(shape, uv[4],
  xy[4])` (stack arguments): uv in 16.16 picture pixels, xy whole screen pixels, corners in
  order round the quad. A shape of type `0x45` goes to `0x90b30` instead (with its type
  set to `0x7b` for the call): the same routine with the row painter `0xa7610`, which
  leaves colour 255 out. **Type `0x45` is made at load** by `0x5b270`: every shape of a
  track's SHPI that holds a 255 and whose name does not start with `!` (all are `0x7b` on
  the CD).
- `0x91360`: the top corner = the first with the least y, the bottom = corner 2 unless
  another has a greater y (strictly). Which way round from the two edges' slopes at the top
  (`(dx << 16) / (dy + 1)`, the next edge's ≥ the previous one's → +1). The left and right
  sides walked from the top to the bottom corner into tables by absolute row (x, u, v each
  side; 0x300 rows at most, `fatalerror` over): `0xa7790` (left x), `0xa7844` (right x),
  `0xa78e4` / `0xa79a4` (u, v): slope `(x1 − x0 << 16) / n` (n = dy + 1, the end row
  included), a half step in on the first row (left: when the slope < 0, right: > 0; u/v
  by the x slope's sign), cut to the window `0xf88a4..0xf88b0` (x0, y0, x1, y1: ends
  outside). An edge going up (n ≤ 0) is skipped: the table keeps what was there (the
  original's stack).
- Each row: n = right − left + 1; **n ≤ 0 → once, the way round flipped and the sides
  built again, the same row again; a second time ends the quad** (`0x91917`). Rows outside
  the window's x are skipped; du = (u_r − u_l) / n, dv likewise (before the clipping), the
  left cut moves u, v by du, dv per pixel cut. The row painter `0xa7a4c` (`0xa7610`):
  source = picture + (v >> 16) × row bytes + (u >> 16); the fractions in 16 bits each and
  their carries step the source by a row and a pixel (`add bx,dx` / `adc esi,[step +
  row?]`): exactly u + k·du, v + k·dv, no wrapping. Row bytes `shaperowbytes` `0xa61f0`
  (bits a pixel from `0xf9888[type & 0x7f]`, 15 → 16, × width, rounded up to bytes);
  the screen's bits a pixel `0xf88b4`, its rows `0xf88c4` (a table of row addresses) +
  `0xf88c8`.
- `tools/raster_emu.py` runs `0x91360` / `0x90b30` in Unicorn against the port's
  `raster.c`: **equal on 22,246 random quads** (convex, crossed, clipped, see-through;
  2026-10-19).

#### Cutting a quad: `draw_quad` `0x35b04` (read, 2026-10-19)

`draw_quad(shape, ushift, level, uindex, vindex, P[4] (3D point pointers), S (4 screen
points, 8 bytes each), near)`, Watcom registers then the stack; `0x35a98` is its helper
that packs the four pointers and screen points of a child.
- Nothing when near > P2.z and near > P1.z.
- **Cut into four when P3.z < `0xeaae8[level]` >> `0xf6aa4` and level < `0xeaad8`**,
  else painted whole: `0x3521c(shape, ushift, level, uindex, vindex, S)` gives the corners'
  picture positions: w = width >> ushift, h = height >> level, u0 = ((1 << ushift) − 1 −
  uindex) · w (mirrored), v0 = ((1 << level) − 1 − vindex) · h, u1 = u0 + w − 1, v1 = v0 +
  h − 1, corners (u0,v0) (u1,v0) (u1,v1) (u0,v1) in 16.16; painted only if w > 1 and
  h > 1, through `[0x12c0dc]`.
- The cut: midpoints m01, m23, the centre c = mid(m01, m23), m12 (each coordinate
  `(a + b) / 2` toward zero), from the unclipped points. A midpoint nearer than `near` is
  moved for its projection only: t = (near − z) / `0x143e5c` in 16.16 (two divisions:
  whole, then the remainder << 16), x += t · `0x143e54`, y += t · `0x143e58` (rounded
  multiplies), z = near. The children (ushift + 1, level + 1): (m01, P1, m12, c) at
  (2u + 1, 2v + 1), (c, m12, P2, m23) at (2u + 1, 2v); then, **unless m23.z < near and
  m01.z < near**, m30 and (P0, m01, c, m30) at (2u, 2v + 1), (m30, c, m23, P3) at (2u, 2v).
- **The view's own cutter `0x35348`** (same arguments; `0x352dc` its helper) is the one
  for the road ahead (`0x5a4f0`); `draw_quad` draws only the mirror (`draw_road_sections`
  `0x5aa84`). Differences: nothing when near > P3.z and near > P0.z; cut when `0xeaadc[level]`
  >> `0xf6aa4` > P1.z **or** > P2.z (and level < `0xeaad8`); the midpoints m01, m23, m03,
  c; the children (P0, m01, c, m03) at (2u + 1, 2v + 1), (m03, c, m23, P3) at (2u + 1, 2v),
  then unless m23.z < near and m01.z < near, m12 and (m01, P1, m12, c) at (2u, 2v + 1),
  (c, m12, P2, m23) at (2u, 2v). A whole piece is painted only if `0x91cee(4, S)` & 7 = 0:
  the outcode of all corners together (1 left of x0, 2 at or right of x1, 4 above y0,
  8 at or below y1; **compared unsigned**, so x < 0 counts as right, and a piece below the
  window is still painted). `0xeaad8` = 2, `0xf6aa4` = 11, both tables 6.4, 4.8, 3.8.
- Each view has a record (camera `+0x88` the road ahead, `+0x8c` the mirror; `0x1486c(cam,
  mirror)` makes one current: `0x143dfc`): `+0` the window's width, `+0x14` its own
  reciprocal table (made current by `0x83ae0`, which changes only `0xf7a14`, not the focal
  `0xf7a30`), `+0x1c` → the window (x, y, w, h at +8.. +0x1c) whose centre `0x83c18`
  makes `0xf7a28` / `0xf7a2c`; the road ahead adds `0x129ec0` / `0x129ec4`; both << `0xf456c`.
- `port/src/poly.c` (projection, both cutters, the corners, the outcode in `raster.c`) is
  **checked by `tools/poly_emu.py`**: the original's cutters caught inside a real frame on
  a snapshot (every top-level call, the globals at that moment) and every piece they
  paint (picture, uv, xy) against the port's given the same quads: **equal on 663 pieces
  in 5 snapshots** (standing, and driving: 2026-10-19).
- **The road into the view: `0x59a74`** (once a frame, before the drawing). The near
  plane = `0xf6aa8` (62259) >> 11 = 30, kept in the view's `+4` (the `near` of the
  cutters). With t = (−camera position) >> 11 (camera `[0x143df4]` +0x10..0x18):
  `0x94db8(n, src, shift, t, dst)` dst = (src >> 11) + t for the `0x13bb78` road points
  `0x137434` → `0x142578`, and (if `0x1365d8` > 0) the `0x1365c0` other points `0x12f540` →
  `0x13d8f8`; `0x94e10(n, src, M, dst)` turns them by the view's matrix `[0x143df8]` (9
  ints) into `0x138c58` (and `0x131f70`) with `0x83a54(p, M, out)`: out.x = (x·M0 + y·M3 +
  z·M6) >> 16, out.y with M1 M4 M7, out.z M2 M5 M8, each sum in 64 bits, no rounding.
  EA's `memmove` `0x8ecc4` takes **(src, dst, n)**: the turned points copied back to
  `0x142578`. Then `0x587b8(node / 4)`: the near plane's direction = point k − point 0 of
  `0x138c58`, k = that section's points across (`0x136624 + 6·sec`, a word); |z| < 0x1999
  → z = 0x1999. Then of `0x142578`: the first n − 11·(`0x136618` − 3) points held to |x| ≤
  `0xf6ab0` (5376, a quarter when z < 2·near) and |y| ≤ `0xf6ab4` (5376) and moved onto
  the near plane, the rest onto 8 × near. Projected by `0x94c80(n, src, dst, 16)` (=
  `0x83b5c` with the starting shift an argument) into `0x13a47c` (and `0x131f70` →
  `0x1349a0`). A section's record `0x136620 + 6·sec`: +0 its kind (byte), +4 its points.
  **Ported (`port/src/road.c`), equal to the original on all points of 5 snapshots
  (`tools/road_emu.py`).**
- **The frame's points: `0x50070(camera)`** keeps the ring round the camera's section
  (`0x4ee30`) and, **only when the section changed** (`0x136610`, the node gathered at, /4
  ≠ the camera's, or < 0; `0x5004c` sets it to −1 to force it), `0x4ffbc(node, node/4,
  0x136618)`. (Then the cars' part, `[cam+0x3c]`, `0x4fed8`: not read yet.) `/4` is C's,
  towards 0.
  `0x4ffbc(node, sec, drawn)`: `0x13660c` = base = node/4, `0x136610` = node, `0x13bb74`
  (filled) = `0x13bb78` (count) = 0; `0x59998(base & mask)`; then `0x4f7d4(base, sec++, 4,
  1)` drawn − 1 times, `(base, sec, 4, 0)`, `(base, sec + 1, 1, 0)` (the far edge: one row).
  `0x59998(s)`: the first placement (16 bytes each, `[0x13bb94]`, `0x1365dc` of them) at
  node 4s by halves (lo 0, hi n−1; t > node → lo), then back over the run of equal nodes
  **and one more** (the first test always matches), then on while t > node and < n−1:
  `0x1365cc`, `0x1365d0` = next, `0x1365d4` = its node; `0x1365c4` (filled) = `0x1365d8`
  (objects) = `0x1365c0` (points) = 0.
  `0x4f7d4(base, sec, rows, objects)`: the detail kind = int `0xed284[node byte +7][sec −
  base]` (8 rows of 32: by the node's type and how far ahead), its record `0xf61bc +
  0x4c·kind`; at +0x18 a list for each row of the point numbers (0..10) kept, rising,
  0xff after each row. The kept points of the ring's section (`+12 + 0x84·row + 12·i`) are
  appended to `0x137434`. With `objects`, while the row's node (masked) = `0x1365d4`: the
  placement's corners into `0x12f540`, `0x1365d8`++, next placement (at the end: 0, and on
  a circuit its node unless it is this row's, else −1). A section's record `0x136620 +
  6·(sec & mask)`: kind byte, word first point, word points; `0x13bb78` += points.
  A placement: +0 node, +4 kind, +5 angle (256 a turn, negated), +0xa/+0xc/+0xe i16 offset
  << 8 from the node's position. Its kind (`[0x13bb84]` + 16·kind): +1 type (1: one point;
  6: a board and a side, 6 points; else a board, 4), +4 width, +8 depth (type 6), +0xc
  height. Angle a = (−angle << 16) − (node i16 +0x18 << 10), sin/cos of a >> 14 (the
  table `0xf8448`, 1024 a turn); c = (width/2 >> 8)·(cos >> 8), s likewise. Standing: (x−c,
  y+h, z−s), (x+c, y+h, z+s), (x+c, y, z+s), (x−c, y, z−s). **Knocked over** (byte
  `[0xea5d8][2·placement]` ≠ 0; the array, 2 bytes a placement, made at `0x1c630` from the
  head's `+0x16218`, cleared by the replay's `0x5f138`/`0x5f18c`/`0x5f1e0`): height 0, the
  top two moved by (cos·h, sin·h) of b + 0x400000 − (the row node's angle), b = −angle <<
  16 (+0x800000 if < −0x400000). Type 6 adds Q + (cos, sin)(a + 0x400000)·depth, at y + h
  and y, Q the third corner. **Ported (`road_gather`, `port/src/road.c`), equal on 6
  snapshots, standing and with every other object knocked over (`tools/gather_emu.py
  [--fallen]`).**
- **The game's fixed tables come from the player's `NFS_DOS.EXE`** (`port/src/exe.c`: the
  LE image laid out and relocated as `unle.py --fixups`, checked by an FNV-1a of the
  0x169a10 bytes = `0x76078ed2`, identical to `build/nfs_flat.bin`): the port holds none
  of the game's data, and reads `exe_i32(0xed284 + ..)` at the original's addresses.
- Projection `0x83b5c(p, out)`: z ≤ `0xf7938` → (0x80000000, 0x80000000) and `0xf7a40`
  |= 1. Else z normalised below 0x400 with a shift (16; ≥ 0x10000: 24 and z >> 8; again
  ≥ 0x10000: +8, >> 8; ≥ 0x1000: +4, >> 4; ≥ 0x800: +2, >> 2; ≥ 0x400: +1, >> 1), r =
  `[0xf7a14]`[z]; x' = `0xf7a28` + (x · r >> shift, rounded by the last bit out), y' =
  `0xf7a2c` − (y · r >> shift). The shift is taken mod 32 by the CPU (z ≥ 2^24: no shift).
  The table (`0x83af4`): r[i] = (`0xf7a30` << 16) / i, i < 10 as 10; 0x400 entries.

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
- **A snapshot to run the original's code on** (2026-10-19): `snap:<f>` in a plan pauses
  DOSBox and writes 32 MB of the game's memory by its own addresses (`<f>.json`: the
  data object's offset). `tools/snap_emu.py` loads it in Unicorn and calls any routine by
  its flat address. **Each object was loaded at its own place**: the code object at flat
  + `0x1e9000`, object 2 + `0xdd000`, object 3 + `0xce000`, the data object and its BSS +
  `0x1c4000` (from the 27,294 relocated pointers, all agreeing; snapshot `r1a`). Calling
  `race_draw_frame` `0x5bc40` on a race snapshot runs (1.4 million instructions, no
  interrupts, no ports) and leaves the whole 320x200 frame (cockpit, mirror, map) at the
  game's `0xa0000`; the 3D view is drawn first into a 320x127 window at `[0xf88c8]`.
  The palette is the card's, not in memory.
- **Driving the original under XTEST**: the gearbox starts in neutral even when automatic:
  `a` (gear up) first, then the keypad's arrows (`KP_8` throttle, `KP_4` / `KP_6` steer);
  `down:<key>` / `up:<key>` keep a key held across other steps. A snapshot is taken
  whenever DOSBox is paused: it may be mid-frame (in `d1` the mirror's globals were
  current), so tests catch the globals at each call.
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
