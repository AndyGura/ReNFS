/* The race's camera (camera_update 0x136b0, NOTES §4.7): where the frame is seen from,
   once a race step. The driver's eyes (modes 0 and 1), the cameras outside (2 .. 13; 6
   and 7 from the rival's car and the police car), the crash's view (13) and the way back
   into the car (10, 11). */
#ifndef RENFS_CAMERA_H
#define RENFS_CAMERA_H

#include "view.h"

/* the cameras' settings, 0x58 bytes a mode (0xe80f0) */
typedef struct {
    int kind;                  /* +0: the view (view_of_kind) */
    vec3 eye;                  /* +4: the eye from the car (in the car: the driver's) */
    int target;                /* +0x10: (the original's pointer to the yaw followed) */
    int ahead;                 /* +0x14: how far along the road the camera looks */
    int follow;                /* +0x18: 1, 2 the eye moved across with the car, 3 the car's
                                  yaw followed, 4 the road's */
    int key_other;             /* +0x1c: the mode the view key gives next, racing, the
                                  car watched not the player's */
    int key_held;              /* +0x20: .. while the race is not running */
    int key_player;            /* +0x24: .. racing, the player's car watched */
    int fill28;
    int shift;                 /* +0x2c: a move's step: 1 / 2^shift of the way (or 1 / it) */
    int divide;                /* +0x30: the step 1 / shift instead */
    int fill34;
    int mirror;                /* +0x38: the mirror drawn */
    int frames;                /* +0x3c: updates the move into it takes (0: none) */
    int next;                  /* +0x40: the mode after the move (-1 none) */
    int eye_end;               /* +0x44: the move ends once the eye's z reaches this */
    vec3 eye2;                 /* +0x48: the eye on coming from the driver's seat .. */
    int use_eye2;              /* +0x54: .. if set */
} camera_mode;

#define CAMERA_MODES 14

extern camera_mode g_cam_modes[CAMERA_MODES];
extern int g_cam_pitch_bias;   /* 0x12ba20: the dashboard's (0 so far) */
extern int g_cam_look_first;   /* 0xe8f68: the rival or the police may be looked at next
                                  by the view key (-1: not yet read from the image) */

/* the settings as the program has them, the driver's eye height the car's (its spec's
   +0x36c: modes 0 and 10, 0x147b2) */
void camera_init(int eye_height);
/* the camera on car c in mode `mode` (camera_set_car 0x13530 / 0x13450) */
void camera_set_car(race_camera *cam, const race_car *c, int mode);
/* the camera this step (0x136b0) */
void camera_update(race_camera *cam);
/* the mode's settings taken up again (0x14b00) */
void camera_set_mode(race_camera *cam, int mode);
/* camera_next 0x1494c: the view key: the next mode for the car watched, by the modes'
   tables; while the race is not running, a car close ahead of or behind the player and
   the police close behind are looked at first (after mode 5) */
void camera_next(race_camera *cam);
/* 0x50428: the police in a chase (its bits) between lo (not) and hi nodes behind the
   player's car */
int camera_police_close(int lo, int hi);
/* 0x50478: the other racer (not a traffic car) between lo (not) and hi nodes behind */
int camera_rival_close(int lo, int hi);
/* camera_check_mode 0x6324c, every step before the update: the camera outside (13)
   while the car watched is not upright along the road, and for a second after; then
   back into the mode wanted, through 10 or 11 for the driver's */
void camera_check(race_camera *cam, const race_car *watched);
/* a shake: the wave's step `phase` (0xec28c) times the amount, / 16 (0x3e900) */
int shake_wave(int amount, int phase);

#endif
