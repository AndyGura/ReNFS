/* The road near the camera, into the view each frame (NOTES §4.7). */
#include "road.h"
#include "exe.h"
#include "track.h"

road_state g_road;

#define NEAR_PLANE 62259       /* 0xf6aa8, >> the far shift: 30 */
#define LIMIT_X 5376           /* 0xf6ab0: the near sections held inside these */
#define LIMIT_Y 5376           /* 0xf6ab4 */

void road_clip_direction(int section)
{
    int k = g_road.sec[section & g_road.sec_mask].points, z;
    g_view.clip_x = g_road.view[k].x - g_road.view[0].x;
    g_view.clip_y = g_road.view[k].y - g_road.view[0].y;
    g_view.clip_z = z = g_road.view[k].z - g_road.view[0].z;
    if (z <= 0)
        z = (int)-(unsigned)z;
    if (z < 0x1999)
        g_view.clip_z = 0x1999;
}

static void translate(int n, const vec3 *src, int shift, const vec3 *t, vec3 *dst)
{
    for (; n > 0; n--, src++, dst++) {
        dst->x = (src->x >> shift) + t->x;
        dst->y = (src->y >> shift) + t->y;
        dst->z = (src->z >> shift) + t->z;
    }
}

int road_transform(const vec3 *pos, const int m[9], int node)
{
    road_state *r = &g_road;
    int shift = g_view.far_shift, near = NEAR_PLANE >> shift, n = r->count, i, first;
    vec3 t;

    t.x = (int)-(unsigned)pos->x >> shift;
    t.y = (int)-(unsigned)pos->y >> shift;
    t.z = (int)-(unsigned)pos->z >> shift;
    translate(n, r->world, shift, &t, r->near);
    if (r->xsets > 0)
        translate(r->xcount, r->xworld, shift, &t, r->xmoved);
    for (i = 0; i < n; i++)
        rotate(&r->near[i], m, &r->view[i]);
    if (r->xsets > 0)
        for (i = 0; i < r->xcount; i++)
            rotate(&r->xmoved[i], m, &r->xview[i]);
    for (i = 0; i < n; i++)
        r->near[i] = r->view[i];
    road_clip_direction(node / 4);

    /* the nearest sections' points held inside the limits (a quarter across when very
       near) and off the near plane; the others off eight times the near plane */
    first = n - (r->drawn - 3) * 11;
    for (i = 0; i < first; i++) {
        vec3 *p = &r->near[i];
        int lim = p->z < 2 * near ? LIMIT_X / 4 : LIMIT_X;
        if (lim < p->x)
            p->x = lim;
        else if (-lim > p->x)
            p->x = -lim;
        if (LIMIT_Y < p->y)
            p->y = LIMIT_Y;
        else if (-LIMIT_Y > p->y)
            p->y = -LIMIT_Y;
        if (near > p->z)
            move_to_near(p, near);
    }
    for (i = first > 0 ? first : 0; i < n; i++)
        if (near * 8 > r->near[i].z)
            move_to_near(&r->near[i], near * 8);
    project_points(n, r->near, r->screen, 16);
    if (r->xsets > 0)
        project_points(r->xcount, r->xview, r->xscreen, 16);
    return near;
}

int road_transform_behind(const vec3 *pos, const int m[9])
{
    road_state *r = &g_road;
    int shift = g_view.far_shift, near = NEAR_PLANE >> shift, close = 0x23333 >> shift, i;
    vec3 t, moved[BEHIND_POINTS];

    t.x = (int)-(unsigned)pos->x >> shift;
    t.y = (int)-(unsigned)pos->y >> shift;
    t.z = (int)-(unsigned)pos->z >> shift;
    translate(r->bcount, r->bworld, shift, &t, moved);
    for (i = 0; i < r->bcount; i++)
        rotate(&moved[i], m, &r->bview[i]);
    project_points(r->bcount, r->bview, r->bscreen, 16);
    /* the points nearer than 70 projected again as if at 140 */
    for (i = 0; i < r->bcount; i++)
        if (close > r->bview[i].z) {
            vec3 p = r->bview[i];
            p.z = 2 * close;
            project_points(1, &p, &r->bscreen[i], 16);
        }
    return near;
}

