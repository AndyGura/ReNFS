/* The tyres' marks on the road (NOTES §4.10's end): a ring of 125 records the player's
   car leaves while its tyres slide hard (0x671dc, as it is drawn), painted with the road's
   rows they lie by (0x679ec), darkening the road as the shadows do. */
#ifndef RENFS_SKID_H
#define RENFS_SKID_H

#include "car.h"
#include "poly.h"

#define SKID_RECORDS 125

/* a record (0x149574 + 0xa0 each) */
typedef struct {
    vec3 corner[4];     /* +0x00: the tyres about the centre (1/512 units): the front pair,
                           then the rear pair */
    vec3 centre;        /* +0x30: the car's shadow's centre, in the world */
    vec3 view[4];       /* +0x3c: the corners seen from the camera, at the last painting */
    scr2 screen[4];     /* +0x6c: .. on the screen */
    int node;           /* +0x8c: the car's road node */
    int front, rear;    /* +0x90, +0x94: marks from here back (the last frame's slides), 0 none */
    int step;           /* +0x98: the step counter then (0x145c94) */
    int kind;           /* +0x9c: 2 on a surface of the second kind, else 1 */
} skid_record;

typedef struct {
    /* rec[0] and the last are the records before and after the ring, which the original
       reaches once a time round (0x1494d4: never written but for its points seen, so
       node 0 and no marks; 0x14e394, never written: empty) */
    skid_record rec[1 + SKID_RECORDS + 1];
    int at;             /* 0xf6e2c: the record being written */
    int front, rear;    /* 0xf6ff4, 0xf6ff8: the last frame's slides */
    vec3 last;          /* 0x14e620: the centre where the last record began */
    unsigned char rows[15];     /* 0x14e5f3: road nodes from the camera's less 2 with marks */
    unsigned random[6];         /* the dust's own EA random numbers (skid_dust) */
} skid_state;

extern skid_state g_skid;

/* 0x989a0: a line w wide from (x0, y0) to (x1, y1), its ends square (the screen's y a
   quarter longer for the angle), the road under it darkened as a shadow (0x8a094) */
void skid_line(int x0, int y0, int x1, int y1, int w);
/* what paints a mark's line (skid_line, unless a test catches it) */
extern void (*g_skid_line)(int x0, int y0, int x1, int y1, int w);

/* record i of the ring, -1 the one before it, SKID_RECORDS the one after */
skid_record *skid_at(int i);

/* a puff of the tyres' dust: the road's (smo1 or smo3, added as the smoke) or the other
   surfaces' (dir1..dir3, solid), pick of them, size 16.16, at the screen's (x, y)
   (0x64714, 0x64768; smoke_stretch / smoke_stretch_solid unless a test catches it) */
extern void (*g_skid_puff)(int dirt, int pick, int size, int x, int y);

/* the ring emptied (0x65094: the race's start, a replay's jump); the dust's random
   numbers from the race's */
void skid_reset(void);
/* the frame's rows with marks (0x67784), before the road is painted */
void skid_rows(int cam_node);
/* the player's car as it is drawn (0x671dc): its half length (along) and half width
   (across) on the road >> 10 and its shadow's centre */
void skid_record_car(const race_car *c, const vec3 *along, const vec3 *across,
                     const vec3 *centre);
/* the road's painter at a row (0x679ec with -999): the marks within 2 nodes of it whose
   ends are not both before the row's edge a -> b */
void skid_paint_row(int cam_node, int row_node, const scr2 *a, const scr2 *b);
/* the player's car's tyres' dust (0x664bc), from its near model: as it is placed (behind
   it: every sliding tyre, half the size) and once it is painted (in front: the sliding
   tyres on the camera's side); puffs at each tyre's last four marks of the last 48 steps,
   bigger the older. EA's random numbers are its own: the original's are the race's, so
   that the frames drawn would change how cars hit each other */
void skid_dust(const race_car *c, int behind);

#endif
