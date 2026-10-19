/* The race's views (NOTES §4.7): the windows the 3D view is painted in, the camera's
   matrices, and the projection's centre and lens for the view being drawn. */
#ifndef RENFS_VIEW_H
#define RENFS_VIEW_H

#include "poly.h"
#include "car.h"

#define VIEW_WINDOWS 17        /* a resolution's, 0xe85c0 + 0x2a8 × resolution */

/* a window (40 bytes) */
typedef struct {
    int id;                    /* +0 */
    int squash;                /* +4: the picture's height for its width, 16.16 */
    int dx, dy;                /* +8, +0xc: the lens's centre moved (dy: the car's horizon) */
    int w, h;                  /* +0x10, +0x14: what the lens covers */
    int x, y, cw, ch;          /* +0x18 ..: the part of the screen painted */
} view_window;

/* a view of the camera (0xe8de8 the road ahead, 0xe8e28 the mirror) */
typedef struct {
    int fov;                   /* +0: degrees across */
    int near;                  /* +4: the near plane (road_transform's) */
    int mode;                  /* +0x10: its window, of the resolution's */
    const view_window *win;    /* +0x1c */
    int lens_h;                /* its layout's (+0x18, 0xe8b10 + 52 × +8) +0x20: the
                                  height the mirror's horizon is set by */
} view_rec;

/* what the frame is drawn from (the camera record [0x143df4], 0x129ecc) */
typedef struct {
    int mode;                  /* +0: 0, 1 the driver's eyes (camera.c) .. 13 */
    vec3 pos;                  /* +0x10: in the world, 16.16 */
    vec3 off;                  /* +0x2c: the eye from the car, turned with it */
    int node;                  /* +0x38: its road node */
    int flags;                 /* +0x3c: 1 the node changed */
    int pitch, yaw, roll;      /* +0x40, +0x44, +0x48: 0x1000000 a turn */
    int tilt[3];               /* +0x5c ..: added to the car's pitch, yaw, roll */
    vec3 eye;                  /* +0x6c: the eye in the car */
    int mirror_on;             /* +0x68: the mirror is drawn */
    int yaw_follow;            /* +0x78 */
    view_rec *ahead, *mirror;  /* +0x88, +0x8c */
    const race_car *car;       /* +0x90: the car followed */
    int m[9];                  /* +0x94: the world into the view ahead */
    int mm[9];                 /* +0xb8: the same for the mirror */
} race_camera;

extern view_window g_windows[2][VIEW_WINDOWS];  /* 0xe85c0: 320x200, 640x480 */
extern int g_view_res;                          /* 0x143e2c: 0 320x200, 1 640x480 */
extern view_rec g_view_ahead, g_view_mirror;    /* 0xe8de8, 0xe8e28 */
extern const view_rec *g_view_now;              /* 0x143dfc */

/* the windows as the program has them, fitted to the player's car (0x14630's end,
   0x145d0): car its number (+0x48a), horizon its spec's +0x370 */
void view_init(int res, int car, int horizon);
/* a view in window `mode` (0x14154): the sky's tables, the lens, the reciprocals */
void view_setup(view_rec *v, int mode);
/* the camera's two matrices from its angles (0x5868c), squashed by the current view's
   window and turned over for the mirrored tracks (g_road_mirror) */
void view_matrix(race_camera *c);
/* the view ahead (0) or the mirror (1) current (0x1486c): its centre and lens; the
   matrix to draw with returned */
const int *view_select(race_camera *c, int mirror);

/* a turn about x (0), y (1) or z (2) by angle, 0x1000000 a turn (0x58500, 0x5855c,
   0x585b8) */
void mat_turn(int axis, int angle, int m[9]);
/* 3 x 3 matrices of 16.16, each product >> 16 on its own (0x839f8): out = a b */
void mat_mul(const int a[9], const int b[9], int out[9]);

#endif
