/* The race's camera (NOTES §4.7). */
#include "camera.h"
#include "exe.h"
#include "track.h"
#include <string.h>

camera_mode g_cam_modes[CAMERA_MODES];
int g_cam_pitch_bias;

void camera_init(int eye_height)
{
    memcpy(g_cam_modes, exe_at(0xe80f0, sizeof g_cam_modes), sizeof g_cam_modes);
    g_cam_modes[0].eye.y = g_cam_modes[10].eye.y = eye_height;
}

/* the car upright and heading within 30 degrees of its road node (0x14de8): else the
   driver's eyes give way to the camera outside (13) */
static int upright_on_road(const race_car *c)
{
    int d = c->yaw - (int)((unsigned)(node_i32(c->node, 0x16) >> 16) << 10);

    if (d > 0x800000)
        d -= 0x1000000;
    else if (d < -0x800000)
        d += 0x1000000;
    return d < 0x1e0000 && d > -0x1e0000 && (c->roll < 0 ? -c->roll : c->roll) <= 0x240000
        && (c->pitch < 0 ? -c->pitch : c->pitch) <= 0x240000;
}

/* the mode's settings taken up (0x14b00) */
static void set_mode(race_camera *cam, int mode)
{
    const camera_mode *m;

    if ((mode < 4 || mode == 10 || mode == 11) && !upright_on_road(cam->car)) {
        mode = 13;
        cam->yaw_follow = (int)((unsigned)(node_i32(cam->car->node, 0x16) >> 16) << 10);
    }
    /* (with 0x15d13a or 0x145c90 set, 0 becomes 1 and 10 becomes 11) */
    m = &g_cam_modes[mode];
    cam->mode = mode;
    cam->ahead = &g_view_ahead;     /* the view of the mode's kind: 0 for the driver's */
    cam->mirror = &g_view_mirror;
    cam->flags = 1;
    cam->mirror_on = m->fill[10];   /* +0x38 */
    if (!m->fill[11])               /* +0x3c */
        cam->eye = m->eye;
    /* (+0x54: a mode taken from another one starts from +0x48's eye; none of the
       driver's) */
    cam->tilt[0] = cam->tilt[1] = cam->tilt[2] = 0;
    camera_update(cam);
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
    if (c != cam->car)
        cam->yaw_follow = (int)((unsigned)(node_i32(c->node, 0x16) >> 16) << 10);
    cam->car = c;
    set_mode(cam, mode);
}

int shake_wave(int amount, int phase)
{
    return exe_i32(0xec28c + 4 * phase) * amount / 16;
}

void camera_update(race_camera *cam)
{
    const race_car *c = cam->car;

    if (cam->mode <= 1) {
        /* the driver's eyes: the car's nodding and leaning (0xea70c = 1, never changed)
           less its shakes; the eye turned with the car (0x13544: in 1/256ths, by
           X(-pitch) Z(-roll) Y(-yaw)) */
        int x[9], z[9], y[9], xz[9], m[9];
        vec3 e;

        cam->tilt[0] = g_cam_pitch_bias + 256 * (c->body_pitch >> 8)
                     - shake_wave(c->pitch_shake, c->pitch_phase);
        cam->tilt[2] = 128 * (c->body_roll >> 8) - shake_wave(c->roll_shake, c->roll_phase);
        mat_turn(0, -c->pitch, x);
        mat_turn(2, -c->roll, z);
        mat_turn(1, -c->yaw, y);
        mat_mul(x, z, xz);
        mat_mul(xz, y, m);
        e.x = cam->eye.x >> 8;
        e.y = cam->eye.y >> 8;
        e.z = cam->eye.z >> 8;
        rotate(&e, m, &cam->off);
        cam->off.x = (int)((unsigned)cam->off.x << 8);
        cam->off.y = (int)((unsigned)cam->off.y << 8);
        cam->off.z = (int)((unsigned)cam->off.z << 8);
        cam->pos.x = c->pos.x + cam->off.x;
        cam->pos.y = c->pos.y + cam->off.y;
        cam->pos.z = c->pos.z + cam->off.z;
        if (c->node != cam->node) {
            cam->flags |= 1;
            cam->node = c->node;
        }
        cam->yaw_follow = c->yaw;
        cam->eye = g_cam_modes[cam->mode].eye;
    }
    cam->pitch = (c->pitch + cam->tilt[0]) & 0xfffc00;
    cam->roll = (c->roll + cam->tilt[2]) & 0xfffc00;
    cam->yaw = (c->yaw + cam->tilt[1]) & 0xfffc00;
    /* (the modes outside the car add 0xf6b8c to the roll) */
}
