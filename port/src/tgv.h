/* EA's TGV movies (FRONTEND/MOVIE/<name>.TGV; NOTES §3.14). */
#ifndef RENFS_TGV_H
#define RENFS_TGV_H

#include "eacs.h"

typedef struct {
    unsigned char *d;
    long len, pos, sound_pos;      /* the pictures' and the sound's read positions */
    int w, h;
    unsigned char *cur, *prev;     /* the pictures: palette numbers */
    unsigned char pal[768];        /* 8 bits a channel */
    int new_palette;               /* set by a key frame */
    eacs sound;
    int has_sound;
} tgv;

int tgv_open(tgv *t, const char *rel);
void tgv_close(tgv *t);
/* the next picture, decoded into t->cur; 0 at the end */
int tgv_next_frame(tgv *t);
/* the next block of sound into out (room for max frames, 16-bit): the frames; -1 at the
   end. The sound is read on its own, ahead of the pictures, as the game's stream is */
int tgv_next_sound(tgv *t, short *out, int max);

#endif
