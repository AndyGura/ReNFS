/* The cars the computer drives (NOTES §4.13): see opponent.h. 16.16 fixed point as
   drive.c and crash.c: the multiplies round by the last bit shifted out (mulr), the
   products of two values each shifted down by 8 first (p8) truncate, 32-bit sums wrap.
   A car's "lane" is 0..7 across the road, 4 the first right of the middle; a node's
   road has lanes offL..offR (its +4 byte), the edge lanes the first and last of them. */
#include "opponent.h"
#include "cardraw.h"
#include "crash.h"
#include "drive.h"
#include "exe.h"
#include "timing.h"
#include "track.h"
#include <string.h>

opp_race g_opp;

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

/* half, towards 0 */
static int half(int v)
{
    return (v - (v >> 31)) >> 1;
}

/* a word of a driver's record (unaligned) */
static int rec32(const unsigned char *r, int at)
{
    r += at;
    return (int)(r[0] | r[1] << 8 | r[2] << 16 | (unsigned)r[3] << 24);
}

#define CFG(at) g_opp.cfg[(at) / 4]
#define RACER(c) ((c)->bits[0] & 4)
#define POLICE(c) ((c)->bits[0] & 8)
#define BACKWARDS(c) ((c)->bits[1] & 0x10)
#define WORD(c) ((c)->bits[0] | (c)->bits[1] << 8)
/* the steadiness a car holds when it is not steering */
#define STEADY(c) (RACER(c) ? 100 : 160)

/* the random numbers (0xeab70): the state times the multiplier, its low half kept, the
   middle bits given */
static unsigned rnd(void)
{
    unsigned p = g_opp.random * g_opp.mult;

    g_opp.last = p;
    g_opp.random = p & 0xffff;
    return (p & 0xffff00) >> 8;
}

/* ---- the road's lanes -------------------------------------------------------------- */

/* the lane x is in across node n's road (the lane's width kept in lane_w) */
static int lane_of(int node, int x)
{
    const unsigned char *nd = node_ptr(node);
    int w = lane_width(nd[4] & 15, nd[1] >> 3) << 8;

    g_opp.lane_w = w;
    if (x < 0)
        return x > -w ? 3 : x > -2 * w ? 2 : x > -3 * w;
    return x < w ? 4 : x < 2 * w ? 5 : 6 + (x >= 3 * w);
}

static int lane_first(int node)
{
    return 3 - (node_ptr(node)[4] >> 4 & 15);
}

static int lane_last(int node)
{
    return (node_ptr(node)[4] & 15) + 4;
}

static int edge(int node, int l)
{
    return l == lane_first(node) || l == lane_last(node);
}

static int outside(int node, int l)
{
    return l < lane_first(node) || l > lane_last(node);
}

/* the road's lanes on the right at node n (its +4's low half) */
static int lanes_right(int node)
{
    return node_ptr(node)[4] & 15;
}

/* the middle of lane l across node n's road */
static int lane_middle(int node, int l)
{
    const unsigned char *nd = node_ptr(node);
    int w = lane_width(nd[4] & 15, nd[1] >> 3) << 8;

    return (l - 4) * w + half(w);
}

/* the road's left and right edges at node n (its +2, +3 bytes) */
static int road_left(int node)
{
    return node_ptr(node)[2] << 13;
}

static int road_right(int node)
{
    return node_ptr(node)[3] << 13;
}

/* ---- the stretches of road round the two racers ------------------------------------ */

/* 0x77ca4: node within the window [lo, hi) round the track */
static int in_window(int node, int lo, int hi)
{
    int total = 4 * g_track.length, len = hi - lo, d;

    if (len < 0)
        len += total;
    d = node > lo ? node - lo : node - lo + total;
    return d < len;
}

int opponent_near(int node)
{
    int p;

    if (g_field > 2)
        return 1;
    node &= g_track.node_mask;
    for (p = 0; p < 2; p++)
        if (in_window(node, g_opp.zone[p][5], g_opp.zone[p][4]))
            return p + 1;
    return 0;
}

/* 0x77e4c: racer a takes stretch b, whose car moves to c; the traffic in b and c swap */
static void zone_swap(int a, int b, int c)
{
    int t = g_opp.zone[b][0], k;

    g_cars[t]->zone = c;
    g_opp.zone[c][0] = (short)t;
    g_cars[a]->zone = b;
    g_opp.zone[b][0] = (short)a;
    for (k = g_field; k < g_ncars; k++) {
        if (g_cars[k]->zone == b)
            g_cars[k]->zone = c;
        else if (g_cars[k]->zone == c)
            g_cars[k]->zone = b;
    }
}

/* 0x77ef4: the stretches given out by the racers' order, the leader first. (The
   replay's highlight 0x50 at a change is left out) */
static void zone_rank(void)
{
    int rank = 0, i;

    for (i = g_ncars - 1; i >= 0; i--) {
        race_car *c = g_cars[g_timing.by_race[i]];

        if (c->slot < 0 || c->slot >= g_field)
            continue;
        if (rank != c->zone)
            zone_swap(c->slot, rank, c->zone);
        rank++;
    }
}

void opponent_zones(void)
{
    short (*z)[6] = g_opp.zone, mask = (short)g_track.node_mask;
    int p, k, acc = 0;

    if (g_field > 2 || g_field == 1)
        return;
    zone_rank();
    for (p = 0; p < 2; p++) {
        int prog = g_cars[z[p][0]]->progress;

        z[p][3] = (short)prog;
        z[p][4] = (short)(prog + 0x2b);
        z[p][5] = (short)(prog - 0xc);
    }
    /* overlapping: the two stretches meet half way */
    if (z[0][5] < z[1][4]) {
        z[0][5] = (short)(z[0][5] + half(acc));
        acc += z[1][4] - z[0][5];
        z[1][5] = (short)(z[1][5] - half(acc));
    }
    z[0][4] = (short)(z[0][4] + half(acc));
    for (p = 0; p < 2; p++) {
        z[p][1] = (short)(z[p][4] + 0xb);
        z[p][2] = (short)(z[p][5] - 8);
    }
    for (p = 0; p < 2; p++)
        for (k = 1; k < 6; k++) {
            if (z[p][k] < 0)
                z[p][k] = 0;
            z[p][k] &= mask;
        }
}

/* 0x7814c */
static void zones_start(void)
{
    int k;

    g_opp.zone[0][0] = 0;
    g_opp.zone[1][0] = 1;
    for (k = 0; k < g_ncars; k++)
        g_cars[k]->zone = k < 2 ? k : 0;
    opponent_zones();
}

/* ---- small things -------------------------------------------------------------------- */

/* car k's place on the grid (0x12dca0[k]; past the eight, the drivers' table after it) */
static int grid_place(int k)
{
    return k < 8 ? g_opp.grid[k] : k < 17 ? g_opp.drivers[k - 8] : 0;
}

/* 0x7cf04: how fast the car may slide across at speed s, towards es's side */
static int slide_limit(const race_car *c, int es, int s)
{
    const unsigned char *r = g_opp.rec[c->player];
    int lim = RACER(c) ? rec32(r, 0x65) : 0x33333;
    int f = RACER(c) ? rec32(r, 0x69) : 0x4000;
    int v = mulr(f, s);

    if (v < lim)
        return es > 0 ? v : -v;
    return es > 0 ? lim : -lim;
}

/* 0x7bf38: car c on the stretch from 20 nodes behind player h to 54 ahead */
static int near_player(const race_car *h, const race_car *c)
{
    int total = 4 * g_track.length;
    int lo = (h->node - 20) & g_track.node_mask, hi = (h->node + 54) & g_track.node_mask;
    int len = hi - lo, d;

    if (len < 0)
        len += total;
    d = lo < c->node ? c->node - lo : c->node - lo + total;
    return d < len;
}

