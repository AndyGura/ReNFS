/* For tools/drive_emu.py: the port's race_car filled from the original's car record (by
   its offsets) and written back the same way, and the driving's globals set and read. */
#include <stddef.h>
#include <string.h>
#include "car.h"
#include "cardraw.h"
#include "drive.h"

#define I(o, f) { o, offsetof(race_car, f), 4 }
#define V(o, f) I(o, f.x), I(o + 4, f.y), I(o + 8, f.z)
#define B(o, f) { o, offsetof(race_car, f), 1 }
#define H(o, f) { o, offsetof(race_car, f), 2 }

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
