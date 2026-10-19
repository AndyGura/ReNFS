/* ReNFS: The Need for Speed Special Edition (1996), written again from its DOS program.
   main 0x4716c -> startup 0x42054 -> fe_outer_loop 0x47000 (NOTES §4.1). */
#include "audio.h"
#include "fe.h"
#include "files.h"
#include "video.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char usage[] =
    "ReNFS: The Need for Speed SE, for Linux.\n"
    "\n"
    "    renfs [-data <folder>] [-home <folder>] [--shot <file.ppm> <frames>]\n"
    "\n"
    "  -data <folder>   the game: the CD's files as they are (else $RENFS_DATA, else\n"
    "                   ./NFSSE, else NFSSE beside the program)\n"
    "  -home <folder>   the port's own settings (else $RENFS_HOME, else ~/.config/renfs)\n"
    "  --shot <f> <n>   no window: write the picture of retrace n (60 a second) to f\n"
    "\n"
    "  RENFS_LANG=de    the German art (GART, ...)\n"
    "  RENFS_NOSOUND=1  no sound\n"
    "  RENFS_SNDWAV=f   write what is heard to the file f (a WAV)\n"
    "\n"
    "Keys: Space, Enter or Esc pass a movie or the title.\n";

int main(int argc, char **argv)
{
    const char *data = 0, *home = 0, *shot = 0;
    int shot_frames = 0;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-data") && i + 1 < argc)
            data = argv[++i];
        else if (!strcmp(argv[i], "-home") && i + 1 < argc)
            home = argv[++i];
        else if (!strcmp(argv[i], "--shot") && i + 2 < argc) {
            shot = argv[++i];
            shot_frames = atoi(argv[++i]);
        } else {
            fputs(usage, strcmp(argv[i], "--help") && strcmp(argv[i], "-h") ? stderr : stdout);
            return strcmp(argv[i], "--help") && strcmp(argv[i], "-h") ? 1 : 0;
        }
    }
    if (!files_init(data, home)) {
        fprintf(stderr, "renfs: the game's files were not found (FRONTEND/ART/TITLE.QFS).\n"
                        "Give the folder with the CD's files: renfs -data <folder>\n");
        return 1;
    }
    if (!video_init(shot, shot_frames))
        return 1;

    if (!shot && !getenv("RENFS_NOSOUND"))
        audio_init();
    srand(shot ? 1 : (unsigned)time(0));   /* pictures without a window: the same each time */
    fe_outer_loop();

    if (shot)                  /* the program ended before the frame asked for */
        fprintf(stderr, "renfs: ended before frame %d\n", shot_frames);
    audio_done();
    video_done();
    return 0;
}
