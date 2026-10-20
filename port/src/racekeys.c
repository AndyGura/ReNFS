/* The race's keys beside the driving (NOTES §4.19). */
#include "racekeys.h"
#include "camera.h"
#include "cardraw.h"
#include "config.h"
#include "drive.h"
#include "exe.h"
#include "racesnd.h"
#include "video.h"

race_keys_state g_rkeys;

/* the scan codes */
enum { K_1 = 2, K_CTRL = 0x1d, K_ALT = 0x38, K_RSHIFT = 0x36, K_F1 = 0x3b, K_F2, K_F3, K_F4,
       K_F5, K_F6 };

void race_keys_start(void)
{
    static const int drawn[3] = { 0x1c, 0x12, 0xd };    /* (0x5bbd3) */

    g_rkeys.view_key = 0x2e;   /* C: the settings' +0x5f4a 'C' through 0x36758's table */
    g_rkeys.view_up = 0;
    g_rkeys.fkey_up = 0;
    g_rkeys.muted = 0;
    g_rkeys.window = config_word(0x83) & 0xff;
    g_rkeys.road = config_word(0x85) & 0xff;
    g_rkeys.plain_sky = config_word(0x89) & 0xff;
    g_no_markers = 0;
    /* 0x1440c: the views' detail and their layouts, the outside's moved on by +0x81 */
    g_view_detail = g_rkeys.window > 2 ? 2 : g_rkeys.window;
    view_layout(&g_view_ahead, g_view_ahead.layout);
    view_layout(&g_view_outside, g_view_outside.layout + (config_word(0x81) & 0xff));
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

    if (!key_down(g_rkeys.view_key)) {
        /* (or the joystick's view button, 0x12f1d8 and 0x12f274 & 4: no joystick yet) */
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
    /* (Shift+End: the player gives the race up, 0x1f0b4; Alt+M: the camera's node and
       mode printed, a developer's display (0xf4568); 0x12da04, the replay's keys: not yet) */
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
