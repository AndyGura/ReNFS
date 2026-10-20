/* The cars drawn into the road (NOTES §4.10). */
#include "cardraw.h"
#include "camera.h"
#include "exe.h"
#include "polyfill.h"
#include "racepal.h"
#include "roaddraw.h"
#include "smoke.h"
#include "track.h"
#include "video.h"
#include <string.h>

race_car *g_cars[RACE_CARS_MAX];
int g_ncars;
race_car *g_car_order[RACE_CARS_MAX];
int g_car_order_n;
int g_player;
int g_racers;
int g_race_type;
int g_field;
int g_humans;
unsigned char g_car_marks[RACE_CARS_MAX];
int g_no_markers;
int g_car_ticket;
unsigned char g_cars_seen[CARS_SEEN];
unsigned char g_cars_behind[CARS_BEHIND];
const race_camera *g_camera;
const int *g_camera_m;

void cars_list(void)
{
    int i;

    g_car_order_n = 0;
    for (i = 0; i < g_ncars; i++) {
        race_car *c = g_cars[i];
        c->draw_node = c->mirror_node = c->node;
        g_car_order[g_car_order_n++] = c;
    }
}

void cars_seen(int cam_node)
{
    int i, node, d;

    memset(g_cars_seen, 0, sizeof g_cars_seen);
    for (i = 0; i < g_ncars; i++) {
        race_car *c = g_cars[i];
        if (c->state == 6)
            continue;
        node = c->draw_node;
        if (c->dist - 0x29999 < 0)
            node--;
        if (node < 0)
            node += g_track.length * 4;
        c->draw_node = node;
        d = node >= cam_node ? node - cam_node : node + g_track.length * 4 - cam_node;
        if (d < CARS_SEEN)
            g_cars_seen[d] = 1;
    }
    /* 0x677f8: the same for the mirror, its node moved on half way along it */
    memset(g_cars_behind, 0, sizeof g_cars_behind);
    for (i = 0; i < g_ncars; i++) {
        race_car *c = g_cars[i];
        if (c->state == 6)
            continue;
        node = c->mirror_node;
        if (c->dist + 0x29999 > 0x50000)
            node++;
        if (node > g_track.length * 4)
            node -= g_track.length * 4;
        c->mirror_node = node;
        d = node <= cam_node ? cam_node - node : g_track.length * 4 - node + cam_node;
        if (d < CARS_BEHIND)
            g_cars_behind[d] = 1;
    }
}

static int iabs(int v)
{
    return v > 0 ? v : -v;
}

/* (x, z) turned by the angle a (0x1000000 a turn) as 0x3e918 does */
static void turn2d(int x, int z, int a, int *ox, int *oz)
{
    int c = ea_cos(a >> 8), s = ea_sin(a >> 8);
    *ox = fix_mul(c, x) - fix_mul(s, z);
    *oz = fix_mul(s, x) + fix_mul(c, z);
}

static int mul(int a, int b)
{
    return (int)((unsigned)a * (unsigned)b);
}

