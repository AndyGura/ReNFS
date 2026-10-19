/* The player's car driven, a race step at a time (NOTES §4.6): the controls
   (car_controls_player 0x10400: it steers and works the pedals), then
   drive_player 0x382d0 (car_physics_player 0x408e0: the gearbox, the engine, the tyres,
   the road's edges; car_check_lap; the car laid on the road 0x38808), then
   car_road_offset 0x51598. */
#ifndef RENFS_DRIVE_H
#define RENFS_DRIVE_H

#include "car.h"

#define DRIVE_PLAYERS 8
#define DRIVE_LAPS 16

/* what of the race the driving reads and keeps (the original's globals; the cars, the
   player and the humans are cardraw.h's g_cars, g_player, g_humans) */
typedef struct {
    int steps;                 /* 0x145c94: 2 a step */
    int flags;                 /* 0x12f1bc: 1 every step, 4 every 2nd, 0x10 every 4th */
    int state;                 /* 0xf6b74: 1 racing */
    int kind;                  /* 0x12da12: the track (0..2 the cities etc., > 2 the
                                  circuits, 3 the acceleration test's) */
    int segment;               /* 0x12da0e */
    int rally;                 /* 0x12dd2c: EARALLYE's tracks */
    int catch_up;              /* 0x12dd10: the leader slowed by up to this */
    int viewed;                /* 0x12dd54: the car the camera follows */
    int camera_mode;           /* 0x129ecc */
    int laps;                  /* 0x13c160 */
    int last_node;             /* 0x13c164: an open road's finish */
    unsigned controls[DRIVE_PLAYERS];  /* 0x12d954: each player's control word */
    int steer_kind[DRIVE_PLAYERS];     /* 0x12d974 (+12 a player): 0 keys, else a slope */
    int pedal_kind[DRIVE_PLAYERS];     /* 0x12d978: 1 the joystick's pedals */
    int nitro;                 /* 0x12cca0: the NITRO code typed */
    int revving[DRIVE_PLAYERS];/* 0x12ccac: held at full revs (the start) */
    int explode;               /* 0xf74f8 = 4: EXPLODE typed */
    int nitro_keys;            /* N held with a shift or B (0xf7af9, 0xf7af2/0xf7afe/0xf7af8) */
    int clock;                 /* 0xed100: the 100 Hz clock */
    int nitro_on;              /* 0xec31c */
    int nitro_until, nitro_off;/* 0xec314, 0xec318 */
    int spinning;              /* 0xec288: the driven wheels spinning (each step) */
    int first_move;            /* 0x13c170 */
    int top_speed[DRIVE_PLAYERS];      /* 0x14ea80 + 0x1d2 a player, +0x1c4: the race's
                                          fastest |speed| */
    int options;               /* 0x12da28 (the settings' word 0x15d12a): 0x20 lands
                                  softer after a long jump, but on tr6 */
    char track[8];             /* 0x12da16: the track's name */
    int air_shake;             /* 0x12ba24: steps of the dashboard's shake after a
                                  landing (the player's) */
    int lap_times[DRIVE_PLAYERS][DRIVE_LAPS]; /* 0x14ea80 + 0x1d2 a player: the steps less
                                  300 when each lap was finished */
    int keys[8];               /* the controls' keys (0x12d92c ..): left, right, faster,
                                  brake, gear up, gear down, handbrake, horn; PC scan
                                  codes */
    int crashed;               /* a hard hit on a wall: the crash (0x1cb14, the car to state
                                  4, tumbling) is not in the port yet; counted here */
} drive_race;

extern drive_race g_drive;

/* the road surfaces' grip (8.8), drag, rolling drag and kind (0x12cc00: four of 4
   words), as the track's kind sets them (0x3f364) */
extern int g_surfaces[4][4];
void drive_surfaces(void);

/* a racer's car made ready to drive (load_car_pbs 0x50c24 with car_torque_load 0x18990
   and 0x3fb90, then car_reset 0x50830): its physics (SIMDATA/CARSPECS, by c->number) into
   c->spec, with the player's automatic gearbox, traction control and ABS asked for; 0 if
   the file is missing. The car's size comes from its PDN (car_load) */
int drive_load(race_car *c, int automatic, int tcs, int abs);

/* the keys' default places (the original's controls settings 0x15cb4e .. as a new
   installation has them): the arrows, A and Z, the space bar, H */
void drive_default_keys(void);
/* 0x48bec: the keyboard's control word for the player: steering full or none (the keys
   swapped on a mirrored track), the pedals all or nothing, the gear keys, the handbrake,
   the horn; B, R and W together with the automatic both gear keys */
unsigned drive_keyboard(int mirrored);

/* every second step (flag 4): the control word applied (0x10400) */
void drive_controls(race_car *c);
/* every step: drive_player 0x382d0, then car_road_offset 0x51598 */
void drive_step(race_car *c);

#endif
