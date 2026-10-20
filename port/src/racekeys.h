/* The keys of a race beside the driving (race_keys 0x47c88, once a race step before the
   cars, NOTES §4.19): the view key (C) changes the camera, Alt+Ctrl with 1 .. 8 watches
   another car, F1 the outside view's size, F2 its pixels doubled, F3 how far the road is
   drawn, F4 the sky's picture, F5 the sounds, F6 the racers' markers; Shift+End gives the race
   up. */
#ifndef RENFS_RACEKEYS_H
#define RENFS_RACEKEYS_H

#include "view.h"

typedef struct {
    int view_key;              /* 0x12d94c: the view key's scan code (the settings' +0x5f4a) */
    int view_up;               /* 0xec5c8: the view key let go since its last change */
    int fkey_up;               /* 0xec5cc: no F key held at the last step (cleared by one) */
    int window;                /* 0x12d9d9: F2's detail, 0 .. 3 (2 and 3 at 640x480 only) */
    int road;                  /* 0x12d9d5: F3's, the road drawn 28, 14 or 9 sections */
    int plain_sky;             /* 0x12d9d6: F4: the sky's colours only */
    int muted;                 /* 0xec528: F5 */
    int drawn;                 /* 0x136618: the road's sections drawn ahead */
    int drawn_next;            /* 0xf6b48: .. from the next frame on (-1 none) */
    int race_clock;            /* 0xed0f8: the race clock's steps (set by race.c) */
} race_keys_state;

extern race_keys_state g_rkeys;

/* the race's start (0x37228, 0x5ba1c): the settings' detail (CONFIG.DAT +0x81 the
   outside view's size, +0x83 the pixels, +0x85 the road, +0x89 the sky) and the view
   key */
void race_keys_start(void);
/* the pause menu's (0x56b48, 0x5818d): the detail as the keys left it into the settings
   (+0x81 .. +0x89) for the graphics screen, and back into the race with the views set up
   again */
void race_keys_to_config(void);
void race_keys_from_config(void);
/* race_keys 0x47c88, once a race step */
void race_keys(race_camera *cam);
/* the frame's start (race_draw_frame 0x5bcf5): the road drawn as far as F3 asked; 1 if
   it changed (its points to be gathered again) */
int race_keys_frame(void);

#endif
