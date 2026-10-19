/* EA's 3D models (NOTES §3.5). */
#include "model.h"
#include "exe.h"
#include "trackfam.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* a polygon with a corner this near is left out when the model asks (0xf9674, set to 30 by
   load_car_cfm 0x66101) */
#define MODEL_NEAR 30

static unsigned u32(const unsigned char *p)
{
    return p[0] | p[1] << 8 | p[2] << 16 | (unsigned)p[3] << 24;
}

/* the model's tables by their offsets in the header (what orip_fix 0x978a8 makes pointers) */
static unsigned char *field(const model *md, int at)
{
    return md->orip + u32(md->orip + at);
}

static unsigned *index_table(const model *md)
{
    return (unsigned *)field(md, 0x50);
}

static const unsigned char *polygon(const model *md, unsigned i)
{
    return field(md, 0x28) + 12 * i;
}

/* the first n letters alike, small and capital letters the same (0xaa3a2) */
static int same_name(const unsigned char *a, const char *b, int n)
{
    for (; n > 0; n--, a++, b++) {
        int x = *a >= 'A' && *a <= 'Z' ? *a + 32 : *a, y = *b >= 'A' && *b <= 'Z' ? *b + 32 : *b;
        if (x != y)
            return 0;
        if (!x)
            break;
    }
    return 1;
}

int model_make(model *md, unsigned char *orip, const unsigned char *lo, const unsigned char *hi,
               unsigned char *bank)
{
    unsigned char *t;
    int i, n;

    memset(md, 0, sizeof *md);
    md->orip = orip;
    md->lo = lo;
    md->hi = hi;
    md->npoints = (int)u32(orip + 0x10);
    n = (int)u32(orip + 0x24);
    md->ntex = (int)u32(orip + 0x38);
    md->view = calloc((size_t)md->npoints + 1, sizeof *md->view);
    md->screen = calloc((size_t)md->npoints + 1, sizeof *md->screen);
    md->list = calloc((size_t)n + 1, sizeof *md->list);
    md->tex = calloc((size_t)md->ntex + 1, sizeof *md->tex);
    if (!md->view || !md->screen || !md->list || !md->tex) {
        model_free(md);
        return 0;
    }
    /* every polygon in the list (0x981f7 with -1). The original tests the first polygon's
       0x400 each time round instead of each one's, and so takes all or none */
    if (n > 0 && !(polygon(md, 0)[1] & 4))
        for (i = 0; i < n; i++)
            md->list[md->count++] = (unsigned)i;
    /* the pictures by their four letters (0x9759a) */
    t = field(md, 0x3c);
    for (i = 0; i < md->ntex && bank; i++) {
        char name[5];
        memcpy(name, t + 20 * i + 8, 4);
        name[4] = 0;
        md->tex[i].shape = shpi_find(bank, name);
    }
    return 1;
}

void model_free(model *md)
{
    free(md->view);
    free(md->screen);
    free(md->list);
    free(md->tex);
    memset(md, 0, sizeof *md);
}

int model_find_texture(const model *md, const char *name)
{
    const unsigned char *t = field(md, 0x3c);
    int i;
    for (i = 0; i < md->ntex; i++)
        if (same_name(t + 20 * i, name, 8))
            return i;
    return -1;
}

void model_set_texture(model *md, int i, const unsigned char *shape)
{
    if (i >= 0 && i < md->ntex)
        md->tex[i].shape = shape;
}

int model_group(const model *md, const char *name, int *count, unsigned **list)
{
    const unsigned char *g = field(md, 0x4c);
    int i, n = (int)u32(md->orip + 0x48);
    for (i = 0; i < n; i++, g += 28)
        if (same_name(g, name, 8)) {
            *count = (int)u32(g + 0x14);
            *list = index_table(md) + u32(g + 0x10);
            return i;
        }
    return -1;
}

void model_mirror(model *md)
{
    unsigned char *p = field(md, 0x28);
    int i, n = (int)u32(md->orip + 0x24);
    for (i = 0; i < n; i++, p += 12)
        p[1] ^= 2;
}

void model_turn(int axis, int a, int m[9])
{
    int s = fix_sin(a & 0x3ff), c = fix_cos(a & 0x3ff);

    memset(m, 0, 9 * sizeof *m);
    m[axis * 4] = 0x10000;
    switch (axis) {
    case 0: m[4] = c; m[5] = s; m[7] = -s; m[8] = c; break;
    case 1: m[0] = c; m[2] = -s; m[6] = s; m[8] = c; break;
    default: m[0] = c; m[1] = s; m[3] = -s; m[4] = c; break;
    }
}

void model_place(model *md, const int m[9], const vec3 *pos)
{
    const unsigned char *p = field(md, 0x18);
    int i;

    /* the library's own camera (0xf9678) is never turned in the game: its product with
       m is m, and the origin's place stays pos */
    memcpy(md->m, m, sizeof md->m);
    md->pos = *pos;
    for (i = 0; i < md->npoints; i++, p += 12) {
        vec3 v = {(int)u32(p), (int)u32(p + 4), (int)u32(p + 8)};
        rotate(&v, md->m, &md->view[i]);
        md->view[i].x += pos->x;
        md->view[i].y += pos->y;
        md->view[i].z += pos->z;
    }
    project_points(md->npoints, md->view, md->screen, 16);
}

