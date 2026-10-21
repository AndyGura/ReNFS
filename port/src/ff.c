/* Force feedback (NOTES §4.26): the original's game side (0x4d670..0x4dcb6) and the
   I-Force commands it sends through its serial driver (0x4b820.., 0x4ba10..0x4bf1c; each
   a packet 0x2b, command, length, bytes, an XOR, 0x4c258), the commands handed to ffdev.c
   as the packets' command and bytes */
#include "ff.h"
#include "ffdev.h"
#include "cardraw.h"
#include "drive.h"
#include "exe.h"

ff_state g_ff;

static int mulr(int a, int b)
{
    long long p = (long long)a * b;
    return (int)(p >> 16) + (int)((p >> 15) & 1);
}

static void send(int cmd, const unsigned char *b, int n)
{
    ffdev_send(cmd, b, n);
}

/* ---- the driver's commands --------------------------------------------------------------- */

/* 0x4ba10 (0x4b948): a kick of `s` percent towards `dir` degrees (90 and 270 across, the
   wheel's axis; 0 and 180 a stick's other) for `ms`: its two axes' parts from the
   driver's quarter tables (0xecf8c, 0xece20: 100 · sin and cos of each degree) */
static void jolt(unsigned s, int dir, unsigned ms)
{
    int a = (dir + 180) % 360, q, r, sx = 1, sy = 1;
    unsigned char b[3];

    if (s > 100)
        s = 100;
    if (ms > 2500)
        ms = 2500;
    if (a < 0)
        a += 360;
    q = a / 90;
    r = a % 90;
    if (q == 1) {
        sy = -1;
        r = 90 - r;
    } else if (q == 2)
        sx = sy = -1;
    else if (q == 3) {
        sx = -1;
        r = 90 - r;
    }
    b[0] = (unsigned char)((int)s * sx * exe_i32(0xecf8c + 4 * r) / 100);
    b[1] = (unsigned char)((int)s * sy * exe_i32(0xece20 + 4 * r) / 100);
    b[2] = (unsigned char)(ms / 10);
    send(0xa1, b, 3);
}

/* 0x4bbdc / 0x4bc70 and 0x4bcbc / 0x4bd50: the two shakes (0xa4, 0xa5): two sizes in
   percent and a rate (50 at most); 0, 0, 0 stops one. The second takes its sizes the
   other way round */
static void shake(int cmd, unsigned x, unsigned y, unsigned rate)
{
    unsigned char b[3];

    b[0] = (unsigned char)(x > 100 ? 100 : x);
    b[1] = (unsigned char)(y > 100 ? 100 : y);
    b[2] = (unsigned char)(rate > 50 ? 50 : rate);
    send(cmd, b, 3);
}

/* 0x4bd9c: the rumble (0xa6), in percent */
static void rumble(unsigned s)
{
    unsigned char b = (unsigned char)(s > 100 ? 100 : s);
    send(0xa6, &b, 1);
}

/* 0x4bf1c: the steady push (0xa7), each axis -100 .. 100 */
static void steady(int x, int y)
{
    unsigned char b[2];

    b[0] = (unsigned char)(x > 100 ? 100 : x < -100 ? -100 : x);
    b[1] = (unsigned char)(y > 100 ? 100 : y < -100 ? -100 : y);
    send(0xa7, b, 2);
}

/* ---- the game's side --------------------------------------------------------------------- */

void ff_open(void)
{
    g_ff.on = ffdev_open();
}

void ff_close(void)
{
    if (g_ff.on) {
        send(0x82, 0, 0);
        ffdev_close();
        g_ff.on = 0;
    }
}

void ff_forces(int on)
{
    if (g_ff.on)
        send(on ? 0x83 : 0x84, 0, 0);
}

void ff_stop(void)
{
    if (g_ff.on) {
        send(0x82, 0, 0);
        g_ff.rumble = 0;
    }
}

/* the car's speed in miles an hour: the player's own speed while a player's car is
   watched, else its speed along the road (0x4dc84) */
static int mph(const race_car *c)
{
    int v = g_drive.viewed < g_humans ? c->speed : c->road_speed;

    return mulr(v > 0 ? v : -v, 0x23d70) >> 16;
}

void ff_object(const race_car *c, int across)
{
    int m, len, s;

    if (!g_ff.on)
        return;
    m = mph(c);
    len = 200 - m;
    if ((unsigned)len < 80)
        len = 80;
    s = (m >> 1) + 70;
    if ((unsigned)s > 100)
        s = 100;
    jolt(s, across > 0 ? 45 : 315, len);
}

