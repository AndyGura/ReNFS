/* The crash (NOTES §4.6). 16.16 fixed point as drive.c: the multiplies round by the last
   bit shifted out (mulr), the products of two values each shifted down by 8 first (p8)
   truncate, the divisions by powers of two round towards zero (/) or down (>>), each as
   the machine code does, 32-bit sums wrapping. The body's coordinates are the car's with
   z turned round: its turn's elements 2, 5, 6 and 7 and its z change sign between them. */
#include "crash.h"
#include "racesnd.h"
#include "cardraw.h"
#include "collide.h"
#include "exe.h"
#include "opponent.h"
#include "track.h"
#include <limits.h>
#include <math.h>
#include <string.h>

crash_race g_crash;

static int mulr(int a, int b)
{
    long long p = (long long)a * b;
    return (int)(p >> 16) + (int)((p >> 15) & 1);
}

static int iabs(int v)
{
    return v < 0 ? (int)(0u - (unsigned)v) : v;
}

/* (a >> 8) * (b >> 8), wrapping */
static int p8(int a, int b)
{
    return (int)((unsigned)(a >> 8) * (unsigned)(b >> 8));
}

static int dot8(const vec3 *a, const vec3 *b)
{
    return p8(a->x, b->x) + p8(a->y, b->y) + p8(a->z, b->z);
}

/* a × b with p8's products */
static vec3 cross8(const vec3 *a, const vec3 *b)
{
    vec3 r;

    r.x = p8(a->y, b->z) - p8(a->z, b->y);
    r.y = p8(a->z, b->x) - p8(a->x, b->z);
    r.z = p8(a->x, b->y) - p8(a->y, b->x);
    return r;
}

/* (a << 16) / b, in the original's two divisions (the whole part, then the rest's); the
   original stops with a division error on 0 */
int fix_div(int a, int b)
{
    if (!b)
        return 0;
    return (int)((unsigned)(a / b) << 16) + (int)(((long long)(a % b) << 16) / b);
}

/* 0x3e7ec: the angle of (x, y), 0x1000000 a turn */
int angle_of(int x, int y)
{
    return (int)((unsigned)ea_atan2(y, x) << 8);
}

/* 0x3e784: the square root of n by Newton's steps from guess, until a step changes it by
   less than 1 << (16 - iters) */
int fix_sqrt(int n, int iters, int guess)
{
    int sh = 16 - iters, x = guess, old;

    if (!n)
        return 0;
    if (sh < 0)
        sh = 0;
    if (sh > 16)
        sh = 16;
    do {
        old = x;
        x = (int)(((unsigned)x + (unsigned)fix_div(n, x)) >> 1);
    } while ((unsigned)(old - x) >> sh);
    return x;
}

/* 0x3e7fc: 1 / x on the x87 (the game keeps it at double precision: 0x127f, set by the
   start-up 0xa8c95), truncated; 0 gives the integer indefinite */
static int recip(int x)
{
    double d = (double)((unsigned)x & 0xffff) * (1.0 / 65536) + (double)((unsigned)x >> 16);
    double r = 1.0 / d * 65536;

    if (!(r > -2147483649.0 && r < 2147483648.0))
        return INT_MIN;
    return (int)trunc(r);
}

/* the sum of the squares of x, y (when `three`) and z, all halved k times first so that
   it fits (0x2c2d8, 0x2c3d8, 0x2c490 share it) */
static int squares(int x, int y, int z, int three, int *k)
{
    int n = 0, sx, sy, sz;

    while (iabs(x) > 0x1000000 || (three && iabs(y) > 0x1000000) || iabs(z) > 0x1000000) {
        x >>= 1;
        y >>= 1;
        z >>= 1;
        n++;
    }
    for (;;) {
        sx = mulr(x, x);
        sy = three ? mulr(y, y) : 0;
        sz = mulr(z, z);
        if ((unsigned)sx / 4 + (unsigned)sy / 4 + (unsigned)sz / 4 <= 0x40000000u)
            break;
        x >>= 1;
        y >>= 1;
        z >>= 1;
        n++;
    }
    *k = n;
    return (int)((unsigned)sx + (unsigned)sy + (unsigned)sz);
}

/* 0x2c2d8: |v| */
static int len3(const vec3 *v)
{
    int k, s = squares(v->x, v->y, v->z, 1, &k);

    return (int)((unsigned)fix_sqrt(s, 8, s) << k);
}

/* 0x2c3d8: |(x, z)| */
static int len2(const vec3 *v)
{
    int k, s = squares(v->x, 0, v->z, 0, &k);

    return (int)((unsigned)fix_sqrt(s, 8, s) << k);
}

/* 0x2c490: |v|² */
static int len3_sq(const vec3 *v)
{
    int k, s = squares(v->x, v->y, v->z, 1, &k);

    return (int)((unsigned)s << 2 * k);
}

/* 0x2c25c: v made a unit long */
static void normalize(vec3 *v)
{
    int s = p8(v->y, v->y) + p8(v->x, v->x) + p8(v->z, v->z), len = fix_sqrt(s, 8, s), inv;

    if (!len)
        return;
    inv = fix_div(0x10000, len) >> 8;
    v->x = (int)((unsigned)(v->x >> 8) * (unsigned)inv);
    v->y = (int)((unsigned)(v->y >> 8) * (unsigned)inv);
    v->z = (int)((unsigned)(v->z >> 8) * (unsigned)inv);
}

