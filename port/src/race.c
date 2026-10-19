/* A race (NOTES §4.3, §4.7): the loading of race_main 0x6356c and the frame of
   race_draw_frame 0x5bc40, as far as the port has them. */
#include "race.h"
#include "camera.h"
#include "car.h"
#include "cardraw.h"
#include "config.h"
#include "dash.h"
#include "drive.h"
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
#define MIRROR_SECTIONS 4      /* 0x13661c */
#define DRAWN 28               /* the sections drawn ahead (0x136618) */

static int quarter(int node)
{
    return node >= 0 ? node / 4 : -(-node / 4);
}

/* the mirror's view into its picture (0x5be3e ..): the road behind and its cars, turned
   round left for right */
static void draw_mirror(race_camera *cam, const int *m)
{
    const int *mm = view_select(cam, 1);
    const view_window *w = g_view_now->win;
    road_window win = { w->x, w->y, w->cw, w->ch };
    int near;

    g_camera_m = mm;
    near = road_transform_behind(&cam->pos, mm);
    raster_window(g_hud_mirror.pix, g_hud_mirror.w, 0, 0, g_hud_mirror.w, g_hud_mirror.h);
    sky_behind(cam->yaw, cam->pitch, cam->node, g_view_now->lens_h, w->dy, g_hud_mirror.w,
               g_hud_mirror.h);
    road_draw_behind(MIRROR_SECTIONS, cam->node, near, &win);
    hud_mirror_flip();
    view_select(cam, 0);
    g_camera_m = m;
}

static int S32(const unsigned char *p, int at)
{
    return (int)(p[at] | p[at + 1] << 8 | p[at + 2] << 16 | (unsigned)p[at + 3] << 24);
}

/* a race step for the player's car (race_step 0x6348c, race_update_cars 0x519d8): the
   flags from the step counter, the keyboard's word (controls_read 0x49248: both gear
   keys at once hold the engine at full revs), every second step the controls applied,
   then the driving; the counter on by 2 */