/* 0x7bf94: near any player */
static int near_players(const race_car *c)
{
    int i;

    for (i = 0; i < g_humans; i++)
        if (near_player(g_cars[i], c))
            return 1;
    return 0;
}

int opponent_near_players(const race_car *c)
{
    return near_players(c);
}

/* 0x51724: how far past the finish (an open road's nodes past its end, or a circuit's
   node once the laps are done); 0 before */
static int past_finish(const race_car *c)
{
    if (g_track.loop)
        return c->lap > g_drive.laps ? c->node : 0;
    return c->node > g_drive.last_node ? c->node - g_drive.last_node : 0;
}

/* 0x2ce20: a dot product by rounded multiplies */
static int dot_r(const vec3 *a, const vec3 *b)
{
    return mulr(a->x, b->x) + mulr(a->y, b->y) + mulr(a->z, b->z);
}

/* the car's height off the road at its node (along the road's up) */
static int off_road(const race_car *c)
{
    vec3 rel;

    rel.x = c->pos.x - c->road_at.x;
    rel.y = c->pos.y - c->road_at.y;
    rel.z = c->pos.z - c->road_at.z;
    return dot_r(&rel, &c->road_up);
}

/* the car put down on the road by d along the road's up */
static void put_down(race_car *c, int d)
{
    c->pos.x -= p8(c->road_up.x, d);
    c->pos.y -= p8(c->road_up.y, d);
    c->pos.z -= p8(c->road_up.z, d);
}

/* the catch-up tables' index: how far behind (0) or ahead (20) of the leading player,
   ten units of progress a step */
static int catch_up_index(const race_car *c)
{
    int i = (c->progress - g_opp.lead->progress) / 10 + 10;

    return i < 0 ? 0 : i > 20 ? 20 : i;
}

/* the road's speed limit at node n (3 bytes each four nodes) */
static int limit_at(int node, int k)
{
    return g_track.speeds[((unsigned)node >> 2) * 3 + k] << 16;
}

/* ---- the plan: speed and slide (0x7cf98) ------------------------------------------- */

/* the steering a slide asks for, by the speed (0x7d815's table) */
static int slide_steer(int speed)
{
    static const int f[12] = { 0x10000, 0x18ccc, 0x15999, 0x15999, 0x11999, 0x11999,
                               0xcccc, 0xcccc, 0x8000, 0x8000, 0x4ccc, 0x4ccc };
    unsigned k = (unsigned)((speed >> 16) / 5);

    return k < 12 ? f[k] : 0;
}

static void plan(race_car *c, int prev)
{
    int racer = RACER(c) != 0, k, a, b, brake, n, g, e, slow, ok, x, es, d, i;
    vec3 dir, vel;

    /* the car's acceleration at its speed, and at the speed below */
    k = iabs(c->road_speed >> 16) + 1;
    if (k > 99)
        k = 99;
    a = c->accel[k];
    b = c->accel[k - 1];
    /* a racer catches up, or waits, by how far it is from the leading player */
    if (racer && !(g_opp.lead->speed_abs < 0x20000 && g_opp.lead->state == 2)
        && g_opp.lead->state != 4) {
        int f;

        i = catch_up_index(c);
        f = g_race_type == 1 ? CFG(0x104 + 4 * i) : CFG(0x158 + 4 * i);
        a = mulr(a, f);
        b = mulr(b, f);
    }
    brake = racer ? g_opp.brake : 0xb333;
    if (!c->on_ground) {
        brake = 0;
        a = 0;
    }
    if (g_drive.steps < 0x136)
        a = 0;
    if (POLICE(c)) {
        n = 6;
        g = 0x2aaa;
    } else if (racer) {
        n = 4;
        g = 0x4000;
    } else {
        n = 8;
        g = 0x2000;
    }
    e = n - prev;
    d = c->steer - c->steer_to;
    slow = iabs(d) > 0x60000 && c->road_speed > 0x70000
         ? (iabs(d) >> 16) * (racer ? 0xccc : 0x1745) : 0;
    /* on its road and not changing gear: it may speed up */
    x = c->road_off;
    ok = !opponent_near(c->node)
      || (-road_left(c->node) <= x && x <= road_right(c->node) && c->body.shift <= 0);
    if (c->body.shift > 0 && --c->body.shift == 0)
        c->body.shift = -20;
    if (c->body.shift < 0)
        c->body.shift++;
    brake *= e;
    a *= e;
    if (BACKWARDS(c)) {
        if (c->speed_to > c->road_speed) {
            c->road_speed += brake;
            if (c->road_speed > c->speed_to)
                c->road_speed = c->speed_to;
        } else if (!b)
            c->road_speed += brake;
        else if (ok) {
            c->road_speed -= a;
            if (c->road_speed < c->speed_to)
                c->road_speed = c->speed_to;
        }
    } else if (c->speed_to < c->road_speed) {
        c->road_speed -= brake;
        if (c->road_speed < c->speed_to)
            c->road_speed = c->speed_to;
        /* (+0x88 is the car itself) */
        if (racer && c->state == 3 && c->road_speed - 0xa0000 > c->speed_to)
            c->braking = 0x11;
    } else if (!b)
        c->road_speed -= brake;
    else if (ok) {
        c->road_speed += a;
        if (c->road_speed > c->speed_to)
            c->road_speed = c->speed_to;
        if (racer) {
            c->braking -= 4;
            if (c->braking < 0)
                c->braking = 0;
        }
    }
    if (slow) {
        if (iabs(c->road_speed) < slow)
            c->road_speed = 0;
        else if (c->road_speed < 0)
            c->road_speed += slow;
        else if (c->road_speed > 0)
            c->road_speed -= slow;
    }
    /* the race's top speed, where the road's limit allows it */
    if (c->player >= 0 && c->player < DRIVE_RESULTS && c->road_speed > g_drive.top_speed[c->player]
        && limit_at(c->node, 0) >= c->road_speed)
        g_drive.top_speed[c->player] = c->road_speed;

    /* the velocity: along the road, or along the car while it turns */
    if ((c->body.steady && c->body.steady != STEADY(c)) || iabs(c->steer - c->steer_to) < 0x20000)
        dir = c->road_fwd;
    else {
        dir = *(const vec3 *)&c->turn[6];
        if (BACKWARDS(c)) {
            dir.x = -dir.x;
            dir.y = -dir.y;
            dir.z = -dir.z;
        }
    }
    vel.x = p8(c->road_speed, dir.x);
    vel.y = p8(c->road_speed, dir.y);
    vel.z = p8(c->road_speed, dir.z);
    /* sliding across to its place */
    es = c->across_to - x;
    {
        int l = lane_of(c->node, x);

        if (edge(c->node, l) || (es && iabs(c->steer - c->steer_to) < 0x80000)) {
            int t = slide_limit(c, es, iabs(c->road_speed)), side = c->side_speed;

            if (iabs(es) < (iabs(t * n) >> 5))
                t = p8(g, es) << 5;
            if (t > side) {
                side += (side > 0 ? iabs(t) >> 4 : 0xb333) * e;
                if (t < side)
                    side = t;
            } else if (t < side) {
                side -= (side < 0 ? iabs(t) >> 4 : 0xb333) * e;
                if (t > side)
                    side = t;
            }
            c->side_speed = side;
            if (BACKWARDS(c))
                c->steer_to = 0;
            else
                c->steer_to = mulr(side, slide_steer(iabs(c->road_speed)));
            vel.x += mulr(side, c->road_side.x);
            vel.y += mulr(side, c->road_side.y);
            vel.z += mulr(side, c->road_side.z);
        }
    }
    /* on the road, or off it over a crest */
    d = off_road(c);
    if (d > 0x666 && near_players(c) && c->progress > 0x64 && racer
        && c->road_speed > 0x1b0000)
        c->on_ground = 0;
    else {
        c->on_ground = 1;
        put_down(c, d);
    }
    if (c->on_ground) {
        if (!c->body.steady || c->body.steady == STEADY(c)) {
            c->vel = vel;
            c->vel.x = -vel.x;
        } else
            c->vel.y = vel.y;
    }
    c->body.wait = n;
}

