/* The race's sounds (NOTES §4.18). */
#include "racesnd.h"
#include "cardraw.h"
#include "collide.h"
#include "crash.h"
#include "drive.h"
#include "exe.h"
#include "files.h"
#include "hud.h"
#include "sndlib.h"
#include "timing.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

race_sound g_rsnd;

static int iabs(int v) { return v < 0 ? -v : v; }

/* the program's tables: the volume curve (0xedf50, 129 of 0..127), the cars' pitches by
   slot (0xf7798), the horns' sounds and pitches (0x7fa80, 0x7faa8), the radar's beeps'
   spacing (0x52560) */
static int curve(int i) { return exe_i32(0xedf50 + 4 * i); }

void rsnd_init(void)
{
    memset(&g_rsnd, 0, sizeof g_rsnd);
    for (int k = 0; k < SND_CHANNELS; k++) {
        g_rsnd.voice[k] = g_rsnd.sound[k] = -1;
        g_rsnd.bend[k] = exe_i32(0xeded8 + 4 * k);
    }
    g_rsnd.engine = exe_i32(0xeded4);
    g_rsnd.engine_chan = exe_i32(0xee352) >> 16;
    g_rsnd.road_pitch = exe_i32(0xee360);
    g_rsnd.road_up = exe_i32(0xee36c);
    g_rsnd.skid[0] = exe_i32(0xf7768);
    g_rsnd.skid[1] = exe_i32(0xf776c);
    g_rsnd.horn_car = exe_i32(0xf777c);
    g_rsnd.echo_wait = exe_i32(0xf7784);
    g_rsnd.siren_bend = exe_i32(0xf778c);
}

static int heard(void)
{
    if (g_drive.viewed != g_player || g_timing.stopped)
        return g_drive.viewed;
    return g_player;
}

race_car *rsnd_heard(void)
{
    return g_cars[heard()];
}

/* 0x52ac8: an angle (0x1000000 a turn, 0 ahead) as a pan: right ahead and behind the
   middle, a quarter turn either way an edge */
static int pan_of(int angle)
{
    int a = angle >> 16;
    if (a <= 0x40 && a >= 0)
        a = 0x40 - a;
    else if (a >= 0xc0 && a <= 0x100)
        a = 0x13f - a;
    else
        a -= 0x40;
    return a < 0 ? 0 : a > 0x7f ? 0x7f : a;
}

/* 0x7fb2c: 127 at no distance down to 0 at 0x200000 (32 m) */
static int near_of(int dist)
{
    if (dist >= 0x200000)
        return 0;
    return ((0x200000 - dist) >> 8) * 0x7f / 0x2000;
}

/* 0x531ac */
static void channel_stop(int ch)
{
    if (g_rsnd.voice[ch] >= 0)
        snd_stop(g_rsnd.voice[ch]);
    g_rsnd.voice[ch] = g_rsnd.sound[ch] = -1;
}

static int scaled(int vol)
{
    return g_rsnd.muted ? 0 : g_rsnd.sfx * vol / 0x7f;
}

/* 0x52ecc: channel ch to sound s at a pitch bend, a volume (of the settings') and a pan:
   the sound it plays already moved there; the one-off hits (0x20 .. 0x25) and the gravel
   (0x50) always started anew; another sound started unless the channel's last was a
   one-off still heard */
static void channel_play(int ch, int s, int bend, int vol, int pan)
{
    int v;
    if (!g_rsnd.sfx)
        return;
    if (g_rsnd.muted)
        vol = 0;
    if (bend > 0x7f)
        bend = 0x7f;
    if (vol > 0x7f)
        vol = 0x7f;
    if (pan > 0x7f)
        pan = 0x7f;
    if (!(s >= 0x20 && s <= 0x25) && s != 0x50) {
        if (s == g_rsnd.sound[ch]) {
            snd_volume(g_rsnd.voice[ch], scaled(vol));
            snd_bend(g_rsnd.voice[ch], bend);
            snd_pan(g_rsnd.voice[ch], pan);
            return;
        }
        if (g_rsnd.sound[ch] <= 0x25 && snd_idle(g_rsnd.voice[ch]) == 0)
            return;
    }
    v = snd_play(s, pan, scaled(vol), bend);
    if (v >= 0) {
        g_rsnd.voice[ch] = v;
        g_rsnd.sound[ch] = s;
    }
}

