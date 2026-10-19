/* Cars meeting cars (NOTES §4.14): see collide.h. 16.16 fixed point as crash.c: mulr
   rounds by the last bit shifted out, p8 multiplies two values each shifted down by 8
   first, 32-bit sums wrap. A body's turn holds its axes across, up and along as rows;
   its half sizes along them are half[0..2]. The program's switches for these routines
   (0xea62c .. 0xea64c) are never written: only the paths they leave live are here. */
#include "collide.h"
#include "cardraw.h"
#include "crash.h"
#include "exe.h"
#include "timing.h"
#include "track.h"

collide_race g_collide;

static int mulr(int a, int b)
{
    long long p = (long long)a * b;
    return (int)(p >> 16) + (int)((p >> 15) & 1);
}

static int iabs(int v)
{
    return v < 0 ? (int)(0u - (unsigned)v) : v;
}

static int p8(int a, int b)
{
    return (int)((unsigned)(a >> 8) * (unsigned)(b >> 8));
}

static int dot8(const vec3 *a, const vec3 *b)
{
    return p8(a->x, b->x) + p8(a->y, b->y) + p8(a->z, b->z);
}

/* 0x2ce20 */
static int dot_r(const vec3 *a, const vec3 *b)
{
    return mulr(a->x, b->x) + mulr(a->y, b->y) + mulr(a->z, b->z);
}

/* 0x3e880: a × b by rounded multiplies */
static vec3 cross_r(const vec3 *a, const vec3 *b)
{
    vec3 r;

    r.x = mulr(a->y, b->z) - mulr(a->z, b->y);
    r.y = mulr(a->z, b->x) - mulr(a->x, b->z);
    r.z = mulr(a->x, b->y) - mulr(a->y, b->x);
    return r;
}

static const vec3 *axis(const car_body *b, int r)
{
    return (const vec3 *)&b->m[3 * r];
}

static int max3(const vec3 *v)
{
    int x = iabs(v->x), y = iabs(v->y), z = iabs(v->z), m = x > y ? x : y;

    return m > z ? m : z;
}

static vec3 neg(vec3 v)
{
    v.x = -v.x;
    v.y = -v.y;
    v.z = -v.z;
    return v;
}

/* ---- EA's random numbers ----------------------------------------------------------- */

void collide_init(void)
{
    int k;

    for (k = 0; k < 6; k++)
        g_collide.random[k] = (unsigned)exe_i32(0xf8a8c + 4 * k);
}

/* 0x91e04: the words from the fifth down added up with their carries into the next, the
   last counted up (its carry running up through them) */
static unsigned ea_rand(void)
{
    unsigned *s = g_collide.random, a = s[5], cf = 0;
    unsigned long long t;
    int k;

    for (k = 4; k >= 0; k--) {
        t = (unsigned long long)a + s[k] + cf;
        a = (unsigned)t;
        cf = (unsigned)(t >> 32);
        if (k)
            s[k] = a;
    }
    if (++s[5] == 0 && ++s[4] == 0 && ++s[3] == 0 && ++s[2] == 0 && ++s[1] == 0)
        a++;
    s[0] = a;
    return a;
}

/* 0x52580: 0 .. n - 1 */
static int rand_below(int n)
{
    return (int)((ea_rand() & 0xffff) * (unsigned)n) >> 16;
}

/* ---- two boxes ----------------------------------------------------------------------- */

/* p within the box's half sizes (its own axes), the faces included */
static int inside(const car_body *b, const int *p)
{
    int k;

    for (k = 0; k < 3; k++)
        if (p[k] > b->half[k] || p[k] < -b->half[k])
            return 0;
    return 1;
}

/* the box's point at s0, s1, s2 (each -1, 0 or 1) of its half sizes along its axes */
static vec3 box_point(const car_body *b, const int *s)
{
    vec3 p = b->pos;
    int r;

    for (r = 0; r < 3; r++) {
        const vec3 *e = axis(b, r);

        if (!s[r])
            continue;
        if (s[r] > 0) {
            p.x += p8(e->x, b->half[r]);
            p.y += p8(e->y, b->half[r]);
            p.z += p8(e->z, b->half[r]);
        } else {
            p.x -= p8(e->x, b->half[r]);
            p.y -= p8(e->y, b->half[r]);
            p.z -= p8(e->z, b->half[r]);
        }
    }
    return p;
}