/* ---- turns: 3 x 3, row by row ---------------------------------------------------- */

/* 0x2c82c, 0x2c7c4, 0x2c75c: turns by a (0x1000000 a turn) about x, y, z */
static void rot_x(int *m, int a)
{
    int c = ea_cos(a >> 8), s = ea_sin(a >> 8);

    m[0] = 0x10000; m[1] = 0;  m[2] = 0;
    m[3] = 0;       m[4] = c;  m[5] = s;
    m[6] = 0;       m[7] = -s; m[8] = c;
}

static void rot_y(int *m, int a)
{
    int c = ea_cos(a >> 8), s = ea_sin(a >> 8);

    m[0] = c; m[1] = 0;       m[2] = -s;
    m[3] = 0; m[4] = 0x10000; m[5] = 0;
    m[6] = s; m[7] = 0;       m[8] = c;
}

static void rot_z(int *m, int a)
{
    int c = ea_cos(a >> 8), s = ea_sin(a >> 8);

    m[0] = c;  m[1] = s; m[2] = 0;
    m[3] = -s; m[4] = c; m[5] = 0;
    m[6] = 0;  m[7] = 0; m[8] = 0x10000;
}

/* 0x2cf14: out = a · b (out may be either) */
static void mat_mul3(int *out, const int *a, const int *b)
{
    int t[9], r, c;

    for (r = 0; r < 3; r++)
        for (c = 0; c < 3; c++)
            t[r * 3 + c] = mulr(a[r * 3], b[c]) + mulr(a[r * 3 + 1], b[3 + c])
                         + mulr(a[r * 3 + 2], b[6 + c]);
    memcpy(out, t, sizeof t);
}

/* 0x3e848 */
static void transpose(int *out, const int *m)
{
    int r, c;

    for (r = 0; r < 3; r++)
        for (c = 0; c < 3; c++)
            out[r * 3 + c] = m[c * 3 + r];
}

/* 0x2ce5c: v · m */
static void vec_mat(int *out, const int *v, const int *m)
{
    int t[3], c;

    for (c = 0; c < 3; c++)
        t[c] = mulr(v[0], m[c]) + mulr(v[1], m[3 + c]) + mulr(v[2], m[6 + c]);
    memcpy(out, t, sizeof t);
}

/* 0x2ccd0: about x by a, then z by b, then y by c */
static void turn_of(int *m, int a, int b, int c)
{
    int t[9];

    rot_x(m, a);
    rot_z(t, b);
    mat_mul3(m, m, t);
    rot_y(t, c);
    mat_mul3(m, m, t);
}

/* 0x2c894: a turn by `angle` about `axis`: the axis turned onto x, turned about x, turned
   back */
static void turn_about(int *out, int angle, const vec3 *axis)
{
    int m1[9], m2[9], m3[9], back[9], about[9];

    rot_y(m1, angle_of(axis->x, axis->z));
    rot_z(m2, -angle_of(len2(axis), axis->y));
    mat_mul3(m3, m1, m2);
    rot_x(about, angle);
    transpose(back, m3);
    mat_mul3(out, m3, about);
    mat_mul3(out, out, back);
}

/* 0x2c584: the turn made square again: its first row a unit long, the third the first
   across the second, a unit long, the second the third across the first (the products
   of a value and a quarter of another, shifted down 14) */
static int q14(int a, int b)
{
    return (int)((unsigned)a * (unsigned)(b >> 2)) >> 14;
}

static void squared_up(int *m)
{
    int t[9], k, i;

    memcpy(t, m, sizeof t);
    k = recip((q14(t[0], t[0]) + q14(t[1], t[1]) + q14(t[2], t[2]) + 0x10000) >> 1);
    for (i = 0; i < 3; i++)
        t[i] = q14(k, t[i]);
    t[6] = q14(t[1], t[5]) - q14(t[2], t[4]);
    t[7] = q14(t[2], t[3]) - q14(t[0], t[5]);
    t[8] = q14(t[0], t[4]) - q14(t[1], t[3]);
    k = recip((q14(t[6], t[6]) + q14(t[7], t[7]) + q14(t[8], t[8]) + 0x10000) >> 1);
    for (i = 6; i < 9; i++)
        t[i] = q14(k, t[i]);
    t[3] = q14(t[7], t[2]) - q14(t[8], t[1]);
    t[4] = q14(t[8], t[0]) - q14(t[6], t[2]);
    t[5] = q14(t[6], t[1]) - q14(t[7], t[0]);
    memcpy(m, t, sizeof t);
}

/* ---- the smoke ------------------------------------------------------------------- */

static unsigned smoke_rand(void)
{
    g_crash.random = g_crash.random * 0x10004005u + 0x11;
    return g_crash.random;
}

/* 0x64588 */
static void smoke_reset(crash_smoke *s)
{
    int i;

    for (i = 0; i < SMOKE_PUFFS; i++)
        s->age[i] = -1;
    s->next = 0;
    s->timer = 0;
}

/* a random spread: ±0.5 by 1/512ths */
static int spread(void)
{
    return (int)((unsigned)((0x10000 - (int)(smoke_rand() >> 15)) >> 8) << 7);
}