void model_sort(model *md, int n, unsigned *list)
{
    int key[1000], gap, i, j;
    const unsigned *ix = index_table(md);

    if (n > 1000)
        n = 1000;
    for (i = 0; i < n; i++) {
        const unsigned *c = ix + u32(polygon(md, list[i]) + 4);
        key[i] = md->view[c[0]].z + md->view[c[1]].z;
    }
    /* Shell's sort, the farthest first (0xaa710) */
    for (gap = n >> 1; gap > 0; gap >>= 1)
        for (i = gap; i < n; i++)
            for (j = i - gap; j >= 0 && key[j] < key[j + gap]; j -= gap) {
                int k = key[j];
                unsigned l = list[j];
                key[j] = key[j + gap];
                key[j + gap] = k;
                list[j] = list[j + gap];
                list[j + gap] = l;
            }
}

/* a picture of the model's file as the painters take it */
static void shape_tex(const model *md, const unsigned char *shape, int see_through, raster_tex *t)
{
    t->pixels = shape + 16;
    t->lo = md->lo;
    t->hi = md->hi;
    t->w = t->rowbytes = shape[4] | shape[5] << 8;
    t->h = shape[6] | shape[7] << 8;
    t->see_through = see_through;
}

/* one polygon (orip_draw_poly 0x98482) */
static void draw_poly(model *md, unsigned i)
{
    const unsigned char *p = polygon(md, i);
    const unsigned *ix = index_table(md);
    unsigned flags = u32(p);
    const unsigned *c = ix + u32(p + 4), *uvi = ix + u32(p + 8);
    const unsigned char *uv = field(md, 0x20);
    int n = (int)(flags & 7), k, see = (int)(flags >> 3 & 15) == 1;
    const model_tex *tex;
    raster_tex t;
    int xy[8], tuv[8];

    for (;;) {
        /* a corner too near: left out when asked (0xaa7df), and the polygons chained to
           it with it */
        if (md->flags & 2)
            for (k = 0; k < n; k++)
                if (md->view[c[k]].z <= MODEL_NEAR)
                    return;
        /* facing away: the same, unless drawn from both sides (0xaa95b, 0x961f0) */
        if (!(flags & 0x500)) {
            const scr2 *a = &md->screen[c[0]], *b = &md->screen[c[1]], *d = &md->screen[c[2]];
            int area = (int)((unsigned)(d->y - b->y) * (unsigned)(a->x - b->x) -
                             (unsigned)(d->x - b->x) * (unsigned)(a->y - b->y));
            if ((area > 0) != !(flags & 0x200))
                return;
        }
        tex = &md->tex[flags >> 16 & 0xff];
        if (!(flags & 0x80) || !tex->shape) {
            /* one colour (0xaa990): no car or traffic model the snapshots reach has one */
            goto next;
        }
        shape_tex(md, tex->shape, see, &t);
        for (k = 0; k < n && k < 4; k++) {
            xy[2 * k] = md->screen[c[k]].x;
            xy[2 * k + 1] = md->screen[c[k]].y;
        }
        if (n == 3) {
            /* 0xaaae7, 0xaac8f: the picture's points in whole pixels */
            for (k = 0; k < 3; k++) {
                tuv[2 * k] = (int)u32(uv + 8 * uvi[k]);
                tuv[2 * k + 1] = (int)u32(uv + 8 * uvi[k] + 4);
            }
            raster_tri(&t, tuv, xy);
        } else if (flags & 0x1000) {
            /* 0xab105, 0xab28a: the picture's points in 16.16 */
            for (k = 0; k < 4; k++) {
                tuv[2 * k] = (int)(u32(uv + 8 * uvi[k]) << 16);
                tuv[2 * k + 1] = (int)(u32(uv + 8 * uvi[k] + 4) << 16);
            }
            raster_quad(&t, tuv, xy);
        } else {
            /* 0xab633, 0xab76b → 0x91bf0, 0x91b90: the whole picture, to its last 1/65536 */
            memset(tuv, 0, sizeof tuv);
            tuv[2] = tuv[4] = (int)((unsigned)t.w << 16) - 1;
            tuv[5] = tuv[7] = (int)((unsigned)t.h << 16) - 1;
            raster_quad(&t, tuv, xy);
        }
    next:
        /* 0x800: the polygon after the corners goes with it */
        if (!(flags & 0x800))
            return;
        p = polygon(md, c[n]);
        flags = u32(p);
        c = ix + u32(p + 4);
        uvi = ix + u32(p + 8);
        n = (int)(flags & 7);
        see = (int)(flags >> 3 & 15) == 1;
    }
}

void model_draw(model *md, int n, const unsigned *list)
{
    int i;
    for (i = 0; i < n; i++)
        draw_poly(md, list[i]);
}
