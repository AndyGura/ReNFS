/* The cars the computer drives (NOTES §4.13): the opponent racers for now, the traffic
   and the police later. Each step a driven car (kind 3, drive_opponent 0x7b34c) follows
   the road: it is turned with the road's axes and its own steering, and every few steps
   (0x7cf98) it speeds up or slows towards a target speed and slides across towards a
   target place. Every fourth step (traffic_police_tick 0x7a040) each one's target speed
   and lane are chosen again: from the leading player, the road's speed limits, how far
   behind or ahead it is (the catch-up tables), and the cars around it in the lanes. */
#ifndef RENFS_OPPONENT_H
#define RENFS_OPPONENT_H

#include "car.h"

#define OPP_CFG 107            /* the traffic's settings, words (0x15d2bc, 0x1ac bytes) */

typedef struct {
    short zone[2][6];          /* 0x15d29c: the two racers' stretches of road, nodes:
                                  the car, outer window's end and start, its progress,
                                  inner window's end and start */
    int cfg[OPP_CFG];          /* 0x15d2bc: the settings (0x63f50) */
    const unsigned char *rec[8]; /* 0x15d490: each racer's driver (0xf4070 + 0x8d a
                                  letter; 0 for the players) */
    int brake;                 /* 0x15d470: a racer's braking a step (0x5999) */
    int lead_index;            /* 0x15d474: the leading player */
    race_car *lead;            /* 0x15d48c */
    race_car *rival;           /* 0x13c270: car 1 */
    int ticks;                 /* 0x15d4b8: 4 a tick */
    int lane_w;                /* 0x15d4bc: the last lane width worked out, 16.16 */
    int lane_rule;             /* 0x15d488: a lane change slowed for the car beside */
    int off;                   /* 0xf74ac: no ticks (the replay) */
    int explode;               /* 0xf74a4 */
    int traffic;               /* 0x12da3c: traffic cars */
    int wander;                /* 0xf74f0: 1 in this many steps the traffic picks another
                                  place in its lane (0x28) */
    int bend_drop;             /* 0x15d47c: the last bend's turn (7e64c) */
    int bend_slow;             /* 0x15d478 */
    int behind;                /* 0x15d484: the catch-up tables' index */
    unsigned random;           /* 0xeab70: the random numbers' state */
    unsigned mult;             /* 0xeab78: and multiplier */
    unsigned last;             /* 0xeab74: the last product */
    int grid[8];               /* 0x12dca0: each racer's place on the grid */
    int humans_only;           /* tests (drive_emu): the tick only refreshes the players'
                                  cars, as before the opponents were ported */
} opp_race;

extern opp_race g_opp;

/* load_traffic_cfg 0x7de84's part: the settings (0x63f50, from the program), each racer's
   driver by the race type and its grid place (0x583ec), the stretches of road (0x7814c),
   the random numbers seeded (0x38c10) */
void opponent_start(unsigned seed);
/* drive_opponent 0x7b34c: a step of a car the computer drives (kind 3) */
void opponent_drive(race_car *c);
/* traffic_police_tick 0x7a040: every fourth step, the cars refreshed and the target
   speeds and lanes chosen */
void opponent_tick(void);
/* 0x786d4's part for the computer's cars: its target speed (+0x160) */
void opponent_target_speed(race_car *c);
/* 0x7dbd8's tail: a computer's car sometimes picks another place in its lane */
void opponent_wander(race_car *c);

/* the stretches of road kept round the two racers (0x77f70), and whether a node is in
   one of them (0x77cfc: 1 + which, 0 none; 1 in a field of more than two) */
void opponent_zones(void);
int opponent_near(int node);

#endif