/* the game's tables of the road's detail (in NFS_DOS.EXE): a kind for each node type and
   distance ahead (8 rows of 32), and a record each kind whose +0x18 lists, a row at a
   time, the points of the row kept (0xff ends a row) */
#define DETAIL_KINDS 0xed284
#define DETAIL_RECORDS 0xf61bc
#define DETAIL_BYTES 0x4c
#define BEHIND_KINDS 0xedc84   /* the mirror's: a kind for each node type, 24 bytes apart */

static int rd32(const unsigned char *p)
{
    return (int)((unsigned)p[0] | (unsigned)p[1] << 8 | (unsigned)p[2] << 16 | (unsigned)p[3] << 24);
}

static int rd16(const unsigned char *p)
{
    return (short)(p[0] | p[1] << 8);
}

static const unsigned char *placement(int i)
{
    return g_track.placements + i * 16;
}

static void add_x(int x, int y, int z)
{
    vec3 *p = &g_road.xworld[g_road.xfill++];
    p->x = x;
    p->y = y;
    p->z = z;
}

/* 0x59998: the first placement at or after section s, by halves */
static void first_object(int s)
{
    road_state *r = &g_road;
    int t = s * 4, n = g_track.nplacements, lo = 0, hi, mid, v;
    r->xfirst = 0;
    if (rd32(placement(0)) >= 0) {
        hi = n - 1;
        mid = hi / 2;
        while (lo + 1 < hi) {
            mid = (hi + lo) / 2;
            if (t > rd32(placement(mid)))
                lo = mid;
            else
                hi = mid;
        }
        /* back past the placements of the same node (and one more) */
        v = rd32(placement(mid));
        if (mid > 0)
            while (rd32(placement(mid)) == v) {
                mid--;
                if (mid <= 0)
                    break;
            }
        while (t > rd32(placement(mid)) && mid < n - 1)
            mid++;
        r->xfirst = mid;
    }
    r->xnext = r->xfirst;
    r->xfill = r->xsets = r->xcount = 0;
    r->xnode = rd32(placement(r->xnext));
}

/* an object's corners: four (a board, standing or fallen over), and two more for kind 6;
   kind 1 is a single point */
static void object_points(int row)
{
    road_state *r = &g_road;
    const unsigned char *pl = placement(r->xnext), *nd = node_ptr(r->xnode), *k;
    int x, y, z, a, b, c, s, h, along, across, hw;

    x = rd32(nd + 8) + (int)((unsigned)rd16(pl + 0xa) << 8);
    y = rd32(nd + 12) + (int)((unsigned)rd16(pl + 0xc) << 8);
    z = rd32(nd + 16) + (int)((unsigned)rd16(pl + 0xe) << 8);
    k = g_track.objects + pl[4] * 16;
    if (k[1] == 1) {
        add_x(x, y, z);
        r->xcount++;
        return;
    }
    hw = rd32(k + 4) / 2 >> 8;
    a = (int)((unsigned)-pl[5] << 16) - (int)((unsigned)rd16(nd + 0x18) << 10);
    c = hw * (fix_cos(a >> 14) >> 8);
    s = hw * (fix_sin(a >> 14) >> 8);
    if (g_track.fallen[r->xnext * 2] == 0) {
        h = rd32(k + 0xc);
        along = across = 0;
    } else {
        /* lying on the ground, its top towards the road */
        h = 0;
        b = (int)((unsigned)-pl[5] << 16);
        if (b < -0x400000)
            b += 0x800000;
        a = b + 0x400000 - (int)((unsigned)rd16(node_ptr(row) + 0x18) << 10);
        along = (fix_cos(a >> 14) >> 8) * (rd32(k + 0xc) >> 8);
        across = (rd32(k + 0xc) >> 8) * (fix_sin(a >> 14) >> 8);
    }
    add_x(x - c + along, y + h, z - s + across);
    add_x(x + c + along, y + h, z + s + across);
    add_x(x + c, y, z + s);
    add_x(x - c, y, z - s);
    r->xcount += 4;
    if (k[1] == 6) {
        const vec3 *q = &r->xworld[r->xfill - 2];
        int w = rd32(k + 8) >> 8, qx = q->x, qy = q->y, qz = q->z;
        a = (a + 0x400000) >> 14;
        c = w * (fix_cos(a) >> 8);
        s = w * (fix_sin(a) >> 8);
        add_x(qx + c, qy + h, qz + s);
        add_x(qx + c, qy, qz + s);
        r->xcount += 2;
    }
}

