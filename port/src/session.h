/* What a race leaves for the screens after it (NOTES §4.21): each racer's results
   (0x15cb68, 8 of 0x95 bytes, a saved game's first part), the settings kept while a race
   runs, the standings worked out at its end (0x68ab0, 0x68dcc) and the records it beat
   (records_segment 0x69608). */
#ifndef RENFS_SESSION_H
#define RENFS_SESSION_H

#define SESSION_RACERS 8

typedef struct {
    char name[9];              /* +0 */
    int laps[20];              /* +9: a circuit's lap times, the player's */
    int time;                  /* +0x59: this race's finishing time (1/60 s; 0 none,
                                  999,999 given up) */
    int distance;              /* +0x5d: its length, the finish node · 4.8 */
    int top;                   /* +0x61: its top speed (16.16 m/s) */
    int total;                 /* +0x65: the times added up, segment after segment (0
                                  once one is missed) */
    int total_distance;        /* +0x69 */
    int best_top;              /* +0x6d: the best top speed of the segments */
    int tickets, warnings;     /* +0x71, +0x75: the police's */
    int cleared;               /* +0x79: only ever cleared */
    int stats[6];              /* +0x7d .. +0x91: the race's record's 0x14ec18, 0x14ec1c,
                                  0x14ec24, 0x14ec20, 0x14ec2c, 0x14ec28 (a circuit's
                                  statistics, drive.h's stats) */
} session_racer;

typedef struct {
    session_racer racer[SESSION_RACERS];
    unsigned char kept[0xac];  /* 0x15d010: CONFIG.DAT +9 .. +0xb4 as before the race,
                                  put back after it (+0x93 one segment on) */
    unsigned char kept_keys[26];       /* 0x15d0bc: +0x5f36, the keys */
    short points[8];           /* 0x15d0d8: a tournament's points */
    short won[9], raced[9];    /* 0x15d0e8, 0x15d0fa: the tracks won in a tournament,
                                  and those raced */
    short gave_up[SESSION_RACERS];     /* 0x15d10c: given up in one of the segments */
    int result;                /* 0x15d122: how the race ended: 1 finished, 2 left from
                                  the pause menu, 4 arrested, 0x10 turned back */
    int race_type;             /* 0x15d11c: the race type as the race began */
    int seg_total[3], seg_top[3];      /* 0x12c324.., 0x12c318..: the player's total and
                                          top speed after each segment */
    /* the standings at the race's end (0x68dcc) */
    int order[9];              /* 0x13c194: the racers by finishing time */
    int place, rival_place;    /* 0x14fae2, 0x14fae4: the player's and the rival's in it */
    int rival;                 /* 0x14fae8: the other racer of the best total */
    int rank, rival_rank;      /* 0x14faea, 0x14fae6: by the totals (0 the best) */
    int rival_car;             /* 0x14faec: its car (the menu's order) */
    short last_order[9];       /* 0x12c278: the order, for the next segment's grid */
    short race_points[8];      /* 0x14ec48 in each racer's race record: the points this
                                  race gave (0x689f8) */
} race_session;

extern race_session g_session;

/* fe_new_session 0x7469c: the keys kept, the results and points cleared (+0x93 the
   first segment), the names: the player's (CONFIG.DAT +0), then Drifter, Burnout ..
   (0x74748) */
void session_new(void);
/* tourn_clear_records 0x745f4: everything a racer's but the name, the given-up flags,
   and the segment back to the first */
void session_clear_results(void);
/* 0x68ab0 with 0x68dcc, at a race's end: the given-up flags, the distances, a time
   worked out for the racers still on the road, the finishing order; then each racer's
   time, total and top speed, the ranks, the rival, the player's laps and police record */
void session_race_end(void);
/* records_segment 0x69608: the segment just driven's best time and top speed records
   (CONFIG.DAT +0xb5 + track · 0xa6b, records idx and idx + 3) taken by the player (not
   with the Warrior); 1 the time, 2 the speed, 3 both */
int session_records_segment(void);
/* a track's records (CONFIG.DAT +0xb5 + track · 0xa6b, 0x27 bytes each): 0..2 the
   segments' best times, 3..5 their top speeds, 6.. the ten best totals (a circuit's ten
   by race length from 6 + length · 10), 36 the track's top speed */
#define SESSION_TOP_TEN 6
#define SESSION_TOP_SPEED 36
/* 0x69980: the player's total into the track's ten best (not with the Warrior): its
   place 0..9, 20 none */
int session_top_ten(void);
/* 0x69e00: the player's best top speed the track's record: 1 */
int session_records_top_speed(void);
/* the segment the standings are about: CONFIG.DAT +0x93 less one (it is one on by
   then), 0 meaning the third */
int session_segment(void);
/* 0x689f8: a tournament's race's points (0xf6ffc: by the order of the totals, none to a
   racer who gave up) */
void session_points(void);
/* 0x691f8: the racers by their totals, the least first (0x14faf0) */
void session_order_by_totals(int order[8]);
/* 0x689b0: the player's points the most (or as many as the most) */
int session_player_leads(void);

/* the formats the screens share (0x74914, 0x74854, 0x74a14): a time in 1/60 s as
   m:ss.t or s.t, a speed (16.16 m/s) in mph or km/h, an average speed from a time and
   a distance; "N/A" when there is none */
void fmt_time(char *s, int t);
void fmt_speed(char *s, int v);
void fmt_average(char *s, int t, int d);

#endif
