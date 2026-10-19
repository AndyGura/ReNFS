/* The road painted, far to near (NOTES §4.7). A section kind's record (0xf61bc + 0x4c ×
   kind, in NFS_DOS.EXE) lists its quads, 11 bytes each at 0xf4880: four corners among the
   section's points, the strip across the road, the tile (how the strip's texture is cut)
   and how many road rows the quad closes. After a quad that closes rows come the row's
   roadside walls, the objects standing on it and the cars on it. */
#include "roaddraw.h"
#include "exe.h"
#include "track.h"
#include "trackfam.h"
#include <stddef.h>

road_painters g_road_paint = { quad_view, paint_whole, paint_picture, 0, 0, 0, 0 };
int g_road_steps;

#define RECORDS 0xf61bc        /* the section kinds: +0 points (far), +4 quads, +8 and +0x10
                                  a pair of points a row for the walls, +0x48 first quad */
#define RECORD_BYTES 0x4c
#define QUADS 0xf4880          /* 11 bytes each */
#define NEAR_VARIANT 0xf4690   /* the texture's size by the row, near sections */
#define FAR_VARIANT 0xf4708    /* the same by four rows, far sections */
#define OBJ_VARIANT 0xf4604    /* an object's picture by the row */
#define OBJ_VARIANT_B 0xf458c  /* the same for placements marked at +8 */
#define NEAR_TRIPLES 0xf6b08   /* three corner triples a quad's facing is tried on */
#define FAR_TRIPLES 0xf6b2c    /* two */
#define MIN_Z 0x86             /* an object nearer than this is not painted */

static int rd32(const unsigned char *p)
{
    return (int)((unsigned)p[0] | (unsigned)p[1] << 8 | (unsigned)p[2] << 16 | (unsigned)p[3] << 24);
}

static int rd16(const unsigned char *p)
{
    return (short)(p[0] | p[1] << 8);
}

static int table_byte(unsigned at, int i)
{
    const unsigned char *p = exe_at(at + (unsigned)i, 1);
    return p ? *p : 0;
}

void shape_tex(const unsigned char *shape, raster_tex *t)
{
    t->pixels = shape + 16;
    t->lo = g_fam.file;
    t->hi = g_fam.file + g_fam.len;
    t->w = t->rowbytes = rd16(shape + 4);
    t->h = rd16(shape + 6);
    t->see_through = shape[0] == 0x45;
}

void paint_whole(const unsigned char *shape, const scr2 s[4])
{
    raster_tex t;
    if (!shape)
        return;
    shape_tex(shape, &t);
    raster_quad_whole(&t, (const int *)s);
}

