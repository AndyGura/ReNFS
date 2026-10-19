/* The race's timing (NOTES §4.12): the finish, the racers' order and places, the splits
   against the rival, and when the race is over. Set up as the race starts (race_laps_init
   0x506dc, timing_setup 0x685e4); kept every step by race_update_cars 0x519d8. The laps' times are
   car_check_lap's (drive.c, g_drive.lap_times). */
#ifndef RENFS_TIMING_H
#define RENFS_TIMING_H

#include "car.h"
#include "drive.h"

#define TIMING_SPLITS 85       /* five a lap, seventeen laps */
#define TIMING_CARS 12

/* (the racers checked at the finish are cardraw.h's g_racers, 0x13c18c) */
typedef struct {
    int humans;                /* 0x13c190: the players' cars among them */
    int lap_limit;             /* 0x12dd14: the settings' laps (the most the bar shows) */
    int table;                 /* 0x12dd24: the race length's table of record times */
    int finish[DRIVE_PLAYERS]; /* 0x14ec3c + 0x1d2 a player: the steps less 300 as it
                                  finished, -999 turned back, 0 still racing */
    int place[DRIVE_PLAYERS];  /* 0x14ec4a: its place */
    int tickets[DRIVE_PLAYERS];/* 0x14ec30 + 0x1d2 a player: the police's tickets (the
                                  second ends the race) */
    int warnings[DRIVE_PLAYERS];/* 0x14ec34: and warnings */
    int split_at[TIMING_SPLITS];       /* 0x14e62c: how far along (+0x4c) each split is */
    int mine[TIMING_SPLITS];   /* 0x14e79c: the steps as the player passed each */
    int theirs[TIMING_SPLITS]; /* 0x14e904: the rival's, or in a time trial the time
                                  to beat, 300 on */
    int passed, rival_passed;  /* 0x14e794, 0x14e798: the splits passed */
    int split;                 /* 0x14ea6c: the rival's time less the player's at the
                                  last split both passed, > 0 ahead */
    int split_shown;           /* 0x14ea70: steps it stays on the status bar */
    int segment_times[3];      /* 0x14ea74: the finishing times of the segments before
                                  (kept from race to race) */
    int by_road[TIMING_CARS];  /* 0x13c298: the cars (g_cars' indices) by node, then
                                  across */
    int by_race[TIMING_CARS];  /* 0x13c274: by how far along the race, the leader last */
    int result;                /* 0x13c178 (and 0x13c17c): 1 finished, 4 arrested (the
                                  second ticket), 0x10 everyone turned back */
    int stopped;               /* 0x145c90: a human finished and stopped (two players) */
    int switched;              /* 0xede80: the camera turned to the other player */
    int place_shown;           /* 0x12b8c8: the place the status bar works out */
    unsigned char places[TIMING_CARS]; /* 0x12bd33: each car's place, for the map */
} race_timing;

extern race_timing g_timing;

/* cars_create 0x51388's part: both orders as the cars come */
void timing_cars(void);
/* race_laps_init 0x506dc's part: the racers by the race type, and 0x685e4: the results
   cleared, the split points (five a lap from the track's table 0xf701c), the times to
   beat */
void timing_start(void);
/* car_check_finish 0x51760, every step for the racers: finished, or turned back; the
   finishing place */
void timing_finish(race_car *c);
/* race_update_cars' every step before the driving: each racer's finish checked, then
   whether the race is over */
void timing_step(void);
/* 0x518c0: the two orders sorted again (by_road, by_race) */
void timing_sort(void);
/* after the driving: the splits passed (0x68d7c) and the split worked out (0x68d00) */
void timing_splits(void);
/* 0x516d4: 2 finished, 1 turned back (before the start line), 0 racing */
int timing_state(const race_car *c);
/* the place of the viewed car (0x2e368's working out): the finishing order once
   finished, else ahead or behind of the rival, or from the race order; the cars'
   places for the map */
int timing_place(int viewed);

#endif
