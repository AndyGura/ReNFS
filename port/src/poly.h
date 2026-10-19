/* The 3D view's points and quads: the projection (0x83b5c) and the two routines that cut a
   textured quad into smaller ones as it nears (0x35348 the view, 0x35b04 the mirror;
   NOTES §4.7). */
#ifndef RENFS_POLY_H
#define RENFS_POLY_H

#include "raster.h"

typedef struct { int x, y, z; } vec3;     /* 16.16, seen from the camera: z ahead */
typedef struct { int x, y; } scr2;        /* screen pixels */

/* what the projection and the cutting read (the original's globals) */
typedef struct {
    int cx, cy;            /* the screen's centre (0xf7a28, 0xf7a2c) */
    int min_z;             /* nearer than this is not projected (0xf7938) */
    int focal;             /* 0xf7a30, and the table made from it (0xf7a14) */
    int recip[0x400];
    unsigned flags;        /* 1: a point was too near (0xf7a40) */
    int clip_x, clip_y, clip_z;    /* a point cut at the near plane moves along this
                                      (0x143e54, 0x143e58, 0x143e5c) */
    int far_shift;         /* 0xf6aa4: the cutting distances >> this */
    int max_level;         /* 0xeaad8: cut at most so many times */
} view3d;

extern view3d g_view;

/* the reciprocals for a focal length (0x83af4) */
void view_set_focal(int focal);
/* a point on the screen; too near: both 0x80000000 and flags |= 1 */
void project(const vec3 *p, scr2 *out);
/* n of them, the shift starting at `shift` (0x94c80; project() is 16) */
void project_points(int n, const vec3 *p, scr2 *out, int shift);
/* a point nearer than `near` put on it, moved along (clip_x, clip_y, clip_z) */
void move_to_near(vec3 *p, int near);
/* p turned by m (0x83a54): each sum in 64 bits, >> 16 */
void rotate(const vec3 *p, const int m[9], vec3 *out);

/* what paints a finished quad (0x12c0dc; raster_quad unless a test catches it) */
extern void (*g_paint)(const raster_tex *t, const int uv[8], const int xy[8]);

/* a textured quad, P[0..3] round it with their screen points s, the picture cut in
   2^ushift x 2^level pieces of which (uindex, vindex) is this one; near: the near plane */
void quad_view(const raster_tex *t, int ushift, int level, int uindex, int vindex,
               const vec3 *const p[4], const scr2 s[4], int near);
void quad_mirror(const raster_tex *t, int ushift, int level, int uindex, int vindex,
                 const vec3 *const p[4], const scr2 s[4], int near);

#endif
