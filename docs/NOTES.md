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

### 3.2 `wwww` containers — layout seen in the data, the reader not yet found

"wwww", u32 count, `count` u32 offsets from the container's start (padded: the first
offset of a 4-entry container is 0x18). Entries nest.
- `.CFM` (a car): 4 entries, `ORIP` (3D model, high detail) + its `SHPI` textures, then a
  second `ORIP` + `SHPI` (low detail).
- `.FAM` (a track family, `<track>_M01` ..): [0] a `wwww` of 17 `SHPI` texture banks,
  [1] a `wwww` of 32 `SHPI` (object sprites?), [2] one `SHPI` (sky / horizon?),
  [3] a `wwww` of `wwww`s of (`ORIP`, `SHPI`) = 3D objects.
- `.FMM` (a dashboard, `<car>DH` / `DL` = 640 / 320): 4 `BAMF`. `.MMB`: 12 `BAMF`.

### 3.3 TRI (track) — loader `0x4f010`, called from `0x5ba1c`

The loader builds `<path><name>.tri` (`"%s%s.tri"` at `0xe2b90`) and reads it in several
pieces (`0x4e880`) into buffers from `0x828a8` (`getmemblock`-like). Header: u32 `0x11`;
the u16 at `+4` is 0 for the point-to-point tracks (AL, CL, CY: an open road) and the
loop length / 4 for the circuits (TR1 0x80, TR2 0x100); `+6` u16 (AL1 0x208, CL1 0x1E0).
Read in the loader: `+0x18` and `+0x20` (u16 into a race record), `+0x15b0c` (3 bytes a
road node: scaled by a factor from a table at `0xed12c` / `0xed1d4` picked by two
settings: colours or lighting per node?), `+0x16214` (a count × 16 bytes, then a list of
16-byte records ending in -1). AL22.TRI is the same as AL2 with 12 bytes more (the
other direction? to check).
Per track a text file `<track>INFO`: horizon / sky ring settings (`#ring y offset`,
`#colour top` / `bottom`, `#RGB TIME OF DAY VALUES`).

### 3.4 Still to read

`ORIP` models, `BAMF`, `30FB`-headed
`.PBS` / `.PDN` (RefPack with flag bits `0x20`?), `.BNK` (EA sound bank), `EACS` / `1SNh`
(EA ADPCM: `.EAS` speech, `.ASF` / `.AS4` music), `kVGT` (TGV video, 320x192?), `FNTF`
fonts, `.INV` (32 KB), `BAMF` (`MASKHI.B00` ..), `.RPL`, `CONFIG.DAT` (player name, replay
names).

## 4. Map of the program (first pass)

`python3 -I tools/calltree.py build/decomp/INDEX.txt <addr> <depth> tools/names.txt`.

- `0x47000` (called by `main`): `0x34284` (video mode switch: `initgraphics` between
  320x200 and 640x480 through `0x341a4`), `0x39c40` / `0x3a134` (loading), **`0x42c30`**
  (17 KB: the front end's state machine, it calls nearly everything: `0x3770c` the race?,
  `0x17724`, `0x195d8`, `0x53a30`, `0x6f070`, `0x76380`), `0x47274` (2.2 KB), `0x550a0`
  (network?).
- The library block `0x71208`..`0x74dcc` (many small routines called from every screen):
  probably the front end's widgets (pictures, text, buttons).