/* 0x64804: puffs let out at the body's place, `rate` the time on */
static void smoke_emit(crash_smoke *s, const race_car *c, int rate)
{
    s->timer += rate;
    while (s->timer > s->next) {
        int i = s->index;

        s->size[i] = (int)((smoke_rand() >> 15 >> 8) << 7);
        s->pos[i] = c->body.pos;
        s->pos[i].z = -c->body.pos.z;
        s->pos[i].x += spread();
        s->pos[i].y += spread();
        s->pos[i].z += spread();
        s->age[i] = 0;
        if (++s->index == SMOKE_PUFFS)
            s->index = 0;
        s->timer -= s->next;
        s->next = (int)((smoke_rand() >> 23) * 0x6666) >> 8;
    }
}

/* 0x64978: the puffs rise and drift, older by dt; those a second old go */
static void smoke_move(crash_smoke *s, int dt)
{
    int i, d = dt >> 8, w = dt >> 9;

    for (i = 0; i < SMOKE_PUFFS; i++) {
        int a = s->age[i];

        if (a < 0)
            continue;
        a += d * 15;
        s->age[i] = a;
        if (a >= 0x10000) {
            s->age[i] = (int)0xffff0000u;
            continue;
        }
        if (a < 0x4000) {
            int q = (a * 4) >> 8;

            s->pos[i].y += (((q * q) >> 8) * d >> 8) * 0x180;
        } else {
            s->pos[i].y += d * 0x180;
            s->pos[i].x += (s->size[i] >> 8) * d;
            s->pos[i].x += (ea_sin((a << 9) >> 8) >> 8) * w;
            s->pos[i].z += (ea_cos((a << 9) >> 8) >> 8) * w;
        }
    }
}

/* ---- the body against the world -------------------------------------------------- */

/* 0x2a918: the push at the corner c of a body moving there at v against a surface
   facing n: back off it (0xea684 of the way) and along it by the friction (0xea688),
   no more than stops the sliding */
static void impulse(car_body *b, const vec3 *c, const vec3 *v, const vec3 *n)
{
    vec3 t = *v, r, rn, f1, a, d, p, rp;
    int vn = dot8(&b->vel, n), den, j, f, fi, len = 0, k;

    r.x = c->x - b->pos.x;
    r.y = c->y - b->pos.y;
    r.z = c->z - b->pos.z;
    rn = cross8(&r, n);
    den = (b->inv_mass >> 1) + (mulr(len3_sq(&rn), b->inv_inertia) >> 1);
    j = mulr(fix_div(-(vn + dot8(&b->spin, &rn)), den), exe_i32(0xea684));
    if (t.x || t.y || t.z) {
        int s = dot8(&t, n) >> 8, q;

        t.x -= (int)((unsigned)(n->x >> 8) * (unsigned)s);
        t.y -= (int)((unsigned)(n->y >> 8) * (unsigned)s);
        t.z -= (int)((unsigned)(n->z >> 8) * (unsigned)s);
        q = p8(t.y, t.y) + p8(t.x, t.x) + p8(t.z, t.z);
        len = fix_sqrt(q, 8, q);
        k = -recip(len);
        t.x = mulr(k, t.x);
        t.y = mulr(k, t.y);
        t.z = mulr(k, t.z);
    }
    f = mulr(exe_i32(0xea688), j);
    k = mulr(f, b->inv_mass);
    f1.x = mulr(k, t.x);
    f1.y = mulr(k, t.y);
    f1.z = mulr(k, t.z);
    fi = mulr(f, b->inv_inertia);
    a = cross8(&t, &r);
    /* (the second product written into its result as it goes, as the original) */
    d.x = p8(a.y, r.z) - p8(a.z, r.y);
    d.y = p8(a.z, r.x) - p8(d.x, r.z);
    d.z = p8(d.x, r.y) - p8(d.y, r.x);
    f1.x += mulr(fi, d.x);
    f1.y += mulr(fi, d.y);
    f1.z += mulr(fi, d.z);
    k = dot8(&f1, &t);
    if (k > len)
        f = mulr(f, fix_div(len, k));
    t.x = mulr(f, t.x);
    t.y = mulr(f, t.y);
    t.z = mulr(f, t.z);
    if (j <= 0)
        return;
    g_crash.impulse = j;
    p.x = mulr(j, n->x) + t.x;
    p.y = mulr(j, n->y) + t.y;
    p.z = mulr(j, n->z) + t.z;
    b->vel.x += mulr(b->inv_mass, p.x);
    b->vel.y += mulr(b->inv_mass, p.y);
    b->vel.z += mulr(b->inv_mass, p.z);
    rp = cross8(&r, &p);
    b->spin.x += mulr(b->inv_inertia, rp.x);
    b->spin.y += mulr(b->inv_inertia, rp.y);
    b->spin.z += mulr(b->inv_inertia, rp.z);
}

/* the corner's speed: the body's and its spin's */
static vec3 point_speed(const car_body *b, const vec3 *r)
{
    vec3 v = cross8(&b->spin, r);

    v.x += b->vel.x;
    v.y += b->vel.y;
    v.z += b->vel.z;
    return v;
}

/* 0x2a334: the body's box against the surface through at facing n: its deepest corner
   pushed half out, pushed back if it moves in, and its fall slowed by the height it was
   pushed up */