static void race_step(race_car *player)
{
    int st = g_drive.steps;
    unsigned w = drive_keyboard(g_road_mirror);

    g_drive.flags = (st & 1 ? 0 : 1) | (st & 3 ? 0 : 4) | (st & 7 ? 0 : 0x10);
    g_drive.revving[g_player] = (w & 0x3000) == 0x3000;
    if (g_drive.revving[g_player])
        w &= 0xcfff;
    g_drive.controls[g_player] = w;
    /* NITRO's keys: N with a shift or B */
    g_drive.nitro_keys = key_down(0x31) && (key_down(0x2a) || key_down(0x36) || key_down(0x30));
    g_drive.clock = (int)clock_ticks();
    if (g_drive.flags & 4)
        drive_controls(player);
    drive_step(player);
    g_drive.steps = st + 2;
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
    hud_map_state map;
    unsigned start;
    int mirror_skip = 0;
    unsigned done = 0;
    const char *car = "F512TR";

    if (!exe_load() || !track_load(name) || !trackinfo_load(name, 1, 0)
        || !trackfam_load(name, 1, 0)) {
        fprintf(stderr, "renfs: the track %s could not be loaded\n", name);
        return 0;
    }
    config_load();             /* the player's settings, the records the status bar shows */
    memset(&g_drive, 0, sizeof g_drive);
    g_drive.state = 1;
    g_drive.kind = 0;          /* City 1 (0x12da12), its first segment */
    g_drive.catch_up = 1;      /* (0x12dd10, race_setup_record) */
    g_drive.laps = 1;
    /* where the race ends (race_main before 0x506dc): a circuit's laps, an open road's
       last section less 45 nodes */
    g_drive.last_node = g_track.loop ? g_track.length * 4
        : (short)(g_track.head[6] | g_track.head[7] << 8) * 4 - 0xb5;
    snprintf(g_drive.track, sizeof g_drive.track, "%s", name);
    drive_default_keys();
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
    /* the player's car ready to drive (cars_create 0x51388: load_car_pbs, car_reset),
       with the options' gearbox, traction control and ABS */
    cars[0].slot = 0;
    if (!drive_load(&cars[0], config_word(CFG_AUTO), config_word(CFG_TCS) == 0,
                    config_word(CFG_ABS) == 0))
        return 0;
    eye = S32(cars[0].spec, 0x36c);         /* the driver's eyes above the road */
    horizon = S32(cars[0].spec, 0x370);     /* where the horizon is in the view */
    redline = S32(cars[0].spec, 0xac);      /* the engine's red line */
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
    view_setup(&g_view_ahead, g_view_ahead.mode);
    view_setup(&g_view_mirror, g_view_mirror.mode);
    view_setup(&g_view_ahead, g_view_ahead.mode);
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
    g_road_paint.cars_behind = cars_behind_at;

    /* the dashboard as the race starts: the status bar still black (its first 360
       steps), standing with the engine idling (+0x3c1 700 at the start) */
    memset(&g_hud, 0, sizeof g_hud);
    g_hud.track = 0;           /* City 1 (0x12da12), its first segment */
    g_hud.table = 1;
    g_hud.shown = 1;
    g_hud.race_type = g_race_type;
    g_hud.running = 1;
    g_hud.map = 1;             /* (0x12b988: on as a race starts) */
    memset(&map, 0, sizeof map);
    map.place = 1;
    for (i = 0; i < RACERS; i++)
        map.places[i] = (unsigned char)(i + 1);
    map.driver = 1;
    driver.steer = 0;
    driver.number = cars[0].number;
    driver.rpm = 700;
    driver.redline = redline;
    driver.speed = 0;
    hud_redraw_due();
    start = clock_ticks();

    while (!g_quit && key_get() != 0x1b) {
        const int *m;

        /* the race's steps: one each three ticks of the clock (the race clock's
           three-cycle, race_timer_tick 0x4c454), the player's car driven */
        while (done < (clock_ticks() - start) / 3) {
            race_step(&cars[0]);
            done++;
        }
        driver.steer = cars[0].steer;
        driver.rpm = cars[0].rpm;
        driver.speed = cars[0].speed;

        camera_update(&cam);
        /* the road's points (0x50070): its sections kept, gathered again when the
           camera is on another quarter */
        track_keep_sections(quarter(cam.node));
        if (gathered < 0 || quarter(gathered) != quarter(cam.node)) {
            road_gather(cam.node, quarter(cam.node), DRAWN);
            gathered = cam.node;
        }
        if (cam.flags)         /* (the road behind for the mirror: 0x50070's end) */
            road_gather_behind(cam.node);
        cam.flags = 0;

        cars_list();           /* (the race's step does this) */

        m = view_select(&cam, 0);
        view_matrix(&cam);
        g_camera_m = m;
        cars_seen(cam.node);
        w = g_view_now->win;
        near = road_transform(&cam.pos, m, cam.node);
        /* the mirror (race_draw_frame 0x5be17), every other frame with the settings'
           +0x87, shown each frame while the camera has one */
        g_hud.mirror = cam.mirror_on;
        if (cam.mirror_on && (!config_word(0x87) || (mirror_skip ^= 1)))
            draw_mirror(&cam, m);
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
        g_hud.steps = g_drive.steps;
        map.node = (short)cars[0].node;
        for (i = 0; i < RACERS; i++)
            map.nodes[i] = (short)cars[i].node;
        hud_map(&map);
        hud_frame(&driver);
        video_retrace();
    }

    g_road_paint.cars = g_road_paint.cars_behind = 0;
    g_ncars = 0;
    for (i = 0; i < RACE_CARS; i++)
        car_free(&cars[i]);
    hud_free();
    dash_free();
    trackfam_free();
    track_free();
    return 1;
}