/* 0x53084: the same with the volume through the curve */
static void channel_level(int ch, int s, int bend, int i, int angle)
{
    int pan = pan_of(angle);
    if (i > 0x7f)
        i = 0x7f;
    channel_play(ch, s, bend, curve(i), pan);
}

/* 0x52b10: a channel's sound by the kind the car gives (its tyres': 4 and 1 on the road,
   4 and 8 off it, 2 and 1 a body on the road) */
static int chan_sound(int ch, int a, int b)
{
    int s = 0;
    if (ch == g_rsnd.engine_chan && a == 4 && b == 1) s = 0x2d;
    if (ch == g_rsnd.engine_chan && a == 2 && b == 1) s = 0x27;
    if (ch == g_rsnd.engine_chan && a == 4 && b == 8) s = 0x2e;
    if (ch == 2 && a == 4 && b == 1) s = 0x38;
    if (ch == 2 && a == 4 && b == 8) s = 0x38;
    if (ch == 2 && a == 2 && b == 1) s = 0x34;
    if (ch == 3 && a == 4 && b == 1) s = 0x3a;
    if (ch == 3 && a == 4 && b == 8) s = 0x3a;
    if (ch == 3 && a == 2 && b == 1) s = 0x36;
    if (ch == 4 && a == 4 && b == 1) s = 0x2a;
    if (ch == 4 && a == 4 && b == 8) s = 0x26;
    if (ch == 0xc && a == 4 && b == 1) s = 0x2b;
    if (ch == 0xc && a == 4 && b == 8) s = 0x28;
    return s;
}

/* 0x52c38: a one-off's sound by its kind and how hard (16.16) */
static int hit_sound(int how, int a, int b)
{
    int v = how >> 16;
    if (a == 0xe || a == 0xc)
        return 0;
    if (a == 0xd)
        return 0x20;           /* the gear's clunk */
    if (a == 4 && b == 1)
        return 0x25;           /* a landing */
    if (a == 2 && b == 9)
        return 0x23;           /* along a wall */
    if (a == 2 && b == 10)
        return 0x21;           /* an object knocked over */
    if (a == 2 && b == 1) {    /* a body thrown about */
        if (v > 0x50)
            return 0x24;
        if (v > 0x16)
            return 0x22;
        if (v > 0x10)
            return 0x23;
        return 0x21;
    }
    if (v > 0x6e)              /* two cars */
        return 0x24;
    if (v > 0x3c)
        return 0x22;
    return 0x21;
}

/* 0x52cf0: a channel's pitch: the tyres' by how hard, the others' as they started */
static int chan_bend(int ch, int s, int how)
{
    int v = how >> 16;
    if (ch == 4)
        return v;
    if (s != g_rsnd.sound[ch])
        g_rsnd.bend[ch] = v;
    return g_rsnd.bend[ch];
}

/* 0x52d2c: a car's sound by its kind and how hard, from dist at angle; channel -1 a
   one-off on channel 4, another stopped when there is nothing to hear */
static void channel_event(int ch, int a, int b, int how, int dist, int angle)
{
    int pan, near, s;
    if (g_drive.state == 3 || !g_rsnd.sfx)
        return;
    if (g_rsnd.mono)
        angle = 0;
    pan = pan_of(angle);
    near = near_of(dist);
    if (ch < 0) {
        channel_play(4, hit_sound(how, a, b), 0x40, near, pan);
        return;
    }
    if (how <= 0) {
        if (g_rsnd.voice[ch] >= 0)
            channel_stop(ch);
        return;
    }
    s = chan_sound(ch, a, b);
    channel_play(ch, s, chan_bend(ch, s, how), ch == 4 ? (int)((unsigned)how * near) >> 7 : near,
                 pan);
}

void rsnd_hit(int kind, int sub, int how, int dist, int angle)
{
    channel_event(-1, kind, sub, how, dist, angle);
}

/* ---- the banks ------------------------------------------------------------------------ */

/* 0x52718: a bank loaded and put in */
static void bank_load(const char *name)
{
    char path[64];
    long len;
    unsigned char *d;
    int b;
    snprintf(path, sizeof path, "SIMDATA/SOUNDBNK/%s", name);
    d = file_load(path, &len);
    if (!d) {
        fprintf(stderr, "renfs: %s is missing\n", path);
        return;
    }
    b = snd_bank_add(d, len);
    if (b < 0) {
        fprintf(stderr, "renfs: %s: its sounds clash with a bank's before\n", path);
        free(d);
        return;
    }
    g_rsnd.bank_data[g_rsnd.nbanks] = d;
    g_rsnd.banks[g_rsnd.nbanks++] = b;
}

