/* The race's camera (NOTES §4.7). */
#include "camera.h"
#include "cardraw.h"
#include "crash.h"
#include "drive.h"
#include "exe.h"
#include "opponent.h"
#include "poly.h"
#include "timing.h"
#include "track.h"
#include <string.h>

camera_mode g_cam_modes[CAMERA_MODES];
int g_cam_pitch_bias;

void camera_init(int eye_height)
{
    memcpy(g_cam_modes, exe_at(0xe80f0, sizeof g_cam_modes), sizeof g_cam_modes);
    g_cam_modes[0].eye.y = g_cam_modes[10].eye.y = eye_height;
}

/* a road node's heading, 0x1000000 a turn */
static int heading(int node)
{
    return (int)((unsigned)(node_i32(node, 0x16) >> 16) << 10);
}

/* the car upright and heading within 30 degrees of its road node (0x14de8): else the
   driver's eyes give way to the camera outside (13) */
static int upright_on_road(const race_car *c)
{
    int d = c->yaw - heading(c->node);

    if (d > 0x800000)
        d -= 0x1000000;
    else if (d < -0x800000)
        d += 0x1000000;
    return d < 0x1e0000 && d > -0x1e0000 && (c->roll < 0 ? -c->roll : c->roll) <= 0x240000
        && (c->pitch < 0 ? -c->pitch : c->pitch) <= 0x240000;
}

/* the mode's settings taken up (0x14b00) */
void camera_set_mode(race_camera *cam, int mode)
{
    const camera_mode *m;
    int old = cam->mode;

    if ((mode < 4 || mode == 10 || mode == 11) && !upright_on_road(cam->car)) {
        mode = 13;
        cam->yaw_follow = heading(cam->car->node);
    }
    /* the race over for this player (or a replay, 0x15d13a): the driver's seat gives
       way to the camera behind */
    if (g_timing.stopped && (mode == 0 || mode == 10)) {
        cam->wanted = mode == 0 ? 1 : 11;
        mode = cam->wanted;
    }
    m = &g_cam_modes[mode];
    cam->mode = mode;
    cam->ahead = view_of_kind(m->kind);
    cam->mirror = &g_view_mirror;
    cam->flags = 1;
    cam->mirror_on = m->mirror;
    cam->frames = m->frames;
    if (!m->frames)
        cam->eye = m->eye;
    /* some modes start from another eye when taken from the driver's seat */
    if (m->use_eye2 && mode != old && !g_crash.view && upright_on_road(g_cars[g_player])
        && (old < 4 || mode == 8))
        cam->eye = m->eye2;
    cam->tilt[0] = cam->tilt[1] = cam->tilt[2] = 0;
    camera_update(cam);
    /* the view's window (0x14210: its layout's for the detail), the screen round it to be
       drawn again */
    if (cam->ahead->current > -1)
        view_layout(cam->ahead, cam->ahead->current);
    g_view_redraw = 1;
}

void camera_set_car(race_camera *cam, const race_car *c, int mode)
{
    cam->pos = c->pos;
    cam->pitch = c->pitch;
    cam->yaw = c->yaw;
    cam->roll = c->roll;
    memset(&cam->off, 0, sizeof cam->off);
    cam->tilt[0] = cam->tilt[1] = cam->tilt[2] = 0;
    cam->node = c->node;
    cam->flags = 1;
    cam->wanted = mode;
    if (c != cam->car)
        cam->yaw_follow = heading(c->node);
    cam->car = c;
    camera_set_mode(cam, mode);
}

void camera_check(race_camera *cam, const race_car *watched)
{
    int mode;

    if (g_crash.view)
        g_crash.view--;
    if (cam->mode == cam->wanted && cam->mode >= 4 && cam->mode != 10)
        return;
    if (!upright_on_road(watched)) {
        g_crash.view = 0x3c;
        if (cam->mode > 3 && cam->mode != 10 && cam->mode != 11)
            return;
        mode = 13;
    } else {
        if (cam->mode == cam->wanted || g_crash.view)
            return;
        mode = cam->wanted == 0 ? 10 : cam->wanted == 1 ? 11 : cam->wanted;
    }
    camera_set_mode(cam, mode);
}

