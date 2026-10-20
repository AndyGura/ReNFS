/* For tools/watch_emu.py: the watching bar's state set as the original's globals have it,
   its drawing (hud_bar_draw) and its step (replay_step, racing), with the keys, the
   mouse, the camera and the engine stood in for: replay.c and menu.c are compiled with
   key_down, mouse_read, mouse_place, mouse_limits, camera_set_car, camera_next and
   rsnd_engine_on renamed to these. */
#include <string.h>
#include "cardraw.h"
#include "drive.h"
#include "hud.h"
#include "menu.h"
#include "racepal.h"
#include "replay.h"
#include "session.h"
#include "timing.h"

static unsigned char held[0x59];
static int mouse_x, mouse_y, buttons;
static int lim[4] = { 0, 0, 639, 479 };
static int calls[16][3], ncalls;

static void clamp(void)
{
    mouse_x = mouse_x < lim[0] ? lim[0] : mouse_x > lim[2] ? lim[2] : mouse_x;
    mouse_y = mouse_y < lim[1] ? lim[1] : mouse_y > lim[3] ? lim[3] : mouse_y;
}

int watch_test_key_down(int scan)
{
    return scan > 0 && scan < 0x59 && held[scan];
}

int watch_test_mouse_read(int *x, int *y)
{
    if (x)
        *x = mouse_x;
    if (y)
        *y = mouse_y;
    return buttons;
}

void watch_test_mouse_place(int x, int y)
{
    mouse_x = x;
    mouse_y = y;
    clamp();
}

void watch_test_mouse_limits(int x0, int y0, int x1, int y1)
{
    lim[0] = x0;
    lim[1] = y0;
    lim[2] = x1 < 639 ? x1 : 639;
    lim[3] = y1 < 479 ? y1 : 479;
    clamp();
}

static void call(int what, int a, int b)
{
    if (ncalls < 16) {
        calls[ncalls][0] = what;
        calls[ncalls][1] = a;
        calls[ncalls][2] = b;
    }
    ncalls++;
}

void watch_test_set_car(race_camera *cam, const race_car *c, int mode)
{
    int i;

    (void)cam;
    for (i = 0; i < g_ncars && g_cars[i] != c; i++)
        ;
    call(1, i < g_ncars ? i : -1, mode);
}

void watch_test_next(race_camera *cam)
{
    (void)cam;
    call(2, 0, 0);
}

void watch_test_engine(void)
{
    call(3, 0, 0);
}

/* the race: the car watched, the player's, the racers, the humans, the camera's mode,
   stopped; the bar's fields as the original's from 0x15d6f8 (pressed, lit, label,
   name_for, (0x15d708), last_pressed, lit_drawn, (0x15d714), redraw, whole, mouse y
   0x15d720, x 0x15d724); the session's names (8 of 9 bytes), the markers' colours */
void watch_test_set(int viewed, int player, int field, int humans, int cam_mode, int stopped,
                    const int *f, const char *names, const unsigned char *colours)
{
    int i;

    g_drive.state = 1;
    g_drive.flags = 0;
    g_replay.mode = 0;
    g_drive.viewed = viewed;
    g_player = player;
    g_field = field;
    g_humans = humans;
    g_hud.cam_mode = cam_mode;
    g_hud.stopped = g_timing.stopped = stopped;
    g_watch.pressed = f[0];
    g_watch.lit = f[1];
    g_watch.label = f[2];
    g_watch.name_for = f[3];
    g_watch.last_pressed = f[5];
    g_watch.lit_drawn = f[6];
    g_watch.redraw = f[8];
    g_watch.whole = f[9];
    g_watch.mouse_was[1] = f[10];
    g_watch.mouse_was[0] = f[11];
    for (i = 0; i < SESSION_RACERS; i++)
        memcpy(g_session.racer[i].name, names + 9 * i, 9);
    memcpy(g_marker_colour, colours, 8);
}

void watch_test_get(int *f)
{
    f[0] = g_watch.pressed;
    f[1] = g_watch.lit;
    f[2] = g_watch.label;
    f[3] = g_watch.name_for;
    f[5] = g_watch.last_pressed;
    f[6] = g_watch.lit_drawn;
    f[8] = g_watch.redraw;
    f[9] = g_watch.whole;
    f[10] = g_watch.mouse_was[1];
    f[11] = g_watch.mouse_was[0];
    f[12] = g_drive.viewed;
    f[13] = g_watch.last_key;
}

/* the keys held (scan codes), the mouse (640x480's pixels) and its buttons */
void watch_test_input(const unsigned char *keys, int x, int y, int b, int last_key)
{
    memcpy(held, keys, sizeof held);
    mouse_x = x;
    mouse_y = y;
    buttons = b;
    g_mouse_active = 1;
    g_watch.last_key = last_key;
    ncalls = 0;
}

int watch_test_mouse(int *xy, int *out)
{
    int n = ncalls < 16 ? ncalls : 16;

    xy[0] = mouse_x;
    xy[1] = mouse_y;
    memcpy(out, calls, sizeof(int) * 3 * n);
    return ncalls;
}

void watch_test_draw(void)
{
    hud_bar_draw();
}

void watch_test_step(void)
{
    replay_step();
}