/* 0x52670: the engine's two loops and the road's noise started, silent */
static void engine_start(void)
{
    int ch = g_rsnd.engine_chan;
    if (g_rsnd.voice[ch] >= 0)
        return;
    g_rsnd.voice[ch] = snd_play(1, 0x40, 0, 0x40);
    g_rsnd.sound[ch] = 1;
    g_rsnd.voice[1] = snd_play(2, 0x40, 0, 0x40);
    g_rsnd.sound[1] = 2;
    g_rsnd.voice[5] = snd_play(0x29, 0x40, 0, 0x40);
    g_rsnd.sound[5] = 0x29;
}

void rsnd_load(int car, int other, int track)
{
    static const unsigned player_names = 0x524c0, other_names = 0x524f0;
    const char *hits;
    if (!g_rsnd.present)
        return;
    /* the player's car's bank (12 by number), the other racer's (by number, 2 a car) */
    bank_load((const char *)exe_at((unsigned)exe_i32(player_names + 4 * car), 16));
    bank_load((const char *)exe_at((unsigned)exe_i32(other_names + 8 * other), 16));
    /* the hits: mono or not, Transtropolis's own */
    if (g_rsnd.mono)
        hits = track == 7 ? "COLLSBMT.BNK" : "COLLSBWT.BNK";
    else
        hits = track == 7 ? "COLLSWMT.BNK" : "COLLSWWT.BNK";
    bank_load(hits);
    bank_load("NFS_FMmb.bnk");
    engine_start();
}

void rsnd_engine_on(void)
{
    int ch = g_rsnd.engine_chan;
    if (g_rsnd.sound[ch] != 1) {
        g_rsnd.voice[ch] = snd_play(1, 0x40, 0, 0x40);
        g_rsnd.sound[ch] = 1;
    }
    g_rsnd.voice[5] = snd_play(0x29, 0x40, 0, 0x40);
    g_rsnd.engine = 1;
    g_rsnd.sound[5] = 0x29;
}

void rsnd_engine_off(void)
{
    channel_stop(g_rsnd.engine_chan);
    channel_stop(5);
    channel_stop(4);
    channel_stop(3);
    g_rsnd.engine = 0;
}

void rsnd_free(void)
{
    for (int ch = 0; ch < SND_CHANNELS; ch++)
        channel_stop(ch);
    for (int k = g_rsnd.nbanks - 1; k >= 0; k--) {
        snd_bank_remove(g_rsnd.banks[k]);
        free(g_rsnd.bank_data[k]);
        g_rsnd.bank_data[k] = 0;
    }
    g_rsnd.nbanks = 0;
}

/* ---- where a car is heard from ------------------------------------------------------ */

void rsnd_where(const race_car *c, int *angle, int *dist)
{
    race_camera *cam = (race_camera *)g_camera;
    int dx = c->pos.x - cam->pos.x, dy = iabs(c->pos.y - cam->pos.y), dz = c->pos.z - cam->pos.z;
    int a, s;
    if (cam->yaw < 0)
        cam->yaw += 0x1000000;
    a = cam->yaw - angle_of(dz, dx);
    if (a < 0)
        a += 0x1000000;
    if (a > 0x1000000)
        a -= 0x1000000;
    *angle = a;
    dx = iabs(dx);
    dz = iabs(dz);
    s = dz < dx ? (dz >> 2) + dx : dz + (dx >> 2);
    *dist = s < dy ? (s >> 2) + dy : s + (dy >> 2);
}

/* 0x7fc2c: the angle alone, from coordinates of 1/256 */
static int angle_to(const race_car *c)
{
    race_camera *cam = (race_camera *)g_camera;
    int dx = (c->pos.x - cam->pos.x) >> 8, dz = (c->pos.z - cam->pos.z) >> 8, a;
    if (cam->yaw < 0)
        cam->yaw += 0x1000000;
    a = cam->yaw - angle_of(dz, dx);
    if (a < 0)
        a += 0x1000000;
    if (a > 0x1000000)
        a -= 0x1000000;
    return a;
}

