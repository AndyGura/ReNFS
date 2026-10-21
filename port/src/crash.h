/* The crash (NOTES §4.6): a hard hit on a wall (0x3fed4) throws the car about as a box,
   its body (0x78204, car_crash 0x1cb14); each step drive_crashed 0x1cfe0 moves it and
   bounces it off the walls and the road, until it lands on its wheels (0x1ceb4) or, its
   wait over, is put back in the road's first lane. And cars_meets_world 0x1c878: every
   fourth step the cars' places across the road and speeds along it (0x786d4) and the
   computer's driving chosen (traffic_police_tick 0x7a040, opponent.c). */
#ifndef RENFS_CRASH_H
#define RENFS_CRASH_H

#include "car.h"
#include "drive.h"

#define SMOKE_PUFFS 100

/* the smoke a tumbling player's car leaves (0x1464d0; drawn with the cars, smoke.c). Its
   layout is the original's */
typedef struct {
    vec3 pos[SMOKE_PUFFS];     /* +0 */
    int age[SMOKE_PUFFS];      /* +0x4b0: 0 .. 0x10000, -1 none */
    int order[SMOKE_PUFFS];    /* +0x640: the puffs as drawn, far first (0..99 at the
                                  start, sorted again each time) */
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

/* the crash's maths, the camera's too: a / b in 16.16 by two divisions (0 for b = 0,
   where the original stops); the angle of (x, y), 0x1000000 a turn (0x3e7ec); the
   square root of n by Newton's steps from guess (0x3e784) */
int fix_div(int a, int b);
int angle_of(int x, int y);
int fix_sqrt(int n, int iters, int guess);

/* the race starts: the refresh due at the first step (0x1ee58), the smoke cleared */
void crash_start(void);
/* the body from the car's PDN (0x2c920 and car_apply_pdn 0x1e9cc): its weight, size and
   turn's resistance; the car's half height and reach */
void crash_body_setup(race_car *c, const unsigned char *pdn);
/* 0x512e8's other half: a car whose PDN gives no size (the traffic's) sized by its near
   model (+0x64e/+0x652/+0x64a, car_extents): its length and width, the body's halves,
   its reach, height and radius */
void crash_body_from_model(race_car *c);
/* car_hard_hit 0x78204: the crash begun, the hit's push a turn of the body: a about its
   axis up, b along, s across */
void crash_hard_hit(race_car *c, int a, int b, int s);
/* 0x78370: the same, then knocked over (0x1e838): EXPLODE's horn */
void crash_push(race_car *c, int a, int b, int s);
/* drive_crashed 0x1cfe0: a step of the tumbling car (state 4) */
void crash_step(race_car *c);
/* cars_meets_world 0x1c878: the hit's sound's timers, every fourth step
   traffic_police_tick (opponent_tick), then the cars against each other (collide.c).
   (The roadside objects 0x1ebf4 come later) */
void crash_world(void);
/* traffic_police_tick's refresh 0x786d4 of a car: its place across the road and how far
   its body reaches across it, its target speed (the computer's, opponent.c), its speed
   along the road */
void crash_refresh(race_car *c);
/* the road's axes at the car's node (0x7dbd8), the turn about y (0x2c7c4), out = a · b
   (0x2cf14): for opponent.c */
void crash_road_frame(race_car *c);
void crash_turn_y(int *m, int a);
void crash_mat_mul(int *out, const int *a, const int *b);
/* 0x1ca9c: a computer's car spins out (to kind 4 for 60 steps) */
void crash_spin_out(race_car *c);
/* for collide.c: the body from the car (0x1cbb0); the car driven by kind k again from its
   body (0x1cd40); the body's speed along the road (0x1e6cc); |v| (0x2c2d8); 1 / x on the
   x87 (0x3e7fc); v made a unit long (0x2c25c) */
void crash_body_from_car(race_car *c);
void crash_set_kind(race_car *c, int k);
int crash_body_road_speed(const race_car *c);
int crash_len3(const vec3 *v);
int crash_recip(int x);
void crash_normalize(vec3 *v);

/* 0x64588 on the player's smoke (the replay's jumps) */
void crash_smoke_reset(void);

#endif