static void contact(car_body *b, const vec3 *n, const vec3 *at)
{
    const vec3 *row = (const vec3 *)b->m;
    vec3 c, rel, push = { 0, 0, 0 }, r, v;
    int d[3], s[3], i, depth;

    g_crash.impulse = 0;
    for (i = 0; i < 3; i++) {
        d[i] = (int)((unsigned)(dot8(&row[i], n) >> 8) * (unsigned)(b->half[i] >> 8));
        s[i] = d[i] < 0 ? 1 : -1;
    }
    rel.x = b->pos.x - at->x;
    rel.y = b->pos.y - at->y;
    rel.z = b->pos.z - at->z;
    g_crash.depth = d[0] * s[0] + d[1] * s[1] + d[2] * s[2] + dot8(&rel, n);
    if (g_crash.depth >= 0)
        return;
    c = b->pos;
    for (i = 0; i < 3; i++) {
        c.x += p8(row[i].x, b->half[i]) * s[i];
        c.y += p8(row[i].y, b->half[i]) * s[i];
        c.z += p8(row[i].z, b->half[i]) * s[i];
    }
    depth = -g_crash.depth;
    if (depth < 0)
        depth = 0;
    if (exe_i32(0xea678) && depth) {
        push.x = mulr(depth >> 1, n->x);
        push.y = mulr(depth >> 1, n->y);
        push.z = mulr(depth >> 1, n->z);
        b->pos.x += push.x;
        b->pos.y += push.y;
        b->pos.z += push.z;
    }
    r.x = c.x - b->pos.x;
    r.y = c.y - b->pos.y;
    r.z = c.z - b->pos.z;
    v = point_speed(b, &r);
    if (dot8(&v, n) < 0) {
        impulse(b, &c, &v, n);
        g_crash.point_vel = point_speed(b, &r);
    }
    if (exe_i32(0xea678) && depth && push.y > 0) {
        int e = 2 * mulr(0x9cf5c, push.y), vv = mulr(b->vel.y, b->vel.y);

        if (e > vv)
            b->vel.y = 0;
        else {
            int q = fix_sqrt(vv - e, 8, vv - e);

            b->vel.y = b->vel.y > 0 ? q : -q;
        }
    }
}

/* 0x2c9c0: a step of the body (1/30 s): moved, slowed a 64th, gravity, turned by its
   spin */
static void body_step(car_body *b)
{
    int r[9], a;

    b->pos.x += mulr(b->vel.x, 0x888);
    b->pos.y += mulr(b->vel.y, 0x888);
    b->pos.z += mulr(b->vel.z, 0x888);
    b->vel.x -= b->vel.x >> 6;
    b->vel.y -= b->vel.y >> 6;
    b->vel.z -= b->vel.z >> 6;
    b->vel.y -= 0x53b6;
    a = mulr(mulr(len3(&b->spin), 0x888), 0x28be63);
    turn_about(r, a, &b->spin);
    mat_mul3(b->m, b->m, r);
}

/* ---- the car and its body -------------------------------------------------------- */

/* 0x7dbd8: the road's axes at the car's node: along it to the next node's point, across
   by the node's +0x1c shorts, up square to both. The computer's cars then sometimes pick
   another place in their lanes */
static void road_frame(race_car *c)
{
    int n = c->node;
    vec3 f, u;

    c->road_at.x = node_i32(n, 8);
    c->road_at.y = node_i32(n, 0xc);
    c->road_at.z = node_i32(n, 0x10);
    f.x = node_i32(n + 1, 8) - c->road_at.x;
    f.y = node_i32(n + 1, 0xc) - c->road_at.y;
    f.z = node_i32(n + 1, 0x10) - c->road_at.z;
    u.x = 2 * (node_i32(n, 0x1a) >> 16);
    u.y = 2 * (node_i32(n, 0x1c) >> 16);
    u.z = 2 * (node_i32(n, 0x1e) >> 16);
    normalize(&f);
    c->road_up = cross8(&f, &u);
    c->road_side = u;
    c->road_fwd = f;
    if (c->slot < 0 || c->slot >= g_humans)
        opponent_wander(c);
}

/* the body's turn into the car's (the z flips) */
static void turn_flipped(int *out, const int *m)
{
    memcpy(out, m, 9 * sizeof *m);
    out[2] = -out[2];
    out[5] = -out[5];
    out[6] = -out[6];
    out[7] = -out[7];
}

/* 0x1cbb0: the body from the car: its place lifted to its middle, its speed, its turn
   (the car's own, else from its angles), spinning as the car turned */
static void body_from_car(race_car *c)
{
    car_body *b = &c->body;

    b->pos = c->pos;
    b->pos.z = -b->pos.z;
    b->vel = c->vel;
    b->spin.x = b->spin.z = 0;
    b->spin.y = -mulr(c->yaw_rate, 0x648);
    b->vel.z = -b->vel.z;
    b->vel.x = -b->vel.x;
    if (!c->own_turn)
        turn_of(b->m, -c->pitch, c->roll, -c->yaw);
    else
        turn_flipped(b->m, c->turn);
    b->pos.x += p8(b->m[3], c->height);
    b->pos.y += p8(b->m[4], c->height);
    b->pos.z += p8(b->m[5], c->height);
}

/* the car's place and speed from the body's (the tail of 0x1cfe0, and 0x1cd40) */
static void car_from_body(race_car *c)
{
    car_body *b = &c->body;

    c->pos = b->pos;
    c->pos.x -= p8(b->m[3], c->height);
    c->pos.y -= p8(b->m[4], c->height);
    c->pos.z = -(c->pos.z - p8(b->m[5], c->height));
    c->vel = b->vel;
    c->vel.z = -c->vel.z;
    c->vel.x = -c->vel.x;
}

