/* The tyres' marks on the road (NOTES §4.10's end). */
#include "skid.h"
#include "cardraw.h"
#include "collide.h"
#include "config.h"
#include "drive.h"
#include "exe.h"
#include "polyfill.h"
#include "race.h"
#include "racekeys.h"
#include "roaddraw.h"
#include "smoke.h"
#include "track.h"
#include "view.h"
#include <string.h>

skid_state g_skid;
void (*g_skid_line)(int x0, int y0, int x1, int y1, int w) = skid_line;

static void puff(int dirt, int pick, int size, int x, int y)
{
    if (dirt)
        smoke_stretch_solid(size, &g_smoke.dirt[pick], x, y);
    else
        smoke_stretch(size, &g_smoke.dust[pick], x, y);
}

void (*g_skid_puff)(int dirt, int pick, int size, int x, int y) = puff;

skid_record *skid_at(int i)
{
    return &g_skid.rec[i + 1];
}

static int next(int i)
{
    return i + 1 == SKID_RECORDS ? 0 : i + 1;
}

static int back(int i)
{
    return i - 1 < 0 ? SKID_RECORDS - 1 : i - 1;
}

static int iabs(int v)
{
    return v > 0 ? v : -v;
}

static int mul(int a, int b)
{
    return (int)((unsigned)a * (unsigned)b);
}

/* the settings' +0x87 (0x12d9d7): the cars in less detail, no marks */
static int marks_off(void)
{
    return config_word(0x87) & 0xff;
}

void skid_reset(void)
{
    int i;

    for (i = 0; i < SKID_RECORDS; i++) {
        skid_at(i)->node = i ? 0 : 0x10000;
        skid_at(i)->front = skid_at(i)->rear = 0;
    }
    g_skid.at = 0;
    memcpy(g_skid.random, g_collide.random, sizeof g_skid.random);
}

/* 0x14e5f3 + d: the rows' marks, and past their 15 the loading pictures' used flags
   (0x14e602: the one shown this race) */
static int row_byte(int d)
{
    if (d < 15)
        return g_skid.rows[d];
    return d - 15 == g_race_slide;
}

void skid_rows(int cam_node)
{
    int base = (int)((unsigned)(cam_node - 2) & g_track.node_mask), i, d;

    if (marks_off())
        return;
    memset(g_skid.rows, 0, sizeof g_skid.rows);
    for (i = 0; i < SKID_RECORDS; i++) {
        const skid_record *r = skid_at(i);
        if (!r->front && !r->rear)
            continue;
        d = r->node < base ? r->node + g_track.length * 4 : r->node;
        d -= base;
        if (d >= 0 && d < 15)
            g_skid.rows[d] = 1;
    }
}

void skid_line(int x0, int y0, int x1, int y1, int w)
{
    int dy = y1 - y0, a, u, v, hu, hv, xy[8];

    a = ea_atan_1024(x1 - x0, dy + (dy >> 2));
    u = fix_mul(w, -fix_cos(a));
    v = fix_mul(w, fix_sin(a));
    v = v - (v >> 2) + (v >> 4);
    hu = u >> 1;
    hv = v >> 1;
    xy[0] = x0 + hu;
    xy[1] = y0 + hv;
    xy[2] = x1 + hu;
    xy[3] = y1 + hv;
    xy[4] = x1 - (u - hu);
    xy[5] = y1 - (v - hv);
    xy[6] = x0 - (u - hu);
    xy[7] = y0 - (v - hv);
    poly_fill(4, xy, poly_shade_rows, 0);
}

/* which side of the line a -> b (x, y) is on: >= 0 beyond it */
static int side(const scr2 *a, const scr2 *b, int x, int y)
{
    return mul(b->y - a->y, x - a->x) - mul(b->x - a->x, y - a->y);
}

/* m moved along to o until it is 0x40 ahead */
static void cut(vec3 *m, const vec3 *o)
{
    int den = o->z - m->z + 1, t = o->z - 0x40;

    m->z = 0x40;
    m->x = mul(m->x - o->x, t) / den + o->x;
    m->y = mul(m->y - o->y, t) / den + o->y;
}

/* 0x679ec: the records from i back, each seen from the camera and joined to the one
   before it (newer) by a line for each tyre that left marks on both, while they lie
   within 2 nodes of `node`: count records (0: as many as the ring holds less two), the
   lines cut by the row's edge a -> b when there is one */
