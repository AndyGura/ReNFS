/* The race's keys beside the driving (NOTES §4.19). */
#include "racekeys.h"
#include "joystick.h"
#include "camera.h"
#include "cardraw.h"
#include "config.h"
#include "drive.h"
#include "exe.h"
#include "racesnd.h"
#include "timing.h"
#include "video.h"

race_keys_state g_rkeys;

/* the scan codes */
enum { K_1 = 2, K_CTRL = 0x1d, K_LSHIFT = 0x2a, K_ALT = 0x38, K_RSHIFT = 0x36, K_END = 0x4f, K_F1 = 0x3b, K_F2, K_F3, K_F4,
       K_F5, K_F6 };

/* 0x1440c: the views' detail and their layouts, the outside's moved on by its size */
static void views_start(int outside)
{
    g_view_detail = g_rkeys.window > 2 ? 2 : g_rkeys.window;
    view_layout(&g_view_ahead, g_view_ahead.layout);
    view_layout(&g_view_outside, g_view_outside.layout + outside);
}

void race_keys_start(void)
{
    static const int drawn[3] = { 0x1c, 0x12, 0xd };    /* (0x5bbd3) */

    g_rkeys.view_key = drive_key_scan(config_word(0x5f4a) & 0xffff);  /* (0x36a0c) C */
    g_rkeys.view_up = 0;
    g_rkeys.fkey_up = 0;
    g_rkeys.muted = 0;
    g_rkeys.window = config_word(0x83) & 0xff;
    g_rkeys.road = config_word(0x85) & 0xff;
    g_rkeys.plain_sky = config_word(0x89) & 0xff;
    g_no_markers = 0;
    views_start(config_word(0x81) & 0xff);
    g_rkeys.drawn = 0x1c;
    g_rkeys.drawn_next = g_rkeys.road < 3 ? drawn[g_rkeys.road] : -1;
}

/* the car watched (Alt+Ctrl and a number), the camera behind it (mode 4) */
static void watch(race_camera *cam, int n)
{
    if (n >= g_ncars)          /* (the original took whatever the table held) */
        return;
    g_drive.viewed = n;
    camera_set_car(cam, g_cars[n], 4);
    rsnd_engine_on();
}

/* 0x1f0b4: player p gives up (not in a time trial, nor once in or given up already):
   driven no more (0x48bec), its finish time 999,999 (with 0x12dd5c, 1 in a race on one
   machine; the network's message 10 too), so the race ends once the car has stopped,
   the player not placed */
static void give_up(int p)
{
    if (!g_race_type || g_timing.finish[p] || g_drive.gave_up[p])
        return;
    g_drive.gave_up[p] = 1;
    g_timing.finish[p] = 999999;
}

/* F2 (0x14370): the view's pixels, at most twice as wide and high */
static void detail(race_camera *cam, int d)
{
    if (d > 2)
        d = 2;
    if (d != g_view_detail) {
        g_view_detail = d;
        view_layout(cam->ahead, cam->ahead->current);
    }
}

void race_keys(race_camera *cam)
{
    static const int drawn[3] = { 0x1c, 0xe, 9 };
    int n;

    if (!key_down(g_rkeys.view_key) && !(g_joy.on && (g_joy.held & 4))) {
        /* (the view key, or the joystick's third button) */
        if (!key_down(K_ALT) || !key_down(K_CTRL))
            g_rkeys.view_up = 1;
        else
            for (n = 0; n < 8; n++)
                if (key_down(K_1 + n)) {
                    /* the cars after the racers only in a race of more than two, or
                       with the right Shift */
                    if (n < 2 || g_race_type >= 2 || key_down(K_RSHIFT))
                        watch(cam, n);
                    break;
                }
    } else {
        /* the view key, once a press, while a player's car is watched (or the replay
           not in its mode 5, 0x145544: no replays yet) */
        if (g_rkeys.view_up && g_drive.viewed < g_humans && g_drive.state == 1)
            camera_next(cam);
        g_rkeys.view_up = 0;
    }
    /* Shift+End, after 100 steps of the race clock (400 in a network race; not in a
       replay) */
    if (key_down(K_END) && (key_down(K_LSHIFT) || key_down(K_RSHIFT)) && g_rkeys.race_clock > 100)
        give_up(g_player);
    /* (Alt+M: the camera's node and mode printed, a developer's display (0xf4568);
       0x12da04, the replay's keys: not yet) */
    if (key_down(K_CTRL))
        return;
    if (key_down(K_F1)) {
        /* the outside view's next size (0x14324), or none */
        if (g_rkeys.fkey_up) {
            n = exe_i32(0xe8b10 + 0x16c * g_view_res + 52 * cam->ahead->current);
            if (n >= 0)
                view_layout(cam->ahead, n);
            g_view_redraw = 1;
        }
    } else if (key_down(K_F2)) {
        if (g_rkeys.fkey_up) {
            g_rkeys.window = (g_rkeys.window + 1) % 4;
            if (!g_view_res && g_rkeys.window > 1)
                g_rkeys.window = 0;
            detail(cam, g_rkeys.window);
        }
    } else if (key_down(K_F3)) {
        if (g_rkeys.fkey_up) {
            g_rkeys.road = (g_rkeys.road + 1) % 3;
            g_rkeys.drawn_next = drawn[g_rkeys.road];
        }
    } else if (key_down(K_F4)) {
        if (g_rkeys.fkey_up)
            g_rkeys.plain_sky = !g_rkeys.plain_sky;
    } else if (key_down(K_F5)) {
        if (g_rkeys.fkey_up) {
            g_rkeys.muted = !g_rkeys.muted;
            rsnd_mute(g_rkeys.muted);
        }
    } else if (key_down(K_F6)) {
        if (g_rkeys.fkey_up)
            g_no_markers = !g_no_markers;
    } else {
        g_rkeys.fkey_up = 1;
        return;
    }
    g_rkeys.fkey_up = 0;
}

int race_keys_frame(void)
{
    if (g_rkeys.drawn_next == -1)
        return 0;
    g_rkeys.drawn = g_rkeys.drawn_next;
    g_rkeys.drawn_next = -1;
    return 1;
}

void race_keys_to_config(void)
{
    /* (0x56b48: at 320x200 the pixels at most doubled across) */
    if (!g_view_res && g_rkeys.window > 1)
        g_rkeys.window = 1;
    config_set_word(0x83, g_rkeys.window);
    config_set_word(0x85, g_rkeys.road);
    config_set_word(0x89, g_rkeys.plain_sky);
    config_set_word(0x81, g_view_outside.current - g_view_outside.layout);   /* 0x144cc */
}

void race_keys_from_config(void)
{
    /* (0x5818d; the road's distance itself changes at the next Esc or F3) */
    g_rkeys.window = config_word(0x83) & 0xff;
    g_rkeys.road = config_word(0x85) & 0xff;
    g_rkeys.plain_sky = config_word(0x89) & 0xff;
    views_start(config_word(0x81) & 0xff);
}
