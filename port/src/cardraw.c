/* The cars drawn into the road (NOTES §4.10). */
#include "cardraw.h"
#include "camera.h"
#include "exe.h"
#include "track.h"
#include "video.h"
#include <string.h>

race_car *g_cars[RACE_CARS_MAX];
int g_ncars;
race_car *g_car_order[RACE_CARS_MAX];
int g_car_order_n;
int g_player;
int g_racers;
unsigned char g_car_marks[RACE_CARS_MAX];
unsigned char g_cars_seen[CARS_SEEN];
const race_camera *g_camera;
const int *g_camera_m;

void cars_list(void)
{
    int i;

    g_car_order_n = 0;
    for (i = 0; i < g_ncars; i++) {
        race_car *c = g_cars[i];
        c->draw_node = c->node;
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
    /* (0x677f8 then marks the eleven nodes round each car for 0x679ec, the player's own
       car's rows: not drawn from the driver's seat) */
}

static int iabs(int v)
{
    return v > 0 ? v : -v;
}

/* the car's place in the view (0x67f1c, 1/128 units); 0 if it is behind, too far or off
   to the side. (Its shadow's four corners are worked out here too: not ported yet) */
static int car_place(race_car *c)
{
    vec3 d;

    d.x = (c->pos.x - g_camera->pos.x) >> 9;
    d.y = (c->pos.y - g_camera->pos.y) >> 9;
    d.z = (c->pos.z - g_camera->pos.z) >> 9;
    rotate(&d, g_camera_m, &c->seen);
    if (c->seen.z < 0x20 || c->seen.z > 0x1900)
        return 0;
    return iabs(c->seen.x) <= c->seen.z;
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
    /* (with 0xea81c or 0xea820 set, never in these races, the police car's lights
       flash in 0xfa.. instead) */
    if (g_car_marks[c->slot] && c->slot < g_racers) {
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

/* a car in the view ahead (0x66d08 with 0): near enough and not off to the side, its
   turn from its own matrix or from its angles and sway, into the view */
static void car_draw(race_car *c)
{
    int x[9], y[9], z[9], zx[9], m[9], r[9];
    int z_ = c->seen.z;

    if (z_ < 0x20 || z_ > 0x5a00 || 2 * z_ < iabs(c->seen.x))
        return;
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
    /* (then, in a race with marks, a racer's marker above it: 0x66b98, not ported yet) */
}

void cars_at_node(int node)
{
    int d = node < g_camera->node ? g_track.length * 4 + node : node, i;

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
        car_place(c);          /* (0x68258: its shadow when it is seen) */
        car_draw(c);
    }
}