/* 0x50428: the police in a chase, from just ahead of the player's car to 12 nodes behind */
static int police_close(void)
{
    const race_car *p = g_cars[2];
    int d;

    if (!g_drive.chase || !((p->bits[1] & 0x24) || (p->bits[2] & 4)))
        return 0;
    d = g_cars[g_player]->node - p->node;
    return d > -1 && d <= 12;
}

/* 0x50478: the other racer (not a traffic car) from 10 nodes ahead to 12 behind */
static int rival_close(void)
{
    int d;

    if (g_opp.rival_car == 9)
        return 0;
    d = g_cars[g_player]->node - g_cars[1 - g_player]->node;
    return d > -10 && d <= 12;
}

int g_cam_look_first = -1;

void camera_next(race_camera *cam)
{
    int was = cam->wanted, mode;
    const race_car *c;

    if (g_cam_look_first < 0)  /* (the image's, kept from race to race) */
        g_cam_look_first = exe_i32(0xe8f68);
    for (;;) {
        const camera_mode *m = &g_cam_modes[cam->mode];

        if (g_drive.state == 1)
            cam->wanted = g_drive.viewed == g_player && !g_timing.stopped ? m->key_player
                                                                         : m->key_other;
        else if (cam->mode != 7 && g_drive.police && police_close() && g_cam_look_first) {
            g_cam_look_first = 0;
            cam->wanted = 7;
        } else if (cam->mode != 6 && rival_close() && g_cam_look_first && cam->mode != 10) {
            g_cam_look_first = 0;
            cam->wanted = 6;
        } else
            cam->wanted = m->key_held == 7 && !g_drive.police ? 0 : m->key_held;
        mode = cam->wanted;
        /* the rival's camera and the police's: the mode passed over when its car is not
           close */
        if (mode == 6) {
            if (rival_close()) {
                c = g_cars[g_player == 0];
                break;
            }
        } else if (mode == 7 && g_drive.chase) {
            if (police_close()) {
                c = g_cars[2];
                break;
            }
        } else {
            /* not into the driver's seat while a tumble is shown */
            if ((mode == 10 || mode == 11 || mode <= 3) && g_crash.view) {
                cam->wanted = was;
                return;
            }
            c = g_cars[g_drive.viewed];
            break;
        }
        cam->mode = cam->wanted;
    }
    camera_set_car(cam, c, mode);
    if (cam->wanted == 5)
        g_cam_look_first = 1;
    camera_set_mode(cam, cam->wanted);
}

int shake_wave(int amount, int phase)
{
    return exe_i32(0xec28c + 4 * phase) * amount / 16;
}

/* the eye from the car turned (camera_turn_eye 0x13544: in 1/256ths, by X(-pitch)
   Z(-roll) Y(-yaw)) */
static void turn_eye(const vec3 *eye, vec3 *out, int pitch, int roll, int yaw)
{
    int x[9], z[9], y[9], xz[9], m[9];
    vec3 e;

    mat_turn(0, -pitch, x);
    mat_turn(2, -roll, z);
    mat_turn(1, -yaw, y);
    mat_mul(x, z, xz);
    mat_mul(xz, y, m);
    e.x = eye->x >> 8;
    e.y = eye->y >> 8;
    e.z = eye->z >> 8;
    rotate(&e, m, out);
    out->x = (int)((unsigned)out->x << 8);
    out->y = (int)((unsigned)out->y << 8);
    out->z = (int)((unsigned)out->z << 8);
}

/* the eye's move into place: each update 1 / 2^n of the way, or 1 / n (0x13630) */
static void ease(race_camera *cam, const vec3 *to, int divide, int n)
{
    if (!n)
        return;
    if (divide) {
        cam->step.x = (to->x - cam->eye.x) / n;
        cam->step.y = (to->y - cam->eye.y) / n;
        cam->step.z = (to->z - cam->eye.z) / n;
    } else {
        cam->step.x = (to->x - cam->eye.x) >> n;
        cam->step.y = (to->y - cam->eye.y) >> n;
        cam->step.z = (to->z - cam->eye.z) >> n;
    }
}

