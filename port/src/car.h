/* The race's cars (NOTES §4.6): what of a car record the port knows so far, and the
   starting grid. */
#ifndef RENFS_CAR_H
#define RENFS_CAR_H

#include "poly.h"
#include "model.h"

typedef struct {
    vec3 pos;                  /* +0: in the world, 16.16 (y up) */
    int pitch, yaw, roll;      /* +0xc, +0x10, +0x14: 0x1000000 a turn */
    int node;                  /* +0x48: the road node it is on */
    int body_roll;             /* +0x365: the body's lean, the driver's view rolls with it */
    int body_pitch;            /* +0x369: the same nodding */
    int dist;                  /* +0x39d */
    unsigned char roll_phase;  /* +0x427: where in the shake wave (0xec28c) */
    unsigned char pitch_phase; /* +0x428 */
    int roll_shake;            /* +0x429: how much, 16.16 */
    int pitch_shake;           /* +0x42d */
    int player;                /* +0x47d: its place in the race's tables */
    int number;                /* +0x48a: which car (0x12da53 ..; the 512TR is 4) */
    int police;                /* +0x174 & 8 */
    unsigned char *cfm;        /* its CFM file (+0x792: the container) */
    long cfm_len;
    unsigned char *colours;    /* +0x526: its body's colours ("!xxx"), 6 bits, tinted */
    unsigned char *pics[2];    /* +0x51e, +0x656: its pictures near and far (entries 1, 3) */
    const unsigned char *pics_pal[2]; /* +0x522, +0x65a: their palettes ("!PAL"), tinted */

    /* what drawing it needs (cardraw.c) */
    int state;                 /* +0x512: 6 off the road (not drawn) */
    int slot;                  /* +0x516: its place in the race's tables */
    int draw_node;             /* +0x341: the node it is drawn at (0x51fa3, 0x678a4) */
    int own_turn;              /* +0x20: turned by its own matrix (the driven cars) */
    int turn[9];               /* +0x24 */
    int yaw_drawn;             /* +0x351: its heading as drawn */
    int half_len, half_wid;    /* +0x78, +0x7c: half its size, 16.16 */
    int braking;               /* +0x3b9: above 10 its brake lights are on */
    unsigned char bits[3];     /* +0x174 .. +0x176 */
    vec3 seen;                 /* +0x391: from the camera, in the view, 1/128 units */
    int far;                   /* +0x78e: drawn by its far model */
    vec3 shadow, along, across;/* +0x359, +0x36d, +0x379: its shadow on the road */
    model near_m, far_m;       /* +0x52e, +0x666: its two models, drawable */
    struct {
        int index, count;      /* +0x5be ..: inside, surface, outside, convex */
        unsigned *list;
    } groups[4];
} race_car;

/* the car's place on the starting grid (0x7c938): grid its slot (0x12dca0[player]),
   racers how many race (0x12da2c), kind the race's (0x12da12: > 2 a circuit) */
void car_grid_place(race_car *c, int grid, int racers, int kind);

/* the car's file (load_car_cfm 0x65874), by its id (0x12dce4[player]): < 100 the
   traffic's names (0xf6e70), 100..199 the racers' with an M before (0xf6ca0), 200..399
   the racers' (0xf6b10), 400 and up the police's CopMust; the number from the racers'.
   Its pictures' palettes tinted as they load (racepal.h). 0 if missing */
int car_load(race_car *c, int id);
void car_free(race_car *c);
/* every car's colours as pieces of the race's palette (0x64e14, one player): at
   0xf6ea0[i], 0xf6ec8[i] of them, the police's none; a mark (0xf6ef0[i], 0x47 and 0x5f
   for the two racers) in the player's car's colour (0xf6f5c + 3 · its number) kept in
   marks[i] (0x14e5bc); and four of the program's at 0xfa (0xf6de0) */
void cars_queue_colours(race_car *const *cars, int n, unsigned char *marks);
/* every car's pictures turned into the race's colours (0x65020, after race_colours): car
   i's mark marks[i] kept in them, and car 2's lights in a chase with the police (0x12dd08) */
void cars_remap(race_car *const *cars, int n, const unsigned char *marks, int chase);

/* the width of one of n lanes of a road `half` wide (the node's +1 / 8), 8.8; the
   original's table 0x15d15c (made by load_traffic_cfg 0x7de84) */
int lane_width(int n, int half);

#endif
