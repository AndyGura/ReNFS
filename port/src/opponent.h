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
    int waiting;               /* 0x15d480: traffic cars waiting off the road (kind 6) */
    int sides[2];              /* 0x15d154, 0x15d158: traffic cars placed coming the
                                  other way, and going the racers' way */
    int rival_car;             /* 0x12da9e: the rival's car (9: a traffic car) */
    int chasing;               /* 0xf74a8: the police chasing the player's car (it sped
                                  past within 5 nodes of the police car) */
    int come_node;             /* 0xf7510: the player's node when the police's coming
                                  was last looked at */
    int come_odds[4];          /* 0xf7500: a police spot's chance in 128 by its kind
                                  (the node's +0x1a & 0x18) */
    int long_chases;           /* 0x15d468: chases that ran past the warning's time */
    int linked;                /* 0x12da24: 1 a race over a link (no police coming) */
    int ticket_up, warning_up; /* 0xea81c, 0xea820: the ticket's or warning's picture
                                  shown (the race's view replaced) */
    int msg_time;              /* 0xea824: -1000 as one is given, the step counter as it
                                  goes */
    int wander;                /* 0xf74f0: 1 in this many steps the traffic picks another
                                  place in its lane (0x28) */
    int bend_drop;             /* 0x15d47c: the last bend's turn (7e64c) */
    int bend_slow;             /* 0x15d478 */
    int behind;                /* 0x15d484: the catch-up tables' index */
    unsigned random;           /* 0xeab70: the random numbers' state */
    unsigned mult;             /* 0xeab78: and multiplier */
    unsigned last;             /* 0xeab74: the last product */
    int grid[8];               /* 0x12dca0: each racer's place on the grid */
    int drivers[9];            /* 0x12dcc0 (right after): each racer's driver, its letter
                                  less 'A' (0x583ec), 99 none. A traffic car's grid place
                                  is read past grid[] into it, as the original does */
    int humans_only;           /* tests (drive_emu): the tick only refreshes the players'
                                  cars, as before the opponents were ported */
} opp_race;

extern opp_race g_opp;

/* load_traffic_cfg 0x7de84's part: the settings (0x63f50, from the program), each racer's
   driver by the race type and its grid place (0x583ec), the stretches of road (0x7814c),
   the traffic's counts (none waiting, all going the racers' way: g_opp.traffic set
   before) */
void opponent_start(void);
/* race_run's 0x38c10: the random numbers (0xeab70) seeded, before the race's cars are
   chosen (0x653a4 draws the traffic from them) */
void opponent_seed(unsigned seed);
/* the next of them (the bits 8..23 of the product) */
unsigned opponent_random(void);
/* drive_opponent 0x7b34c: a step of a car the computer drives (kind 3) */
void opponent_drive(race_car *c);
/* 0x7c030's loop: after a player's crash, the racers thrown about put back on the road,
   the traffic and the police out of reach (or thrown about) placed again */
void opponent_put_back(void);
/* chase_drop 0x781ac: the police's chase dropped, the player's car's chase flags off
   (with p the police car's +0x176 & 8 too) */
void opponent_chase_drop(int p);
/* drive_kind5 0x7d9e4: a step of a computer's car stood up after a crash (kind 5) */
void opponent_recover(race_car *c);
/* drive_opponent_wait 0x7e0c8: a step of a traffic car waiting off the road (kind 6):
   held out of the way; its wait over, it is put on the road again ahead of a racer
   once the racers are far enough along and few enough cars are on the road */
void opponent_wait(race_car *c);
/* 0x7bf94: is the car near any player (in their stretches of road)? */
int opponent_near_players(const race_car *c);
/* traffic_police_tick 0x7a040: every fourth step, the cars refreshed and the target
   speeds and lanes chosen */
void opponent_tick(void);
/* drive_player_wait 0x7ea20: a step of a player's car pulled over by the police (kind
   1): it stands while the ticket or warning is shown, then is driven again (kind 2),
   in neutral */
void opponent_player_wait(race_car *c);
/* 0x786d4's part for the computer's cars: its target speed (+0x160) */
void opponent_target_speed(race_car *c);
/* 0x7dbd8's tail: a computer's car sometimes picks another place in its lane */
void opponent_wander(race_car *c);

/* the stretches of road kept round the two racers (0x77f70), and whether a node is in
   one of them (0x77cfc: 1 + which, 0 none; 1 in a field of more than two) */
void opponent_zones(void);
int opponent_near(int node);

#endif