void car_road_plane(race_car *c, int yaw, vec3 *centre)
{
    vec3 a, b, ctr = c->pos, pa, pb, pc, u, v, n;
    const unsigned char *r = node_ptr(c->node);
    int inv, q;

    turn2d(0, c->half_len, -yaw, &a.x, &a.z);
    turn2d(-c->half_wid, 0, -yaw, &b.x, &b.z);
    a.y = b.y = 0;
    pa.x = node_i32(c->node, 8);
    pa.y = node_i32(c->node, 12);
    pa.z = node_i32(c->node, 16);
    pb.x = pa.x + 2 * (short)(r[0x1c] | r[0x1d] << 8);
    pb.y = pa.y + 2 * (short)(r[0x1e] | r[0x1f] << 8);
    pb.z = pa.z + 2 * (short)(r[0x20] | r[0x21] << 8);
    pc.x = node_i32(c->node + 1, 8);
    pc.y = node_i32(c->node + 1, 12);
    pc.z = node_i32(c->node + 1, 16);
    u.x = pa.x - pb.x;
    u.y = pa.y - pb.y;
    u.z = pa.z - pb.z;
    v.x = pc.x - pb.x;
    v.y = pc.y - pb.y;
    v.z = pc.z - pb.z;
    /* the plane's normal (0x3e880) */
    n.x = fix_mul(u.y, v.z) - fix_mul(u.z, v.y);
    n.y = fix_mul(v.x, u.z) - fix_mul(v.z, u.x);
    n.z = fix_mul(v.y, u.x) - fix_mul(v.x, u.y);
    /* 1 / n.y in 16.16, in two divisions */
    q = 0x10000 / n.y;
    inv = (int)((unsigned)q << 16) + (int)(((long long)(0x10000 % n.y) << 16) / n.y);
    inv >>= 8;
    a.y = mul((-mul(a.x >> 8, n.x >> 8) - mul(n.z >> 8, a.z >> 8)) >> 8, inv);
    b.y = mul((-mul(b.x >> 8, n.x >> 8) - mul(b.z >> 8, n.z >> 8)) >> 8, inv);
    ctr.y = pa.y + mul((-mul(n.x >> 8, (ctr.x - pa.x) >> 8)
                        - mul((ctr.z - pa.z) >> 8, n.z >> 8)) >> 8, inv);
    *centre = ctr;
    c->along = a;
    c->across = b;
}

/* the car's place in the view (0x67f1c, 1/128 units); 0 if it is behind, too far or off
   to the side, or in the mirror (no shadow there). Then its shadow's four corners on the
   screen */
static int car_place(race_car *c, int xy[8], int mirror)
{
    static const int sa[4] = { -1, -1, 1, 1 }, sb[4] = { 1, -1, -1, 1 };
    vec3 d, ctr, p, q;
    scr2 s;
    int k;

    d.x = (c->pos.x - g_camera->pos.x) >> 9;
    d.y = (c->pos.y - g_camera->pos.y) >> 9;
    d.z = (c->pos.z - g_camera->pos.z) >> 9;
    rotate(&d, g_camera_m, &c->seen);
    if (mirror)
        return 0;
    if (c->seen.z < 0x20 || c->seen.z > 0x1900)
        return 0;
    if (iabs(c->seen.x) > c->seen.z)
        return 0;
    car_road_plane(c, c->yaw_drawn, &c->shadow);
    d.x = (c->shadow.x - g_camera->pos.x) >> 9;
    d.y = (c->shadow.y - g_camera->pos.y) >> 9;
    d.z = (c->shadow.z - g_camera->pos.z) >> 9;
    rotate(&d, g_camera_m, &ctr);
    for (k = 0; k < 4; k++) {
        p.x = sa[k] * (c->along.x >> 10) + sb[k] * (c->across.x >> 10);
        p.y = sa[k] * (c->along.y >> 10) + sb[k] * (c->across.y >> 10);
        p.z = sa[k] * (c->along.z >> 10) + sb[k] * (c->across.z >> 10);
        rotate(&p, g_camera_m, &q);
        q.x += ctr.x;
        q.y += ctr.y;
        q.z += ctr.z;
        project(&q, &s);
        xy[2 * k] = s.x;
        xy[2 * k + 1] = s.y;
    }
    /* (the player's own car then 0x671dc: never from the driver's seat) */
    return 1;
}

/* its models (0x66880): a racer's mark in its colour first (its brake lights' when
   braking), then the near model's groups each sorted and painted, or the far model */
