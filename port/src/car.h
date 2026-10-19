/* The race's cars (NOTES §4.6): what of a car record the port knows so far, and the
   starting grid. */
#ifndef RENFS_CAR_H
#define RENFS_CAR_H

#include "poly.h"
#include "model.h"

/* the car thrown about after a hard hit (+0xc4, made by 0x2c920; crash.c): a box with
   its own turn and spin. Its coordinates are the car's with z turned round */
typedef struct {
    int m[9];                  /* +0: its turn; rows its axes across, up and along */
    vec3 pos, vel;             /* +0x24, +0x30 */
    vec3 spin;                 /* +0x3c: its turning, radians a second about an axis */
    int mass, inertia;         /* +0x60, +0x64: the PDN's +0x10, +0xc */
    int inv_mass, inv_inertia; /* +0x68, +0x6c: 1 over them */
    int radius;                /* +0x70: the length of half */
    int half[3];               /* +0x74: half its width, height and length (the PDN's first
                                  three words) */
    int wait;                  /* +0x80: steps until it is put back on the road; for the
                                  computer's cars, steps to its next plan (0x7cf98) */
    int honks;                 /* +0x84: a computer's car stuck behind a player: steps
                                  it has wanted to sound its horn */
    int steady;                /* +0x88: counts down while its steering moves (100 a
                                  racer, 160 the traffic, when it holds still) */
    int shift;                 /* +0x8c: steps of a gear change (revs held), then -20
                                  counting back up */
    int grip;                  /* +0x90: how much of a target speed the traffic takes */
} car_body;

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
    int state;                 /* +0x512: how it is driven (the routine 0x13c1b8 holds):
                                  2 the player's, 4 thrown about (crash.c), 6 off the road
                                  (not drawn) */
    int slot;                  /* +0x516: its place in the race's tables */
    int draw_node;             /* +0x341: the node it is drawn at (0x51fa3, 0x678a4) */
    int mirror_node;           /* +0x345: the same in the mirror (0x51fa3, 0x677f8) */
    int own_turn;              /* +0x20: turned by its own matrix (the driven cars) */
    int turn[9];               /* +0x24 */
    int yaw_drawn;             /* +0x351: its heading as drawn */
    int half_len, half_wid;    /* +0x78, +0x7c: half its size, 16.16 */
    int braking;               /* +0x3b9: above 10 its brake lights are on */
    unsigned char bits[3];     /* +0x174 .. +0x176 */
    vec3 ground;               /* +0x385: the road under it (0x38548 with 0) */
    vec3 seen;                 /* +0x391: from the camera, in the view, 1/128 units */
    int far;                   /* +0x78e: drawn by its far model */
    vec3 shadow, along, across;/* +0x359, +0x36d, +0x379: its shadow on the road */
    model near_m, far_m;       /* +0x52e, +0x666: its two models, drawable */
    struct {
        int index, count;      /* +0x5be ..: inside, surface, outside, convex */
        unsigned *list;
    } groups[4];

    /* what driving needs (drive.c, NOTES §4.6): speeds 16.16 metres a second */
    unsigned char *spec;       /* +0x479: its physics, the PBS file as load_car_pbs left
                                  it (read by offset, drive.c) */
    int steer;                 /* +0x18: the front wheels' turn, 0x1000000 a turn */
    int steer_to;              /* +0x1c: the computer's: where it steers to */
    int progress;              /* +0x4c: how far along the road */
    int lap;                   /* +0x50 */
    vec3 vel;                  /* +0x54: in the world */
    int side_speed;            /* +0x60: across the car, + right */
    int up_speed;              /* +0x64 */
    int speed;                 /* +0x68: along the car, + forwards */
    int wheel_speed;           /* +0x6c: the driven wheels' */
    int speed_abs;             /* +0x70: the larger of |vel.x|, |vel.z| and a quarter of
                                  the other */
    int yaw_rate;              /* +0x74 */
    int snd_dist;              /* +0x3a1: its distance from the camera last step (0x7ff88) */
    int snd_mode;              /* +0x3ad: the camera's mode then */
    int throttle;              /* +0x3b1: 0..255 */
    int throttle_kept;         /* +0x3b5: given back after a shift */
    int shift_wait;            /* +0x3bd: steps until the clutch bites, -1 none */
    int rpm;                   /* +0x3c1 */
    int rpm_road;              /* +0x3c5: the engine's turns the wheels make */
    int grip_extra;            /* +0x3c9 */
    int grip_rear;             /* +0x3cd */
    int grip_front;            /* +0x3d1 */
    int shift_last;            /* +0x3d5: the last control's gear request */
    int push_side, push_up, push_fwd; /* +0x3d9, +0x3dd, +0x3e1: forces from outside:
                                  the slope's pull (0x38808), other cars */
    int engine_force;          /* +0x3e5 */
    int gearbox;               /* +0x3e9: 0 by hand, 1 reverse, 2 neutral, 3 drive */
    int gear;                  /* +0x3ed: -2 reverse, -1 neutral, 0 first .. */
    int clutch;                /* +0x3f1: 1 engaged */
    int handbrake;             /* +0x3f5: the control's bit 15 */
    int shifting;              /* +0x3f9 */
    int on_ground;             /* +0x3fd */
    int air_steps;             /* +0x401: steps in the air; -2 .. -1 just landed */
    int slide_front, slide_rear; /* +0x405, +0x409 */
    int wheelspin;             /* +0x40d */
    int force_side, force_fwd; /* +0x411, +0x415: the last step's */
    int gear_now, gear_before; /* +0x419, +0x41d */
    int gear_shown;            /* +0x421: counts down after a shift */
    unsigned char tyres[2];    /* +0x425, +0x426: front, rear: 1 sliding, 2 at the
                                  limit, 4 skidding */
    int mass_front;            /* +0x43d: the weight on the front wheels (0x3fb90) */
    int weight_shift;          /* +0x44d */
    int grip_rear_base;        /* +0x451 */
    int grip_front_base;       /* +0x455 */
    int yaw_damp;              /* +0x459 */
    int turn_front, turn_rear; /* +0x461, +0x465 */
    int tcs, abs;              /* +0x48e, +0x492: the player's traction control, ABS */
    short downforce[4];        /* +0x49a, +0x49c, +0x49e, +0x4a0 (read as the high
                                  halves of the words 2 bytes before) */
    int surface;               /* +0x4a2: of 0x12cc00's */
    int surface_flag;          /* +0x4a6 */
    int node_flags[5];         /* +0x4aa, +0x4ae, +0x4c6, +0x4ca, +0x4ce: its node's
                                  kind (0x4008c) */
    int tcs_now, abs_now;      /* +0x4b2, +0x4b6 */
    int horn;                  /* +0x4ba: the control's bit 14 */
    int turn_target;           /* +0x4f6 */
    int turn_count;            /* +0x4fa */
    int auto_mode;             /* +0x4fe: driving itself (0x10180), 0 none */
    int auto_steer;            /* +0x502 */
    int dt;                    /* +0x506: a step, 16.16 seconds (1/30) */
    int rate;                  /* +0x50a: steps a second (30) */

    /* what the crash needs (crash.c, NOTES §4.6) */
    int road_off;              /* +0x80: how far to the side of its node's point (each
                                  fourth step, 0x786d4) */
    int reach;                 /* +0x84: how far its body reaches across the road */
    vec3 road_side, road_up, road_fwd; /* +0x8c, +0x98, +0xa4: the road's axes at its
                                  node (0x7dbd8) */
    vec3 road_at;              /* +0xb0: its node's point */
    int height;                /* +0xbc: half its height (the body's) */
    car_body body;             /* +0xc4 (+0xc0 points to it) */
    int road_speed;            /* +0x15c: its speed along the road (each fourth step; the
                                  computer's cars drive by it) */

    /* what the computer's driving needs (opponent.c, NOTES §4.13) */
    int zone;                  /* +0x158: the stretch of road it is in (of 0x15d29c) */
    int speed_to;              /* +0x160: the speed it drives towards */
    int across_to;             /* +0x164: the place across the road it slides towards */
    int bend;                  /* +0x168: the PDN's +0x14: how much a bend slows it */
    int bend_inv;              /* +0x16c: 1 over it */
    int top;                   /* +0x170: the PDN's +0x18 */
    int accel[100];            /* +0x178: speed gained a step at each speed (m/s) */
    int gear_top[6];           /* +0x308: each gear's top speed */
    int gear_inv[6];           /* +0x320: 1 over them (0 stays 0) */
    int rev_scale;             /* +0x338: the revs' scale, its top in the high half */
    int lane_offset;           /* +0x33c: its place in its lane, from the middle */
    int horn_sound;            /* +0x469: the PDN's +0x1c8 */
    int crashing;              /* +0x4f2: thrown about after a hard hit, until it is put
                                  back on the road */

    /* what the race's timing needs (timing.c) */
    unsigned char part;        /* +0x51a: 4 a racer (its finish checked), 2 it meets
                                  the other cars */
    int smoke_point;           /* +0x616: its near model's point "smok", where the smoke
                                  is drawn from (0x65f8b) */
    int size[3];               /* +0x64e, +0x652, +0x64a: its near model's width, height
                                  and length (0x65160), 16.16 */
    int obj_node, obj_index;   /* +0x4ea, +0x4ee: the roadside object it was last tried
                                  against (collide.c): its node, its place in the TRI's
                                  list */
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