/* ---- the traffic placed on the road (0x7cac8) ----------------------------------------- */

static int by_grip(const race_car *c, int v);
static int oncoming(const race_car *c);
static void lane_pick(race_car *c);

/* the road has no lanes coming the other way (its first node's) */
static int one_way(void)
{
    return (node_ptr(0)[4] >> 4 & 15) == 0;
}

/* 0x77c78: node within one of the outer windows round the racers (1 + which, 0 none) */
static int in_reach(int node)
{
    int p;

    node &= g_track.node_mask;
    for (p = 0; p < 2; p++)
        if (in_window(node, g_opp.zone[p][2], g_opp.zone[p][1]))
            return p + 1;
    return 0;
}

/* 0x7c5b4: where the car comes on: its side (1 the racers' way, 0 coming the other way)
   and node. Behind the player's car when it must (+0x175 & 0x24, or the police on a
   one-way road); else a side at random, the traffic kept to at most all but one of its
   cars on a side, and a node just ahead of its stretch's inner window, or just behind
   it for a car going the racers' way when the stretch's racer stands */
static void place_where(race_car *c, int *side, int *node)
{
    int ow = one_way(), k = c->zone, n;

    if ((c->bits[1] & 0x24) || (POLICE(c) && ow)) {
        *side = 1;
        n = g_cars[0]->node - (POLICE(c) ? 0x12 : 0xc);
    } else {
        if (POLICE(c))
            *side = 0;
        else {
            *side = rnd() > 0x7fff;
            if (ow)
                *side = 1;
            if (!RACER(c)) {
                int *was = &g_opp.sides[BACKWARDS(c) ? 0 : 1];

                if (--*was < 0)
                    *was = 0;
                if (g_opp.sides[*side] >= g_opp.traffic - 1 && !ow)
                    *side = !*side;
                g_opp.sides[*side]++;
            }
        }
        if (g_cars[g_opp.zone[k][0]]->speed_abs < 0x10000 && *side == 1 && !RACER(c))
            n = g_opp.zone[k][5] - (int)(rnd() & 15);
        else
            n = g_opp.zone[k][4] + (int)(rnd() * 11 >> 16);
    }
    *node = (n < 0 ? 0 : n) & g_track.node_mask;
}

/* 0x7c7e8: a racer back on its grid place (unless the rival is a traffic car): its
   lane by the grid place's side, its node by its row */
static void place_racer(race_car *c, int *node)
{
    const unsigned char *nd, *n20;
    int w, g, odd;

    if (!RACER(c) || g_opp.rival_car == 9)
        return;
    nd = node_ptr(10 & g_track.node_mask);
    w = lane_width(nd[4] & 15, nd[1] >> 3) << 8;
    g = g_opp.grid[c->player];
    odd = g % 2;
    c->across_to = odd * w + -w / 2 + one_way() * w + c->lane_offset;
    n20 = node_ptr(20 & g_track.node_mask);
    if ((n20[4] >> 4 & 15) == 2 && (n20[4] & 15) == 1 && !odd)
        c->across_to -= w;
    *node = g_field > 2 ? 0x20 - 2 * g : 0x20 - g / 2 * 4;
}

/* 0x7c3f8: the flags of a car placed */
static void place_flags(race_car *c)
{
    c->bits[1] &= 0x7f;
    c->bits[2] &= 0xfd;
    if (c->bits[1] & 0x20)
        c->bits[1] = (c->bits[1] & 0xdb) | 4;
    else if (POLICE(c))
        g_opp.chase_off = 0;
    if (POLICE(c) && !(c->bits[1] & 4))
        c->part &= ~4;
}

/* 0x7c4d4: its speed: going the racers' way (or a racer) half the settings' and half its
   stretch's racer's, by its grip; coming the other way by how fast that racer goes */
static void place_speed(race_car *c, int side)
{
    int v = g_cars[g_opp.zone[c->zone][0]]->speed_abs;

    if (side || RACER(c)) {
        int s;

        c->speed_to = (CFG(0x48) >> 1) + (v >> 1);
        s = c->speed_to = by_grip(c, c->speed_to);
        if (RACER(c))
            c->road_speed = 0;
        else if (WORD(c) & 0x404)
            c->road_speed = c->speed_to = g_cars[0]->road_speed;
        else
            c->road_speed = s;
        c->bits[0] &= ~0x10;
        c->bits[1] &= ~0x10;
    } else {
        c->road_speed = c->speed_to = -oncoming(c);
        c->bits[0] &= ~0x10;
        c->bits[1] |= 0x10;
    }
}

/* 0x7c294: put at its node's point, a random bit along and its place across, turned
   along the road (round, coming the other way) */
static void put_on_road(race_car *c)
{
    int a, k, m[9];

    crash_road_frame(c);
    c->pos = c->road_at;
    c->pos.z += (int)rnd();
    a = c->across_to >> 8;
    c->pos.x += a * (c->road_side.x >> 8);
    c->pos.y += a * (c->road_side.y >> 8);
    c->pos.z += a * (c->road_side.z >> 8);
    if (drive_check_lap(c))
        crash_road_frame(c);
    memcpy(c->turn, &c->road_side, sizeof c->turn);
    if (BACKWARDS(c))
        for (k = 0; k < 3; k++) {
            c->turn[k] = -c->turn[k];
            c->turn[6 + k] = -c->turn[6 + k];
        }
    if (c->steer) {
        crash_turn_y(m, c->steer);
        crash_mat_mul(c->turn, m, c->turn);
    }
}

/* 0x7ccc0: the car placed at node, on that side, across: its progress, grip, a new place
   in its lane, steering and waits cleared, flags, speed, on the road. With `wait` a
   traffic car goes off the road to wait (kind 6) a random while */
static void place_at(race_car *c, int node, int side, int across, int wait)
{
    c->node = node < 0 ? 0 : node;
    c->progress = g_track.length * (c->lap - 1) * 4 + c->node;
    c->body.grip = CFG(0x38 + 4 * (int)(rnd() & 3));
    lane_pick(c);
    c->body.shift = 0;
    c->body.honks = 0;
    c->body.steady = STEADY(c);
    c->steer_to = c->steer = 0;
    c->body.wait = 0;
    c->yaw_rate = 0;
    /* (a police car placed while the camera watches from it: the camera back to the
       player's car, mode 5: with the police) */
    place_flags(c);
    place_speed(c, side);
    c->across_to = across;
    put_on_road(c);
    c->body.grip = CFG(0x38 + 4 * (int)(rnd() & 3));
    if (wait && (!RACER(c) || g_opp.rival_car == 9) && !POLICE(c)
        && (c->slot < 0 || c->slot >= g_humans)) {
        c->body.honks = (int)((unsigned)CFG(0x30) * rnd() >> 16) + 1;
        c->state = 6;
        if (!RACER(c))
            g_opp.waiting++;
    }
}

/* 0x7cac8: the car placed again: where (0x7c5b4), in a lane at random on that side, at
   its own place in it (a racer on its grid place) */