/* 0x7ff88: the Doppler shift of a car coming or going, 256 as it is (0 when the camera
   changed mode or the car jumped) */
static int doppler(race_car *c)
{
    const race_camera *cam = g_camera;
    int dz = iabs(c->pos.z - cam->pos.z), dx = iabs(c->pos.x - cam->pos.x), d, diff, r = 0;
    if (dx > dz)
        dz >>= 2;
    else
        dx >>= 2;
    d = dz + dx;
    diff = d - c->snd_dist;
    c->snd_dist = d;
    if (c->snd_mode == cam->mode && iabs(diff) < 0x96000)
        r = 0x1bcccc00 / (diff + 0x1bcccc);
    c->snd_mode = cam->mode;
    return r;
}

/* 0x8001c: how fast a car goes against the camera's, or itself (the larger), of 64 */
static int against(const race_car *c)
{
    const race_car *w = g_camera->car;
    int a = iabs(w->vel.z - c->vel.z), b = iabs(w->vel.x - c->vel.x), s, t, x, z;
    if (b > a)
        a >>= 2;
    else
        b >>= 2;
    s = a + b;
    z = iabs(c->vel.z);
    x = iabs(c->vel.x);
    if (x > z)
        z >>= 2;
    else
        x >>= 2;
    t = (x + z) * 2;
    if (t > s)
        s = t;
    s >>= 16;
    return s > 0x40 ? 0x40 : s;
}

/* the distance from the camera as 0x7fb58 works it out */
static int dist_to(const race_car *c)
{
    const race_camera *cam = g_camera;
    int dx = iabs(c->pos.x - cam->pos.x), dy = iabs(c->pos.y - cam->pos.y);
    int dz = iabs(c->pos.z - cam->pos.z), s;
    s = dz < dx ? (dz >> 2) + dx : dz + (dx >> 2);
    return s < dy ? (s >> 2) + dy : s + (dy >> 2);
}

/* ---- the player's car ---------------------------------------------------------------- */

/* 0x530b0: the radar's beep, the nearer the police the faster */
static void radar_beep(void)
{
    int gap = exe_i32(0x52560 + 4 * g_hud.radar), w = g_rsnd.beep_wait;
    if (!g_rsnd.beep_on) {
        g_rsnd.beep_on = 1;
        w = gap;
    } else if (w > gap)
        w = gap;
    if (!w) {
        g_rsnd.beep_wait = 0;
        snd_play(0x61, 0x40, g_rsnd.muted ? 0 : g_rsnd.sfx, 0x40);
        g_rsnd.beep_on = 0;
        w = g_rsnd.beep_wait;
    } else
        w--;
    g_rsnd.beep_wait = w;
}

/* 0x53338: the engine (its two loops by the throttle as heard, pitched by the revs), the
   road's noise (by the speed, pitched by the surface), under a bridge or in a tunnel the
   echo, on the rally's gravel the stones */
