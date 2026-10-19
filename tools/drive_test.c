/* For tools/drive_emu.py: the port's race_car filled from the original's car record (by
   its offsets) and written back the same way, and the driving's and the crash's globals
   set and read. */
#include <stddef.h>
#include <string.h>
#include "car.h"
#include "cardraw.h"
#include "crash.h"
#include "drive.h"

#define I(o, f) { o, offsetof(race_car, f), 4 }
#define V(o, f) I(o, f.x), I(o + 4, f.y), I(o + 8, f.z)
#define B(o, f) { o, offsetof(race_car, f), 1 }
#define H(o, f) { o, offsetof(race_car, f), 2 }
#define VA(o, f, i) I(o, f[i]), I(o + 4, f[i + 1]), I(o + 8, f[i + 2])
#define M9(o, f) VA(o, f, 0), VA(o + 12, f, 3), VA(o + 24, f, 6)

static const struct { int at, off, size; } fields[] = {
    V(0, pos), I(0xc, pitch), I(0x10, yaw), I(0x14, roll), I(0x18, steer), I(0x48, node),
    I(0x4c, progress), I(0x50, lap), V(0x54, vel), I(0x60, side_speed), I(0x64, up_speed),
    I(0x68, speed), I(0x6c, wheel_speed), I(0x70, speed_abs), I(0x74, yaw_rate),
    I(0x78, half_len), I(0x7c, half_wid), I(0x365, body_roll), I(0x369, body_pitch),
    V(0x36d, along), V(0x379, across), V(0x385, ground), I(0x39d, dist),
    I(0x3b1, throttle), I(0x3b5, throttle_kept), I(0x3b9, braking), I(0x3bd, shift_wait),
    I(0x3c1, rpm), I(0x3c5, rpm_road), I(0x3c9, grip_extra), I(0x3cd, grip_rear),
    I(0x3d1, grip_front), I(0x3d5, shift_last), I(0x3d9, push_side), I(0x3dd, push_up),
    I(0x3e1, push_fwd), I(0x3e5, engine_force), I(0x3e9, gearbox), I(0x3ed, gear),
    I(0x3f1, clutch), I(0x3f5, handbrake), I(0x3f9, shifting), I(0x3fd, on_ground),
    I(0x401, air_steps), I(0x405, slide_front), I(0x409, slide_rear), I(0x40d, wheelspin),
    I(0x411, force_side), I(0x415, force_fwd), I(0x419, gear_now), I(0x41d, gear_before),
    I(0x421, gear_shown), B(0x425, tyres[0]), B(0x426, tyres[1]), B(0x427, roll_phase),
    B(0x428, pitch_phase), I(0x429, roll_shake), I(0x42d, pitch_shake), I(0x43d, mass_front),
    I(0x44d, weight_shift), I(0x451, grip_rear_base), I(0x455, grip_front_base),
    I(0x459, yaw_damp), I(0x461, turn_front), I(0x465, turn_rear), I(0x47d, player),
    I(0x48a, number), I(0x48e, tcs), I(0x492, abs), H(0x49a, downforce[0]),
    H(0x49c, downforce[1]), H(0x49e, downforce[2]), H(0x4a0, downforce[3]),
    I(0x4a2, surface), I(0x4a6, surface_flag), I(0x4aa, node_flags[0]),
    I(0x4ae, node_flags[1]), I(0x4c6, node_flags[2]), I(0x4ca, node_flags[3]),
    I(0x4ce, node_flags[4]), I(0x4b2, tcs_now), I(0x4b6, abs_now), I(0x4ba, horn),
    I(0x4f6, turn_target), I(0x4fa, turn_count), I(0x4fe, auto_mode), I(0x502, auto_steer),
    I(0x506, dt), I(0x50a, rate),
    /* the crash's */
    I(0x20, own_turn), M9(0x24, turn), I(0x80, road_off),
    I(0x84, reach), V(0x8c, road_side), V(0x98, road_up), V(0xa4, road_fwd), V(0xb0, road_at),
    I(0xbc, height), M9(0xc4, body.m),
    V(0xe8, body.pos), V(0xf4, body.vel), V(0x100, body.spin), I(0x124, body.mass),
    I(0x128, body.inertia), I(0x12c, body.inv_mass), I(0x130, body.inv_inertia),
    I(0x134, body.radius), VA(0x138, body.half, 0), I(0x144, body.wait), I(0x15c, road_speed),
    B(0x174, bits[0]), B(0x175, bits[1]), B(0x176, bits[2]), I(0x4f2, crashing),
    I(0x512, state), I(0x516, slot),
    /* the timing's */
    B(0x51a, part),
};

/* the i-th field's offset in the original's record and its size; 0 past the last */
int drive_test_field(int i, int *at, int *size)
{
    if (i < 0 || i >= (int)(sizeof fields / sizeof fields[0]))
        return 0;
    *at = fields[i].at;
    *size = fields[i].size;
    return 1;
}