/* the face a corner pushes on: in a fast hit (the two bodies' speeds more than 15 apart
   along an axis) the box's axis most along v (the moving body's speed); else the face
   the corner is least deep behind. Turned to face the corner's side: away from it
   (out) or towards it */
static vec3 corner_face(const car_body *box, const car_body *a, const car_body *b,
                        const int *p, const vec3 *v, int out)
{
    vec3 rel = { b->vel.x - a->vel.x, b->vel.y - a->vel.y, b->vel.z - a->vel.z }, e[3];
    int d[3], k;

    for (k = 0; k < 3; k++) {
        e[k] = *axis(box, k);
        if (out ? p[k] > 0 : p[k] < 0)
            e[k] = neg(e[k]);
    }
    if (max3(&rel) > 0xf0000) {
        for (k = 0; k < 3; k++)
            d[k] = iabs(dot8(&e[k], v));
        if (d[0] > d[1] && d[0] > d[2])
            return e[0];
        return d[1] > d[2] ? e[1] : e[2];
    }
    for (k = 0; k < 3; k++)
        d[k] = p[k] < 0 ? box->half[k] + p[k] : box->half[k] - p[k];
    if (d[0] < d[1] && d[0] < d[2])
        return e[0];
    return d[1] < d[2] ? e[1] : e[2];
}

/* the corners' signs, in the order tried */
static const int corner_signs[8][3] = {
    { 1, 1, 1 }, { 1, 1, -1 }, { 1, -1, 1 }, { 1, -1, -1 },
    { -1, 1, 1 }, { -1, 1, -1 }, { -1, -1, 1 }, { -1, -1, -1 },
};

/* 0x26324: do boxes a and b touch, and where (pt) and which way (n, against a)? First
   the middles of b's faces inside a, then a's inside b, then b's corners inside a, a's
   inside b. q5[r][c] is b's axis c (times its half size) in a's frame, q6[r][c] a's
   axis r (times its half size) in b's; ra, rb where b is from a in a's and b's frames */
static int box_contact(const car_body *a, const car_body *b, vec3 *pt, vec3 *n,
                       const int *q5, const int *q6, const int *ra, const int *rb)
{
    int s, c, r, p[3], k;

    for (s = 1; s >= -1; s -= 2)
        for (c = 0; c < 3; c++) {
            int at[3] = { 0, 0, 0 };

            for (r = 0; r < 3; r++)
                p[r] = ra[r] + s * q5[r * 3 + c];
            if (!inside(a, p))
                continue;
            *n = s > 0 ? *axis(b, c) : neg(*axis(b, c));
            at[c] = s;
            *pt = box_point(b, at);
            return 1;
        }
    for (s = 1; s >= -1; s -= 2)
        for (r = 0; r < 3; r++) {
            int at[3] = { 0, 0, 0 };

            for (c = 0; c < 3; c++)
                p[c] = s * q6[r * 3 + c] - rb[c];
            if (!inside(b, p))
                continue;
            *n = s > 0 ? neg(*axis(a, r)) : *axis(a, r);
            at[r] = s;
            *pt = box_point(a, at);
            return 1;
        }
    for (k = 0; k < 8; k++) {
        const int *sg = corner_signs[k];

        for (r = 0; r < 3; r++)
            p[r] = ra[r] + sg[0] * q5[r * 3] + sg[1] * q5[r * 3 + 1] + sg[2] * q5[r * 3 + 2];
        if (inside(a, p)) {
            *pt = box_point(b, sg);
            *n = corner_face(a, a, b, p, &b->vel, 1);
            return 1;
        }
    }
    for (k = 0; k < 8; k++) {
        const int *sg = corner_signs[k];

        for (c = 0; c < 3; c++)
            p[c] = sg[0] * q6[c] + sg[1] * q6[3 + c] + sg[2] * q6[6 + c] - rb[c];
        if (inside(b, p)) {
            *pt = box_point(a, sg);
            *n = corner_face(b, a, b, p, &a->vel, 0);
            return 1;
        }
    }
    return 0;
}

