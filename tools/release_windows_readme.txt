ReNFS @VERSION@ for Windows (64-bit)
=====================================

The Need for Speed Special Edition (Electronic Arts Canada, 1996, MS-DOS),
written again by reading the original program. The whole single-player game:
every car and track, head to head, single race, time trial, tournaments, the
rally, police and traffic, replays, the cars' showcase, records, saved
tournaments, the original's codes, music and speech, English and German;
races at 640x480 or 320x200, smoothly at 60 frames a second. Not in it: the
network and modem race.

Beta. Please report problems at https://github.com/ItsDanik/ReNFS/issues

YOU NEED THE GAME'S CD
  ReNFS contains no part of the game. Copy everything on your Need for Speed
  SE CD (or in an image of it) into a folder named "NFSSE" next to
  renfs.exe:

      renfs.exe
      NFSSE/NFS_DOS.EXE
      NFSSE/FRONTEND/...
      NFSSE/GAMEDATA/...
      NFSSE/SIMDATA/...

  (or anywhere, and start the program with  -data <that folder>). Upper or
  lower case does not matter; nothing has to be installed or unpacked.

RUN
  Double-click renfs.exe. Windows 10 or 11, 64-bit. If Windows warns about
  an unknown program: More info -> Run anyway.
  For the German game make a shortcut to renfs.exe and add  -lang de  after
  it in the shortcut's Target.

KEYS
  Menus:  the mouse, or the arrows, ENTER and ESC.
  Alt+Enter: window or the whole screen.
  Driving (Options -> Controls changes them):
          LEFT / RIGHT steer, UP / DOWN accelerate / brake,
          A / Z gear up / down (the automatic starts in neutral: one up),
          SPACE handbrake, H horn, C next camera, M map.
  Race:   ESC pause menu, P pause, F1 view size, F2 detail, F3 road distance,
          F4 sky, F5 sound, F6 markers, F7 / F8 the top bar, F9 bumper view,
          F11 smooth motion on / off, Shift+End give up,
          Ctrl+Alt+1..8 watch another car.
  Game pad: left stick steers, right / left trigger accelerate / brake,
          shoulders gear up / down, A horn, B map, X camera, Y handbrake,
          START = ESC. Calibrated the first time. For analogue steering and
          pedals choose Joystick in Options -> Controls, as in the original.
          It rumbles; a force-feedback wheel gets the original's effects.

FILES IT KEEPS
  %APPDATA%\renfs\   the game's settings and records, saved tournaments,
                     replays, the joystick's calibration, and renfs.cfg:
                       smooth 1           smooth motion (F11); 0 the original's
                       fullscreen 0       1 the whole screen (Alt+Enter)
                       forcefeedback 100  rumble's strength, 0..100
  (or -home <folder>)

LICENCE
  GPL-3.0 (LICENSE.txt). SDL2.dll: zlib licence (LICENSE-SDL2.txt).
  Source: https://github.com/ItsDanik/ReNFS
  The Need for Speed is a trademark of Electronic Arts; ReNFS is not connected
  with Electronic Arts.