void ff_cars(const race_car *a, int push_x, int size)
{
    int s, len;

    if (!g_ff.on)
        return;
    /* a quarter of a second at least between two (the 100 Hz clock 0xed100) */
    if (g_drive.clock >= g_ff.last_hit) {
        if (g_ff.last_hit + 25 > g_drive.clock)
            return;
    }
    g_ff.last_hit = g_drive.clock;
    if (size < 0x2000)
        len = 100, s = 80;
    else if (size < 0x5000)
        len = 150, s = 85;
    else if (size < 0x10000)
        len = 200, s = 90;
    else if (size < 0x100000)
        len = 240, s = 95;
    else
        len = 280, s = 100;
    jolt(s, push_x > 0 && a->player != g_drive.viewed ? 45 : 315, len);
}

void ff_wall(int right)
{
    if (g_ff.on)
        jolt(70, right ? 90 : 270, 180);
}

void ff_gear(const race_car *c)
{
    int s;

    if (!g_ff.on || c->gear == -1)
        return;
    s = c->rpm < 0x1000 ? 12 : c->rpm < 0x1300 ? 22 : c->rpm < 0x1700 ? 32 : 42;
    jolt(s, c->gear < c->gear_now ? 180 : 0, 80);
}

/* 0x4d9a8: the road's shake, by the speed and the surface under the car: none on 0,
   the first on 1 (a little less) and 2, on 3 the first slow and the second across */
static void road_shake(const race_car *c)
{
    static const int below[8] = { 5, 10, 15, 20, 30, 40, 80, 150 };
    int m = mph(c), rate, size, k;

    if (!m) {
        if (g_ff.vib[0]) {
            shake(0xa4, 0, 0, 0);
            g_ff.vib[0] = 0;
        }
        if (g_ff.vib[1]) {
            shake(0xa5, 0, 0, 0);
            g_ff.vib[1] = 0;
        }
        return;
    }
    for (k = 0; k < 8 && m >= below[k]; k++)
        ;
    rate = 5 + k;
    size = k < 8 ? 15 + k : 24;
    switch ((unsigned)c->surface) {
    case 1:
    case 2:
        if (!g_ff.vib[0] || rate != g_ff.vib_rate || size != g_ff.vib_size) {
            int d = c->surface == 1 ? 2 : 0;

            g_ff.vib[0] = 1;
            shake(0xa4, size - d, size - d, rate - d);
            g_ff.vib_size = size;
            g_ff.vib_rate = rate;
            return;
        }
        break;
    case 3:
        if (!g_ff.vib[0] || rate != g_ff.vib_rate || size != g_ff.vib_size) {
            g_ff.vib[0] = 1;
            shake(0xa4, size, size, 1);
            rate = 1;
        }
        if (!g_ff.vib[1] || rate != g_ff.vib_rate || size != g_ff.vib_size) {
            g_ff.vib[1] = 1;
            shake(0xa5, size - 4, 0, rate);
            g_ff.vib_size = size;
            g_ff.vib_rate = rate;
            return;
        }
        break;
    default:                   /* 0 and the rest: none */
        if (g_ff.vib[0]) {
            shake(0xa4, 0, 0, 0);
            g_ff.vib[0] = 0;
        }
        if (g_ff.vib[1]) {
            shake(0xa5, 0, 0, 0);
            g_ff.vib[1] = 0;
        }
        break;
    }
    g_ff.vib_size = size;
    g_ff.vib_rate = rate;
}

/* 0x4d928: the car's sideways and forward forces (+0x411, +0x415) as a steady push,
   sent when it changes, not while the crash rumbles. (The forward one above -100 is
   made 100, as the original) */
static void steady_push(const race_car *c)
{
    int x = c->force_side >> 13, y = c->force_fwd >> 13;

    if (g_ff.rumble)
        return;
    x = x > 100 ? 100 : x < -100 ? -100 : x;
    if (y > 100 || y < -100)
        y = 100;
    if (x == g_ff.steady[0] && y == g_ff.steady[1])
        return;
    g_ff.steady[0] = x;
    g_ff.steady[1] = y;
    steady(x, y);
}

void ff_step(const race_car *c)
{
    if (!g_ff.on)
        return;
    road_shake(c);
    steady_push(c);
}

void ff_crash(int on)
{
    if (!g_ff.on)
        return;
    if (g_ff.rumble && !on)
        ff_stop();
    if (!g_ff.rumble && on) {
        ff_stop();
        g_ff.rumble = 1;
        rumble(80);
    }
}