/* 0x2bf9c: v made a unit long, halved first as far as its squares need */
static void unit_halved(vec3 *v)
{
    int x = v->x, y = v->y, z = v->z, sx, sy, sz, len;

    while (iabs(x) > 0x1000000 || iabs(y) > 0x1000000 || iabs(z) > 0x1000000) {
        x >>= 1;
        y >>= 1;
        z >>= 1;
    }
    for (;;) {
        sx = mulr(x, x);
        sy = mulr(y, y);
        sz = mulr(z, z);
        if ((unsigned)sx / 4 + (unsigned)sy / 4 + (unsigned)sz / 4 <= 0x40000000u)
            break;
        x >>= 1;
        y >>= 1;
        z >>= 1;
    }
    sx += sy + sz;
    len = fix_sqrt(sx, 8, sx);
    if (!len)
        return;
    v->x = fix_div(x, len);
    v->y = fix_div(y, len);
    v->z = fix_div(z, len);
}

/* 0x2c1c0: v made a unit long */
static void unit(vec3 *v)
{
    int s = mulr(v->x, v->x) + mulr(v->y, v->y) + mulr(v->z, v->z), len = fix_sqrt(s, 8, s), k;

    if (!len)
        return;
    k = fix_div(0x10000, len);
    v->x = mulr(v->x, k);
    v->y = mulr(v->y, k);
    v->z = mulr(v->z, k);
}

/* 0x2b864: boxes a and b pushed apart until they no longer touch, 32 times at most, by
   a push chosen at the first touch: along their own speeds in a fast hit (backwards, by
   1 over their difference), else apart along the line between their middles, each by
   the other's share of the weight. 1 if they touched, with where and which way (the
   last touch's) */
static int box_separate(car_body *a, car_body *b, vec3 *pt, vec3 *n)
{
    const vec3 *ar[3] = { axis(a, 0), axis(a, 1), axis(a, 2) };
    const vec3 *br[3] = { axis(b, 0), axis(b, 1), axis(b, 2) };
    int m[9], q5[9], q6[9], r, c, k, hit = 0;
    vec3 pa = { 0, 0, 0 }, pb = { 0, 0, 0 };

    for (r = 0; r < 3; r++)
        for (c = 0; c < 3; c++) {
            m[r * 3 + c] = dot8(ar[r], br[c]);
            q5[r * 3 + c] = p8(m[r * 3 + c], b->half[c]);
            q6[r * 3 + c] = p8(m[r * 3 + c], a->half[r]);
        }
    for (k = 0; k < 32; k++) {
        vec3 d = { b->pos.x - a->pos.x, b->pos.y - a->pos.y, b->pos.z - a->pos.z };
        int ra[3], rb[3];

        for (r = 0; r < 3; r++) {
            ra[r] = dot8(ar[r], &d);
            rb[r] = dot8(br[r], &d);
        }
        if (!box_contact(a, b, pt, n, q5, q6, ra, rb))
            return hit;
        if (!hit) {
            vec3 rv = { a->vel.x - b->vel.x, a->vel.y - b->vel.y, a->vel.z - b->vel.z };
            int fast = 0;

            if (max3(&rv) > 0x140000) {
                if (max3(&d) >= 0x3333) {
                    vec3 u = rv, w = d;

                    unit_halved(&u);
                    crash_normalize(&w);
                    fast = dot8(&u, &w) > 0xd999;
                } else
                    fast = 1;
            }
            if (fast) {
                int e = (-crash_recip(crash_len3(&rv))) >> 8;

                pa.x = (e * (a->vel.x >> 8)) >> 5;
                pa.y = (e * (a->vel.y >> 8)) >> 5;
                pa.z = (e * (a->vel.z >> 8)) >> 5;
                pb.x = (e * (b->vel.x >> 8)) >> 5;
                pb.y = (e * (b->vel.y >> 8)) >> 5;
                pb.z = (e * (b->vel.z >> 8)) >> 5;
            } else {
                vec3 u = d;
                int sum, fa, fb;

                unit(&u);
                sum = a->mass + b->mass;
                fa = fix_div(a->mass, sum) >> 2;
                fb = fix_div(b->mass, sum) >> 2;
                pa.x = (-((u.x * fb) >> 14)) >> 5;
                pa.y = (-((u.y * fb) >> 14)) >> 5;
                pa.z = (-((u.z * fb) >> 14)) >> 5;
                pb.x = (u.x * fa) >> 19;
                pb.y = (u.y * fa) >> 19;
                pb.z = (u.z * fa) >> 19;
            }
            hit = 1;
        }
        a->pos.x += pa.x;
        a->pos.y += pa.y;
        a->pos.z += pa.z;
        b->pos.x += pb.x;
        b->pos.y += pb.y;
        b->pos.z += pb.z;
    }
    return hit;
}