static void place(race_car *c, int wait)
{
    const unsigned char *nd;
    int side, node, w;

    if (g_drive.police == 1 && POLICE(c))
        g_cars[2]->zone = g_cars[0]->zone;
    c->slide_front = c->slide_rear = 0;
    if (c == g_cars[0]) {
        c->bits[1] &= ~0x24;
        c->bits[2] &= ~1;
    }
    place_where(c, &side, &node);
    nd = node_ptr(node);
    w = lane_width(nd[4] & 15, nd[1] >> 3) << 8;
    if (side || RACER(c))
        c->across_to = w * (int)((rnd() * (nd[4] & 15) >> 16) + 1) - w / 2;
    else
        c->across_to = -w * (int)((rnd() * (nd[4] >> 4 & 15) >> 16) + 1) + w / 2;
    c->across_to += c->lane_offset;
    place_racer(c, &node);
    place_at(c, node, side, c->across_to, wait);
}

/* 0x77d3c: a traffic car out of the racers' reach placed again ahead of a racer: in its
   stretch, else the next one's, else back to waiting off the road. (A police car's
   return: with the police) */
static void place_again(race_car *c)
{
    if (g_field > 2 || POLICE(c))
        return;
    while (c->zone < 2) {
        place(c, 0);
        if (!opponent_near(c->node))
            break;
        c->zone++;
    }
    if (c->zone < 2)
        return;
    c->zone = 0;
    place(c, 1);
}

/* 0x77de8: the traffic, or a racer before its first move, moved on: a car not yet on
   the road placed; one that has fallen out of both racers' outer windows placed again */
static void move_on(race_car *c)
{
    if (c->progress < 2) {
        if (!RACER(c))
            c->zone = 0;
        place(c, 1);
    } else if (g_field <= 2 && !in_reach(c->node)) {
        c->zone++;
        place_again(c);
    }
}

void opponent_wait(race_car *c)
{
    int top = 0, i;

    for (i = 0; i < g_field; i++)
        if (g_cars[i]->node > top)
            top = g_cars[i]->node;
    if (!RACER(c)) {
        c->zone = 0;
        if (top < 0x78 && (rnd() & 1))
            c->zone = 1;
    }
    c->node = 1;
    c->progress = 1;
    c->pos.x = 0x80000;
    c->pos.y = (int)((unsigned)(c->slot * 10 + 200) << 16);
    c->pos.z = 0x80000;
    if (--c->body.honks)
        return;
    if (top < c->slot * 16 + 15 || g_opp.traffic - CFG(0x34) >= g_opp.waiting || RACER(c)
        || (g_drive.options & 4)) {
        c->body.honks = 1;
        return;
    }
    g_opp.waiting--;
    c->state = 3;
    place_again(c);
}

/* ---- a step (drive_opponent 0x7b34c) -------------------------------------------------- */

/* 0x7b178: off the road's edge: its speed across turned back towards the road */
static void steer_back(race_car *c)
{
    vec3 v = { c->vel.x, 0, c->vel.z }, s = { -c->road_side.x, c->road_side.y, c->road_side.z };
    int x = c->road_off, e, q;

    e = mulr(p8(v.z, s.z) + p8(s.x, v.x), 0x14000);
    if ((x > 0 && e < 0) || (x < 0 && e > 0))
        e = 0;
    v.x -= mulr(e, s.x);
    v.y = -mulr(e, s.y);
    v.z -= mulr(e, s.z);
    q = p8(s.z, v.z) + p8(s.x, v.x);
    if (x > 0 && q > -0x10000) {
        v.x += mulr(-0x10000, s.x);
        v.z += mulr(-0x10000, s.z);
    } else if (x < 0 && q < 0x10000) {
        v.x += mulr(0x10000, s.x);
        v.z += mulr(0x10000, s.z);
    }
    c->vel.x = v.x;
    c->vel.z = v.z;
}

void opponent_drive(race_car *c)
{
    int racer = RACER(c) != 0, was = c->body.wait, m[9];

    if (racer) {
        int ax = iabs(c->vel.x), az = iabs(c->vel.z);

        c->speed_abs = ax > az ? ax + (az >> 2) : az + (ax >> 2);
    }
    c->own_turn = 1;
    if (!racer || c->progress < 2)
        move_on(c);
    if (drive_check_lap(c)) {
        crash_road_frame(c);
        memcpy(c->turn, &c->road_side, sizeof c->turn);
        if (BACKWARDS(c)) {
            int k;

            for (k = 0; k < 3; k++) {
                c->turn[k] = -c->turn[k];
                c->turn[6 + k] = -c->turn[6 + k];
            }
        }
        if (c->steer) {
            crash_turn_y(m, c->steer);
            crash_mat_mul(c->turn, m, c->turn);
        }
        c->body.wait = 0;
    }
    if (c->steer) {
        c->slide_rear = 0;
        c->slide_front = 0;
        if (!opponent_near(c->node))
            c->steer = c->steer_to = c->yaw_rate = 0;
        if (iabs(c->steer - c->steer_to) > 0xa0000 && iabs(c->road_speed) > 0x90000) {
            int l = lane_of(c->node, c->road_off);

            c->slide_rear = outside(c->node, l) || edge(c->node, l) ? 0x408 : 0x401;
            c->slide_front = iabs(c->steer - c->steer_to) * 20;
        } else
            c->slide_rear = c->slide_front = 0;
        /* turned too far: it spins out (0x1ca9c), a police car's chase dropped. (The
           replay's highlight 0x52 near a player: not in the port) */
        if (iabs(c->steer) > (WORD(c) & 0x404 ? 0x200000 : 0x160000) && !(c->bits[2] & 2)) {
            crash_spin_out(c);
            if (POLICE(c))
                opponent_chase_drop(0);
        }
    } else
        c->slide_rear = c->slide_front = 0;
    if (c->steer != c->steer_to && iabs(c->steer) < 0xf0000 && c->on_ground) {
        if (c->body.steady)
            c->body.steady--;
    } else
        c->body.steady = STEADY(c);
    if (c->progress < 150)
        c->yaw_rate = c->steer = c->steer_to = 0;
    if (c->yaw_rate) {
        int d;

        if (iabs(c->yaw_rate) > 0x12c0000 || iabs(c->steer) > 0x12c0000)
            c->steer = c->yaw_rate = 0;
        d = mulr(c->yaw_rate, 0x444);
        c->steer += d;
        crash_turn_y(m, d);
        crash_mat_mul(c->turn, m, c->turn);
    }
    /* the steering moved towards where it steers to */
    if (c->steer < c->steer_to) {
        if (c->yaw_rate < 0)
            c->yaw_rate += racer ? 0x23333 : 0x13333;
        else if (iabs(c->road_speed) > 0x30000) {
            c->yaw_rate = -9 * c->steer;
            if (c->steer_to - 0x30000 < c->steer) {
                c->yaw_rate = 0;
                c->steer = c->steer_to;
            }
        }
    } else if (c->steer > c->steer_to) {
        if (c->yaw_rate > 0)
            c->yaw_rate -= racer ? 0x23333 : 0x13333;
        else if (iabs(c->road_speed) > 0x30000) {
            c->yaw_rate = -9 * c->steer;
            if (c->steer_to + 0x30000 > c->steer) {
                c->yaw_rate = 0;
                c->steer = c->steer_to;
            }
        }
    } else if (iabs(c->steer) < iabs(c->steer_to) + 0x20000) {
        c->yaw_rate = 0;
        c->steer = c->steer_to;
    }
    if (!c->body.wait)
        plan(c, was);
    c->body.wait--;
    /* slower in the edge lanes */
    if (!g_track.loop && opponent_near(c->node) && edge(c->node, lane_of(c->node, c->road_off))
        && c->progress > 0x46)
        c->road_speed = (c->road_speed >> 8) * 255;
    /* off the road's sides */
    if (c->road_off < -road_left(c->node) || c->road_off > road_right(c->node)) {
        if (!opponent_near(c->node))
            c->steer_to = c->steer = 0;
        else if (c->lane_offset < -0x40000) {
            c->speed_to = c->road_speed = 0;
            c->vel.x = c->vel.y = c->vel.z = 0;
        } else
            steer_back(c);
    }
    if (!c->on_ground)
        c->vel.y -= 0x6838;
    /* moved: a step is 1/32 + 1/512 + 1/128 + 1/2048 of a speed (x the other way) */
    c->pos.x -= (c->vel.x >> 5) + (c->vel.x >> 9);
    c->pos.y += (c->vel.y >> 5) + (c->vel.y >> 9);
    c->pos.z += (c->vel.z >> 5) + (c->vel.z >> 9);
    c->pos.x -= (c->vel.x >> 7) + (c->vel.x >> 11);
    c->pos.y += (c->vel.y >> 11) + (c->vel.y >> 7);
    c->pos.z += (c->vel.z >> 11) + (c->vel.z >> 7);
    if (c->body.steady && c->body.steady != STEADY(c))
        put_down(c, off_road(c));
}