static void car_models(race_car *c, const int m[9])
{
    int k;

    if (c->far) {
        model_place(&c->far_m, m, &c->seen);
        model_sort(&c->far_m, c->far_m.count, c->far_m.list);
        model_draw(&c->far_m, c->far_m.count, c->far_m.list);
        return;
    }
    if (g_car_ticket) {
        /* while the police's ticket or warning is shown, the police car (slot 2) that
           chased flashes its lights instead: the program's four colours at 0xfa, one
           set or the other, swapped every 16 steps (0xf6ff0, 0x14e61c) */
        static int flash = -1, flash_at;

        if (c->slot == 2 && (c->bits[2] & 0xc)) {
            if (flash < 0)
                flash = exe_i32(0xf6ff0);
            if (g_road_steps - flash_at > 0xf) {
                flash = 1 - flash;
                flash_at = g_road_steps;
            }
            {
                const unsigned char *rgb = exe_at(flash ? 0xf6de0 : 0xf6dec, 12);
                unsigned char p8[12];
                int k;

                for (k = 0; k < 12; k++)
                    p8[k] = (unsigned char)(rgb[k] << 2);
                video_palette(p8, 0xfa, 4);
            }
        }
    } else if (g_car_marks[c->slot] && c->slot < g_racers) {
        const unsigned char *rgb = exe_at((c->braking > 10 ? 0xf6f83 : 0xf6f5c) + 3 * c->number, 3);
        unsigned char p8[3] = {(unsigned char)(rgb[0] << 2), (unsigned char)(rgb[1] << 2),
                               (unsigned char)(rgb[2] << 2)};
        video_palette(p8, g_car_marks[c->slot], 1);
    }
    model_place(&c->near_m, m, &c->seen);
    for (k = 0; k < 4; k++)
        if ((unsigned)c->groups[k].index < 8) {
            model_sort(&c->near_m, c->groups[k].count, c->groups[k].list);
            model_draw(&c->near_m, c->groups[k].count, c->groups[k].list);
        }
}

/* a racer's marker (0x66b98): a small triangle pointing down at (x, y), in the racer's
   colour and outlined, bigger the nearer it is (z) in 640x480; a player's (slot below
   g_humans) has a second one on top */
static void car_marker(int x, int y, int slot, int z)
{
    int h = 9, w = 4, k, xy[6];

    if (g_no_markers || slot < 0)
        return;
    if (!g_view_res || z > 0xbb8) {
        h = 3;
        w = 1;
    } else if (z > 0x7d0) {
        h = 5;
        w = 2;
    } else if (z > 0x3e8) {
        h = 8;
        w = 3;
    }
    for (k = 0; k < (g_humans > slot ? 2 : 1); k++, y -= h) {
        xy[0] = x - w;
        xy[1] = y - h;
        xy[2] = x + w - 1;
        xy[3] = y - h;
        xy[4] = x;
        xy[5] = y;
        poly_fill(3, xy, poly_solid_rows, g_marker_colour[slot & 7]);
        poly_line(x - w, y - h, x + w, y - h, g_marker_line);
        poly_line(x + w, y - h, x, y, g_marker_line);
        poly_line(x - w, y - h, x, y, g_marker_line);
    }
}

/* 0x591a4: the car's place seen (1/32 units) and on the screen (put on the near plane
   if nearer) */
static void car_seen_screen(const race_car *c, vec3 *seen, scr2 *s)
{
    vec3 d, p;

    d.x = (c->pos.x - g_camera->pos.x) >> 11;
    d.y = (c->pos.y - g_camera->pos.y) >> 11;
    d.z = (c->pos.z - g_camera->pos.z) >> 11;
    rotate(&d, g_camera_m, seen);
    p = *seen;
    if (p.z < g_view_now->near)
        move_to_near(&p, g_view_now->near);
    project(&p, s);
}

static void car_marker_at(const race_car *c)
{
    vec3 seen;
    scr2 s;

    /* its marker (0x66fe5): raised by a height that shrinks with the distance */
    car_seen_screen(c, &seen, &s);
    if (g_view_res)
        car_marker(s.x, s.y - 0x3800 / seen.z - 15, c->slot, seen.z);
    else
        car_marker(s.x, s.y - 0x2000 / seen.z - 7, c->slot, seen.z);
}