static int wrap(int a)
{
    while (a > 0x800000)
        a -= 0x1000000;
    while (a < -0x800000)
        a += 0x1000000;
    return a;
}

/* the road's height under p, on the plane through the node, the next and the node's
   across, from the next node's point (0x14c9c) */
static int road_height(int node, const vec3 *p)
{
    const unsigned char *r = node_ptr(node);
    vec3 a, b, u, v, n;

    b.x = node_i32(node, 8);
    b.y = node_i32(node, 12);
    b.z = node_i32(node, 16);
    a.x = node_i32(node + 1, 8);
    a.y = node_i32(node + 1, 12);
    a.z = node_i32(node + 1, 16);
    u.x = a.x - b.x;
    u.y = a.y - b.y;
    u.z = a.z - b.z;
    v.x = 2 * (short)(r[0x1c] | r[0x1d] << 8);
    v.y = 2 * (short)(r[0x1e] | r[0x1f] << 8);
    v.z = 2 * (short)(r[0x20] | r[0x21] << 8);
    /* the plane's normal (0x3e880) */
    n.x = fix_mul(u.y, v.z) - fix_mul(u.z, v.y);
    n.y = fix_mul(v.x, u.z) - fix_mul(v.z, u.x);
    n.z = fix_mul(v.y, u.x) - fix_mul(v.x, u.y);
    return fix_div(-fix_mul(n.x, p->x - a.x) - fix_mul(n.z, p->z - a.z), n.y) + a.y;
}

/* the cameras outside the car (0x138e3 ..): the eye from the car turned towards a point
   ahead along the road, kept inside the road's edges. 1 if a move ended in the next
   mode (taken up, and updated, already) */