void opponent_chase_drop(int p)
{
    race_car *pc = g_ncars > 2 ? g_cars[2] : 0, *pl = g_cars[0];

    if (g_drive.police != 1)
        return;
    if (pc) {
        if (p)
            pc->bits[2] &= 0xf7;
        pc->bits[1] &= 0xdb;
    }
    pl->bits[1] &= 0xdb;
    pl->bits[2] &= 0xfe;
    if (pc) {
        pc->bits[1] &= 0xdb;
        pc->bits[2] &= 0x87;
    }
    g_opp.chase_off = 0;
}

/* 0x7c030's loop: a player's car put back on the road after its crash puts back the
   racers thrown about or stood up (kinds 4, 5) that are within the road's edges: driven
   again (kind 3), standing, along the road. The other cars thrown about, the police and
   those out of the stretches round the racers are placed again (0x7cac8), to wait off
   the road. Their chase flags are cleared */
void opponent_put_back(void)
{
    int n = g_track.loop ? g_field : g_ncars, i;

    for (i = g_humans; i < n; i++) {
        race_car *c = g_cars[i];
        const unsigned char *nd;
        int far, k;

        if (!c)
            continue;
        far = !opponent_near(c->node);
        c->bits[1] &= 0xdb;
        c->bits[2] &= 0xf7;
        g_opp.chase_off = 0;
        k = c->state;
        if (k != 5 && k != 4 && !(c->bits[0] & 8) && !far)
            continue;
        if (!RACER(c)) {
            if (c->slot < 0 || c->slot >= g_field) {
                if (k == 6)
                    g_opp.waiting--;
                c->state = 3;
                place(c, 1);
            }
            continue;
        }
        if (k != 4 && k != 5)
            continue;
        nd = node_ptr(c->node);
        if (-(int)((unsigned)nd[2] << 13) > c->road_off || (int)((unsigned)nd[3] << 13) < c->road_off)
            continue;
        if (k == 4)
            crash_set_kind(c, 3);
        else
            c->state = 3;
        c->yaw_rate = 0;
        c->steer = 0;
        c->steer_to = 0;
        memcpy(c->turn, &c->road_side, sizeof c->turn);
        c->vel.x = c->vel.y = c->vel.z = 0;
        c->road_speed = 0;
        c->on_ground = 1;
    }
}

/* drive_kind5 0x7d9e4: a computer's car stood up again after being thrown about (kind
   5). Its turning eased out; while a player is near, it drives with nothing to reach (it
   stops); out of their sight it is put back along the road, standing, and driven as
   before (kind 3). A police car's chase is dropped first */
void opponent_recover(race_car *c)
{
    int d = c->bits[0] & 4 ? 0x23333 : 0x13333;

    if (POLICE(c))
        opponent_chase_drop(1);
    /* the turning eased out: a turn one way by d a step; the other way (positive) the
       original eases it too, then clears whatever is left at once */
    if (c->yaw_rate > 0)
        c->yaw_rate = 0;
    else if (c->yaw_rate < 0) {
        c->yaw_rate += d;
        if (c->yaw_rate > 0)
            c->yaw_rate = 0;
    }
    if (near_players(c)) {
        c->speed_to = 0;
        opponent_drive(c);
        return;
    }
    c->yaw_rate = 0;
    c->steer = 0;
    c->steer_to = 0;
    memcpy(c->turn, &c->road_side, sizeof c->turn);
    c->vel.x = c->vel.y = c->vel.z = 0;
    c->road_speed = 0;
    c->on_ground = 1;
    c->state = 3;
}

/* ---- every fourth step: the target speed (0x786d4's part) ----------------------------- */

/* 0x7e548: a target speed taken by the car's grip (the traffic's), a positive one kept
   between 0x8e666 and 0xd6666 below it */
static int by_grip(const race_car *c, int v)
{
    int r;

    if (v <= 0)
        return mulr(v, c->body.grip);
    r = mulr(v, c->body.grip);
    if (r < v - 0xd6666)
        r = v - 0xd6666;
    return r < 0x8e666 ? 0x8e666 : r;
}

/* 0x78660: a car coming the other way: slower the faster its stretch's racer goes */
static int oncoming(const race_car *c)
{
    int rs = g_cars[g_opp.zone[c->zone][0]]->road_speed;

    return rs < 0xdcccc ? 0x136666 : rs < 0x1bcccc ? 0xde666 : rs < 0x29b333 ? 0xb1c28
         : rs < 0x378000 ? 0x6e666 : rs < 0x456666 ? 0x428f5 : 0x2c51e;
}

/* 0x7e64c: the speed the road lets the car drive at: the traffic the limit (by the
   player's car), a racer its top slowed in bends, by its driver and how far it is from
   the leading player */
static int road_allows(race_car *c)
{
    int nt = g_drive.options & 4, v0 = limit_at(c->node, 0), f, dh, q, r, i, gg, cc, m, t;
    const unsigned char *rec = g_opp.rec[c->player];
    race_car *lead = g_opp.lead;

    if (!RACER(c)) {
        race_car *p = g_cars[0];

        if (p->number >= 0 && p->number <= 7)
            v0 = mulr(v0, exe_i32(0xf74b0 + 4 * p->number));
        return p->crashing ? 0 : v0;
    }
    f = c->top;
    if (!nt)
        f = mulr(rec32(rec, 0x55), f);
    dh = iabs(((node_i32(c->node, 0x16) >> 16) << 10) - ((node_i32(c->node - 1, 0x16) >> 16) << 10));
    g_opp.bend_drop = dh;
    if (dh > 0x800000)
        dh = 0x1000000 - dh;
    q = mulr(c->bend - dh, c->bend_inv);
    r = (q >> 3) + (q >> 4) + (q >> 6);
    g_opp.bend_slow = r;
    if (!nt)
        g_opp.bend_slow = r = mulr(r, rec32(rec, 0x59));
    i = (c->progress - lead->progress) / 10 + 10;
    g_opp.behind = i = i < 0 ? 0 : i > 20 ? 20 : i;
    gg = nt ? 0x10000 : rec32(rec, 1 + 4 * i);
    cc = g_race_type == 1 ? CFG(0x5c + 4 * i) : CFG(0xb0 + 4 * i);
    m = mulr(cc, gg);
    if (lead->speed_abs < 0x20000 && lead->state == 2)
        m = 0x10000;
    else if (c->progress < lead->progress && lead->state == 4 && m > 0x10000)
        m = 0x10000;
    t = mulr(f, mulr(r + 0xcccc, m));
    if (t > 0x13333 && near_players(c))
        t = 0x13333;
    if (t < 0xcccc && near_players(c))
        t = 0xcccc;
    if (c->progress < 0x41)
        t = mulr(t, (0x41 - c->progress) * 0x3f0 + 0x10000);
    t = mulr(v0, t);
    if (drive_race_progress(c) == 2 && past_finish(c) >= g_opp.grid[c->player] / 2 * 2)
        t = 0;
    return t;
}