/* 0x2cb78 and its callers: the car's pitch, roll and heading from the body's turn */
static void body_angles(race_car *c)
{
    const int *m = c->body.m;
    int yaw = angle_of(m[0], -m[2]), s = mulr(m[0], m[0]) + mulr(m[2], m[2]);
    int roll = angle_of(fix_sqrt(s, 8, s), m[1]), a[9], z[9], o[3];

    rot_y(a, -yaw);
    rot_z(z, -roll);
    mat_mul3(a, a, z);
    vec_mat(o, &m[6], a);
    c->pitch = -angle_of(o[2], -o[1]) & 0xffffff;
    c->roll = roll & 0xffffff;
    c->yaw = -yaw & 0xffffff;
    if (c->pitch >= 0xc00000)
        c->pitch -= 0x1000000;
    if (c->roll >= 0xc00000)
        c->roll -= 0x1000000;
}

/* 0x1cd40: the car driven by kind k again, from the body: its place, its speed, its turn
   half way to the body's spin; the car heard, thrown about, heard again */
static void set_kind(race_car *c, int k)
{
    if (c == rsnd_heard() && c->state == 4)
        rsnd_engine_on();
    c->state = k;
    car_from_body(c);
    c->yaw_rate += (-mulr(c->body.spin.y, 0x28be63) - c->yaw_rate) >> 1;
    c->body.wait = 0;
    c->slide_front = 0;
    c->slide_rear = 0;
}

/* car_stand_up 0x7daec: the body turned upright, its up along the road's (up) and its
   other two axes squared to it, across first */
static void stand_up(car_body *b, const vec3 *up)
{
    int *m = b->m;

    m[3] = up->x;
    m[4] = up->y;
    m[5] = up->z;
    m[0] = p8(m[4], m[8]) - p8(m[5], m[7]);
    m[1] = p8(m[5], m[6]) - p8(m[3], m[8]);
    m[2] = p8(m[3], m[7]) - p8(m[4], m[6]);
    normalize((vec3 *)&m[0]);
    m[6] = p8(m[1], m[5]) - p8(m[2], m[4]);
    m[7] = p8(m[2], m[3]) - p8(m[0], m[5]);
    m[8] = p8(m[0], m[4]) - p8(m[1], m[3]);
}

/* 0x1ceb4: landed on its wheels: driven again, the gearbox's automatic in neutral */
static void land(race_car *c)
{
    if (c == rsnd_heard() && c->state == 4)
        rsnd_engine_on();      /* (and again in set_kind, as the original) */
    set_kind(c, 2);
    c->crashing = 0;
    body_angles(c);
    g_crash.view = 0;
    smoke_reset(&g_crash.smoke);
    c->own_turn = 0;
    c->bits[1] &= ~2;
    if (c->gearbox)
        c->gearbox = 2;
}

/* car_crash 0x1cb14: the car to kind 4, tumbling, put back in 300 steps at the latest.
   (A highlight for the replay, 0x5c: not in the port.) The car heard falls silent */
static void crash_begin(race_car *c)
{
    body_from_car(c);
    c->crashing = 1;
    c->state = 4;
    smoke_reset(&g_crash.smoke);
    c->bits[1] &= ~2;
    c->body.wait = 300;
    if (c == rsnd_heard())
        rsnd_engine_off();
}

void crash_hard_hit(race_car *c, int a, int b, int s)
{
    int *m = c->body.m;

    crash_begin(c);
    c->body.spin.x += mulr(s, m[0]) + mulr(b, m[6]) - mulr(a, m[3]);
    c->body.spin.y += mulr(s, m[1]) + mulr(b, m[7]) - mulr(a, m[4]);
    c->body.spin.z += mulr(s, m[2]) + mulr(b, m[8]) - mulr(a, m[5]);
    if (c->bits[0] & 4)
        opponent_chase_drop(0);
}

/* the player's driving stopped: no pedals, the engine running down, out of gear */
static void controls_off(race_car *c)
{
    c->throttle = 0;
    c->braking = 0;
}

/* 0x1d8a8: the wait over, the car put back on the road: in the middle of its node's first
   lane, upright along the road, standing, in neutral; a crash counted */