static int outside(race_camera *cam, camera_mode *mt)
{
    const race_car *c = cam->car;
    int s = c->dist >> 8, w = 0x600 - s, node = c->node, d, x, z, road, to_yaw, to_pitch;
    int target, yaw, a, b, n, across, edge = 0;
    vec3 v, p, rel;
    const unsigned char *r;

    /* the camera far from the car (another car watched, a car put back): it starts there */
    d = node - cam->node;
    d = d < 0 ? -d : d;
    if (d > 0x10) {
        if (d > g_track.length * 4 / 2)
            d -= g_track.length * 4;
        d = d < 0 ? -d : d;
        if (d > 0x10) {
            cam->node = node;
            cam->pos = c->pos;
        }
    }
    /* the road's way, 12 nodes either side, by how far the car is along its node */
    x = (int)((unsigned)s * (unsigned)((node_i32(node + 13, 8) - node_i32(node - 11, 8)) >> 8)
              + (unsigned)((node_i32(node + 12, 8) - node_i32(node - 12, 8)) >> 8) * (unsigned)w)
        / 0x600;
    z = (int)((unsigned)s * (unsigned)((node_i32(node + 13, 16) - node_i32(node - 11, 16)) >> 8)
              + (unsigned)((node_i32(node + 12, 16) - node_i32(node - 12, 16)) >> 8) * (unsigned)w)
        / 0x600;
    road = angle_of(z, x);
    /* the point looked at, `ahead` along it from the car (0x2c1c0 the way made 1 long) */
    {
        int len2 = fix_mul(x, x) + fix_mul(0, 0) + fix_mul(z, z), len = fix_sqrt(len2, 8, len2);

        v.x = x;
        v.y = 0;
        v.z = z;
        if (len) {
            int inv = fix_div(0x10000, len);

            v.x = fix_mul(v.x, inv);
            v.y = fix_mul(v.y, inv);
            v.z = fix_mul(v.z, inv);
        }
    }
    p.x = c->pos.x + ((int)((unsigned)(v.x >> 8) * (unsigned)mt->ahead) >> 8);
    p.y = c->pos.y + ((int)((unsigned)(v.y >> 8) * (unsigned)mt->ahead) >> 8);
    p.z = c->pos.z + ((int)((unsigned)(v.z >> 8) * (unsigned)mt->ahead) >> 8);
    to_yaw = angle_of(p.z - cam->pos.z, p.x - cam->pos.x);
    {
        int dx = p.x - cam->pos.x, dz = p.z - cam->pos.z;
        int flat = fix_mul(dx, dx) + fix_mul(dz, dz);    /* 0x13368 */

        to_pitch = -angle_of(fix_sqrt(flat, 8, flat), p.y - cam->pos.y);
    }

    /* the eye's move: across with the car on the road (1, 2), or round to the car's
       yaw (3) or the road's (4) */
    target = mt->follow == 4 ? road : c->yaw;
    if (mt->follow) {
        int h = heading(c->node) >> 8;

        r = node_ptr(c->node);
        across = (int)((unsigned)((node_i32(c->node, 16) - c->pos.z) >> 8) * (unsigned)(ea_sin(h) >> 8)
                       - (unsigned)(ea_cos(h) >> 8) * (unsigned)((node_i32(c->node, 8) - c->pos.x) >> 8));
        across -= ((r[1] << 13) - (r[0] << 13)) / 2;
        if ((int)((unsigned)(node_i32(c->node, 0x14) >> 16) << 10) >> 16)
            across = 0;
        switch (mt->follow) {
        case 1:
            mt->eye.x = -(across + (across >> 1));
            break;
        case 2:
            mt->eye.x = -across >> 1;
            break;
        }
        if (mt->follow <= 4) {
            ease(cam, &mt->eye, mt->divide, mt->shift);
            cam->frames = 2;
        }
    }
    if (cam->frames) {
        cam->eye.x += cam->step.x;
        cam->eye.y += cam->step.y;
        cam->eye.z += cam->step.z;
        d = wrap(target - cam->yaw_follow);
        cam->frames--;
        cam->yaw_follow += d >> (mt->shift & 31);
        d = mt->eye.z - cam->eye.z;
        d = d < 0 ? -d : d;
        if ((d < 0x1999 && mt->divide) || cam->mode == 0 || cam->mode == 1
            || cam->eye.z >= mt->eye_end || !cam->frames) {
            cam->eye = mt->eye;
            cam->yaw_follow = target;
            cam->frames = 0;
            if (mt->next >= 0) {
                cam->wanted = mt->next;
                camera_set_mode(cam, mt->next);
                return 1;
            }
        }
    }

    /* the eye turned: by the car (3), towards the point (the crash's, 13), else flat */
    yaw = mt->follow == 3 || mt->follow == 4 ? cam->yaw_follow : road;
    if (mt->follow == 3)
        turn_eye(&cam->eye, &cam->off, c->pitch, c->roll, yaw);
    else if (cam->mode == 13)
        turn_eye(&cam->eye, &cam->off, to_pitch, 0, yaw);
    else
        turn_eye(&cam->eye, &cam->off, 0, 0, yaw);
    cam->pos.x = c->pos.x + cam->off.x;
    /* raised or lowered by the road's slope under the car, less going downhill */
    a = (int)((unsigned)(node_i32(c->node, 0x12) >> 16) << 10);
    if (a > 0x800000)
        a -= 0x1000000;
    b = (int)((unsigned)(node_i32(c->node + 1, 0x12) >> 16) << 10);
    if (b > 0x800000)
        b -= 0x1000000;
    d = (int)((unsigned)(a >> 8) * (unsigned)w + (unsigned)(b >> 8) * (unsigned)s) >> 2;
    if (d > 0)
        d /= 2;
    cam->off.y += (ea_sin(d >> 8) >> 8) * (cam->eye.z >> 9);
    cam->pos.y = c->pos.y + cam->off.y;
    cam->pos.z = c->pos.z + cam->off.z;
    /* the car tumbling: the camera no more than 6 above the road */
    if (c->state == 4) {
        int top = road_height(cam->node, &cam->pos) + 0x60000;

        if (top < cam->pos.y)
            cam->pos.y = top;
    }
    cam->flags |= track_follow(&cam->pos, &cam->node);

    /* kept inside the road's edges (its lanes either side) */
    n = cam->node;
    r = node_ptr(n);
    rel.x = cam->pos.x - node_i32(n, 8);
    rel.z = cam->pos.z - node_i32(n, 16);
    a = -heading(n) >> 8;
    across = (int)((unsigned)(rel.z >> 8) * (unsigned)ea_sin(a)
                   + (unsigned)ea_cos(a) * (unsigned)(rel.x >> 8)) >> 8;
    if (r[3] << 13 < across) {
        edge = 1;
        across = r[3] << 13;
    } else if (-(r[2] << 13) > across) {
        edge = 1;
        across = -(r[2] << 13);
    }
    if (edge) {
        vec3 o, t;

        o.x = across;
        o.y = 0;
        o.z = (int)((unsigned)(rel.z >> 8) * (unsigned)ea_cos(a)
                    - (unsigned)ea_sin(a) * (unsigned)(rel.x >> 8)) >> 8;
        turn_eye(&o, &t, 0, 0, heading(n));
        cam->pos.x = t.x + node_i32(n, 8);
        cam->pos.z = t.z + node_i32(n, 16);
    }

    /* the angles: towards the point, or the yaw followed */
    if (mt->follow == 3 || mt->follow == 4 || cam->mode == 10 || cam->mode == 13) {
        yaw = cam->yaw_follow;
        if (mt->follow == 3)
            to_pitch = c->pitch * 5 / 4;
    } else
        yaw = to_yaw;
    cam->tilt[1] = yaw - c->yaw;
    cam->tilt[0] = to_pitch - c->pitch;
    if (mt->follow != 3)
        cam->tilt[2] = -c->roll;
    return 0;
}