/* a car in the view (0x66d08: 0 ahead, 1 the mirror): near enough and not off to the
   side, its turn from its own matrix or from its angles and sway, into the view */
static void car_draw(race_car *c, int mirror)
{
    int x[9], y[9], z[9], zx[9], m[9], r[9];
    int z_ = c->seen.z;

    if (z_ < 0x20 || z_ > 0x5a00 || 2 * z_ < iabs(c->seen.x))
        return;
    if (mirror) {
        if (z_ > 0x2100)
            return;
        c->far = z_ > 0xa00;
    } else
        c->far = z_ > 0x1900 && c->slot != 2;
    if (c->own_turn)
        memcpy(m, c->turn, sizeof m);
    else {
        model_turn(0, (shake_wave(c->pitch_shake, c->pitch_phase) + c->pitch + c->body_pitch) >> 14, x);
        model_turn(2, (shake_wave(c->roll_shake, c->roll_phase) + c->roll + c->body_roll
                       - c->body_roll / 4) >> 14, z);
        model_turn(1, c->yaw_drawn >> 14, y);
        mat_mul(z, x, zx);
        mat_mul(zx, y, m);
    }
    mat_mul(m, g_camera_m, r);
    if (z_ < 0x219) {
        c->near_m.flags |= 2;
        c->far_m.flags |= 2;
    } else {
        c->near_m.flags &= ~2u;
        c->far_m.flags &= ~2u;
    }
    car_models(c, r);
    /* the player's own car: its smoke once it has slowed in a crash (+0x175 & 2), from its
       model's point "smok" as painted (none from the far model). (With the word 0x15d13a,
       0 in a race played, it would get a marker instead) */
    if (c->slot == g_player) {
        if (c->bits[1] & 2) {
            int dx = 0, dy = 0;

            if (!c->far && c->smoke_point >= 0) {
                vec3 seen;
                scr2 s;

                car_seen_screen(c, &seen, &s);
                dx = c->near_m.screen[c->smoke_point].x - s.x;
                dy = c->near_m.screen[c->smoke_point].y - s.y;
            }
            smoke_draw(&g_crash.smoke, dx, dy);
        }
        return;
    }
    if (g_race_type >= 1 && c->slot <= g_field - 1)
        car_marker_at(c);
}

void cars_at_node(int node)
{
    int d = node < g_camera->node ? g_track.length * 4 + node : node, i, xy[8];

    d -= g_camera->node;
    if (d >= CARS_SEEN || d < 0 || !g_cars_seen[d])
        return;
    for (i = g_car_order_n - 1; i >= 0; i--) {
        race_car *c = g_car_order[i];
        if (c->state == 6 || node != c->draw_node)
            continue;
        if (c == g_cars[g_player] && g_camera->mode == 0)
            continue;          /* the player's own, from the driver's seat (0x6829c) */
        if (c->own_turn)
            c->yaw_drawn = (int)((unsigned)ea_atan2(c->turn[6], c->turn[8]) << 8);
        if (car_place(c, xy, 0))  /* 0x68258: its shadow first, darkening the road */
            poly_fill(4, xy, poly_shade_rows, 0);
        car_draw(c, 0);
    }
}

void cars_behind_at(int node)
{
    int d = node <= g_camera->node ? g_camera->node - node
                                   : g_camera->node + g_track.length * 4 - node, i, xy[8];

    if (d >= CARS_BEHIND || d < 0 || !g_cars_behind[d])
        return;
    for (i = 0; i < g_car_order_n; i++) {
        race_car *c = g_car_order[i];
        if (c->state == 6 || node != c->mirror_node)
            continue;
        if (c == g_cars[g_player] && g_camera->mode == 0)
            continue;
        if (c->own_turn)
            c->yaw_drawn = (int)((unsigned)ea_atan2(c->turn[6], c->turn[8]) << 8);
        car_place(c, xy, 1);
        car_draw(c, 1);
    }
}