static void respawn(race_car *c, const vec3 *side, const vec3 *at)
{
    car_body *b = &c->body;
    const unsigned char *nd;
    vec3 rel;
    int a;

    c->crashing = 0;
    /* (0x7c030: the other cars thrown about put back too) */
    if (g_drive.chase)
        opponent_chase_drop(1);
    opponent_put_back();
    memcpy(b->m, &c->road_side, 9 * sizeof(int));
    b->m[2] = -b->m[2];
    b->m[5] = -b->m[5];
    b->m[6] = -b->m[6];
    b->m[7] = -b->m[7];
    rel.x = b->pos.x - at->x;
    rel.y = b->pos.y - at->y;
    rel.z = b->pos.z - at->z;
    nd = node_ptr(c->node);
    a = ((lane_width(nd[4] & 15, nd[1] >> 3) << 8 >> 1) - dot8(&rel, side)) >> 8;
    b->pos.x += (int)((unsigned)(side->x >> 8) * (unsigned)a);
    b->pos.y += (int)((unsigned)(side->y >> 8) * (unsigned)a);
    b->pos.z += (int)((unsigned)(side->z >> 8) * (unsigned)a);
    body_angles(c);
    g_crash.view = 0;
    smoke_reset(&g_crash.smoke);
    controls_off(c);
    c->rpm = 700;
    c->rpm_road = 0;
    c->speed = c->up_speed = c->side_speed = 0;
    c->vel.x = c->vel.y = c->vel.z = 0;
    c->yaw_rate = 0;
    c->wheel_speed = 0;
    c->tyres[0] = c->tyres[1] = 0;
    c->clutch = 0;
    c->gear = -1;
    c->shift_wait = -1;
    c->throttle_kept = 0;
    c->shifting = 0;
    c->bits[1] &= ~2;
    c->steer = 0;
    if (g_drive.state == 1)
        g_crash.restart = 0x1e;
    if (c->gearbox)
        c->gearbox = 2;
    set_kind(c, 2);
    c->own_turn = 0;
    drive_on_road(c, 0);
    c->yaw_rate = 0;
    c->speed = c->up_speed = c->side_speed = 0;
    c->vel.x = c->vel.y = c->vel.z = 0;
    c->speed_abs = 0;
    /* (counted in a race, 0x13c168 1; not when it is 2 or 3) */
    if (g_drive.state == 1 && c->player >= 0 && c->player < DRIVE_PLAYERS)
        g_crash.crashes[c->player]++;
    g_crash.respawned = 1;
}

