/* The replay (NOTES §3.15, §4.22): the race is not filmed but driven again from its
   controls. While racing every second step the players' control words go into a ring of
   7,200 slots, and every 27 seconds a snapshot of every car and of the race's other
   state; at the race's end all of it is written to REPLAY.RPL as the original writes it.
   Played back, the race is set up again from the file's race record and driven from the
   controls; a snapshot puts the race back at a point to start from. */
#ifndef RENFS_REPLAY_H
#define RENFS_REPLAY_H

#include "view.h"

/* the race record (0x12da0c, 0x348 bytes, race_setup_record 0x36c5c): what the race is,
   as the file keeps it. Offsets: */
#define REC_SIZE 0x348
#define REC_SEGMENT 0x2        /* 0x12da0e */
#define REC_TRACK 0x6          /* 0x12da12 (CONFIG.DAT +0x95's order) */
#define REC_STEM 0xa           /* 0x12da16: its files' name ("al1", "tr5") */
#define REC_TYPE 0x14          /* 0x12da20: the race type */
#define REC_LINKED 0x18        /* 0x12da24: over a link */
#define REC_FLAGS 0x1c         /* 0x12da28: the codes' word (0x15d12a) */
#define REC_RACERS 0x20        /* 0x12da2c */
#define REC_HUMANS 0x24        /* 0x12da30 */
#define REC_POLICE 0x28        /* 0x12da34 */
#define REC_CARS 0x2c          /* 0x12da38: cars in all */
#define REC_TRAFFIC 0x30       /* 0x12da3c */
#define REC_GEARBOX 0x34       /* 0x12da40: CONFIG.DAT +0x97 */
#define REC_TIME 0x38          /* 0x12da44: seconds from 1970, the random numbers' seed */
#define REC_ENTRY 0x3c         /* 0x12da48: the players, 0x4b bytes each: */
#define REC_ENTRY_SIZE 0x4b
#define ENT_NAME 0x2           /*   the name (9) */
#define ENT_CAR 0xb            /*   the car (the game's number, 0x36250) */
#define ENT_AUTO 0xf           /*   the automatic gearbox (CONFIG.DAT +0x8d) */
#define ENT_TCS 0x13           /*   traction control on */
#define ENT_ABS 0x17           /*   ABS on */
#define ENT_STEER 0x33         /*   the steering's kind (0x12d974): 0 keys, 1 a slope */
#define ENT_PEDALS 0x37        /*   the pedals' (0x12d978): 1 a joystick's, by how far */
#define REC_GRID 0x294         /* 0x12dca0: each racer's place on the grid */
#define REC_DRIVERS 0x2b4      /* 0x12dcc0: each racer's driver, 99 none */
#define REC_CLOCK 0x300        /* 0x12dd0c: the race clock's setting */
#define REC_CATCH_UP 0x304     /* 0x12dd10 */
#define REC_LAPS 0x308         /* 0x12dd14 */
#define REC_PLAYER 0x310       /* 0x12dd1c: the player the file was recorded for */
#define REC_TABLE 0x318        /* 0x12dd24: the race length's table */
#define REC_MIRROR 0x31c       /* 0x12dd28 */
#define REC_RALLY 0x320        /* 0x12dd2c */

extern unsigned char g_rec[REC_SIZE];

int rec_get(int off);
void rec_put(int off, int v);

/* the replay's state (0xf6b74 is g_drive.state: 1 racing, 2 playing back, 3 running
   fast to a point before it is shown, 4 held) */
typedef struct {
    int mode;                  /* 0x14554c: what to do at the next step: 1 the pause
                                  menu's replay, 2 the highlights, 3 the whole race, 4 the
                                  file then 3, 5 the file then 2, 8 the attract mode's, 6
                                  no file, 7 Done pressed */
    int from_menu;             /* 0x145554: a replay of the menus' (the race ends after) */
    int attract;               /* 0x1455f8 */
    int end;                   /* 0x13c17c: 0x20 a replay's end, 0x40 the attract mode's */
    int pos;                   /* 0x144ba4: the step counter in the ring (mod 28,800) */
    int last;                  /* 0x143edc: the counter at the last step recorded */
    int countdown;             /* 0x143ee8: calls to the next snapshot */
    int stop;                  /* 0x145534: where the playing stops */
    int target;                /* 0x145538: where running fast stops */
    int speed;                 /* 0xf6b7c: 4 full, 3, 2, 1 slower, 0 held */
    int resume;                /* 0xf6b78: the state at the end */
    int restart;               /* 0x14553c: the camera's car and the sounds again */
    int put_back;              /* 0x145540: a snapshot put back for a jump */
    int start_count;           /* 0x144d98: the counter the replay started from */
    int clock_kept[3];         /* 0x144b94, 0x144b98, 0x144b9c: the race clock, the
                                  steps done and due when the replay began */
    int highlight;             /* 0x144d8c: the highlight played, -2 none, -1 none yet */
    int nhighlights;           /* 0x144d88 */
    int hl[30][3];             /* 0x144ba8: each one's start, end and event */
    int events[480];           /* 0x144da8: each 60 counts' greatest event, its slot
                                  << 8 | event + 0x80 */
    int auto_camera;           /* 0x145530: the camera changed every 256 counts */
    int camera_kept;           /* 0x144d94 */
    int engine_again;          /* 0x145548: the engine's sounds started again */
    int mark_start;            /* 0x144d90: the counter to be kept as the start */
    int hold;                  /* 0xf6b80: steps held (the pause button's) */
    int wait;                  /* 0xf6b84: frames between steps when slower */
    int plays;                 /* 0x14552c: steps played */
    int slowed;                /* 0xf6bbc: the last highlight played slower */
    int back;                  /* racing again after the pause menu's replay: the race's
                                  clocks as they were (0x144b94 ..) */
    race_camera *cam;          /* the race's camera (0x129ecc) */
} replay_state;

extern replay_state g_replay;

/* race_setup_record 0x36c5c: the record from CONFIG.DAT's settings */
void replay_setup_record(void);
/* replay_settings 0x5c340: the record from REPLAY.RPL (the name in CONFIG.DAT +0x3b,
   0x145558); 0 no file (the race ends at once, 0x20) */
int replay_settings(void);
/* replay_start 0x5dd68: the ring, the highlights cleared */
void replay_start(void);
/* race_main's start: the first step records a snapshot */
void replay_race_begins(void);
/* 0x5efb0: race_update_cars's first call each step (not before the race starts) */
void replay_step(void);
/* replay_end 0x5de88: REPLAY.RPL written (not after a replay) */
void replay_end(void);
/* replay_mark_highlight 0x5c6d4: an event for the highlights (racing only) */
void replay_mark(int event);

/* RENFS_REPLAYCHECK: each snapshot of the file compared with the race played back */
void replay_check(void);

#endif
