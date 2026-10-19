/* For tools/collide_emu.py and the race checks (drive_emu.py, opponent_emu.py,
   timing_emu.py): collide.c itself, so that its parts can be tried one by one, and its
   state set from the original's. */
#include "collide.c"
#include <string.h>

/* EA's random numbers' state (0xf8a8c, six words) */
void collide_test_random(unsigned *r, int set)
{
    int k;

    for (k = 0; k < 6; k++)
        if (set)
            g_collide.random[k] = r[k];
        else
            r[k] = g_collide.random[k];
}

/* the cars by node (0x13c298) */
void collide_test_order(const int *by_road, int n)
{
    int k;

    for (k = 0; k < n; k++)
        g_timing.by_road[k] = by_road[k];
}

int collide_test_impulse(void)
{
    return g_collide.impulse;
}

void collide_test_set_impulse(int v)
{
    g_collide.impulse = v;
}

/* 0x2b864 on two cars' bodies: out[0..2] the point, out[3..5] the normal */
int collide_test_separate(race_car *a, race_car *b, int *out)
{
    return box_separate(&a->body, &b->body, (vec3 *)out, (vec3 *)(out + 3));
}

/* 0x2afb8 */
int collide_test_push(race_car *a, race_car *b, const int *pt, const int *n)
{
    return box_impulse(a, b, (const vec3 *)pt, (const vec3 *)n);
}

/* 0x1c6a4 */
int collide_test_cars(race_car *a, race_car *b)
{
    return collide(a, b);
}

/* the roadside objects knocked over (0xea5d8: 2 bytes a placement), n bytes */
void collide_test_fallen(unsigned char *b, int n, int set)
{
    if (set)
        memcpy(g_track.fallen, b, (size_t)n);
    else
        memcpy(b, g_track.fallen, (size_t)n);
}

int collide_test_placements(void)
{
    return g_track.nplacements;
}

/* a car's model size (0x65160: +0x64e, +0x652, +0x64a) */
void collide_test_size(const race_car *c, int *out)
{
    out[0] = c->size[0];
    out[1] = c->size[1];
    out[2] = c->size[2];
}

unsigned collide_test_rand(void)
{
    return ea_rand();
}