/* 0x35090 → 0x91bf0: the corners on the picture's own, to its last 1/65536 */
void paint_picture(const unsigned char *shape, const scr2 s[4])
{
    raster_tex t;
    int uv[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    if (!shape)
        return;
    shape_tex(shape, &t);
    uv[2] = uv[4] = (int)((unsigned)t.w << 16) - 1;
    uv[5] = uv[7] = (int)((unsigned)t.h << 16) - 1;
    g_paint(&t, uv, (const int *)s);
}

/* --- the roadside objects --- */

static const unsigned char *placement(int i)
{
    return g_track.placements + i * 16;
}

static unsigned char *picture_slot(int i)
{
    return i >= 0 && i < ROADSIDE_PICTURES * 4 ? g_fam.picture[i / 4][i % 4] : 0;
}

void road_objects_start(void)
{
    road_state *r = &g_road;
    if (r->xsets > 180)
        r->xsets = 0;
    if (r->xsets <= 0) {
        r->xnode = -1;
        return;
    }
    r->xnext = r->xfirst + r->xsets - 1;
    if (r->xnext >= g_track.nplacements)
        r->xnext -= g_track.nplacements;
    r->xcursor = r->xcount;
    r->xnode = rd32(placement(r->xnext));
}

/* 0x59430: a picture's corners all within 2048 pixels across */
static int near_enough(const scr2 *s)
{
    int i;
    for (i = 0; i < 4; i++)
        if (s[i].x > 0x800 || s[i].x < -0x800)
            return 0;
    return 1;
}

static int far_enough(int first, int n)
{
    int i;
    for (i = 0; i < n; i++)
        if (g_road.xview[first + i].z < MIN_Z)
            return 0;
    return 1;
}

/* the next object back (0x59490), its points the last ones gathered: a 3D model, one
   picture, or a board and its side at a right angle. Then the cursor goes back by one */
static void draw_object(int row, int draw)
{
    road_state *r = &g_road;
    const unsigned char *pl = placement(r->xnext), *k = g_track.objects + pl[4] * 16;
    int node = rd32(pl);

    if (k[1] == 1) {
        vec3 pos;
        r->xcursor--;
        pos = r->xworld[r->xcursor];
        if (g_road_paint.model)
            g_road_paint.model(k[2], &pos, (pl[5] << 16) + (int)((unsigned)rd16(node_ptr(r->xnode) + 0x18) << 10));
    } else {
        r->xcursor -= k[1];
        if (draw && r->xcursor >= 0 && far_enough(r->xcursor, 4)) {
            scr2 *o = &r->xscreen[r->xcursor], *s = o, turned[4], side[4];
            if (k[1] == 4) {
                int v = table_byte(pl[8] ? OBJ_VARIANT_B : OBJ_VARIANT, row), i = k[2] + v;
                if (k[0] & 4 && k[8] && k[9])  /* animated: k[8] frames of k[9] steps */
                    i += g_road_steps / k[9] % k[8] * 4;
                if (near_enough(s))
                    g_road_paint.whole(picture_slot(i), s);
            } else if (k[1] == 6 && far_enough(r->xcursor + 4, 2)) {
                int v = table_byte(OBJ_VARIANT, row);
                if (k[0] & 1) {        /* the board turned round */
                    turned[0] = s[1];
                    turned[1] = s[2];
                    turned[2] = s[3];
                    turned[3] = s[0];
                    s = turned;        /* (the original's side then reads past its copy) */
                }
                if (near_enough(s))
                    g_road_paint.picture(picture_slot(k[2] + v), s);
                if (k[0] & 2) {
                    side[0] = o[4];
                    side[1] = o[5];
                    side[2] = s[2];
                    side[3] = s[1];
                } else {
                    side[0] = s[1];
                    side[1] = o[4];
                    side[2] = o[5];
                    side[3] = s[2];
                }
                if (near_enough(side))
                    g_road_paint.picture(picture_slot(k[3] + v), side);
            }
        }
    }

    if (--r->xnext < 0) {
        r->xnext = g_track.nplacements - 1;
        r->xnode = g_track.loop ? rd32(placement(r->xnext)) : -1;
        if (r->xnode == node)
            r->xnode = -1;
    } else
        r->xnode = rd32(placement(r->xnext));
}

/* --- the road's quads --- */

static int g_left, g_top, g_right, g_bottom;   /* 0x143da4, 0x143da8, 0x143da0, 0x143d9c */

int g_road_mirror;

/* 0x961f0 through 0x58eec: which way round three corners go (the mirror reverses them) */
static int area(const scr2 *s, unsigned triple)
{
    int i0 = exe_i32(triple), i1 = exe_i32(triple + 4), i2 = exe_i32(triple + 8), t;
    const scr2 *a, *b, *c;
    if (g_road_mirror) {
        t = i0;
        i0 = i2;
        i2 = t;
    }
    a = &s[i0];
    b = &s[i1];
    c = &s[i2];
    return (int)((unsigned)(c->y - b->y) * (unsigned)(a->x - b->x) -
                 (unsigned)(c->x - b->x) * (unsigned)(a->y - b->y));
}

/* facing the camera: the strips left of the middle (< 5) one way round, the right ones
   the other, on any of n triples */
static int facing(const scr2 *s, int strip, unsigned triples, int n)
{
    int i, a;
    for (i = 0; i < n; i++) {
        a = area(s, triples + 12 * (unsigned)i);
        if (strip < 5 ? a < 0 : a > 0)
            return 1;
    }
    return 0;
}

/* a near quad (0x58f70): not wholly beside the window, nor with its near side below it */
static int near_visible(const scr2 *s, int strip)
{
    int i, right = 1, left = 1;
    for (i = 0; i < 4; i++) {
        if (g_right >= s[i].x)
            right = 0;
        if (g_left <= s[i].x)
            left = 0;
    }
    if (right || left || (g_bottom < s[0].y && g_bottom < s[3].y))
        return 0;
    return facing(s, strip, NEAR_TRIPLES, 3);
}

/* a far quad (0x590b0): not wholly to one side of the raster's window */
static int far_visible(const scr2 *s, int strip)
{
    return !raster_outcode(4, (const int *)s) && facing(s, strip, FAR_TRIPLES, 2);
}

/* where the walls and the objects and cars of a quad's rows go: rows closes, the row's
   node going back one each */
typedef struct {
    int row_node, row, variant, first;
    const unsigned char *rec, *sec, *quad;
} rows_state;

static void walls(rows_state *st, const unsigned char *tile)
{
    int f = st->sec[1], side = (f & 0x1f) + 0x20;
    if (!g_road_paint.roadside)
        return;
    if (f & 0x80)
        g_road_paint.roadside(st->rec + 0x10 + (st->row_node & 3) * 2, st->row_node, st->first,
                              tile, st->variant, side, (f & 0x20) == 0);
    if (f & 0x40)
        g_road_paint.roadside(st->rec + 8 + (st->row_node & 3) * 2, st->row_node, st->first,
                              tile, st->variant, side, (f & 0x20) == 0);
}

static void cars(int node)
{
    if (g_road_paint.cars)
        g_road_paint.cars(node & (int)g_track.node_mask);
}

/* the quads of one section, the last first (from the camera's side) */
static const unsigned char *section_quads(rows_state *st, unsigned kind, int *n)
{
    const unsigned char *rec = exe_at(RECORDS + kind * RECORD_BYTES, RECORD_BYTES);
    if (!rec)
        return 0;
    st->rec = rec;
    *n = rd32(rec + 4);
    return exe_at(QUADS + (unsigned)(rd32(rec + 0x48) + *n - 1) * 11, 11);
}

/* a quad's tile (a pointer in the program): +0 the texture's columns shift, +4 which
   column, +8 the first of its sizes */
static const unsigned char *quad_tile(const unsigned char *q)
{
    return exe_at((unsigned)rd32(q + 6), 12);
}

static unsigned char *quad_texture(const rows_state *st, const unsigned char *q,
                                   const unsigned char *tile)
{
    int row = st->sec[2 + q[4]], size = rd32(tile + 8) + st->variant;
    return row < ROAD_TEXTURES && size >= 0 && size < 8 ? g_fam.texture[row][size] : 0;
}

/* the sections from `drawn` - 1 ahead down to `near` (0x59d3c): quads whose far corner
   is in front, painted whole */
static void far_sections(int drawn, int node, int nearer)
{
    road_state *r = &g_road;
    int first = r->count - 11, row = 4 * drawn - 4, sec = node / 4 + drawn - 1, i, n;
    rows_state st;

    st.row = 4 * drawn + 3 - (node & 3);
    st.variant = table_byte(FAR_VARIANT, st.row / 4);
    st.row_node = (int)((unsigned)((node & ~3) + 4 * drawn - 1) & g_track.node_mask);
    /* (0x143e60, a bit for the row, is set here for 0x5a0a8, not taken in a race) */
    while (row >= 4 * nearer) {
        unsigned kind = r->sec[(unsigned)sec & r->sec_mask].kind;
        const unsigned char *q = section_quads(&st, kind, &n);
        if (!q)
            return;
        first -= rd32(st.rec);
        st.first = first;
        st.sec = track_section(sec);
        for (i = n - 1; i >= 0; i--, q -= 11) {
            const unsigned char *tile = quad_tile(q);
            scr2 s[4];
            int c, j;
            if (first < 0 || !tile)
                return;
            for (j = 0; j < 4; j++)
                s[j] = r->screen[first + q[j]];
            if (far_visible(s, q[4]) && r->view[first + q[3]].z > 0)
                g_road_paint.whole(quad_texture(&st, q, tile), s);
            if (!q[10])
                continue;
            walls(&st, tile);
            for (c = q[10]; c > 0; c--) {
                while (st.row_node == r->xnode)
                    draw_object(st.row, 1);
                cars(st.row_node + 1);
                st.row_node = (int)((unsigned)(st.row_node - 1) & g_track.node_mask);
            }
            st.row -= q[10];
            row -= q[10];
            st.variant = table_byte(FAR_VARIANT, st.row / 4);
        }
        sec = st.row_node >> 2;
    }
}

void road_draw(int drawn, int node, int near_z, const road_window *win)
{
    road_state *r = &g_road;
    int nearer = drawn > 4 ? 4 : drawn, first, sec, k, i, n;
    rows_state st;

    g_left = win->x;
    g_top = win->y;
    g_right = win->x + win->w;
    g_bottom = win->y + win->h;
    far_sections(drawn, node, nearer);

    first = r->count - 11 - (drawn - nearer) * 11;
    st.row = 4 * nearer + 3 - (node & 3);
    st.variant = table_byte(NEAR_VARIANT, st.row);
    sec = (int)((unsigned)(node / 4 + nearer - 1) & r->sec_mask);
    st.row_node = (int)((unsigned)((node & ~3) + 4 * nearer - 1) & g_track.node_mask);
    for (k = nearer - 1; k >= 0; k--, sec = (int)((unsigned)(sec - 1) & r->sec_mask)) {
        const road_section *rs = &r->sec[(unsigned)sec & r->sec_mask];
        const unsigned char *q = section_quads(&st, rs->kind, &n);
        if (!q)
            break;
        first -= rs->points;
        st.first = first;
        st.sec = track_section(sec);
        for (i = n - 1; i >= 0; i--, q -= 11) {
            const unsigned char *tile = quad_tile(q);
            const vec3 *p[4];
            scr2 s[4];
            int c, j;
            if (first < 0 || !tile)
                return;
            for (j = 0; j < 4; j++) {
                s[j] = r->screen[first + q[j]];
                p[j] = &r->view[first + q[j]];
            }
            if (near_visible(s, q[4])) {
                unsigned char *shape = quad_texture(&st, q, tile);
                if (shape) {
                    raster_tex t;
                    shape_tex(shape, &t);
                    g_road_paint.quad(&t, rd32(tile), 0, rd32(tile + 4), 0, p, s, near_z);
                }
            }
            if (!q[10])
                continue;
            walls(&st, tile);
            for (c = q[10]; c > 0; c--) {
                while (st.row_node == r->xnode)
                    draw_object(st.row, 1);
                if (g_road_paint.car_row)
                    g_road_paint.car_row(node, st.row_node, &s[0], &s[3]);
                cars(st.row_node + 1);
                st.row_node = (int)((unsigned)(st.row_node - 1) & g_track.node_mask);
                /* objects marked at +8 wait for the next row */
                while (r->xnode == st.row_node && !placement(r->xnext)[8])
                    draw_object(st.row, 1);
            }
            st.row -= q[10];
            st.variant = table_byte(NEAR_VARIANT, st.row);
        }
    }
    cars(st.row_node + 1);
    /* (with 0x143dcc, 0x5a0a8 after: not in a race) */
}