void opponent_target_speed(race_car *c)
{
    race_car *lead = g_opp.lead;
    int r;

    if (BACKWARDS(c)) {
        c->speed_to = by_grip(c, -oncoming(c));
        return;
    }
    if (RACER(c))
        c->speed_to = lead->road_speed;
    else
        c->speed_to = by_grip(c, (CFG(0x48) >> 1) + (lead->road_speed >> 1));
    if (WORD(c) & 0x404) {
        int v = lead->road_speed + CFG(0x18);

        if (v > c->speed_to)
            c->speed_to = v;
        /* (a police car on a chase follows the player's car: not yet) */
    } else {
        int v = by_grip(c, limit_at(c->node, 2));

        if (v < c->speed_to)
            c->speed_to = v;
    }
    r = road_allows(c);
    if (r < c->speed_to || RACER(c))
        c->speed_to = r;
}

void opponent_wander(race_car *c)
{
    int lim = c->slot >= g_humans && c->slot < g_field
            ? rec32(g_opp.rec[c->slot], 0x89) : g_opp.wander;

    if ((unsigned)lim * rnd() >> 16 == 1)
        lane_pick(c);
}

/* 0x7e5e4: a new place in its lane, from its driver's record or the settings */
static void lane_pick(race_car *c)
{
    unsigned k = rnd() & 3;

    c->lane_offset = RACER(c) ? rec32(g_opp.rec[c->player], 0x79 + 4 * k) : CFG(0x4c + 4 * k);
}

/* ---- every fourth step: the lanes (0x7a040) --------------------------------------------- */

/* 0x78c88: the nearest cars ahead (behind, driving the wrong way) in the lane to the
   left, its own and the right (nb), and the speeds they leave it: a (0x50000 a node
   between them) and b (0x1b6db a node). occ: the lanes taken already */
static void neighbours(race_car *c, const unsigned *bits, int l, unsigned *occ, int *a, int *b,
                       race_car **nb)
{
    int me = 0, i, found = 0;
    unsigned mask[3];
    race_car *o = 0;

    for (i = 0; i < g_ncars; i++)
        if (g_cars[g_timing.by_road[i]] == c)
            me = i;
    mask[1] = 1u << l;
    mask[0] = 1u << ((l - 1) & 31);
    mask[2] = 1u << ((l + 1) & 31);
    if (!BACKWARDS(c)) {
        int k, pos;

        for (k = 1, pos = me + 1; k < g_ncars; k++, pos++) {
            int idx = pos % g_ncars, dn, dp, lim, j;

            o = g_cars[g_timing.by_road[idx]];
            if (o == c || !(o->part & 4) || o->state == 6)
                continue;
            dn = o->node - c->node;
            dp = o->progress - c->progress;
            if (iabs(dn) < 2) {
                vec3 rel;

                rel.x = o->pos.x - c->pos.x;
                rel.z = o->pos.z - c->pos.z;
                if (p8(rel.z, c->road_fwd.z) + p8(c->road_fwd.x, rel.x) < 0x50000)
                    continue;
            }
            if (dn < 0 && dp > 0)
                dn = -dn;
            lim = RACER(c) && BACKWARDS(o) ? rec32(g_opp.rec[c->slot], 0x6d) : 0x1b;
            if (dn <= 0 || dn >= lim)
                continue;
            dn -= 2;
            if ((WORD(c) & 0x404) && dn < 1)
                dn = 1;
            for (j = 0; j < 3; j++) {
                int jj = j == 0 ? 1 : j == 1 ? 0 : 2;

                if ((jj == 0 && l <= 0) || (jj == 2 && l >= 7))
                    continue;
                if ((bits[idx] & mask[jj]) && !(*occ & mask[jj])) {
                    b[jj] = o->road_speed + dn * 0x1b6db;
                    a[jj] = o->road_speed + dn * 0x50000;
                    *occ |= mask[jj];
                    found++;
                    nb[jj] = o;
                }
            }
            if (found == 3)
                break;
        }
        /* the traffic slows behind the car ahead */
        if (c->slot >= g_field + g_drive.police && c->slot < g_field + g_drive.police + g_opp.traffic
            && nb[1] && nb[1]->node - c->node < 3) {
            a[1] >>= 1;
            b[1] >>= 1;
            if (c->speed_to <= nb[1]->road_speed) {
                a[0] >>= 1;
                b[0] >>= 1;
                a[2] >>= 1;
                b[2] >>= 1;
            }
        }
        /* a racer right behind a car: (the last car looked at's) speed less an eighth */
        if (c->slot >= 0 && c->slot < g_field && nb[1] && nb[1]->node - c->node < 2
            && o && o->road_speed > 0x140000) {
            int v = o->road_speed - (o->road_speed >> 3);

            a[1] = b[1] = v < 0 ? 0x10000 : v;
        }
        if (l + 1 == lane_last(c->node)) {
            a[2] >>= 1;
            b[2] >>= 1;
        } else if (l - 1 == lane_first(c->node)) {
            a[0] >>= 1;
            b[0] >>= 1;
        }
    } else {
        int pos;

        for (pos = g_ncars + me; pos > me && found != 3; pos--) {
            int idx = pos % g_ncars, dn, dp, j;

            o = g_cars[g_timing.by_road[idx]];
            if (o == c || !(o->part & 4) || o->state == 6)
                continue;
            dn = o->node - c->node;
            dp = o->progress - c->progress;
            if (dn < 0 && dp > 0)
                dn = -dn;
            if (dn > 0 || -dn >= 0x1b)
                continue;
            dn += 2;
            for (j = 0; j < 3; j++) {
                int jj = j == 0 ? 1 : j == 1 ? 0 : 2;

                if ((jj == 0 && l <= 0) || (jj == 2 && l >= 7))
                    continue;
                if ((bits[idx] & mask[jj]) && !(*occ & mask[jj])) {
                    b[jj] = o->road_speed + dn * 0x1b6db;
                    a[jj] = o->road_speed + dn * 0x50000;
                    *occ |= mask[jj];
                    found++;
                    nb[jj] = o;
                }
            }
        }
        if (nb[1] && c->slot >= g_field + g_drive.police
            && c->slot < g_field + g_drive.police + g_opp.traffic && c->node - nb[1]->node < 3) {
            a[1] >>= 1;
            b[1] >>= 1;
            if (c->speed_to >= nb[1]->road_speed) {
                a[0] >>= 1;
                b[0] >>= 1;
                a[2] >>= 1;
                b[2] >>= 1;
            }
        }
    }
    /* a racer that is faster than a racer ahead keeps out of its lane while it closes */
    if (c->road_speed > 0xa0000 && nb[1] && nb[1]->slot >= 0 && nb[1]->slot < g_field
        && a[0] > a[1] && a[1] < a[2]) {
        int dn = iabs(c->node - nb[1]->node), dv = c->road_speed - nb[1]->road_speed;

        if (dn > 2 && dn < 0xf && dv > 0) {
            int t = iabs(slide_limit(c, 1, c->road_speed));

            if (mulr(mulr(0x50000, dv), 0x20000) > mulr(dn * 0x60000, t))
                a[1] = b[1] = 0;
        }
    }
}

/* 0x78a80: each lane scored by how much the speeds it leaves fall short of the target
   and the car's speed; the target cut to its own lane's */