static void engine_sound(race_car *c, int angle, int dist)
{
    int sfx = g_rsnd.sfx, ch = g_rsnd.engine_chan, sp, lvl, on, off, level, pan, t;
    if (!sfx)
        return;
    if (g_hud.radar)
        radar_beep();
    if (c->node_flags[3]) {
        g_rsnd.under_pan = 0;
        if (g_rsnd.under_vol < 0x7a)
            g_rsnd.under_vol += 5;
        if (g_drive.kind == 7)
            channel_play(0xb, 0x3d, 0, g_rsnd.under_vol, g_rsnd.under_pan);
        else
            channel_play(0xb, 0x3e, 0x40, g_rsnd.under_vol, g_rsnd.under_pan);
    } else if (c->node_flags[4]) {
        g_rsnd.under_pan = 0x7f;
        if (g_rsnd.under_vol < 0x7a)
            g_rsnd.under_vol += 5;
        channel_play(0xb, 0x3e, 0x40, g_rsnd.under_vol, g_rsnd.under_pan);
    } else if (g_rsnd.under_vol >= 5) {
        g_rsnd.under_vol -= 5;
        channel_play(0xb, 0x3e, 0x40, g_rsnd.under_vol, g_rsnd.under_pan);
    }

    sp = iabs(c->speed_abs >> 16) * 2;
    if (c->node_flags[0]) {
        sp += 0x14;
        g_rsnd.road_pitch = 0x5e;
        if (g_drive.kind == 5) {
            int vol = (int)((collide_rand() & 0xffff) * 0x7f) >> 16;
            int p = (int)((collide_rand() & 0xffff) * 0x7f) >> 16;
            if (!((int)((collide_rand() & 0xffff) * 30) >> 16))
                channel_play(0xf, 0x50, 0x40, vol, p);
        }
    } else if (c->node_flags[2]) {
        if (g_drive.kind != 5) {
            t = c->speed_abs >> 17;
            g_rsnd.road_pitch += g_rsnd.road_up ? t : -t;
            if (g_rsnd.road_pitch <= 0x40)
                g_rsnd.road_up = 1;
            else if (g_rsnd.road_pitch > 0x5e)
                g_rsnd.road_up = 0;
        } else {
            static const int gravel[7] = { 0x19, 0x1e, 0x1e, 0x28, 0x32, 0x41, 0x4b };
            unsigned k = (unsigned)(sp / 10 - 2);
            channel_play(4, 0x21, 0x40, k <= 6 ? gravel[k] : sp, 0x40);
        }
    } else if (c->surface) {
        sp += sp / 4;
        g_rsnd.road_pitch = 0x18;
    } else
        g_rsnd.road_pitch = 0x40;
    lvl = sp > 0x7f ? 0x7f : sp;
    if (dist > 2 && !c->node_flags[2] && !c->node_flags[0])
        lvl = lvl * 6 / 10;
    snd_volume(g_rsnd.voice[5], scaled(lvl));
    snd_bend(g_rsnd.voice[5], g_rsnd.road_pitch);

    /* the revs of the car watched against the player's car's red line */
    g_rsnd.revs = g_cars[g_drive.viewed]->rpm * 0x7f
                  / (*(const int *)(g_cars[g_player]->spec + 0xac) + 2000);
    if (g_rsnd.revs > 0x7f)
        g_rsnd.revs = 0x7f;
    t = c->throttle;
    if (g_rsnd.load <= t)
        g_rsnd.load += (t - g_rsnd.load) / 2;
    else
        g_rsnd.load += (t - g_rsnd.load) / 8;
    on = curve(g_rsnd.load >> 1);
    off = curve(0x80 - (g_rsnd.load >> 1));
    level = (g_rsnd.revs + 0x14) * (near_of(dist) + 1) / 128;
    if (level > 0x7f)
        level = 0x7f;
    off = off * level >> 7;
    on = on * level >> 7;
    if (g_rsnd.mono)
        angle = 0;
    pan = pan_of(angle);
    if (g_rsnd.engine) {
        snd_volume(g_rsnd.voice[ch], scaled(on));
        snd_bend(g_rsnd.voice[ch], g_rsnd.revs);
        snd_pan(g_rsnd.voice[ch], pan);
        if (snd_idle(g_rsnd.voice[ch])) {
            int v = snd_play(1, pan, scaled(on), g_rsnd.revs);
            if (v >= 0) {
                g_rsnd.voice[ch] = v;
                g_rsnd.sound[ch] = 1;
            }
        }
    }
    snd_volume(g_rsnd.voice[1], scaled(off));
    snd_bend(g_rsnd.voice[1], g_rsnd.revs);
    snd_pan(g_rsnd.voice[1], pan);
    if (g_rsnd.voice[0xe] != -1) {
        snd_volume(g_rsnd.voice[0xe], scaled(level));
        snd_pan(g_rsnd.voice[0xe], pan);
    }
}

/* 0x7fef8: the engine heard from the driver's seat, or from where the camera is */
static void engine_heard(void)
{
    race_car *c = g_cars[heard()];
    int angle = 0, dist = 1;
    if (g_camera->mode > 1)
        rsnd_where(c, &angle, &dist);
    engine_sound(c, angle, dist);
}

/* 0x5316c: the player's horn */
static void horn_sound(int dist, int angle, int viewed)
{
    int pan = viewed < 2 ? 0x40 : pan_of(angle);
    channel_play(0xe, 3, 0x40, near_of(dist), pan);
}

/* 0x7fcbc: the tyres (front on channel 4, rear on 0xc: their sound by the surface's kind,
   how hard by the slide) and the horn of the car heard; from the seat the front tyres
   heard to the side the car is pushed */
