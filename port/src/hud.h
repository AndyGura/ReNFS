/* The race's dashboard and HUD at 320x200 (NOTES §4.11): the dashboard's picture, the
   status bar on top, the map's box, and what changes each frame over them. */
#ifndef RENFS_HUD_H
#define RENFS_HUD_H

#include "fsh.h"

/* an off-screen picture the game draws into, a window of its own (0xf4578 the view,
   0xf4580 the map's background, 0xf4584 the map) */
typedef struct {
    int w, h;
    unsigned char *pix;        /* w a row */
} hud_picture;

/* what the status bar shows (the race's state) */
typedef struct {
    int shown;                 /* 0x12b940 */
    int steps;                 /* 0x145c94: the race's steps; the bar from 360 on */
    int mode;                  /* 0x12b8cc: 0 a segment, 1 the race, 2 the acceleration
                                  test */
    int table;                 /* 0x12dd24: the race length's table of times */
    int track, segment;        /* 0x12da12, 0x12da0e */
    int circuit;               /* 0x13bb80: laps, not miles */
    int ahead;                 /* 0x14ea6c: the split, >= 0 ahead (plus) */
    int splits;                /* 0x14ea70: splits shown */
    int best_lap;              /* 0x13c20c */
    int race_type;             /* 0x12da20 */
} hud_status;

extern hud_status g_hud;
extern fsh_file g_mask;              /* the HUD's pictures (0x145604) */
extern hud_picture g_hud_screen;     /* the screen (0xa0000 at 320x200) */
extern hud_picture g_hud_map_back;   /* 0xf4580 */

/* the HUD's pictures (load_cockpit_mask 0x5f7c8: SIMDATA/MISC/MASKLO.FSH, MASKLOG.FSH
   in German), their colours queued for re-colouring with the program's 13 at 0xf0 */
int hud_load(void);
void hud_free(void);
/* the whole dashboard drawn again, as a view starts (0x30094): the dashboard's picture,
   the map's dim background taken from it, the status bar */
void hud_redraw(void);

#endif