/* 0x4f7d4: section sec's points the detail keeps, `rows` rows of it (4, or 1 for the far
   edge), and with `objects` the objects placed on its rows */
static void section_points(int base, int sec, int rows, int objects)
{
    road_state *r = &g_road;
    int row = (int)((unsigned)(sec * 4) & g_track.node_mask), first = r->fill, kind, i, idx;
    const unsigned char *ring = track_section(sec) + 12, *list;
    road_section *e;

    kind = exe_i32(DETAIL_KINDS + node_ptr(row)[7] * 128 + (sec - base) * 4);
    list = exe_at(DETAIL_RECORDS + kind * DETAIL_BYTES + 0x18, 1);
    for (i = 0; i < rows; i++, ring += 0x84) {
        if (*list != 0xff) {
            idx = 0;
            do {
                if (*list == idx) {
                    vec3 *p = &r->world[r->fill++];
                    p->x = rd32(ring + idx * 12);
                    p->y = rd32(ring + idx * 12 + 4);
                    p->z = rd32(ring + idx * 12 + 8);
                    list++;
                }
                idx++;
            } while (*list != 0xff);
        }
        list++;
        while (objects && row == r->xnode) {
            object_points(row);
            r->xsets++;
            if (++r->xnext != g_track.nplacements)
                r->xnode = rd32(placement(r->xnext));
            else {
                r->xnext = 0;
                r->xnode = g_track.loop ? rd32(placement(0)) : -1;
                if (r->xnode == row)
                    r->xnode = -1;
            }
        }
        row = (int)((unsigned)(row + 1) & g_track.node_mask);
    }
    e = &r->sec[(unsigned)sec & g_track.sec_mask];
    e->kind = (unsigned char)kind;
    e->a = (short)first;
    e->points = (short)(r->fill - first);
    r->count += e->points;
}

void road_gather(int node, int sec, int drawn)
{
    road_state *r = &g_road;
    int base = node / 4, i;
    g_track.last_node = node;
    /* (the original's are the globals themselves: 0x13bb90 is the track's mask, 0x136618
       the count 0x50070 hands over) */
    r->sec_mask = g_track.sec_mask;
    r->drawn = drawn;
    r->base = base;
    r->fill = r->count = 0;
    first_object((int)((unsigned)base & g_track.sec_mask));
    for (i = 0; i < drawn - 1; i++)
        section_points(base, sec++, 4, 1);
    section_points(base, sec, 4, 0);
    section_points(base, sec + 1, 1, 0);
}

void road_refresh(void)
{
    g_track.last_node = -1;
    g_track.last_kinds = -1;
}

/* 0x4fed8: section sec's first row as the mirror's detail keeps it */
static void behind_points(int sec)
{
    road_state *r = &g_road;
    const unsigned char *ring = track_section(sec) + 12, *list;
    int kind = exe_i32(BEHIND_KINDS + node_ptr((int)((unsigned)(sec * 4) & g_track.node_mask))[7] * 24), i;

    list = exe_at(DETAIL_RECORDS + kind * DETAIL_BYTES + 0x18, 1);
    for (i = 0; i < 11; i++)
        if (*list == i) {
            vec3 *p = &r->bworld[r->bcount++];
            p->x = rd32(ring + i * 12);
            p->y = rd32(ring + i * 12 + 4);
            p->z = rd32(ring + i * 12 + 8);
            list++;
        }
    r->sec[(unsigned)sec & g_track.sec_mask].behind = (unsigned char)kind;
}

void road_gather_behind(int node)
{
    int sec = (int)((unsigned)((node + 3) / 4 - 4) & g_track.sec_mask), k;

    if (sec == g_track.last_kinds)
        return;
    g_track.last_kinds = sec;
    g_road.bcount = 0;
    for (k = 0; k <= 4; k++) {
        behind_points(!g_track.loop && sec < 0 ? 0 : sec);
        sec = (int)((unsigned)(sec + 1) & g_track.sec_mask);
    }
}
