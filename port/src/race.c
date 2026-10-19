/* A race (NOTES §4.3, §4.7): the loading of race_main 0x6356c and the frame of
   race_draw_frame 0x5bc40, as far as the port has them. */
#include "race.h"
#include "camera.h"
#include "car.h"
#include "cardraw.h"
#include "config.h"
#include "dash.h"
#include "hud.h"
#include "exe.h"
#include "files.h"
#include "racepal.h"
#include "road.h"
#include "roaddraw.h"
#include "sky.h"
#include "track.h"
#include "trackfam.h"
#include "trackinfo.h"
#include "unpack.h"
#include "video.h"
#include "view.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* the race the snapshots were taken in: the 512TR against the Testarossa, a police car
   and six of the traffic (0x12dce4) */
#define RACE_CARS 9
static const int race_ids[RACE_CARS] = { 204, 201, 400, 7, 5, 0, 11, 6, 4 };
#define RACERS 2
#define DRAWN 28               /* the sections drawn ahead (0x136618) */

/* the player's car's physics (SIMDATA/CARSPECS, 30FB): +0x36c the driver's eyes above
   the road, +0x370 where the horizon is in the view, +0xac the engine's red line */
static int car_spec(const char *name, int *eye, int *horizon, int *redline)
{
    char path[64];
    long len, n;
    unsigned char *d, *u;

    snprintf(path, sizeof path, "SIMDATA/CARSPECS/%s.PBS", name);
    d = file_load(path, &len);
    if (!d)
        return 0;
    u = is_packed(d, len) ? unpack(d, len, &n) : (n = len, d);
    if (u != d)
        free(d);
    if (!u || n < 0x374) {
        free(u);
        return 0;
    }
    *eye = u[0x36c] | u[0x36d] << 8 | u[0x36e] << 16 | u[0x36f] << 24;
    *horizon = u[0x370] | u[0x371] << 8 | u[0x372] << 16 | u[0x373] << 24;
    *redline = u[0xac] | u[0xad] << 8 | u[0xae] << 16 | u[0xaf] << 24;
    free(u);
    return 1;
}

static int quarter(int node)
{
    return node >= 0 ? node / 4 : -(-node / 4);
}

