/* The smoke of a crash drawn (NOTES §4.10): its picture (load_smoke 0x645bc), the table that
   adds it to the screen (0x64488), and the puffs painted far to near as stretched pictures
   (0x64a74, EA's 0x96e40 and 0x96aa8). The puffs themselves are crash.c's. */
#ifndef RENFS_SMOKE_H
#define RENFS_SMOKE_H

#include "crash.h"
#include "fsh.h"

typedef struct {
    fsh_file file;             /* 0xf6dac: SIMDATA/MISC/SMOKE.QFS */
    shape puff;                /* 0xf6db0: "smok", 64 x 64 */
    unsigned char add[256][256]; /* 0x1464cc: a smoke colour (row) over a screen colour:
                                  three quarters of its red, green and blue added */
    shape dust[4];             /* 0x1464bc: "smo1".."smo4", the road's dust */
    shape dirt[4];             /* 0x1460ac: "dir1".."dir4", the other surfaces' */
} smoke_art;

extern smoke_art g_smoke;

/* load_smoke: the file, its palette tinted as the track's and queued with the pictures
   to re-colour; the puffs' order 0..99 */
int smoke_load(crash_smoke *s);
void smoke_free(void);
/* 0x64488, at the end of the re-colouring (0x55fa8): the table from the palette in the
   card (read back, 6 bits made 8, 0x80c80 the nearest of each sum) */
void smoke_table(void);
/* 0x64a74: the puffs alive and not too near, sorted far first (kept sorted in s->order),
   each the picture stretched by its distance and age at its place on the screen moved by
   (dx, dy); into the raster window (raster.h) */
void smoke_draw(crash_smoke *s, int dx, int dy);
/* 0x96e40: a picture stretched by scale (16.16; < 0 mirrored) with its centre at (x, y),
   each pixel through add[picture][screen] (0x9498d) */
void smoke_stretch(int scale, const shape *sh, int x, int y);
/* 0x973d0: the same with the picture's pixels put on the screen, 0xff see-through
   (0x97038, 0x9473e) */
void smoke_stretch_solid(int scale, const shape *sh, int x, int y);

#endif
