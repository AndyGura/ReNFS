# ReNFS

A hand-written reimplementation of *The Need for Speed Special Edition* (1996, MS-DOS) for
Linux, and later for the MiSTer. You need the game's CD: the port reads its files as they
are and contains none of them.

## Building and running

```sh
cmake -S port -B port/build && cmake --build port/build
port/build/renfs -data <the CD's folder>
```

Settings, records, saved tournaments, replays and the joystick's calibration go in
`~/.config/renfs` (or `-home <folder>`), never in the game's folder. `renfs --help`
lists the options.

## Controls

The keyboard as in the original; every driving key can be changed in Options → Controls.

**A game pad or a joystick** is found when the game starts. The first time, the
original's calibration screen appears: move the stick round its corners, let it go and
press a button (Esc skips it). Options → Controls → Calibrate joystick does it again.

On a pad:

| | |
|---|---|
| Left stick (or the d-pad) | steering; the menus' arrows |
| Right trigger / left trigger | gas / brake |
| Right / left shoulder | gear up / down (the automatic starts in neutral: one gear up) |
| A | horn; Space in the menus |
| B | map; Enter in the menus |
| X | camera |
| Y | handbrake |
| Start | Esc: the pause menu, back in the menus |

**For analogue steering, gas and brake, choose Joystick in Options → Controls**: Steering
"Joystick Left/Right" and Accelerate/brake "Joystick Up/Down". This is how the original
works. With the controls left on Keyboard the pad still drives, but all or nothing, like
keys. With Joystick chosen, the keyboard no longer drives those controls. Shift can be
set to "Joystick buttons" too (A gear up, B gear down; then A and B no longer give the
horn and the map); the shoulders shift either way.

The mouse can steer as well (Controls: Mouse): move it left and right; its buttons give
gas and brake.
