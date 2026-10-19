/* The road painted, far to near (0x5a4f0, NOTES §4.7): each section's quads with their
   textures, the roadside objects and the cars between its rows as they come. */
#ifndef RENFS_ROADDRAW_H
#define RENFS_ROADDRAW_H

#include "road.h"

/* the window the view is painted in (the view record's +0x18..+0x24) */
typedef struct {
    int x, y, w, h;
} road_window;

/* what the road hands its pieces to. quad, whole and picture are the painters (0x35348,
   0x350d4, 0x35090) and are set to them; the others are left 0 until they are ported:
   roadside 0x59294 (a row's pair of points and the strip's tile), model 0x66738 (a 3D
   object at pos, turned by angle), car_row 0x679ec and cars 0x6837c (the cars on a row) */
typedef struct {
    void (*quad)(const raster_tex *t, int ushift, int level, int uindex, int vindex,
                 const vec3 *const p[4], const scr2 s[4], int near);
    void (*whole)(const unsigned char *shape, const scr2 s[4]);
    void (*picture)(const unsigned char *shape, const scr2 s[4]);
    void (*roadside)(const unsigned char *pair, int node, int first, const unsigned char *tile,
                     int variant, int side, int flag);
    void (*model)(int model, const vec3 *pos, int angle);
    void (*car_row)(int camera_node, int row_node, const scr2 *a, const scr2 *b);
    void (*cars)(int node);
    void (*cars_behind)(int node);     /* 0x68430: the cars on a row, in the mirror */
} road_painters;

extern road_painters g_road_paint;

/* the race's step counter (0x145c94, 2 a step): the objects' animations */
extern int g_road_steps;

/* the mirror: the corners go round the other way (0x12dd28) */
extern int g_road_mirror;

/* a family shape as a picture to paint with */
void shape_tex(const unsigned char *shape, raster_tex *t);

/* the painters: the whole picture less its border on a quad as two triangles (0x350d4),
   and the whole picture on a quad (0x35090); nothing for no shape */
void paint_whole(const unsigned char *shape, const scr2 s[4]);
void paint_picture(const unsigned char *shape, const scr2 s[4]);

/* the roadside objects' cursor before the road is painted (0x597e4) */
void road_objects_start(void);

/* the road's `drawn` sections ahead of road node `node` (the camera's), far ones first
   (0x5a4f0; 0x59d3c the far ones). near: the near plane (road_transform's) */
void road_draw(int drawn, int node, int near, const road_window *win);
/* the road behind in the mirror (0x5aa84): `drawn` sections from the farthest of those
   road_gather_behind took, the camera at road node `node`, each section's quads the last
   first, with draw_quad's cutter */
void road_draw_behind(int drawn, int node, int near, const road_window *win);

#endif