/* 0x2afb8: the hit's push between car a's and b's bodies at pt along n (a's side): as
   big as stops them closing there (both weights, both turns' resistance), times
   0xea680; 0 if they are moving apart already. (A player's force feedback, 0x4d7a4:
   not in the port) */
static int box_impulse(race_car *ca, race_car *cb, const vec3 *pt, const vec3 *n)
{
    car_body *a = &ca->body, *b = &cb->body;
    vec3 rA = { pt->x - a->pos.x, pt->y - a->pos.y, pt->z - a->pos.z };
    vec3 rB = { pt->x - b->pos.x, pt->y - b->pos.y, pt->z - b->pos.z };
    vec3 cA = cross_r(&rA, n), cB = cross_r(&rB, n), p, t;
    int num, den, j;

    num = dot_r(&b->vel, n) - dot_r(&a->vel, n) - dot_r(&a->spin, &cA) + dot_r(&b->spin, &cB);
    den = (a->inv_mass >> 1) + (b->inv_mass >> 1);
    den += mulr(a->inv_inertia, dot_r(&cA, &cA)) >> 1;
    den += mulr(b->inv_inertia, dot_r(&cB, &cB)) >> 1;
    j = fix_div(num, den);
    if (j < 0)
        return 0;
    j = mulr(exe_i32(0xea680), j);
    g_collide.impulse = j;
    p.x = mulr(j, n->x);
    p.y = mulr(j, n->y);
    p.z = mulr(j, n->z);
    a->vel.x += mulr(a->inv_mass, p.x);
    a->vel.y += mulr(a->inv_mass, p.y);
    a->vel.z += mulr(a->inv_mass, p.z);
    b->vel.x -= mulr(b->inv_mass, p.x);
    b->vel.y -= mulr(b->inv_mass, p.y);
    b->vel.z -= mulr(b->inv_mass, p.z);
    t = cross_r(&rA, &p);
    a->spin.x += mulr(a->inv_inertia, t.x);
    a->spin.y += mulr(a->inv_inertia, t.y);
    a->spin.z += mulr(a->inv_inertia, t.z);
    t = cross_r(&rB, &p);
    b->spin.x -= mulr(b->inv_inertia, t.x);
    b->spin.y -= mulr(b->inv_inertia, t.y);
    b->spin.z -= mulr(b->inv_inertia, t.z);
    return 1;
}

/* ---- two cars ------------------------------------------------------------------------- */

static int human(const race_car *c)
{
    return c->slot >= 0 && c->slot < g_humans;
}

/* 0x1e838: a player's car knocked over by a hard hit: its spin cut to a third and given
   a turn about the road's across and along, a jump up its own axis */
static void knock(race_car *c)
{
    car_body *b = &c->body;
    int k = exe_i32(0xea5b8) >> 8, s = exe_i32(0xea5bc) >> 8, f = exe_i32(0xea5c0) >> 8;
    int u = exe_i32(0xea5c4) >> 8;

    b->spin.x = (b->spin.x >> 8) * k;
    b->spin.y = (b->spin.y >> 8) * k;
    b->spin.z = (b->spin.z >> 8) * k;
    b->spin.x += s * (c->road_side.x >> 8);
    b->spin.y += s * (c->road_side.y >> 8);
    b->spin.z += s * (c->road_side.z >> 8);
    b->spin.x += f * (c->road_fwd.x >> 8);
    b->spin.y += f * (c->road_fwd.y >> 8);
    b->spin.z += f * (c->road_fwd.z >> 8);
    b->vel.x += u * (b->m[3] >> 8);
    b->vel.y += u * (b->m[4] >> 8);
    b->vel.z += u * (b->m[5] >> 8);
}

static int moving(const car_body *b)
{
    return iabs(b->vel.z) > 0x8000 || iabs(b->vel.x) > 0x8000 || iabs(b->vel.y) > 0x8000;
}