void crash_step(race_car *c)
{
    car_body *b = &c->body;
    vec3 up, side, fwd, at, rel, p, n;
    const unsigned char *nd;
    int best = 0, a, player = c->player >= 0 && c->player < g_humans;

    c->own_turn = 1;
    /* (the rival's, 0x13c270: its revs eased to 1500) */
    if (g_ncars > 1 && c == g_cars[1])
        c->rpm += (1500 - c->rpm) / 8;
    if (drive_check_lap(c))
        road_frame(c);
    up = c->road_up;
    up.z = -up.z;
    side = c->road_side;
    side.z = -side.z;
    fwd = c->road_fwd;
    fwd.z = -fwd.z;
    at = c->road_at;
    at.z = -at.z;
    body_step(b);

    /* the walls (the nearer side's: its distance the node's +2 / +3, and 1 or 3.25 more
       by its kind), a tunnel's roof, the road */
    rel.x = b->pos.x - at.x;
    rel.y = b->pos.y - at.y;
    rel.z = b->pos.z - at.z;
    nd = node_ptr(c->node);
    if (dot8(&rel, &side) > 0) {
        a = ((int)((unsigned)nd[3] << 13) + (!(nd[5] & 15) || nd[7] == 5
             ? exe_i32(0xea5c8) : exe_i32(0xea5cc))) >> 8;
        n.x = -side.x;
        n.y = -side.y;
        n.z = -side.z;
    } else {
        a = (-(int)((unsigned)nd[2] << 13) - (!(nd[5] >> 4) || nd[7] == 5
             ? exe_i32(0xea5c8) : exe_i32(0xea5cc))) >> 8;
        n = side;
    }
    p.x = (int)((unsigned)a * (unsigned)(side.x >> 8)) + at.x;
    p.y = (int)((unsigned)a * (unsigned)(side.y >> 8)) + at.y;
    p.z = (int)((unsigned)a * (unsigned)(side.z >> 8)) + at.z;
    contact(b, &n, &p);
    if (best < g_crash.impulse)
        best = g_crash.impulse;
    if (nd[7] == 4) {
        p.x = up.x * 7 + at.x;
        p.y = up.y * 7 + at.y;
        p.z = up.z * 7 + at.z;
        n.x = -up.x;
        n.y = -up.y;
        n.z = -up.z;
        contact(b, &n, &p);
        if (best < g_crash.impulse)
            best = g_crash.impulse;
    }
    contact(b, &up, &at);
    if (best < g_crash.impulse)
        best = g_crash.impulse;

    if (player) {
        /* the player's car: its angles the body's, nothing driving it */
        body_angles(c);
        controls_off(c);
        c->rpm = c->rpm > 200 ? c->rpm - 200 : 0;
        c->speed = c->up_speed = c->side_speed = 0;
        c->yaw_rate = 0;
        c->wheel_speed = 0;
        c->tyres[0] = c->tyres[1] = 0;
        c->clutch = 0;
        c->gear = -1;
        if (c->gearbox)
            c->gearbox = 2;
        c->shift_wait = -1;
        c->throttle_kept = 0;
        c->shifting = 0;
        c->steer = 0;
    }

    /* nearly still on the road, flat or on its side: its moving halved */
    if (iabs(b->vel.x) < exe_i32(0xea5a4) && iabs(b->vel.y) < exe_i32(0xea5a4)
        && iabs(b->vel.z) < exe_i32(0xea5a4) && iabs(b->spin.x) < exe_i32(0xea5a8)
        && iabs(b->spin.y) < exe_i32(0xea5a8) && iabs(b->spin.z) < exe_i32(0xea5a8)
        && g_crash.depth < 0x3333) {
        int t = dot8((const vec3 *)&b->m[3], &up), e = exe_i32(0xea5ac);

        if (t > 0x10000 - e || t < e - 0x10000 || (t < e && t > -e)) {
            int *v[6] = { &b->vel.x, &b->vel.y, &b->vel.z, &b->spin.x, &b->spin.y, &b->spin.z };
            int i;

            for (i = 0; i < 6; i++)
                *v[i] = (int)((unsigned)(*v[i] >> 8) << 7);
        }
    }

    if (player && c->crashing) {
        int s;

        /* the smoke: let out once the body has slowed */
        if (c->bits[1] & 2)
            smoke_move(&g_crash.smoke, 0x888);
        s = iabs(b->vel.x) + iabs(b->vel.y) + iabs(b->vel.z);
        if (c->bits[1] & 2)
            smoke_emit(&g_crash.smoke, c, 0x888);
        else if (s < 0x80000) {
            c->bits[1] |= 2;
            smoke_emit(&g_crash.smoke, c, 0x5555);
        }
        g_crash.view = 0x3c;
        /* waited out: another racer less than a second behind keeps it a step more */
        if (!b->wait) {
            int i;

            for (i = 0; i < g_field; i++) {
                const race_car *o = g_cars[i];

                if (i == c->player || !o)
                    continue;
                if (o->road_speed > 0x90000 && o->node <= c->node
                    && fix_div((int)((unsigned)((c->node - o->node) * 6) << 16), o->road_speed)
                       < 0x10000)
                    b->wait = 1;
            }
        }
        if (!b->wait) {
            respawn(c, &side, &at);
            return;
        }
        /* waiting: landed on its wheels along the road, it drives on */
        c->speed_abs = iabs(c->road_speed);
        if (--b->wait != 0 && iabs(b->vel.y) < 0x18000 && iabs(b->spin.z) < 0x18000) {
            if (iabs(b->vel.x) < 0x18000 && iabs(b->vel.z) < 0x18000
                && iabs(b->spin.x) < 0x18000 && iabs(b->spin.y) < 0x18000) {
                int t1, t2;

                b->wait--;
                t1 = dot8((const vec3 *)&b->m[3], &up);
                t2 = dot8(&fwd, (const vec3 *)&b->m[6]);
                if (t1 > 0xf70a || t1 < -0xf70a || (t1 < 0x8f5 && t1 > -0x8f5)) {
                    if (t1 > 0xf70a && -t2 > 0xb333)
                        land(c);
                    else if (b->wait)
                        b->wait--;
                }
            } else if (dot8((const vec3 *)&b->m[6], &up) > 0xe666
                       && dot8((const vec3 *)&b->m[3], &up) > 0xf70a)
                land(c);
        }
    } else {
        /* a computer's car (0x1de8a): waited out, upright or out of the players' sight,
           it is stood up and handed to kind 5 (a police car's chase dropped) */
        int t = dot8((const vec3 *)&b->m[3], &up);

        if (!b->wait && (t > 0xf70a || !opponent_near_players(c))) {
            stand_up(b, &up);
            if (c->bits[0] & 8)
                opponent_chase_drop(0);
            set_kind(c, 5);
            goto placed;
        }
        /* still moving, or neither on its wheels, roof nor side: a second more to wait */
        if (b->wait) {
            b->wait--;
            if (iabs(b->vel.x) >= 0x18000 || iabs(b->vel.y) >= 0x18000
                || iabs(b->vel.z) >= 0x18000 || iabs(b->spin.x) >= 0x18000
                || iabs(b->spin.y) >= 0x18000 || iabs(b->spin.z) >= 0x18000
                || !(t > 0xf70a || t < -0xf70a || (t < 0x8f5 && t > -0x8f5))) {
                b->wait = (b->wait & 3) + 0x3c;
                if (c->bits[0] & 8)
                    opponent_chase_drop(0);
            }
        }
        /* a long one (over 3 m from its middle to its end) kept upright, turning only
           about its up */
        if (b->half[2] > 0x30000 && !c->crashing) {
            int e;

            stand_up(b, &up);
            e = (p8(b->spin.y, up.y) + p8(b->spin.x, up.x) + p8(up.z, b->spin.z)) >> 8;
            b->spin.x = e * (up.x >> 8);
            b->spin.y = e * (up.y >> 8);
            b->spin.z = e * (up.z >> 8);
        }
    }

    /* touching: the tyres' screech by the corner's speed, the hit heard */
    if (g_crash.depth < 0x3333) {
        if (g_crash.impulse > 0) {
            int s = iabs(g_crash.point_vel.x) + iabs(g_crash.point_vel.z);
            int e = (s * 16 - s) * 2;

            if (e < 0x230000)
                e = 0;
            if (e > 0x2bc0000)
                e = 0x2bc0000;
            c->slide_front = e;
        }
        c->slide_rear = (b->m[4] > 0xcccc ? 4 : 2) << 8 | 1;
        if (g_crash.hit_level < best) {
            /* the hit heard (0x52e40), for a body under 8 */
            int angle, dist;
            rsnd_where(c, &angle, &dist);
            if (b->mass < 0x80000)
                rsnd_hit(b->m[4] > 0xcccc ? 4 : 2, 1, best * 20, dist, angle);
            g_crash.hit_level = best + 0x8000;
            g_crash.hit_wait = 10;
        }
    } else
        c->slide_front = 0;
placed:
    car_from_body(c);
    if ((b->wait & 3) == 2 || b->wait == 0)
        squared_up(b->m);
    turn_flipped(c->turn, b->m);
}

/* ---- the refresh: 0x786d4 ------------------------------------------------------- */

