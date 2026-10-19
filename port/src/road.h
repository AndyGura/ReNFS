/* The road as the race draws it (NOTES §4.7): its points near the camera, moved into the
   camera's view and onto the screen each frame. */
#ifndef RENFS_ROAD_H
#define RENFS_ROAD_H

#include "poly.h"

#define ROAD_POINTS 515        /* 0x137434 .. 0x138c58 */
#define ROAD_SECTIONS 600      /* 0x136620 .. 0x137434, 6 bytes each */
#define EXTRA_POINTS 900       /* 0x12f540 .. 0x131f70 */

typedef struct {
    unsigned char kind;        /* +0: the record of its faces (0xf61c0 + kind × 0x4c) */
    short a;                   /* +2 */
    short points;              /* +4: its points across */
} road_section;

typedef struct {
    road_section sec[ROAD_SECTIONS];   /* 0x136620 */
    unsigned sec_mask;                 /* 0x13bb90 */
    int drawn;                         /* 0x136618: the sections drawn */

    int count;                         /* 0x13bb78: the points near the camera */
    vec3 world[ROAD_POINTS];           /* 0x137434: in the world (16.16) */
    vec3 view[ROAD_POINTS];            /* 0x138c58: from the camera, >> 11 */
    vec3 near[ROAD_POINTS];            /* 0x142578: the same, held off the near plane */
    scr2 screen[ROAD_POINTS];          /* 0x13a47c: projected from near[] */

    int xsets, xcount;                 /* 0x1365d8, 0x1365c0: another set of points */
    vec3 xworld[EXTRA_POINTS];         /* 0x12f540 */
    vec3 xmoved[EXTRA_POINTS];         /* 0x13d8f8 */
    vec3 xview[EXTRA_POINTS];          /* 0x131f70 */
    scr2 xscreen[EXTRA_POINTS];        /* 0x1349a0 */

    int fill;                          /* 0x13bb74: world[] filled so far */
    int xfill;                         /* 0x1365c4: xworld[] filled so far */
    int base;                          /* 0x13660c: the camera's section */
    int xfirst;                        /* 0x1365cc: the first placement near it */
    int xnext;                         /* 0x1365d0: the next placement to take */
    int xnode;                         /* 0x1365d4: its node (-1: none left) */
} road_state;

extern road_state g_road;

/* the points of the sections drawn this frame (0x4ffbc): `drawn` sections from section
   `sec`, the camera at road node `node`; the roadside objects' corners with them */
void road_gather(int node, int sec, int drawn);

/* the road's direction ahead from the camera's section (0x587b8): what the near plane
   moves points along */
void road_clip_direction(int section);

/* the points into the view (0x59a74): the camera at pos, turned by m (3 x 3, 16.16), in
   section node / 4. Returns the near plane (also the view's +4) */
int road_transform(const vec3 *pos, const int m[9], int node);

#endif