static void score_speeds(race_car *c, const int *a, const int *b, int *s)
{
    int t = c->speed_to, rs = c->road_speed, j;

    if (!BACKWARDS(c)) {
        for (j = 0; j < 3; j++) {
            if (t > a[j])
                s[j] += ((t - a[j]) >> 8) * -205;
            if (t > b[j])
                s[j] -= ((t - b[j]) >> 8) << 7;
            if (a[j] < rs)
                s[j] -= ((rs - a[j]) >> 8) << 8;
            if (b[j] < rs)
                s[j] += ((rs - b[j]) >> 8) * -26;
        }
        if (t > a[1])
            c->speed_to = a[1];
        if (c->speed_to < -0x14ccc)
            c->speed_to = -0x14ccc;
    } else {
        for (j = 0; j < 3; j++) {
            if (t < a[j])
                s[j] += ((a[j] - t) >> 8) * -205;
            if (t < b[j])
                s[j] -= ((b[j] - t) >> 8) << 7;
            if (a[j] > rs)
                s[j] -= ((a[j] - rs) >> 8) << 8;
            if (b[j] > rs)
                s[j] += ((b[j] - rs) >> 8) * -26;
        }
        if (t < a[1])
            c->speed_to = a[1];
        if (c->speed_to > 0x14ccc)
            c->speed_to = 0x14ccc;
    }
}

/* 0x7850c: changing lanes beside a car: the target cut to the new lane's speed unless
   the car can slide over before it closes */
static void change_speed(race_car *c, race_car *const *nb, int l, int best, const int *a)
{
    int v = a[1];

    if (best != l && nb[1] && nb[1]->state != 4) {
        int dp = c->progress - nb[1]->progress, dv, dx, t;

        if (dp < 0)
            dp += 4 * g_track.length;
        dp = iabs(dp) * 3 << 17;
        dv = iabs(c->road_speed - nb[1]->road_speed);
        dx = iabs(c->across_to - c->road_off);
        t = iabs(slide_limit(c, 1, c->road_speed));
        if (mulr(mulr(dx, dv), 0x13333) < mulr(dp, t))
            v = best > l ? a[2] : a[0];
    }
    if (!BACKWARDS(c)) {
        if (v < c->speed_to)
            c->speed_to = v;
        if (c->speed_to < -0x14ccc)
            c->speed_to = -0x14ccc;
    } else {
        if (v > c->speed_to)
            c->speed_to = v;
        if (c->speed_to > 0x14ccc)
            c->speed_to = 0x14ccc;
    }
}

/* 0x79b60: a racer near a player: the lanes the player blocks scored down; stuck behind
   one in its lane it wants to sound its horn (the sounds: not yet) */
static void near_humans(race_car *c, int l, int *s, int node, int prog)
{
    const unsigned char *rec = g_opp.rec[c->slot];
    int i;

    for (i = 0; i < g_humans; i++) {
        race_car *h = g_cars[i];
        int lh = lane_of(h->node, h->road_off);

        if (h->node - 1 <= node && h->node + rec32(rec, 0x5d) > node && prog > 0x86) {
            int dd = -(p8(h->pos.z - c->pos.z, c->road_fwd.z) + p8(c->road_fwd.x, h->pos.x - c->pos.x));

            if (dd >= rec32(rec, 0x61))
                s[lh < l ? 0 : lh == l ? 1 : 2] += 0x70000;
        }
        if (c->node < h->node && lh == l && !g_track.loop) {
            unsigned rs = (unsigned)iabs(g_opp.rival->road_speed);
            /* 0x3e770: a product of halves, times 4 */
            unsigned lim = (unsigned)mulr(0x35555 >> 1, (int)((unsigned)((h->node - c->node) * 6 << 16) >> 1)) << 2;

            if (rs > lim) {
                unsigned r = ((rnd() * 10) >> 16);

                c->body.honks++;
                if ((int)r < c->body.honks)
                    c->body.honks = 0;
            }
        } else
            c->body.honks = 0;
    }
}

/* 0x79e28: a racer: past the finish it pulls over to its side by its grid place; its
   revs follow its speed through the gears */
static void racer_engine(race_car *c, int l, int *s)
{
    int node = c->node, prog = c->progress, i, target;

    if (drive_race_progress(c) == 2 && !edge(c->node, l)
        && past_finish(c) + 10 >= g_opp.grid[c->player] / 2 * 2) {
        if (g_opp.grid[c->player] % 2)
            s[2] += 0x640000;
        else
            s[0] += 0x640000;
    }
    for (i = 0; i < 5; i++) {
        int d = c->road_speed - c->gear_top[i];

        if (d > 0 && d < 0x8000 && c->road_speed >> 16 && !c->body.shift)
            c->body.shift = exe_i32(0xf74d0 + 4 * c->number);
        if (c->road_speed < c->gear_top[i])
            break;
    }
    if (c->body.shift > 0)
        target = 3000;
    else {
        int v = mulr(mulr(c->road_speed, c->gear_inv[i]), c->rev_scale);

        target = (v >> 17) + (v >> 18);
    }
    c->rpm += (target - c->rpm) / 8;
    if ((c->rev_scale >> 16) - 200 < c->rpm)
        c->rpm = c->rev_scale >> 16;
    near_humans(c, l, s, node, prog);
}

