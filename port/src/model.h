/* EA's 3D models (ORIP, NOTES §3.5): a model made drawable, turned and placed in the view,
   its polygons sorted far to near and painted. The library at 0x94000..0x9a000 and
   0xaa000..0xac000 of the original. */
#ifndef RENFS_MODEL_H
#define RENFS_MODEL_H

#include "poly.h"

/* a texture of the model (the instance's 20 bytes) */
typedef struct {
    const unsigned char *shape;  /* +0xc: its picture, 0 if none */
} model_tex;

/* a model made drawable (orip_instance 0x95522): one per car and level of detail */
typedef struct {
    unsigned char *orip;         /* +0: the model, in its file */
    const unsigned char *lo, *hi;/* the file it is in (what the painters may read) */
    int npoints;                 /* +4 */
    unsigned flags;              /* +0x10: 2 polygons with a corner near are left out */
    int m[9];                    /* +0x28 then +0x4c: its turn into the view */
    vec3 pos;                    /* the view's place of its origin (0x9803e, negated) */
    vec3 *view;                  /* +0x78: its points in the view */
    scr2 *screen;                /* +0x7c: and on the screen */
    int count;                   /* +8: its polygons in the list */
    unsigned *list;              /* +0x84: all of them (0x981f7), sorted as drawn */
    model_tex *tex;              /* +0x14 */
    int ntex;
} model;

/* the model `orip` of a file lo..hi made drawable (0x95522 with the points shared) and
   its textures found in the bank of pictures (0x9759a); 0 if out of memory */
int model_make(model *md, unsigned char *orip, const unsigned char *lo, const unsigned char *hi,
               unsigned char *bank);
void model_free(model *md);

/* a texture by its part's name (0x97521), its picture changed (0x977f3); -1 if none */
int model_find_texture(const model *md, const char *name);
void model_set_texture(model *md, int i, const unsigned char *shape);
/* a polygon group by name (0x97bf2) and its list of polygons (0x97c6b), in the model's
   index table (sorted in place, as the original does); -1 if none */
int model_group(const model *md, const char *name, int *count, unsigned **list);
/* every polygon's 0x200 turned over (the mirrored tracks, 0x65874) */
/* a named point (0x97afa: the model's +0x54 records of 12 bytes, an 8-letter name then
   the point); -1 if none */
int model_find_point(const model *md, const char *name);
void model_mirror(model *md);

/* the model turned by m and its origin at pos in the view (0x957ec, 0x9803e), its points
   into the view (orip_transform 0x98137) and onto the screen (0x98090) */
void model_place(model *md, const int m[9], const vec3 *pos);
/* n of its polygons sorted far to near by their first two corners (orip_sort 0x982e1),
   then painted (orip_draw_list 0x9868a) */
void model_sort(model *md, int n, unsigned *list);
void model_draw(model *md, int n, const unsigned *list);

/* the library's turns (0x98860 x, 0x988c8 y, 0x98930 z): a angle in 1024ths of a turn
   on the table without steps between (0xab808); the other way round from mat_turn */
void model_turn(int axis, int a, int m[9]);

#endif
