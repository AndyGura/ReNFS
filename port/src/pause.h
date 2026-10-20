/* The race held (NOTES §4.20): Esc opens the pause menu, P holds the race until it is
   pressed again (race_check_pause 0x63304, the timed calls 0x1fa1c, 0x63110). */
#ifndef RENFS_PAUSE_H
#define RENFS_PAUSE_H

#include "view.h"

typedef struct {
    int pending;               /* 0x145c7c: a pause asked for, not yet over */
    int last;                  /* 0x145c80: the step counter when one was last asked */
    int at[2];                 /* 0x129fb0: the step each call is due (0 none): the menu,
                                  P's hold */
    unsigned long held_ticks;  /* the clock's ticks spent held, for race.c to take off
                                  its own clock (the original's stops, 0x12f1b8) */
    int quit;                  /* the race left from the menu (0x13c17c = 2) */
    int track, segment;        /* CONFIG.DAT +0x95 and +0x93: the menu's map */
} race_pause;

extern race_pause g_pause;

/* race_step's start (0x6348c): the calls due at step `done` (0x1fa1c), then the keys
   Esc and P (0x63304); 0 the race goes on */
void race_pause_step(race_camera *cam, int done);

#endif