void opponent_tick(void)
{
    unsigned bits[RACE_CARS_MAX];
    int counts[8], i;

    if (g_opp.humans_only) {
        for (i = 0; i < g_humans && i < g_ncars; i++)
            if (g_cars[i] && g_cars[i]->state != 6)
                crash_refresh(g_cars[i]);
        return;
    }
    if (g_opp.off)
        return;
    g_opp.ticks += 4;
    opponent_zones();
    {
        int best = 0;

        for (i = 0; i < g_humans; i++)
            if (best < g_cars[i]->progress) {
                best = g_cars[i]->progress;
                g_opp.lead = g_cars[i];
                g_opp.lead_index = i;
            }
    }
    /* (the traffic's coming and going, 0x79500: not yet) */
    memset(counts, 0, sizeof counts);
    /* each car's lanes taken */
    for (i = 0; i < g_ncars; i++) {
        race_car *c = g_cars[g_timing.by_road[i]];
        int l1, l2;

        if (c->state == 6)
            continue;
        crash_refresh(c);
        l1 = lane_of(c->node, c->road_off - c->reach + 0x8000);
        l2 = lane_of(c->node, c->road_off + c->reach - 0x8000);
        if (l2 == l1) {
            bits[i] = 1u << l1;
            counts[l1]++;
        } else if (l2 == l1 + 1) {
            bits[i] = 3u << l1;
            counts[l2]++;
            counts[l1]++;
        } else {
            bits[i] = 7u << l1;
            counts[l1]++;
            if (l1 + 1 < 8)
                counts[l1 + 1]++;
            if (l1 + 2 < 8)
                counts[l1 + 2]++;
        }
    }
    g_cars[0]->bits[1] &= 0xf3;
    /* each computer's car: its lane and target speed */
    for (i = 0; i < g_ncars; i++) {
        race_car *c = g_cars[g_timing.by_road[i]], *nb[3] = { 0, 0, 0 };
        int s[3], a[3], b[3], l, near, n, best, m;
        unsigned occ = 0;

        if (c->state == 6)
            continue;
        near = opponent_near(c->node);
        if (!(c->part & 4) || (c->slot >= 0 && c->slot < g_humans) || c->state == 4)
            continue;
        n = c->node;
        l = lane_of(n, c->road_off);
        if (BACKWARDS(c)) {
            s[0] = 0x10000;
            s[1] = 0;
            s[2] = -0x10000;
        } else {
            s[0] = -0x10000;
            s[1] = s[2] = 0;
            if (g_race_type >= 2 && c->progress < 200
                && c->progress < grid_place(c->player) * 5 + 100 && !edge(n, l))
                s[1] = 0x1b580000;
            else if (g_race_type == 1 && RACER(c) && n < 0x3c && iabs(n - g_cars[0]->node) < 5
                     && !edge(n, l))
                s[1] = 0x460000;
        }
        /* (a police car's lanes, 0x79728: not yet) */
        if (RACER(c))
            racer_engine(c, l, s);
        /* alone in its lane on a straight road: it keeps to it */
        if (!nb[1] && lanes_right(n) == lanes_right(n + 0xb) && counts[l] == 1
            && ((BACKWARDS(c) && l < 4) || (!BACKWARDS(c) && l >= 4))
            && !edge(n, l) && !outside(n, l) && !(WORD(c) & 0x404)) {
            c->across_to = lane_middle(n, l);
            continue;
        }
        if (c->bits[2] & 2) {
            c->across_to = lane_middle(n, l);
            continue;
        }
        /* each lane scored */
        if (g_race_type == 1) {
            if (!BACKWARDS(c)) {
                if (l - 1 < 4)
                    s[0] -= 0x30000;
                if (l < 4)
                    s[1] -= 0x30000;
                if (l + 1 < 4)
                    s[2] -= 0x30000;
            } else {
                if (l - 1 >= 4)
                    s[0] -= 0x50000;
                if (l >= 4)
                    s[1] -= 0x50000;
                if (l + 1 >= 4)
                    s[2] -= 0x50000;
            }
        }
        /* a road narrowing ahead (or just behind): away from its closing side */
        if (!BACKWARDS(c) && lanes_right(n) > lanes_right(n + 0xb)) {
            if (edge(n, l + 1))
                s[0] += 0x280000;
            else if (edge(n, l + 2))
                s[1] += 0x280000;
        } else if (!BACKWARDS(c) && n > 0xb && c->body.grip < 0x10000
                   && lanes_right(n) > lanes_right(n - 0xb)) {
            if (edge(n, l + 1))
                s[1] += 0x280000;
            else
                s[2] += 0x280000;
        }
        /* the edge lanes, and too narrow a road beside */
        {
            int pen = RACER(c) ? -0xf0000 : -0x260000;
            const unsigned char *nd = node_ptr(n);
            int left = (nd[2] << 13) - (nd[0] << 13), right = (nd[3] << 13) - (nd[1] << 13);

            if (edge(n, l - 1)) {
                s[0] += pen;
                if (left < c->body.half[0])
                    s[0] -= 0x640000;
            }
            if (edge(n, l)) {
                s[1] += pen;
                if ((l < 0 ? left : right) < c->body.half[0])
                    s[1] -= 0x640000;
            }
            if (edge(n, l + 1)) {
                s[2] += pen;
                if (right < c->body.half[0])
                    s[2] -= 0x640000;
            }
        }
        if (outside(n, l - 1))
            s[0] -= 0x3e80000;
        if (outside(n, l))
            s[1] -= 0x3e80000;
        if (outside(n, l + 1))
            s[2] -= 0x3e80000;
        a[0] = a[1] = a[2] = b[0] = b[1] = b[2] = c->speed_to;
        neighbours(c, bits, l, &occ, a, b, nb);
        /* (EXPLODE's push, 0x78370: not yet) */
        score_speeds(c, a, b, s);
        if (BACKWARDS(c) && nb[1] && nb[1]->road_speed > 0xa0000)
            s[2] -= 0x1e0000;
        /* the traffic coming the other way sometimes sounds its horn (the sound: not yet) */
        if (!(WORD(c) & 0x404) && !POLICE(c) && BACKWARDS(c) && near && a[1] > -0x30000)
            rnd();
        /* the best lane, its middle, kept on the road, moved by its place in the lane */
        best = l;
        m = s[1];
        if (s[0] > s[1]) {
            best = l - 1;
            m = s[0];
        }
        if (m < s[2])
            best = l + 1;
        c->across_to = lane_middle(n, best);
        if (road_right(n) - c->body.half[0] < c->across_to)
            c->across_to = road_right(n) - c->body.half[0];
        if (c->body.half[0] - road_left(n) > c->across_to)
            c->across_to = c->body.half[0] - road_left(n);
        if (BACKWARDS(c))
            c->across_to += c->lane_offset;
        else
            c->across_to -= c->lane_offset;
        if (g_opp.lane_rule && best != l)
            change_speed(c, nb, l, best, a);
        if (RACER(c) && c->progress < 0x41)
            c->speed_to = mulr(c->speed_to, (0x41 - c->progress) * 0x1f80 + 0x10000);
    }
}

/* ---- the race starts -------------------------------------------------------------- */

/* 0x63f50: the settings, as the program's stores of them (mov [eax+d], imm32), then
   two of them made steps (x 60) and one scaled */
static void settings(void)
{
    const unsigned char *p = exe_at(0x63f54, 0x400);

    memset(g_opp.cfg, 0, sizeof g_opp.cfg);
    while (p && (p[0] == 0xc7 || (p[0] == 0x8b && p[1] == 0x50))) {
        if (p[0] == 0x8b) {
            /* (a read of +0x24 for the sums after, scheduled among the stores) */
            p += 3;
            continue;
        }
        int at = p[1] == 0x00 ? 0 : p[1] == 0x40 ? p[2] : (int)(p[2] | p[3] << 8 | p[4] << 16 | (unsigned)p[5] << 24);
        int len = p[1] == 0x00 ? 2 : p[1] == 0x40 ? 3 : 6;

        if (at >= 0 && at / 4 < OPP_CFG)
            g_opp.cfg[at / 4] = (int)(p[len] | p[len + 1] << 8 | p[len + 2] << 16 | (unsigned)p[len + 3] << 24);
        p += len + 4;
    }
    CFG(0x24) *= 60;
    CFG(0x28) *= 60;
    CFG(0x48) = mulr(CFG(0x48), 0x471c);
}

/* 0x7dde8: the set of drivers by the race type (a single race: by the player's car) */
static int driver_set(void)
{
    static const int by_car[8] = { 2, 0, 1, 1, 0, 1, 2, 2 };

    switch (g_race_type) {
    case 0:
        return 7;
    case 1:
        return g_drive.options & 4 ? 6 : 5;
    case 2:
        return g_cars[2]->number == g_cars[3]->number ? 4 : 3;
    default:
        return g_cars[0]->number >= 0 && g_cars[0]->number <= 7 ? by_car[g_cars[0]->number] : 0;
    }
}

void opponent_start(unsigned seed)
{
    int set = driver_set(), i;

    settings();
    memset(g_opp.rec, 0, sizeof g_opp.rec);
    for (i = 0; i < 9; i++)
        g_opp.drivers[i] = 99;         /* (0x373ea, race_setup) */
    for (i = g_humans; i < g_field && i < 8; i++) {
        int letter = exe_at(0xf4038 + set * 8 + g_opp.grid[i], 1)[0] - 'A';

        g_opp.drivers[i] = letter;
        g_opp.rec[i] = exe_at(0xf4070 + 0x8d * letter, 0x8d);
    }
    g_opp.lead = g_cars[0];
    g_opp.lead_index = 0;
    g_opp.rival = g_ncars > 1 ? g_cars[1] : g_cars[0];
    zones_start();
    g_opp.off = 0;
    g_opp.ticks = 0;
    g_opp.lane_rule = 1;
    g_opp.wander = exe_i32(0xf74f0);
    g_opp.brake = 0x5999;
    g_opp.waiting = 0;
    g_opp.sides[0] = 0;
    g_opp.sides[1] = g_opp.traffic;
    g_opp.chase_off = 0;
    if (g_race_type == 0 || (g_race_type == 1 && g_track.loop))
        CFG(0x34) = 0;
    /* 0x38c10 */
    g_opp.mult = seed | 1;
    g_opp.random = 3 * seed + (seed % 2 == 0);
}