static void paint(int i, int node, int count, const scr2 *a, const scr2 *b)
{
    int joined = 0, prev = i, n, k;

    for (n = 0; n < SKID_RECORDS - 2; n++) {
        skid_record *r = skid_at(i), *p = skid_at(prev);
        int dn = node - r->node;

        if (dn < -2 || dn > 2)
            joined = 0;
        else {
            vec3 d, ctr;

            d.x = (r->centre.x - g_camera->pos.x) >> 9;
            d.y = (r->centre.y - g_camera->pos.y) >> 9;
            d.z = (r->centre.z - g_camera->pos.z) >> 9;
            rotate(&d, g_camera_m, &ctr);
            for (k = 0; k < 4; k++) {
                vec3 *cv = &r->view[k], *pv = &p->view[k];
                int z, w;

                rotate(&r->corner[k], g_camera_m, cv);
                cv->x += ctr.x;
                cv->y += ctr.y;
                cv->z += ctr.z;
                if (joined && (cv->z < 0x40 || pv->z < 0x40)) {
                    if (cv->z <= 0x40) {
                        if (pv->z < 0x40)
                            continue;          /* both behind: nothing, not projected */
                        cut(cv, pv);
                    } else {
                        cut(pv, cv);
                        project(pv, &p->screen[k]);
                    }
                }
                project(cv, &r->screen[k]);
                z = pv->z < cv->z ? cv->z : pv->z;
                w = z ? 0x2580 / z : 100;
                if (w > 15)
                    w = 15;
                w = H(w / 2, w);
                if (g_rkeys.window)
                    w /= 2;
                if (w < 1)
                    w = 1;
                if (!joined)
                    continue;
                if (k < 2 ? !p->front || !r->front : !p->rear || !r->rear)
                    continue;
                if (a && side(a, b, r->screen[k].x, r->screen[k].y) < 0
                    && side(a, b, p->screen[k].x, p->screen[k].y) < 0)
                    continue;          /* (0x67950) */
                g_skid_line(r->screen[k].x, r->screen[k].y, p->screen[k].x, p->screen[k].y, w);
            }
            joined = 1;
        }
        prev = i;
        i = back(i);
        if (count && --count <= 0)
            break;
    }
}

void skid_paint_row(int cam_node, int row_node, const scr2 *a, const scr2 *b)
{
    int base = (int)((unsigned)(cam_node - 2) & g_track.node_mask), d;

    if (marks_off())
        return;
    d = (base > row_node ? g_track.length * 4 + row_node : row_node) - base;
    if (d > 15 || !row_byte(d))
        return;
    paint(back(g_skid.at), row_node, 0, a, b);
}

