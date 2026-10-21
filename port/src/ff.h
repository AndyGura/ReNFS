/* Force feedback (NOTES §4.26): the race's pushes, kicks and shakes as the original sends
   them to an I-Force wheel or stick (its serial driver 0x4b3a0.., the game's side
   0x4d670..0x4dcb6, on when 0x15d144), each with the original's strength, direction and
   length; ffdev.c plays them on what is there (a force feedback wheel, a pad's rumble) or
   writes them down (RENFS_FFLOG). */
#ifndef RENFS_FF_H
#define RENFS_FF_H

#include "car.h"

typedef struct {
    int on;                    /* 0x15d144: a device to send them to */
    int last_hit;              /* 0xed114: the 100 Hz clock at the last cars' hit */
    int vib[2];                /* 0xed10c, 0xed110: the road's two shakes playing */
    int vib_rate, vib_size;    /* 0xed120, 0xed124: what they were started with */
    int steady[2];             /* 0xed118, 0xed11c: the steady push last sent */
    int rumble;                /* 0x12f1e0: the crash's rumble playing */
} ff_state;

extern ff_state g_ff;

/* startup's 0x4d670 (a device looked for) and main's end 0x4d6c8 (let go) */
void ff_open(void);
void ff_close(void);
/* 0x4d700 / 0x4d6ec: the forces on, or held off (the pause, the race's end) */
void ff_forces(int on);
/* 0x4d714: every effect stopped, the crash's rumble too */
void ff_stop(void);
/* 0x4d730: car_over_object: a roadside object under the car, `across` its side */
void ff_object(const race_car *c, int across);
/* 0x4d7a4: box_impulse: a's and b's hit (one of them the car watched), the push's x
   and its size */
void ff_cars(const race_car *a, int push_x, int size);
/* 0x4d878: the car watched against a wall, the road's left (0) or right (1) */
void ff_wall(int right);
/* 0x4d8c0: the car watched changing gear (before gear_now takes the new one) */
void ff_gear(const race_car *c);
/* race_update_cars 0x51f22, each step not running fast: the road's shake by speed and
   surface (0x4d9a8) and the steady push of the car's sideways and forward forces
   (0x4d928), for the player's car */
void ff_step(const race_car *c);
/* 0x4dc40: the crash's rumble on (the car watched knocked over, 0x1e9b6) or off (its
   engine heard again, 0x53295) */
void ff_crash(int on);

#endif
