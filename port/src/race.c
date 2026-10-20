/* A race (NOTES §4.3, §4.7): the loading of race_main 0x6356c and the frame of
   race_draw_frame 0x5bc40, as far as the port has them. */
#include "race.h"
#include "opponent.h"
#include "camera.h"
#include "car.h"
#include "cardraw.h"
#include "config.h"
#include "crash.h"
#include "dash.h"
#include "drive.h"
#include "hud.h"
#include "exe.h"
#include "files.h"
#include "racepal.h"
#include "smoke.h"
#include "pause.h"
#include "racekeys.h"
#include "racesnd.h"
#include "sound.h"
#include "sndlib.h"
#include "road.h"
#include "roaddraw.h"
#include "sky.h"
#include "timing.h"
#include "track.h"
#include "trackfam.h"
#include "trackinfo.h"
#include "unpack.h"
#include "video.h"
#include "view.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* the race the snapshots were taken in: the 512TR against the Testarossa, a police car
   and six of the traffic (0x12dce4) */
#define RACE_CARS 9
static const int race_ids[RACE_CARS] = { 204, 201, 400, 7, 5, 0, 11, 6, 4 };
#define RACERS 2
#define MIRROR_SECTIONS 4      /* 0x13661c */

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

/* RENFS_THROW="<step>:<x>,<y>,<z>": a test, the player's car given that speed in metres a
   second (east, up, north) at that step, as drive_emu's "thrown" runs do, to see a crash */
static void race_throw(race_car *player, int st)
{
    const char *e = getenv("RENFS_THROW");
    double x, y, z;
    int at;

    if (e && sscanf(e, "%d:%lf,%lf,%lf", &at, &x, &y, &z) == 4 && st == at * 2) {
        player->vel.x = (int)(x * 65536);
        player->vel.y = (int)(y * 65536);
        player->vel.z = (int)(z * 65536);
    }
}

/* RENFS_PLACE="<step>:<along>,<across>,<speed>": a test, the opponent put that many
   metres along and across the road from the player's car at that step, driving at that
   speed (m/s), as opponent_emu's placed runs do, to see the cars meet */
static void race_place(const race_car *player, int st)
{
    const char *e = getenv("RENFS_PLACE");
    race_car *o = g_ncars > 1 ? g_cars[1] : 0;
    double along, across, speed;
    int at;

    if (!o || !e || sscanf(e, "%d:%lf,%lf,%lf", &at, &along, &across, &speed) != 4 || st != at * 2)
        return;
    o->pos.x = player->pos.x + (int)(along * player->road_fwd.x) + (int)(across * player->road_side.x);
    o->pos.y = player->pos.y + (int)(along * player->road_fwd.y) + (int)(across * player->road_side.y);
    o->pos.z = player->pos.z + (int)(along * player->road_fwd.z) + (int)(across * player->road_side.z);
    o->yaw = player->yaw;
    o->node = player->node;
    o->road_side = player->road_side;
    o->road_up = player->road_up;
    o->road_fwd = player->road_fwd;
    o->road_at = player->road_at;
    o->road_off = o->across_to = player->road_off + (int)(across * 65536);
    o->road_speed = o->speed = o->speed_to = (int)(speed * 65536);
}

/* a race step for the player's car (race_step 0x6348c, race_update_cars 0x519d8): the
   flags from the step counter, the keyboard's word (controls_read 0x49248: both gear
   keys at once hold the engine at full revs), every second step the controls applied,
   then the driving; the counter on by 2 */
