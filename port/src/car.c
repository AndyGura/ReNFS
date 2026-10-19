/* The race's cars (NOTES §4.6). */
#include "car.h"
#include "track.h"
#include "trackfam.h"
#include "racepal.h"
#include "files.h"
#include "exe.h"
#include <stdio.h>
#include <stdlib.h>

int lane_width(int n, int half)
{
    /* row n of the table holds i / n in 8.8 for i < 40; only rows 1..3 are made (row 0
       is cleared, the rows past 3 are other variables of the original) */
    if (n < 1 || n > 3 || half < 0 || half >= 40)
        return 0;
    return (short)((unsigned)((half / n << 16) + (half % n << 16) / n) >> 8);
}

void car_grid_place(race_car *c, int grid, int racers, int kind)
{
    /* the lanes' width at node 10, the racers two abreast from node 32 back */
    const unsigned char *n10 = node_ptr(10), *n20 = node_ptr(20);
    int w = lane_width(n10[4] & 15, n10[1] >> 3) << 8;

    c->node = 32 - (racers > 2 ? grid * 2 : grid / 2 * 4);
    c->pos.x = -w / 2 + grid % 2 * w;
    if (kind > 2) {
        c->pos.x += node_i32(c->node, 8);
        c->yaw = (int)((unsigned)(node_i32(c->node, 0x16) >> 16) << 10);
    }
    /* two lanes on the left and one on the right at node 20: the even slots one lane
       further left */
    if (n20[4] >> 4 == 2 && (n20[4] & 15) == 1 && grid % 2 == 0)
        c->pos.x -= w;
    c->dist = 0;
    c->pos.z = node_i32(c->node, 0x10);
}

/* a palette shape in the car's pictures: its colours, 8-bit ones made 6-bit, tinted
   (0x562e8: w · h / 3 entries) */
static unsigned char *car_palette(unsigned char *bank, const char *name)
{
    unsigned char *sh = shpi_find(bank, name), *pal;
    int i, n;

    if (!sh)
        return 0;
    pal = sh + 0x10;
    if (sh[0] == 0x24)
        for (i = 0; i < 0x300; i++)
            pal[i] >>= 2;
    n = (short)(sh[4] | sh[5] << 8) * (short)(sh[6] | sh[7] << 8) / 3;
    pal_tint(0, n, pal);
    return pal;
}

int car_load(race_car *c, int id)
{
    const char *name;
    char path[80];
    long len;
    unsigned char *bank;

    if (id >= 400)
        name = (const char *)exe_at((unsigned)exe_i32(0xf6e60), 1);
    else if (id >= 200) {
        name = (const char *)exe_at((unsigned)exe_i32(0xf6b10 + 4 * id), 1);
        c->number = id - 200;
    } else if (id >= 100) {
        name = (const char *)exe_at((unsigned)exe_i32(0xf6ca0 + 4 * id), 1);
        c->number = id - 100;
    } else
        name = (const char *)exe_at((unsigned)exe_i32(0xf6e70 + 4 * id), 1);
    snprintf(path, sizeof path, "SIMDATA/CARFAMS/%s%s.CFM", id >= 100 && id < 200 ? "M" : "", name);
    c->cfm = file_load(path, &len);
    if (!c->cfm || ww_count(c->cfm) < 4) {
        fprintf(stderr, "renfs: %s is missing or not a car\n", path);
        car_free(c);
        return 0;
    }
    /* the pictures near (entry 1) and far (entry 3): their whole palettes and the body's
       colours, all tinted in place */
    bank = ww_entry(c->cfm, 1);
    car_palette(bank, "!PAL");
    c->colours = car_palette(bank, "!xxx");
    if (c->police)
        c->colours = 0;
    bank = ww_entry(c->cfm, 3);
    car_palette(bank, "!PAL");
    return 1;
}

void car_free(race_car *c)
{
    free(c->cfm);
    c->cfm = c->colours = 0;
}

void cars_queue_colours(race_car *const *cars, int n, unsigned char *marks)
{
    int i;

    for (i = 0; i < n; i++) {
        if (cars[i]->police)
            continue;
        pal_queue_piece(exe_i32(0xf6ea0 + 4 * i), exe_i32(0xf6ec8 + 4 * i), cars[i]->colours);
        marks[i] = *exe_at(0xf6ef0 + 4 * i, 1);
        if (marks[i])
            pal_queue_piece(marks[i], 1, exe_at(0xf6f5c + 3 * cars[0]->number, 3));
    }
    pal_queue_piece(0xfa, 4, exe_at(0xf6de0, 12));
}
