/* Cars meeting cars (NOTES §4.14): every step after the driving, each car against the
   next ones along the road (cars_meets_world 0x1c878's loop). Two cars close enough are
   taken as boxes (their bodies, crash.c): pushed apart until they no longer overlap
   (0x2b864, the box test 0x26324), given the push of the hit at the point they touched
   (0x2afb8), and then driven on, or, after a hard hit, both thrown about (kind 4). */
#ifndef RENFS_COLLIDE_H
#define RENFS_COLLIDE_H

#include "car.h"

typedef struct {
    unsigned random[6];        /* 0xf8a8c: EA's random numbers' state (0x91e04) */
    int impulse;               /* 0xea668: the last hit between two cars */
} collide_race;

extern collide_race g_collide;

/* the random numbers' state as the program starts (the image's) */
void collide_init(void);
/* 0x1c878's loop: the police's catch (0x1c800), each car against those just ahead */
void collide_world(void);

#endif
