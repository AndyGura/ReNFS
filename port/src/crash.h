/* The crash (NOTES §4.6): a hard hit on a wall (0x3fed4) throws the car about as a box,
   its body (0x78204, car_crash 0x1cb14); each step drive_crashed 0x1cfe0 moves it and
   bounces it off the walls and the road, until it lands on its wheels (0x1ceb4) or, its
   wait over, is put back in the road's first lane. And what of cars_meets_world 0x1c878
   the player's car needs: every fourth step its place across the road and its speed
   along it (traffic_police_tick 0x7a040 → 0x786d4). */
#ifndef RENFS_CRASH_H
#define RENFS_CRASH_H

#include "car.h"
#include "drive.h"

#define SMOKE_PUFFS 100

/* the smoke a tumbling player's car leaves (0x1464d0; drawn with the cars, 0x66d08: not
   in the port yet). Its layout is the original's */
typedef struct {
    vec3 pos[SMOKE_PUFFS];     /* +0 */
    int age[SMOKE_PUFFS];      /* +0x4b0: 0 .. 0x10000, -1 none */
    int spare[SMOKE_PUFFS];    /* +0x640: not used by the crash */
    int size[SMOKE_PUFFS];     /* +0x7d0: its drift sideways */
    int index;                 /* +0x960: the next puff's */
    int timer, next;           /* +0x964, +0x968: when the next is let out */
} crash_smoke;

typedef struct {
    int hit_wait;              /* 0xea5d0: steps before a hit is heard again */
    int hit_level;             /* 0xea5d4: how hard a hit must be to be heard */
    int impulse;               /* 0xea66c: the last contact's push */
    int depth;                 /* 0xea670: how far the body is from the last surface
                                  (below 0 into it) */
    vec3 point_vel;            /* 0x12b4c4: the touching corner's speed */
    int refresh;               /* 0x129fac: steps to the next refresh (each fourth) */
    int view;                  /* 0x145c98: 0x3c while a player's car tumbles (the
                                  camera's crash view, 0x6324c: not in the port yet) */
    int respawned;             /* 0xf6b70: a car was put back on the road */
    int restart;               /* 0xea700: 0x1e as a player's car is put back */
    unsigned random;           /* 0xf6dd8: the smoke's */
    int crashes[DRIVE_PLAYERS];/* the racers' results +0x1bc (0x14ec38 + 0x1d2 a player) */
    crash_smoke smoke;
} crash_race;

extern crash_race g_crash;

/* the race starts: the refresh due at the first step (0x1ee58), the smoke cleared */
void crash_start(void);
/* the body from the car's PDN (0x2c920 and car_apply_pdn 0x1e9cc): its weight, size and
   turn's resistance; the car's half height and reach */
void crash_body_setup(race_car *c, const unsigned char *pdn);
/* car_hard_hit 0x78204: the crash begun, the hit's push a turn of the body: a about its
   axis up, b along, s across */
void crash_hard_hit(race_car *c, int a, int b, int s);
/* drive_crashed 0x1cfe0: a step of the tumbling car (state 4) */
void crash_step(race_car *c);
/* cars_meets_world 0x1c878, as far as the port has it: the hit's sound's timers, and
   every fourth step the players' cars refreshed (their place across the road, how far
   they reach, their speed along it). (The police's steering 0x1c800, the roadside
   objects 0x1ebf4 and the cars against each other 0x1c6a4 come later) */
void crash_world(void);

#endif