static void tyres_heard(void)
{
    race_car *c = g_cars[heard()];
    int dist, angle, rdist, rangle, a, b;
    if (g_camera->mode <= 1) {
        angle = -c->force_side * 6;
        if (angle < 0)
            angle += 0x1000000;
        dist = 0;
        rangle = 0x800000;
        rdist = 0;
    } else {
        rsnd_where(c, &angle, &dist);
        rdist = dist;
        rangle = angle;
    }
    if (g_rsnd.skid[0] >= 0) {
        a = g_rsnd.skid[0];
        b = g_rsnd.skid[1];
    } else {
        a = 4;
        b = g_surfaces[c->surface][3] ? 8 : 1;
    }
    if (g_rsnd.engine) {
        if (c->slide_front > 0 || (g_rsnd.sound[4] <= 0x2e && g_rsnd.sound[4] >= 0x26))
            channel_event(4, a & 0xff, b & 0xff, c->slide_front, dist + 1, angle);
        if (c->slide_rear > 0 || (g_rsnd.sound[0xc] <= 0x2e && g_rsnd.sound[0xc] >= 0x26))
            channel_event(0xc, a & 0xff, b & 0xff, c->slide_rear, rdist + 1, rangle);
    } else
        g_rsnd.horn_time = 0;
    if (c->horn && g_drive.viewed >= 0 && g_drive.viewed < g_humans)
        horn_sound(dist, angle, g_drive.viewed);
    else
        channel_stop(0xe);
}

/* 0x7fe88: two players: the other one's horn heard */
static void other_horn(void)
{
    for (short i = 0; i < g_humans; i++)
        if (i != g_drive.viewed && g_cars[i]->horn && !g_timing.finish[i]) {
            g_rsnd.horn_car = i;
            g_rsnd.horn_time = 999;
            g_rsnd.horn_kind = 1;
            return;
        }
    g_rsnd.horn_time = 0;
}

void rsnd_horn(int slot, int kind)
{
    if (g_rsnd.horn_time)
        return;
    if (slot >= 0 && slot < g_field)
        g_rsnd.horn_car = g_rsnd.horn_kind = 1;
    else {
        g_rsnd.horn_car = slot;
        g_rsnd.horn_kind = kind;
    }
    g_rsnd.horn_time = ((int)((collide_rand() & 0xffff) * (unsigned)exe_i32(0xedf4c)) >> 16)
                       + exe_i32(0xedf48);
}

/* 0x80974: a computer's car's horn while it sounds (on channel 4), stopped after */
static void horns(void)
{
    if (g_rsnd.horn_time) {
        race_car *h = g_cars[g_rsnd.horn_car];
        int angle, dist, d;
        rsnd_where(h, &angle, &dist);
        d = doppler(h);
        g_rsnd.horn_time--;
        if (d)
            channel_level(4, exe_i32(0x7fa80 + 4 * g_rsnd.horn_car),
                          d * exe_i32(0x7faa8 + 4 * g_rsnd.horn_kind) / 256, near_of(dist), angle);
    } else if (g_rsnd.voice[4] >= 0 && g_rsnd.sound[4] >= 0x3f && g_rsnd.sound[4] <= 0x41)
        channel_stop(4);
}

/* one of the two nearest cars on channel 2 or 3 (0x800a0's end): its engine, Doppler
   shifted, or its tyres while it slides */
static void near_car(int ch, race_car *c, int lvl)
{
    int angle = angle_to(c), d, bend;
    if (!c->slide_rear || g_timing.finish[(short)heard()]) {
        d = doppler(c);
        if (d) {
            if (c->slot >= 0 && c->slot < g_field) {
                bend = exe_i32(0xf7798 + 4 * c->slot) + d * 64 / 256;
                channel_level(ch, ch == 2 ? 0x43 : 0x45, bend, lvl, angle);
            } else {
                bend = exe_i32(0xf7798 + 4 * c->slot) + d * 95 / 256;
                channel_level(ch, ch == 2 ? 0x30 : 0x32, bend, lvl, angle);
            }
        }
    } else if (c->slot >= g_humans)
        channel_event(ch, (c->slide_rear >> 8) & 0xff, c->slide_rear & 0xff, c->slide_front >> 4,
                      lvl, angle);
    else
        channel_event(ch, 4, 1, c->slide_front >> 4, lvl, angle);
    g_rsnd.near[ch - 2] = c;
}