void camera_update(race_camera *cam)
{
    const race_car *c = cam->car;
    camera_mode *mt = &g_cam_modes[cam->mode];

    /* the views from the rival's car (6) and the police car (7), while it is close and
       upright; else the camera behind the player's car, outside (13) */
    if ((cam->mode == 6 && (!rival_close() || !upright_on_road(g_cars[1])))
        || (cam->mode == 7 && (!police_close() || !upright_on_road(g_cars[2])))) {
        camera_set_car(cam, g_cars[g_player], 13);
        return;
    }
    if (cam->mode <= 1) {
        /* the driver's eyes: the car's nodding and leaning (0xea70c = 1, never changed)
           less its shakes; the eye turned with the car */
        cam->tilt[0] = g_cam_pitch_bias + 256 * (c->body_pitch >> 8)
                     - shake_wave(c->pitch_shake, c->pitch_phase);
        cam->tilt[2] = 128 * (c->body_roll >> 8) - shake_wave(c->roll_shake, c->roll_phase);
        turn_eye(&cam->eye, &cam->off, c->pitch, c->roll, c->yaw);
        cam->pos.x = c->pos.x + cam->off.x;
        cam->pos.y = c->pos.y + cam->off.y;
        cam->pos.z = c->pos.z + cam->off.z;
        if (c->node != cam->node) {
            cam->flags |= 1;
            cam->node = c->node;
        }
        cam->yaw_follow = c->yaw;
        cam->eye = mt->eye;
    } else if (cam->mode == 9) {
        /* fixed to the car, unturned */
        cam->eye = mt->eye;
        cam->off = cam->eye;
        cam->pos.x = c->pos.x + cam->off.x;
        cam->pos.y = c->pos.y + cam->off.y;
        cam->pos.z = c->pos.z + cam->off.z;
        cam->flags |= track_follow(&cam->pos, &cam->node);
    } else if (outside(cam, mt))
        return;
    cam->pitch = (c->pitch + cam->tilt[0]) & 0xfffc00;
    cam->roll = (c->roll + cam->tilt[2]) & 0xfffc00;
    cam->yaw = (c->yaw + cam->tilt[1]) & 0xfffc00;
    if (cam->mode > 1)
        cam->roll += exe_i32(0xf6b8c);
}
