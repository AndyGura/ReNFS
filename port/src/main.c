/* ReNFS: The Need for Speed Special Edition (1996), written again from its DOS program.
   main 0x4716c -> startup 0x42054 -> fe_outer_loop 0x47000 (NOTES §4.1). */
#include "joystick.h"
#include "audio.h"
#include "collide.h"
#include "exe.h"
#include "fe.h"
#include "files.h"
#include "race.h"
#include "video.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char usage[] =
    "ReNFS: The Need for Speed SE, for Linux.\n"
    "\n"
    "    renfs [-data <folder>] [-home <folder>] [--shot <file.ppm> <frames>]\n"
    "          [--race <track>] [--replay <file.rpl> [highlights]]\n"
    "\n"
    "  -data <folder>   the game: the CD's files as they are (else $RENFS_DATA, else\n"
    "                   ./NFSSE, else NFSSE beside the program)\n"
    "  -home <folder>   the port's own settings (else $RENFS_HOME, else ~/.config/renfs)\n"
    "  --shot <f> <n>   no window: write the picture of retrace n (60 a second) to f\n"
    "  --race <track>   straight into a race on the track (cy1 .. al3, tr1 .. tr7,\n"
    "                   tr5 the rally) with the car, opponent and race type last\n"
    "                   chosen in Control Central; P holds it, Esc the pause menu\n"
    "  --replay <f>     a replay: the last race's (replay.rpl in the home folder) or the\n"
    "                   CD's (REPLAY1.RPL ..); with \"highlights\" only those\n"
    "\n"
    "  RENFS_LANG=de    the German art (GART, ...)\n"
    "  RENFS_NOSOUND=1  no sound\n"
    "  RENFS_SNDWAV=f   write what is heard to the file f (a WAV)\n"
    "  RENFS_THROW=s:x,y,z  a test: at race step s the car given that speed (m/s)\n"
    "  RENFS_SEED=n     a test: the race's random numbers from n (else the clock)\n"
    "  RENFS_PLACE=s:a,c,v  a test: at race step s the opponent put a m ahead of the\n"
    "                   car and c m to the side, at v m/s\n"
    "  RENFS_LAPS=n     n laps of a circuit (tr1 ..; else the race length's, 8 by default)\n"
    "  RENFS_VIEW=n     a test: the race watched from camera n (2..5, 8..12 outside)\n"
    "  RENFS_RES=n      a test: races at 640 (640x480) or 320 (320x200), whatever the\n"
    "                   graphics screen says\n"    "\n"
    "Keys: Space, Enter or Esc pass a movie or the title.\n"
    "\n"
    "A pad or joystick: calibrated the first time (or Options, Controls, Calibrate\n"
    "joystick). It drives even with the controls on Keyboard, but all or nothing; for\n"
    "analogue steering and pedals choose Joystick for Steering and Accelerate/brake in\n"
    "Options, Controls, as in the original. Pad: left stick steers, right trigger\n"
    "gas, left trigger brake, shoulders gear up / down, A horn, B map, X camera,\n"
    "Y handbrake, Start Esc.\n";

int main(int argc, char **argv)
{
    const char *data = 0, *home = 0, *shot = 0, *race = 0, *replay = 0;
    int shot_frames = 0, highlights = 0;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-data") && i + 1 < argc)
            data = argv[++i];
        else if (!strcmp(argv[i], "-home") && i + 1 < argc)
            home = argv[++i];
        else if (!strcmp(argv[i], "--shot") && i + 2 < argc) {
            shot = argv[++i];
            shot_frames = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--race") && i + 1 < argc)
            race = argv[++i];
        else if (!strcmp(argv[i], "--replay") && i + 1 < argc) {
            replay = argv[++i];
            if (i + 1 < argc && !strcmp(argv[i + 1], "highlights"))
                highlights = ++i;
        }
        else {
            fputs(usage, strcmp(argv[i], "--help") && strcmp(argv[i], "-h") ? stderr : stdout);
            return strcmp(argv[i], "--help") && strcmp(argv[i], "-h") ? 1 : 0;
        }
    }
    if (!files_init(data, home)) {
        fprintf(stderr, "renfs: the game's files were not found (FRONTEND/ART/TITLE.QFS).\n"
                        "Give the folder with the CD's files: renfs -data <folder>\n");
        return 1;
    }
    if (!exe_load())           /* the game's tables, the built-in settings among them */
        return 1;
    collide_init();            /* EA's random numbers as the program starts */
    if (!video_init(shot, shot_frames))
        return 1;

    if (!shot && !getenv("RENFS_NOSOUND"))
        audio_init();
    srand(shot ? 1 : (unsigned)time(0));   /* pictures without a window: the same each time */
    if (race || replay)
        joy_startup();         /* (fe_outer_loop's, for the tests straight into a race) */
    if (race)
        race_test(race);
    else if (replay)
        race_replay_test(replay, highlights ? 2 : 1);
    else
        fe_outer_loop();

    if (shot)                  /* the program ended before the frame asked for */
        fprintf(stderr, "renfs: ended before frame %d\n", shot_frames);
    audio_done();
    video_done();
    return 0;
}
