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
  per clock tick**, then one frame drawn: the physics' rate is the clock's (33.3 a second
  unless changed; to confirm in DOSBox). In the modes `0x15d134` = 1..3 (network, replay?)
  the steps due come from elsewhere (`0x12dd18`, `0xf6d68`).
- `race_step` `0x6348c`: the controls (`0x71cec` or `0x83908`), `0x1fa1c`, `0x63304`,
  `0x47c88` (1.5 KB: the race's rules? reads the clock against 100 / 400), then
  `0x519d8` (1.6 KB: every car? calls `0x136b0` and the sub-steps) with the flags
  `0x12f1bc` = 1 / 4 / 0x10 on every 2nd / 4th / 8th step (`0x145c94` counts in twos).
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
  (`findmemblock` + `purgememblock`), `free` `0xa5ff4` (Watcom's), `memmove` `0x8ecc4`
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
  fourth step through `0x13c1e0` (1 = `car_sound_player` `0x10400`, 2 = nothing):

  | kind | routine | what | set by |
  |---|---|---|---|
  | 1 | `drive_player_wait` `0x7ea20` | counts down, then kind 2 | `0x79728`, `0x7e64c` |
  | 2 | `drive_player` `0x382d0` → `car_physics_player` `0x408e0` (4.4 KB) | the player | `load_car_pbs` |
  | 3 | `drive_opponent` `0x7b34c` (3 KB) | a computer's car | `load_car_pbs`, `0x51200`, kinds 5, 6 |
  | 4 | `drive_crashed` `0x1cfe0` | after a crash? | `0x1ca9c`, `0x1cb14`, `0x1e314` (collisions) |
  | 5 | `drive_kind5` `0x7d9e4` | slows by `0x13333` a step (`0x23333` with flag 4), runs the opponent's routine | `0x1cd40` |
  | 6 | `drive_opponent_wait` `0x7e0c8` | held at (8, 200 + 10n, 8), counts down, then kind 3 | `0x7ccc0` |

  After its routine, every car goes through `car_road_offset` `0x51598` (its place
  across the road: the road node's position `0x137430 + node × 0x24`, turned by the node's
  angle).
- `car_physics_player` `0x408e0`: `0x3ea98` / `0x3e9c4` every fourth step (the gearbox?),
  `0x3ed68` (the engine: reads `Carspecs` through `0x18a04`), `0x3f260` twice (front 0 and
  rear 1, from `+0x60` / `+0x68`), `0x3f7b8` twice (the axles' forces?), `0x4071c`,
  `0x40230`, `0x4008c`, `replay_mark_highlight` `0x5c6d4` (events 0x28, 0x57, 0x78),
  `race_stats_max` `0x2d71c` (the race's greatest values, capped at 1200).
- `replay_mark_highlight` `0x5c6d4`: for each slot of 60 half-steps (`0x145c94` counts 2
  a step) keeps the greatest event and its time (`0x144da8`): the replay's highlights
  ("Load highlight ?"). 60 half-steps = 1 s at 30 steps a second: a hint that the game was
  made for 30 (race clock setting 0) while `race_setup_record` sets 1 (33.3). To measure.
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

### 4.8 Still to name

The opponents' and traffic routines under `drive_opponent` (`0x7cf98` 2.6 KB, `0x7b178`,
`0x7dbd8`, `0x77c..`), the collisions (`0x1e314`, `0x2afb8`, `0x2a918`), the police
(`0x30424` shows the tickets; the chase itself?), `0x47c88` (the race's rules), `0x1fa1c`
/ `0x63304` in `race_step`, the second drawing pass `0x5a4f0`, the widget block
`0x71208`..`0x74dcc`, Watcom's runtime.