int race_test(const char *name)
{
    static race_car cars[RACE_CARS];
    race_car *list[RACE_CARS];
    unsigned char marks[RACE_CARS] = { 0 };
    race_camera cam;
    road_window win;
    const view_window *w;
    int i, eye, horizon, redline, near, gathered = -1;
    hud_car driver;
    unsigned start;
    const char *car = "F512TR";

    if (!exe_load() || !track_load(name) || !trackinfo_load(name, 1, 0)
        || !trackfam_load(name, 1, 0) || !car_spec(car, &eye, &horizon, &redline)) {
        fprintf(stderr, "renfs: the track %s or its car could not be loaded\n", name);
        return 0;
    }
    g_view.min_z = exe_i32(0xf7938);
    g_view.far_shift = *exe_at(0xf6aa4, 1);
    g_view.max_level = exe_i32(0xeaad8);
    g_road_mirror = 0;

    /* the cars and the palette, in race_main's order */
    for (i = 0; i < RACE_CARS; i++) {
        memset(&cars[i], 0, sizeof cars[i]);
        cars[i].player = i;
        cars[i].police = race_ids[i] >= 400;
        if (!car_load(&cars[i], race_ids[i]))
            return 0;
        list[i] = &cars[i];
    }
    cars_queue_colours(list, RACE_CARS, marks);
    if (!dash_load(car, cars[0].number, 0))
        return 0;
    dash_queue_colours();
    if (!hud_load())
        return 0;
    video_mode(320, 200);
    pal_apply_pieces();
    race_colours(marks, RACE_CARS, 0);
    needle_colours(cars[0].number);     /* (0x8a390, in race_colours) */
    /* (race_colours 0x2e74c goes on with the dashboard's pictures, then tints its palette)
       then the cars' pictures; in these races one racer against the player with the police
       on (0x12da30 = 1, 0x12da34), the chase of 0x6577c */
    pal_remap(g_dash.file.d, g_dash.pal);
    pal_tint(0, 0x100, g_dash.pal);
    cars_remap(list, RACE_CARS, marks, 1);
    hud_colours();             /* 0x55abc: the HUD's colours, then the shade table */
    shade_make(0x9999);
    pal_apply_remaps();

    /* the views and the camera in the driver's seat; the racers on the grid, the player
       in slot 7 (0x12dca0) */
    view_init(0, cars[0].number, horizon);
    camera_init(eye);
    view_setup(&g_view_ahead, exe_i32(0xe8de8 + 0x10));
    view_setup(&g_view_mirror, exe_i32(0xe8e28 + 0x10));
    view_setup(&g_view_ahead, exe_i32(0xe8de8 + 0x10));
    for (i = 0; i < RACERS; i++) {
        car_grid_place(&cars[i], 7 - i, RACERS, 0);
        track_follow(&cars[i].pos, &cars[i].node);
    }
    memset(&cam, 0, sizeof cam);
    camera_set_car(&cam, &cars[0], 0);

    /* the cars on the road: only the racers so far, standing (their turn the one the grid
       gives, +0x351) */
    for (i = 0; i < RACERS; i++) {
        cars[i].slot = i;
        cars[i].yaw_drawn = cars[i].yaw;
        g_cars[i] = &cars[i];
    }
    g_ncars = RACERS;
    g_player = 0;
    g_racers = RACERS;
    g_race_type = 1;           /* head to head (0x36c5c from CONFIG.DAT) */
    g_field = RACERS;
    g_humans = 1;
    memcpy(g_car_marks, marks, RACERS);
    g_camera = &cam;
    g_road_paint.cars = cars_at_node;

    /* the dashboard as the race starts: the status bar still black (its first 360
       steps), standing with the engine idling (+0x3c1 700 at the start) */
    memset(&g_hud, 0, sizeof g_hud);
    config_load();             /* the records the status bar shows */
    g_hud.track = 0;           /* City 1 (0x12da12), its first segment */
    g_hud.table = 1;
    g_hud.shown = 1;
    g_hud.race_type = g_race_type;
    g_hud.running = 1;
    driver.steer = 0;
    driver.number = cars[0].number;
    driver.rpm = 700;
    driver.redline = redline;
    driver.speed = 0;
    hud_redraw_due();
    start = clock_ticks();

    while (!g_quit && key_get() != 0x1b) {
        const int *m;

        camera_update(&cam);
        /* the road's points (0x50070): its sections kept, gathered again when the
           camera is on another quarter */
        track_keep_sections(quarter(cam.node));
        if (gathered < 0 || quarter(gathered) != quarter(cam.node)) {
            road_gather(cam.node, quarter(cam.node), DRAWN);
            gathered = cam.node;
        }
        cam.flags = 0;

        cars_list();           /* (the race's step does this) */

        m = view_select(&cam, 0);
        view_matrix(&cam);
        g_camera_m = m;
        cars_seen(cam.node);
        w = g_view_now->win;
        near = road_transform(&cam.pos, m, cam.node);
        win.x = w->x;
        win.y = w->y;
        win.w = w->cw;
        win.h = w->ch;
        /* the view painted off the screen (0xf4578), then the dashboard puts it on the
           screen through its windows (0x31684) */
        raster_window(g_hud_view.pix, g_hud_view.w, win.x, win.y, win.x + win.w, win.y + win.h);
        sky_draw(cam.yaw, m, &win, 0);
        road_objects_start();
        road_draw(DRAWN, cam.node, near, &win);
        /* the race's steps, a third of the clock's ticks (until the race's own clock,
           0x4c454's three-cycle, comes with the race's step) */
        g_hud.steps = (int)((clock_ticks() - start) / 3);
        hud_frame(&driver);
        video_retrace();
    }

    g_road_paint.cars = 0;
    g_ncars = 0;
    for (i = 0; i < RACE_CARS; i++)
        car_free(&cars[i]);
    hud_free();
    dash_free();
    trackfam_free();
    track_free();
    return 1;
}