/* 0x800a0: the radar (the police within 64 nodes, nearer ahead), the siren and its echo
   under a bridge, and the two cars heard loudest (near and fast) on channels 2 and 3 */
static void others(void)
{
    race_car *w = g_cars[heard()], *p = g_cars[2], *best = 0, *second = 0, *siren = 0, *t;
    int lv1 = 0, lv2 = 0, near = 0, sdist = 0, i, k;

    if (g_drive.chase && (p->part & 4) && iabs(p->node - w->node) < 0x40) {
        if (w->state != 4 && p->state != 4) {
            k = (0x40 - iabs(p->node - w->node)) / 8;
            g_hud.radar = k > 6 ? 6 : k < 0 ? 0 : k;
            near = 1;
        }
        if (g_hud.radar && p->node <= w->node)
            g_hud.radar /= 2;
        if (p->bits[2] & 0xc) {
            k = dist_to(p);
            if (k < 0x400000) {
                siren = p;
                sdist = k;
            }
        }
    }
    for (i = 0; i < g_ncars; i++) {
        race_car *c = g_cars[i];
        const race_camera *cam = g_camera;
        int dx, dy, dz, s, d, v;
        if ((g_drive.chase && i == 2) || c == w || !(c->part & 4))
            continue;
        dx = iabs(c->pos.x - cam->pos.x);
        dy = iabs(c->pos.y - cam->pos.y);
        dz = iabs(c->pos.z - cam->pos.z);
        s = dz < dx ? (dz >> 2) + dx : dz + (dx >> 2);
        if (s >= 0x200000)
            continue;
        d = s < dy ? (s >> 2) + dy : s + (dy >> 2);
        if (!g_rsnd.sfx)
            continue;
        k = against(c);
        v = near_of(d) * k / 64;
        if (v > lv1) {
            second = best;
            lv2 = lv1;
            lv1 = v;
            best = c;
        } else if (v > lv2) {
            lv2 = v;
            second = c;
        }
    }
    if (g_drive.chase) {
        if (!near)
            g_hud.radar = g_hud.radar_flash = 0;
        else if (g_drive.flags & 4)
            g_hud.radar_flash = 1 - g_hud.radar_flash;
    }
    if (!g_rsnd.sfx)
        return;

    g_rsnd.echo_now = 0;
    if (siren && g_drive.chase) {
        int vol = sdist < 0x400000 ? (0x400000 - sdist) * 0x7f / 0x400000 : 0;
        int angle = angle_to(siren), bend = 0x40, d = doppler(siren);
        if (d && iabs(g_rsnd.siren_node - siren->node) < 2) {
            bend = d * 0x40 / 256;
            if (p->state == 3)
                g_rsnd.siren_bend = bend;
            else if (--g_rsnd.siren_bend < 5)
                g_rsnd.siren_bend = 1;
            channel_level(9, 0x62, g_rsnd.siren_bend, vol, angle);
            g_rsnd.siren_on = 1;
        }
        if (w->node_flags[0]) {
            if (!g_rsnd.echo_wait) {
                channel_level(0xb, 100, bend, vol, angle);
                g_rsnd.echo_now = 1;
                g_rsnd.echo_wait = 3;
            } else if (!g_rsnd.echo_now)
                g_rsnd.echo_wait--;
        } else {
            g_rsnd.echo_now = 0;
            channel_stop(0xb);
        }
    } else if (g_rsnd.siren_on) {
        channel_stop(9);
        channel_stop(0xb);
        g_rsnd.siren_on = 0;
    }
    if (g_drive.chase)
        g_rsnd.siren_node = p->node;

    /* a car keeps its channel */
    if (best == g_rsnd.near[1] || second == g_rsnd.near[0]) {
        k = lv1, lv1 = lv2, lv2 = k;
        t = best, best = second, second = t;
    }
    if (best)
        near_car(2, best, lv1);
    else if (g_rsnd.voice[2] >= 0)
        channel_stop(2);
    if (second)
        near_car(3, second, lv2);
    else if (g_rsnd.voice[3] >= 0)
        channel_stop(3);
}

void rsnd_step(void)
{
    if (g_drive.state == 3 || !g_rsnd.sfx)
        return;
    horns();
    engine_heard();
    tyres_heard();
    if (g_humans > 1)
        other_horn();
    others();
}
