/* The race's sounds (NOTES §4.18): the player's engine (two loops mixed by the throttle,
   pitched by the revs) and the road's noise, the tyres, the horn, the two nearest cars'
   engines, the police's siren and the radar's beep, the hits, the landings and the gear
   changes. Sixteen channels, each a voice of the sound library (sndlib.c) and the sound
   it plays; once a race step (race_update_cars 0x519d8) the routines 0x80974, 0x7fef8,
   0x7fcbc, 0x7fe88 and 0x800a0 set them from the cars and the camera. */
#ifndef RENFS_RACESND_H
#define RENFS_RACESND_H

#include "car.h"

#define SND_CHANNELS 16

typedef struct {
    int voice[SND_CHANNELS];   /* 0x13c2dc: each channel's voice (-1 none) .. */
    int sound[SND_CHANNELS];   /* 0x13c2e0: .. and its sound */
    int bend[SND_CHANNELS];    /* 0xeded8: each channel's last pitch (0x52cf0) */
    int sfx;                   /* 0x12da67 + 0x4b · player: the settings' sound volume */
    int music;                 /* 0x12da6b: and the music's */
    int mono;                  /* 0x12da6f = 2: the settings' mono (every pan the middle) */
    int present;               /* 0x12da73: a sound card (the banks loaded) */
    int muted;                 /* 0xeded0: every volume 0 (the pause) */
    int engine;                /* 0xeded4: the engine heard (off while thrown about) */
    int engine_chan;           /* 0xee352 >> 16: the engine's channel (7) */
    int beep_on, beep_wait;    /* 0xee358, 0xee35c: the radar's beep */
    int road_pitch;            /* 0xee360: the road's noise's pitch */
    int revs;                  /* 0xee364: the engine's pitch, the revs of 127 */
    int load;                  /* 0xee368: the throttle as heard, 0..255 */
    int road_up;               /* 0xee36c: the gravel's pitch going up */
    int under_vol, under_pan;  /* 0xee370, 0xee374: the sound under a bridge or in a
                                  tunnel (the node's kind) */
    int skid[2];               /* 0xf7768, 0xf776c: the tyres' sound kinds (-1: by the
                                  surface) */
    int echo_now;              /* 0xf7770 */
    int horn_kind;             /* 0xf7774: a computer's horn's pitch (0x7faa8) */
    int horn_time;             /* 0xf7778: steps its horn still sounds */
    int horn_car;              /* 0xf777c: whose */
    int siren_on;              /* 0xf7780 */
    int echo_wait;             /* 0xf7784 */
    int siren_node;            /* 0xf7788: the police car's node last step */
    int siren_bend;            /* 0xf778c */
    race_car *near[2];         /* 0xf7790, 0xf7794: the cars heard on channels 2 and 3 */
    int banks[5];              /* 0x13c360: the banks put in */
    unsigned char *bank_data[5]; /* 0x13c374 */
    int nbanks;                /* 0xedecc */
} race_sound;

extern race_sound g_rsnd;

/* the channels' state and the program's starting values (as the image has them) */
void rsnd_init(void);
/* snd_load_car_banks 0x52774 (after 0x529f4): the player's car's bank, the opponent's,
   the hits' (by the track and mono), the police and the radar (NFS_FMmb), then the engine
   started (0x52670). car: the player's car's number, other: the other racer's */
void rsnd_load(int car, int other, int track);
/* 0x53214: the engine heard again, the road's noise started */
void rsnd_engine_on(void);
/* 0x531dc: the engine, the road's noise and the tyres stopped (the car thrown about) */
void rsnd_engine_off(void);
/* 0x52a70: every channel stopped and the banks freed (the race's end) */
void rsnd_free(void);
/* race_update_cars' sounds, once a step after the camera (0x80974, 0x7fef8, 0x7fcbc,
   0x7fe88 with two players, 0x800a0) */
void rsnd_step(void);
/* 0x80a60: car slot's horn wanted (kind its pitch, 0x7faa8): for 5 .. 19 steps from EA's
   random numbers, if none sounds already (the game's logic: with the sound off too) */
void rsnd_horn(int slot, int kind);
/* 0x52d2c with channel -1 (the one-off sounds on channel 4): kind and sub pick the sound
   (0x52c38) with how hard (16.16), heard from dist (0 .. 0x200000 near) at angle (a turn
   0x1000000, 0 straight ahead) */
void rsnd_hit(int kind, int sub, int how, int dist, int angle);
/* the car the sounds are heard from: the one watched, or the player's while it races and
   no replay plays (0x12dd54, 0x12dd58, 0x145c90) */
race_car *rsnd_heard(void);
/* 0x7fb58: where a car is heard from: its angle from the camera's heading and its
   distance (the camera's yaw made positive, as the original does) */
void rsnd_where(const race_car *c, int *angle, int *dist);

#endif