/* car_car_impact 0x1e314: the two bodies parted and pushed; a light hit leaves both
   driven as before (from their bodies), a hard one (above 0xea5b0, 0xea5b4 with a
   player in it) throws both about (kind 4), a player's car knocked over once. (The
   replay's highlights 0x5c6d4 and the viewed car's sound 0x531dc: not in the port) */
static int impact(race_car *a, race_car *b)
{
    vec3 pt, n;
    int ha = human(a), hb = human(b), limit = exe_i32(ha || hb ? 0xea5b4 : 0xea5b0);

    if (!box_separate(&a->body, &b->body, &pt, &n) || !box_impulse(a, b, &pt, &n))
        return 0;
    if (limit >= g_collide.impulse && !(ha && a->crashing == 1) && !(hb && b->crashing == 1)) {
        if (a->state != 4)
            crash_set_kind(a, a->state);
        /* (the original asks a's kind again for b) */
        if (a->state != 4)
            crash_set_kind(b, b->state);
        a->road_speed = crash_body_road_speed(a);
        b->road_speed = crash_body_road_speed(b);
        return 1;
    }
    a->state = 4;
    b->state = 4;
    if (moving(&a->body))
        a->body.wait = 300;
    if (moving(&b->body))
        b->body.wait = 300;
    if (ha && !a->crashing) {
        crash_road_frame(a);
        a->crashing = 1;
        knock(a);
    }
    if (hb && !b->crashing) {
        crash_road_frame(b);
        b->crashing = 1;
        knock(b);
    }
    return 1;
}

/* car_car_collide 0x1c6a4: the cars as bodies, and the hit; one hard enough to be heard
   shakes both views (the sound, 0x7fb58 and 0x52e40: not in the port) */
static int collide(race_car *a, race_car *b)
{
    int s, t;

    if (a->state != 4)
        crash_body_from_car(a);
    if (b->state != 4)
        crash_body_from_car(b);
    if (!impact(a, b))
        return 0;
    if (g_collide.impulse >= g_crash.hit_level) {
        g_crash.hit_level = g_collide.impulse + 0x8000;
        g_crash.hit_wait = 10;
        s = g_collide.impulse > 0xa0000 ? 0xa0000 : g_collide.impulse;
        if (rand_below(10) > 5)
            s = -s;
        a->roll_shake = s;
        b->roll_shake = -s;
        t = rand_below(10) > 5 ? -s : s;
        a->pitch_shake = t;
        b->pitch_shake = t;
        a->roll_phase = b->roll_phase = 0x1c;
        a->pitch_phase = b->pitch_phase = 0x1c;
    }
    return 1;
}

/* 0x1c800: a player's car the police have caught (+0x176 & 1) steers towards the police
   car (car 2), its throttle off */
static void steer_to_police(race_car *c)
{
    const race_car *p = g_cars[2];
    int a;

    if (!(c->bits[2] & 1) || !p)
        return;
    a = (angle_of(p->pos.z - c->pos.z, p->pos.x - c->pos.x) - c->yaw) & 0xffffff;
    if (a > 0x800000)
        a -= 0x1000000;
    c->steer = (c->steer >> 1) + (a >> 1);
    if (c->steer > 0x140000)
        c->steer = 0x140000;
    else if (c->steer < -0x140000)
        c->steer = -0x140000;
    c->throttle = 0;
}

void collide_world(void)
{
    int total = 4 * g_track.length, i, j;

    for (i = 0; i < g_ncars; i++) {
        race_car *c = g_cars[g_timing.by_road[i]];
        int last;

        if (human(c))
            steer_to_police(c);
        /* (a player's car outside the lanes against the roadside objects, 0x1ebf4:
           not yet) */
        if (c->state == 6 || (c->part & 6) != 6)
            continue;
        last = total - c->node < 4 ? g_ncars + 4 : g_ncars;
        for (j = i + 1; j < last; j++) {
            race_car *o = g_cars[g_timing.by_road[j >= g_ncars ? j % g_ncars : j]];
            int r;

            if (c->node + 2 < o->node && total - c->node > 4)
                break;
            if ((o->part & 5) != 5)
                continue;
            r = c->body.radius + o->body.radius;
            if (r > iabs(c->pos.x - o->pos.x) && iabs(c->pos.z - o->pos.z) < r)
                collide(c, o);
        }
    }
}
