/* For tools/ff_emu.py: the force feedback's game side (port/src/ff.c) on a car of the
   test's, its commands caught (ffdev.c's record); the rest of the race left out. */
#include "ff.h"
#include "ffdev.h"
#include "drive.h"
#include <string.h>

drive_race g_drive;
int g_humans, g_player;

static race_car car[2];
static unsigned char rec[4096];

/* car k's fields the original's routines read */
race_car *ff_test_car(int k, int speed, int road_speed, int gear, int gear_now, int rpm,
                      int side, int fwd, int surface, int player)
{
    race_car *c = &car[k];

    c->speed = speed;
    c->road_speed = road_speed;
    c->gear = gear;
    c->gear_now = gear_now;
    c->rpm = rpm;
    c->force_side = side;
    c->force_fwd = fwd;
    c->surface = surface;
    c->player = player;
    return c;
}

void ff_test_globals(int viewed, int humans, int clock)
{
    g_drive.viewed = viewed;
    g_humans = humans;
    g_drive.clock = clock;
}

/* the commands since the last call: their bytes (cmd, n, bytes ..), their length */
int ff_test_take(unsigned char *out)
{
    int n = g_ffdev_rec_len;

    memcpy(out, rec, n);
    g_ffdev_rec = rec;
    g_ffdev_rec_cap = sizeof rec;
    g_ffdev_rec_len = 0;
    return n;
}