void skid_record_car(const race_car *c, const vec3 *along, const vec3 *across,
                     const vec3 *centre)
{
    int at = g_skid.at, front, rear, d, k;
    vec3 f;
    skid_record *r;

    if (marks_off())
        return;
    front = c->slide_front >= 0x3c0000 && (c->tyres[0] & 3) ? c->slide_front : 0;
    rear = c->slide_rear >= 0x3c0000 && (c->tyres[1] & 3) ? c->slide_rear : 0;
    if (front || rear) {
        f.x = along->x - along->x / 4;
        f.y = along->y - along->y / 4;
        f.z = along->z - along->z / 4;
        r = skid_at(at);
        r->centre = *centre;
        r->node = c->node;
        r->step = g_road_steps;
        r->kind = (g_surfaces[c->surface][3] != 0) + 1;
        r->front = front ? g_skid.front : 0;
        if (front) {
            r->corner[0].x = f.x + across->x;
            r->corner[0].y = f.y + across->y;
            r->corner[0].z = f.z + across->z;
            r->corner[1].x = f.x - across->x;
            r->corner[1].y = f.y - across->y;
            r->corner[1].z = f.z - across->z;
        }
        r->rear = rear ? g_skid.rear : 0;
        if (rear) {
            r->corner[2].x = -f.x + across->x;
            r->corner[2].y = -f.y + across->y;
            r->corner[2].z = -f.z + across->z;
            r->corner[3].x = -f.x - across->x;
            r->corner[3].y = -f.y - across->y;
            r->corner[3].z = -f.z - across->z;
        }
        d = iabs(g_skid.last.z - centre->z) + iabs(g_skid.last.x - centre->x);
        if (d <= 0xa666) {
            /* not far from where this record began: it moves on with the car */
            if (rear) {
                g_skid.at = at;
                paint(at, skid_at(at)->node, 3, 0, 0);
                at = g_skid.at;
            }
        } else {
            int was = at;

            at = next(at);
            skid_at(at)->front = skid_at(at)->rear = 0;
            if (d >= 0x30000) {
                /* a long way: when the record before this one left the same marks, a
                   copy of this one put in after it and this one moved half way back (the
                   one before the first is taken from past the ring's end: empty) */
                skid_record *w = skid_at(was), *e = skid_at(was ? was - 1 : SKID_RECORDS);

                if ((e->front && w->front) || (e->rear && w->rear)) {
                    *skid_at(at) = *w;
                    for (k = 0; k < 4; k++) {
                        w->corner[k].x = (int)((unsigned)w->corner[k].x + (unsigned)e->corner[k].x) / 2;
                        w->corner[k].y = (int)((unsigned)w->corner[k].y + (unsigned)e->corner[k].y) / 2;
                        w->corner[k].z = (int)((unsigned)w->corner[k].z + (unsigned)e->corner[k].z) / 2;
                    }
                    at = next(at);
                }
            }
            skid_at(next(at))->front = skid_at(next(at))->rear = 0;
            g_skid.last = *centre;
            if (rear) {
                g_skid.at = at;
                paint(at - 1, skid_at(at - 1)->node, 3, 0, 0);
                at = g_skid.at;
            }
        }
    }
    g_skid.front = front;
    g_skid.rear = rear;
    g_skid.at = at;
}

/* 0x663e0: a tyre's dust at its last four records (from 0xf6e2c - 1 back) that left its
   pair's mark less than 48 steps ago and are on the screen, size going up by `amount`
   each one older; on the road (0x64714) the smoke's smo1 or smo3, elsewhere (0x64768)
   dir1..dir3 at most 1:1, both halved at 320x200 */
static void wheel_dust(int wheel, int amount)
{
    int i = back(g_skid.at), size = amount, n;

    for (n = 0; n < 4; n++, i = back(i), size += amount) {
        const skid_record *r = skid_at(i);
        int age = g_road_steps - r->step, x = r->screen[wheel].x, y = r->screen[wheel].y, s;

        if (!(wheel < 2 ? r->front : r->rear) || age >= 0x30 || age <= 0 || x == (int)0x80000000)
            continue;
        if (r->kind == 1) {
            int pick = 2 * collide_below_with(g_skid.random, 2);
            g_skid_puff(0, pick, H(size / 2, size), x, y);
        } else {
            int pick = collide_below_with(g_skid.random, 3);
            s = H(size / 2, size);
            g_skid_puff(1, pick, s > 0x10000 ? 0x10000 : s, x, y);
        }
    }
}

/* a pair's dust's step: its slide times 2..5 (EA's random) >> 18 towards 0, 1 .. 0x180 */
static int dust_step(int slide, int base)
{
    int k = (int)((unsigned)slide * (unsigned)(collide_below_with(g_skid.random, 4) + 2)) / 0x40000;

    if (k > 0x180)
        k = 0x180;
    if (k < 1)
        k = 1;
    return base / 256 * k;
}

void skid_dust(const race_car *c, int behind)
{
    int a, base, amount;

    if (!c->tyres[0] && !c->tyres[1])
        return;
    if (!c->slide_front && !c->slide_rear)
        return;
    a = car_seen_angle(c);
    if (c->far)
        return;
    base = behind && !exe_i32(0xf6b8c) ? 0x8000 : 0x10000;
    base = base / 256 * (0x38400 / c->seen.z);
    if (c->slide_front) {
        amount = dust_step(c->slide_front, base);
        if (behind || (a > 0xc0000 && a < 0x740000))
            wheel_dust(0, amount);
        if (behind || (a < -0xc0000 && a > -0x740000))
            wheel_dust(1, amount);
    }
    if (c->slide_rear) {
        amount = dust_step(c->slide_rear, base);
        if (behind || (a > 0xc0000 && a < 0x740000))
            wheel_dust(2, amount);
        if (behind || (a < -0xc0000 && a > -0x740000))
            wheel_dust(3, amount);
    }
}
