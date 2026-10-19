/* The race's cars (NOTES §4.6). */
#include "car.h"
#include "track.h"
#include "trackfam.h"
#include "racepal.h"
#include "files.h"
#include "exe.h"
#include "roaddraw.h"
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

/* the polygon groups drawn one after the other (0xe3d10 ..), and the wheels' pictures */
static const char *const group_names[4] = {"inside", "surface", "outside", "convex"};

/* the near and far models made drawable with their pictures (0x65c11 ..), their groups,
   and a racer's wheels showing their fourth picture (0x65e61 ..) */
static int car_models(race_car *c, int id)
{
    const unsigned char *lo = c->cfm, *hi = c->cfm + c->cfm_len;
    int k, front, rear, tire4;

    if (!model_make(&c->near_m, ww_entry(c->cfm, 0), lo, hi, ww_entry(c->cfm, 1)) ||
        !model_make(&c->far_m, ww_entry(c->cfm, 2), lo, hi, ww_entry(c->cfm, 3))) {
        car_free(c);
        return 0;
    }
    if (g_road_mirror) {
        model_mirror(&c->near_m);
        model_mirror(&c->far_m);
    }
    for (k = 0; k < 4; k++) {
        c->groups[k].index = model_group(&c->near_m, group_names[k], &c->groups[k].count,
                                         &c->groups[k].list);
        if (c->groups[k].index < 0)
            c->groups[k].count = 0;
    }
    if (id >= 200) {
        front = model_find_texture(&c->near_m, "frntires");
        rear = model_find_texture(&c->near_m, "rertires");
        tire4 = model_find_texture(&c->near_m, "tire4");
        if (tire4 >= 0) {
            model_set_texture(&c->near_m, front, c->near_m.tex[tire4].shape);
            model_set_texture(&c->near_m, rear, c->near_m.tex[tire4].shape);
        }
    }
    return 1;
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
    c->cfm_len = len;
    if (!c->cfm || ww_count(c->cfm) < 4) {
        fprintf(stderr, "renfs: %s is missing or not a car\n", path);
        car_free(c);
        return 0;
    }
    /* the pictures near (entry 1) and far (entry 3): their whole palettes and the body's
       colours, all tinted in place */
    bank = c->pics[0] = ww_entry(c->cfm, 1);
    c->pics_pal[0] = car_palette(bank, "!PAL");
    c->colours = car_palette(bank, "!xxx");
    if (c->police)
        c->colours = 0;
    bank = c->pics[1] = ww_entry(c->cfm, 3);
    c->pics_pal[1] = car_palette(bank, "!PAL");
    if (!car_models(c, id))
        return 0;
    return 1;
}

void car_free(race_car *c)
{
    model_free(&c->near_m);
    model_free(&c->far_m);
    free(c->cfm);
    c->cfm = c->colours = 0;
}

void cars_remap(race_car *const *cars, int n, const unsigned char *marks, int chase)
{
    int i;

    for (i = 0; i < n; i++) {
        car_pics_remap(cars[i]->pics[0], cars[i]->pics_pal[0], marks[i], chase && i == 2);
        car_pics_remap(cars[i]->pics[1], cars[i]->pics_pal[1], marks[i], chase && i == 2);
        /* (cars 0 and 1 then have their models' one-colour polygons matched again, 0x976c0:
           none drawn yet) */
    }
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