static void race_step(race_car *player, race_camera *cam, int done)
{
    int st = g_drive.steps, i;
    /* (0x48bec) given up: straight on, no pedals, the handbrake */
    unsigned w = g_drive.gave_up[g_player] ? 0x87a0 : drive_keyboard(g_road_mirror);

    g_drive.flags = (st & 1 ? 0 : 1) | (st & 3 ? 0 : 4) | (st & 7 ? 0 : 0x10);
    g_drive.revving[g_player] = (w & 0x3000) == 0x3000;
    if (g_drive.revving[g_player])
        w &= 0xcfff;
    g_drive.controls[g_player] = w;
    /* NITRO's keys: N with a shift or B */
    g_drive.nitro_keys = key_down(0x31) && (key_down(0x2a) || key_down(0x36) || key_down(0x30));
    g_drive.clock = (int)clock_ticks();
    /* race_step 0x6348c: the calls due and the pause's keys, then the race's other keys */
    race_pause_step(cam, done);
    race_keys(cam);
    race_throw(player, st);
    race_place(player, st);
    if (g_drive.flags & 4)
        drive_controls(player);
    /* (race_update_cars 0x519d8) the finish, the driving, the cars sorted round the
       world's checks, the splits */
    timing_step();
    for (i = 0; i < g_ncars; i++) {
        race_car *c = g_cars[g_timing.by_road[i]];

        if ((c->part & 4) && c->state)
            drive_step(c);
    }
    timing_sort();
    crash_world();
    timing_sort();
    timing_splits();
    /* the camera: outside while the car watched is upside down or turned across the road
       (0x6324c), then moved (0x136b0) */
    camera_check(cam, g_cars[g_drive.viewed]);
    camera_update(cam);
    /* the sounds, heard from where the camera now is (the voices that played to their
       end freed first, as the mixer frees them) */
    snd_time(clock_ticks());
    rsnd_step();
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
    int i, k, eye, horizon, redline, near;
    hud_car driver;
    hud_map_state map;
    unsigned start;
    int mirror_skip = 0, redraw, in_seat = 1;
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
    {
        /* the track (0x12da12, CONFIG.DAT +0x95) and segment (0x12da0e, +0x93) by the name
           asked for (the circuits' files by track, 0x3629c) */
        static const char *const tracks[9] = { "cy", "cl", "al", "tr1", "tr2", "tr6", "tr3",
                                               "tr7", "tr4" };
        for (i = 8; i >= 0; i--)
            if (!strncasecmp(name, tracks[i], strlen(tracks[i])))
                break;
        g_drive.kind = i < 0 ? 0 : i;
        g_drive.segment = i >= 0 && i < 3 && name[2] >= '1' && name[2] <= '3' ? name[2] - '1' : 0;
    }
    g_drive.catch_up = 1;      /* (0x12dd10, race_setup_record) */
    /* the laps: an open road's one; a circuit's by the race length (+0xb3) as Control
       Central sets them (0x437fc): 4, 8 or 16, or 2, 6 or 12 on tracks 4..7 (or
       RENFS_LAPS for a test) */
    g_drive.laps = 1;
    if (g_track.loop) {
        static const int laps[2][3] = { { 4, 8, 16 }, { 2, 6, 12 } };
        int len = config_word(0xb3);

        g_drive.laps = laps[g_drive.kind >= 4 && g_drive.kind <= 7][len < 0 || len > 2 ? 2 : len];
        if (getenv("RENFS_LAPS"))
            g_drive.laps = atoi(getenv("RENFS_LAPS"));
        if (g_drive.laps < 1 || g_drive.laps > DRIVE_LAPS)
            g_drive.laps = 1;
    }
    /* where the race ends (race_main before 0x506dc): a circuit's laps, an open road's
       last section less 45 nodes */
    g_drive.last_node = g_track.loop ? g_track.length * 4
        : (short)(g_track.head[6] | g_track.head[7] << 8) * 4 - 0xb5;
    snprintf(g_drive.track, sizeof g_drive.track, "%s", name);
    /* the best lap to beat: the track's lap record (load_dash 0x2e8d4) */
    g_drive.best_lap = S32(g_config, 0xd0 + g_drive.kind * 0xa6b);
    g_drive.chase = 1;         /* (the police on in these races, 0x12dd08, 0x12da34) */
    g_drive.police = 1;
    memset(&g_crash, 0, sizeof g_crash);
    if (!smoke_load(&g_crash.smoke))       /* (after cars_create 0x51388) */
        return 0;
    crash_start();
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
    /* the computer's cars (car_setup 0x51200; their size the PDN's or the model's,
       0x512e8, as car_load left it): driven by the computer (kind 3), a metre along
       until their first step: the opponent's puts it on the road from the grid, the
       traffic's sends each car off the road to wait (kind 6), the police car's (car 2)
       puts it behind the player's, off the race (part without 4) until it comes out
       ahead (0x79500) */
    for (i = 1; i < RACE_CARS; i++) {
        cars[i].slot = i;
        cars[i].state = 3;
        cars[i].pos.z = 0x10000;
        cars[i].lap = 1;
        if (i < RACERS)
            cars[i].bits[0] |= 4;
    }
    cars[2].bits[0] |= 8;
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
    smoke_table();             /* (0x55fa8's end) */

    /* the views and the camera in the driver's seat; the racers on the grid, the player
       in slot 7 (0x12dca0) */
    view_init(0, cars[0].number, horizon);
    camera_init(eye);
    view_setup(&g_view_ahead, g_view_ahead.mode);
    view_setup(&g_view_mirror, g_view_mirror.mode);
    view_setup(&g_view_ahead, g_view_ahead.mode);
    memset(&g_pause, 0, sizeof g_pause);
    g_pause.track = g_drive.kind;      /* (the pause menu's map) */
    g_pause.segment = g_drive.segment;
    race_keys_start();         /* (the settings' detail, 0x5ba1c's 0x1440c) */
    /* (the opponent keeps the grid's node until its first step: the road's axes it
       drives by are worked out as that step moves it to the nearest, drive_opponent) */
    for (i = 0; i < RACERS; i++)
        car_grid_place(&cars[i], 7 - i, RACERS, 0);
    track_follow(&cars[0].pos, &cars[0].node);
    memset(&cam, 0, sizeof cam);
    camera_set_car(&cam, &cars[0], 0);

    /* the cars: the racers standing on the grid (their turn the one the grid gives,
       +0x351), the traffic and the police after them */
    for (i = 0; i < RACE_CARS; i++) {
        cars[i].slot = i;
        cars[i].yaw_drawn = cars[i].yaw;
        g_cars[i] = &cars[i];
    }
    g_ncars = RACE_CARS;
    g_player = 0;
    if (getenv("RENFS_VIEW"))  /* a test: the race watched from camera mode n */
        camera_set_car(&cam, &cars[0], atoi(getenv("RENFS_VIEW")));
    g_racers = RACERS;
    g_race_type = 1;           /* head to head (0x36c5c from CONFIG.DAT) */
    g_field = RACERS;
    g_humans = 1;
    memcpy(g_car_marks, marks, RACE_CARS);
    for (i = 0; i < RACE_CARS; i++)
        cars[i].part = 7;      /* (+0x51a: driven, their finish checked, meeting the others) */
    /* the race's timing (race_start 0x506dc): the settings' laps (CONFIG.DAT +0xb1,
       0x12dd14, as Control Central left them), the race length's times (+0xb3, 0x12dd24) */
    g_timing.lap_limit = g_drive.laps;
    g_timing.table = config_word(0xb3) < 0 || config_word(0xb3) > 2 ? 1 : config_word(0xb3);
    timing_cars();
    timing_start();
    /* the computer's driving (load_traffic_cfg 0x7de84): the drivers by the grid places,
       the random numbers from the clock (0x38c10 with time(), 0x91d9b) */
    for (i = 0; i < RACERS; i++)
        g_opp.grid[i] = 7 - i;
    g_opp.traffic = RACE_CARS - RACERS - 1;    /* (0x12da3c: the cars after the police) */
    g_opp.rival_car = cars[1].number;          /* (0x12da9e) */
    /* the sounds (race_main 0x6363b, before the random numbers are seeded): the settings'
       volumes and stereo (0x12da53's record; none without a sound card, 0x529f4), the
       banks (0x52774), the engine started silent (0x52670) and heard (0x53214); voice 0
       held for the race's music */
    rsnd_init();
    g_rsnd.present = !getenv("RENFS_NOSOUND");
    g_rsnd.sfx = g_rsnd.present ? config_word(CFG_SFX_VOLUME) : 0;
    g_rsnd.music = g_rsnd.present ? config_word(0x75) : 0;
    g_rsnd.mono = config_word(0x71) == 2;
    snd_init(g_rsnd.present);
    if (g_rsnd.sfx && g_rsnd.music)
        snd_hold(0, 101, g_rsnd.music);
    snd_time(clock_ticks());
    rsnd_load(cars[0].number, cars[1].number, g_drive.kind);
    rsnd_engine_on();
    opponent_start(getenv("RENFS_SEED") ? (unsigned)strtoul(getenv("RENFS_SEED"), 0, 0)
                                        : (unsigned)time(0));
    g_camera = &cam;
    g_road_paint.cars = cars_at_node;
    g_road_paint.cars_behind = cars_behind_at;

    /* the dashboard as the race starts: the status bar still black (its first 360
       steps), standing with the engine idling (+0x3c1 700 at the start) */
    memset(&g_hud, 0, sizeof g_hud);
    g_hud.track = g_drive.kind;
    g_hud.table = g_timing.table;
    g_hud.shown = 1;
    g_hud.race_type = g_race_type;
    g_hud.running = 1;
    g_hud.map = 1;             /* (0x12b988: on as a race starts) */
    g_hud.laps = g_drive.laps;
    g_hud.lap_limit = g_timing.lap_limit;
    g_hud.finish_node = g_drive.last_node;
    g_hud.circuit = g_track.loop;
    g_hud.segment = g_drive.segment;
    /* (kilometres an hour: the settings' 0x15d140, not read yet; miles) */
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
    driver.viewed = 0;
    driver.human = 1;
    driver.player = 0;
    hud_redraw_due();
    /* the race's music, the loading's last (race_main 0x6356c: 0x63d88, with 0xf749a) */
    race_music_start();
    start = clock_ticks();

    while (!g_quit && !g_pause.quit && !g_timing.result) {
        const int *m;

        /* the race's steps: one each three ticks of the clock (the race clock's
           three-cycle, race_timer_tick 0x4c454), the player's car driven */
        g_rkeys.race_clock = (int)((clock_ticks() - start - g_pause.held_ticks) / 3);
        while (done < (unsigned)g_rkeys.race_clock) {
            race_step(&cars[0], &cam, done);
            done++;
        }
        /* the dials the player's car's, the status bar's numbers the car watched's
           (Alt+Ctrl and a number) */
        driver.steer = cars[0].steer;
        driver.rpm = cars[0].rpm;
        {
            const race_car *v = g_cars[g_drive.viewed];

            driver.viewed = g_drive.viewed;
            driver.human = g_drive.viewed < g_humans;
            driver.player = v->player;
            driver.speed = v->speed;
            driver.road_speed = v->road_speed;
            driver.gear = v->gear;
            driver.lap = v->lap;
            driver.node = v->node;
        }

        /* the road's points (0x50070): its sections kept, gathered again when the
           camera is on another quarter (or an object was knocked over) */
        /* (race_draw_frame 0x5bcf5: the road drawn as far as F3 asked, gathered again) */
        if (race_keys_frame())
            road_refresh();
        g_hud.detail = g_view_detail;
        track_keep_sections(quarter(cam.node));
        if (g_track.last_node < 0 || quarter(g_track.last_node) != quarter(cam.node))
            road_gather(cam.node, quarter(cam.node), g_rkeys.drawn);
        /* (the road behind for the mirror: 0x50070's end, when the camera's node changed
           or an object was knocked over: 0x5004c sets the camera's flag) */
        if (cam.flags || g_track.last_kinds < 0)
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
        /* the view changed: the screen round it into the view's picture first (0x3081c,
           whatever the view; from the driver's seat the dashboard covers it) */
        redraw = g_view_redraw;
        if (redraw)
            hud_outside_screen(cam.ahead->box);
        /* the view painted off the screen (0xf4578), then the dashboard puts it on the
           screen through its windows (0x31684), or from outside the HUD's lists (0x314e4) */
        raster_window(g_hud_view.pix, g_hud_view.w, win.x, win.y, win.x + win.w, win.y + win.h);
        sky_draw(cam.yaw, cam.roll, &cam.pos, m, w, &win, g_rkeys.plain_sky);
        g_car_ticket = g_opp.ticket_up || g_opp.warning_up;
        g_road_steps = g_drive.steps;
        road_objects_start();
        road_draw(g_rkeys.drawn, cam.node, near, &win);
        g_hud.steps = g_drive.steps;
        g_hud.ahead = g_timing.split;
        g_hud.splits = g_timing.split_shown;
        g_hud.best_lap = g_drive.best_lap;
        map.node = (short)g_cars[g_drive.viewed]->node;
        for (i = 0; i < RACERS; i++)
            map.nodes[i] = (short)cars[i].node;
        map.place = g_timing.place_shown;   /* (the last frame's, as the map comes first) */
        memcpy(map.places, g_timing.places, sizeof map.places);
        map.driver = cam.mode == 0;
        hud_map(&map);
        k = hud_ticket(g_opp.ticket_up, g_opp.warning_up, g_timing.result);
        if (k == 1)
            ;                  /* the police's ticket or warning instead */
        else if (cam.mode == 0) {
            /* back in the driver's seat: the whole dashboard again (0x143e68, 0x2d284) */
            if (!in_seat)
                hud_redraw_due();
            hud_frame(&driver);
        } else
            hud_outside_frame(&driver, redraw, cam.ahead->current);
        in_seat = cam.mode == 0;
        /* (after the ticket, the screen round the view drawn again: 0xeaaa8) */
        g_view_redraw = k == 2;
        video_retrace();
    }

    rsnd_free();               /* (0x52a70) */
    music_fade_out();          /* (race_run 0x37228: 0x720d8, 0x72150) */
    music_wait_end();
    g_road_paint.cars = g_road_paint.cars_behind = 0;
    g_ncars = 0;
    for (i = 0; i < RACE_CARS; i++)
        car_free(&cars[i]);
    hud_free();
    smoke_free();
    dash_free();
    trackfam_free();
    track_free();
    return 1;
}
