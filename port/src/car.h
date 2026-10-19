/* The race's cars (NOTES §4.6): what of a car record the port knows so far, and the
   starting grid. */
#ifndef RENFS_CAR_H
#define RENFS_CAR_H

#include "poly.h"

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
} race_car;

/* the car's place on the starting grid (0x7c938): grid its slot (0x12dca0[player]),
   racers how many race (0x12da2c), kind the race's (0x12da12: > 2 a circuit) */
void car_grid_place(race_car *c, int grid, int racers, int kind);

/* the width of one of n lanes of a road `half` wide (the node's +1 / 8), 8.8; the
   original's table 0x15d15c (made by load_traffic_cfg 0x7de84) */
int lane_width(int n, int half);

#endif