void drive_test_in(race_car *c, const unsigned char *raw, unsigned char *spec)
{
    size_t i;

    for (i = 0; i < sizeof fields / sizeof fields[0]; i++)
        memcpy((char *)c + fields[i].off, raw + fields[i].at, fields[i].size);
    c->spec = spec;
}

void drive_test_out(const race_car *c, unsigned char *raw)
{
    size_t i;

    for (i = 0; i < sizeof fields / sizeof fields[0]; i++)
        memcpy(raw + fields[i].at, (const char *)c + fields[i].off, fields[i].size);
}

/* the globals, in the order of drive_emu.py's GLOBALS */
#define G(x) &x
static int *globals(int k)
{
    static int *const g[] = {
        G(g_drive.steps), G(g_drive.flags), G(g_drive.state), G(g_drive.kind),
        G(g_drive.segment), G(g_drive.rally), G(g_drive.catch_up), G(g_drive.viewed),
        G(g_drive.camera_mode), G(g_drive.laps), G(g_drive.last_node), G(g_drive.nitro),
        G(g_drive.explode), G(g_drive.clock), G(g_drive.nitro_on), G(g_drive.nitro_until),
        G(g_drive.nitro_off), G(g_drive.first_move), G(g_drive.options), G(g_drive.air_shake),
        G(g_player), G(g_humans), G(g_drive.spinning), G(g_drive.nitro_keys),
    };
    if (k < (int)(sizeof g / sizeof g[0]))
        return g[k];
    k -= sizeof g / sizeof g[0];
    if (k < 8)
        return (int *)&g_drive.controls[k];
    if (k < 16)
        return &g_drive.steer_kind[k - 8];
    if (k < 24)
        return &g_drive.pedal_kind[k - 16];
    if (k < 32)
        return &g_drive.revving[k - 24];
    if (k < 40)
        return &g_drive.top_speed[k - 32];
    return 0;
}

int drive_test_global(int k, int set, int v)
{
    int *p = globals(k);

    if (!p)
        return 0;
    if (set)
        *p = v;
    return *p;
}

/* the crash's globals, in the order of drive_emu.py's CRASH_GLOBALS */
int crash_test_global(int k, int set, int v)
{
    int *const g[] = {
        &g_crash.hit_wait, &g_crash.hit_level, &g_crash.impulse, &g_crash.depth,
        &g_crash.point_vel.x, &g_crash.point_vel.y, &g_crash.point_vel.z, &g_crash.refresh,
        &g_crash.view, &g_crash.respawned, &g_crash.restart, (int *)&g_crash.random,
        &g_drive.chase, &g_drive.police, &g_field, &g_ncars,
        &g_crash.crashes[0], &g_crash.crashes[1], &g_crash.crashes[2], &g_crash.crashes[3],
        &g_crash.crashes[4], &g_crash.crashes[5], &g_crash.crashes[6], &g_crash.crashes[7],
    };

    if (k < 0 || k >= (int)(sizeof g / sizeof g[0]))
        return 0;
    if (set)
        *g[k] = v;
    return *g[k];
}

/* the smoke's block, the original's layout (0x96c bytes) in or out */
int crash_test_smoke(unsigned char *raw, int set)
{
    if (set)
        memcpy(&g_crash.smoke, raw, sizeof g_crash.smoke);
    else
        memcpy(raw, &g_crash.smoke, sizeof g_crash.smoke);
    return (int)sizeof g_crash.smoke;
}

void drive_test_track(const char *name)
{
    strncpy(g_drive.track, name, sizeof g_drive.track - 1);
}

/* the race's car k (g_cars, 0x13c24c) */
void drive_test_car(int k, race_car *c)
{
    if (k >= 0 && k < RACE_CARS_MAX)
        g_cars[k] = c;
}

/* the port's loader on a car numbered `number`: its physics into spec (0x800 bytes,
   copied out) and the fields set into raw (the original's offsets) */
int drive_test_load(int number, int slot, int automatic, int tcs, int abs, int rally, int kind,
                    unsigned char *spec_out, unsigned char *raw)
{
    static race_car c;

    memset(&c, 0, sizeof c);
    c.number = number;
    c.slot = slot;
    g_drive.rally = rally;
    g_drive.kind = kind;
    if (!drive_load(&c, automatic, tcs, abs))
        return 0;
    memcpy(spec_out, c.spec, 0x800);
    drive_test_out(&c, raw);
    return 1;
}

/* crash.c itself, so that its parts can be tried one by one (crash_emu checks) */
#include "crash.c"

int crash_test_recip(int x) { return recip(x); }
int crash_test_sqrt(int n, int iters, int guess) { return fix_sqrt(n, iters, guess); }
int crash_test_len(const int *v, int which)
{
    return which == 0 ? len3((const vec3 *)v) : which == 1 ? len2((const vec3 *)v)
         : len3_sq((const vec3 *)v);
}