void crash_refresh(race_car *c)
{
    const vec3 *row = (const vec3 *)c->turn;
    vec3 rel;
    int w;

    rel.x = c->pos.x - node_i32(c->node, 8);
    rel.y = c->pos.y - node_i32(c->node, 0xc);
    rel.z = c->pos.z - node_i32(c->node, 0x10);
    if (!c->own_turn) {
        road_frame(c);
        turn_of(c->turn, c->pitch, c->roll, c->yaw);
    }
    c->road_off = dot8(&rel, &c->road_side);
    c->reach = p8(iabs(dot8(&c->road_side, &row[0])), c->body.half[0])
             + p8(iabs(dot8(&c->road_side, &row[1])), c->body.half[1])
             + p8(iabs(dot8(&c->road_side, &row[2])), c->body.half[2]);
    if (c->slot < 0 || c->slot >= g_humans)
        opponent_target_speed(c);
    /* the speed along the road: the car's (0x7b130) or its body's (0x1e6cc); the
       computer's cars drive by it */
    if (c->state == 4)
        c->road_speed = crash_body_road_speed(c);
    else if (c->state != 3)
        c->road_speed = p8(-c->vel.x, c->road_fwd.x) + p8(c->vel.y, c->road_fwd.y)
                      + p8(c->vel.z, c->road_fwd.z);
    if (c->bits[2] & 2)
        c->speed_to = 0;
    w = (c->bits[0] | c->bits[1] << 8) & 0xbe1c;
    w |= 0x1e0;
    c->bits[0] = (unsigned char)w;
    c->bits[1] = (unsigned char)(w >> 8);
}

void crash_world(void)
{
    if (g_crash.hit_wait)
        g_crash.hit_wait--;
    else
        g_crash.hit_level = 0x6666;
    if (--g_crash.refresh == 0) {
        opponent_tick();
        g_crash.refresh = 4;
    }
    collide_world();
}

void crash_road_frame(race_car *c)
{
    road_frame(c);
}

void crash_body_from_car(race_car *c)
{
    body_from_car(c);
}

void crash_set_kind(race_car *c, int k)
{
    set_kind(c, k);
}

/* 0x1e6cc */
int crash_body_road_speed(const race_car *c)
{
    return p8(c->body.vel.x, c->road_fwd.x) + p8(c->body.vel.y, c->road_fwd.y)
         + p8(-c->body.vel.z, c->road_fwd.z);
}

int crash_len3(const vec3 *v)
{
    return len3(v);
}

int crash_recip(int x)
{
    return recip(x);
}

void crash_normalize(vec3 *v)
{
    normalize(v);
}

void crash_turn_y(int *m, int a)
{
    rot_y(m, a);
}

void crash_mat_mul(int *out, const int *a, const int *b)
{
    mat_mul3(out, a, b);
}

/* 0x1ca9c: a computer's car turned too far spins out: thrown about as a box for 60
   steps (the car heard falls silent) */
void crash_spin_out(race_car *c)
{
    body_from_car(c);
    c->state = 4;
    c->body.wait = 60;
    if (c == rsnd_heard())
        rsnd_engine_off();
}

void crash_start(void)
{
    g_crash.hit_wait = exe_i32(0xea5d0);
    g_crash.hit_level = exe_i32(0xea5d4);
    g_crash.refresh = 1;
    g_crash.random = (unsigned)exe_i32(0xf6dd8);
    smoke_reset(&g_crash.smoke);
}

static int pdn32(const unsigned char *p, int at)
{
    p += at;
    return (int)(p[0] | p[1] << 8 | p[2] << 16 | (unsigned)p[3] << 24);
}

void crash_body_from_model(race_car *c)
{
    car_body *b = &c->body;

    c->half_len = c->size[2];
    c->half_wid = c->size[0];
    b->half[0] = c->size[0] >> 1;
    b->half[1] = c->size[1] >> 1;
    b->half[2] = c->size[2] >> 1;
    c->height = b->half[1];
    b->radius = len3((const vec3 *)b->half);
    c->reach = b->half[0];
}

void crash_body_setup(race_car *c, const unsigned char *pdn)
{
    car_body *b = &c->body;
    int i;

    memset(b, 0, sizeof *b);
    b->mass = pdn32(pdn, 16);
    b->inv_mass = recip(b->mass);
    b->inertia = pdn32(pdn, 12);
    b->inv_inertia = recip(b->inertia);
    for (i = 0; i < 3; i++)
        b->half[i] = pdn32(pdn, 4 * i);
    b->m[0] = b->m[4] = b->m[8] = 0x10000;
    b->radius = len3((const vec3 *)b->half);
    c->height = b->half[1];
    c->reach = b->half[0];
    c->road_off = 0;
    /* the computer's driving numbers (car_apply_pdn 0x1e9cc) */
    c->bend = pdn32(pdn, 0x14);
    c->bend_inv = fix_div(0x10000, c->bend);
    c->top = pdn32(pdn, 0x18);
    for (i = 0; i < 100; i++)
        c->accel[i] = pdn32(pdn, 0x1c + 4 * i);
    for (i = 0; i < 6; i++) {
        c->gear_top[i] = pdn32(pdn, 0x1ac + 4 * i);
        c->gear_inv[i] = c->gear_top[i] ? fix_div(0x10000, c->gear_top[i]) : 0;
    }
    c->rev_scale = pdn32(pdn, 0x1c4);
    c->horn_sound = pdn32(pdn, 0x1c8);
    memset(c->turn, 0, sizeof c->turn);
    c->turn[0] = c->turn[4] = c->turn[8] = 0x10000;
}
