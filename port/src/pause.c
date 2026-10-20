/* The race held (NOTES §4.20). */
#include "pause.h"
#include "camera.h"
#include "drive.h"
#include "hud.h"
#include "opponent.h"
#include "racekeys.h"
#include "racesnd.h"
#include "video.h"

race_pause g_pause;

enum { K_ESC = 1, K_P = 0x19 };

/* the screen shown until P is down (or up) */
static void wait_key(int down)
{
    while (!g_quit && !key_down(K_P) != !down)
        video_retrace();
}

/* 0x63110: P: "paus" over the race, every sound quiet, until P is let go, pressed and
   let go again; then the dashboard drawn again, the camera's view set up again */
static void hold(race_camera *cam)
{
    unsigned long from = clock_ticks();

    hud_paused();
    rsnd_quiet();
    wait_key(0);
    wait_key(1);
    hud_redraw_due();
    camera_set_mode(cam, cam->mode);
    wait_key(0);
    /* (0x52640: the music's volume back, unless F5 has it off: no music yet) */
    g_pause.pending = 0;
    g_pause.last = g_drive.steps;
    g_pause.held_ticks += clock_ticks() - from;
}

/* race_pause_menu 0x56474: not yet; the race left as the port did before */
static void menu(void)
{
    g_pause.quit = 1;
}

void race_pause_step(race_camera *cam, int done)
{
    static const int drawn[3] = { 0x1c, 0xe, 9 };
    int k;

    /* 0x1fa1c: the calls due */
    for (k = 0; k < 2; k++)
        if (done && g_pause.at[k] && g_pause.at[k] <= done) {
            g_pause.at[k] = 0;
            if (k == 0)
                menu();
            else
                hold(cam);
        }
    /* 0x63304: Esc after the first 120 steps' counts (60 steps), once in 90 steps; P the
       same once in 60, not while the police's ticket is shown (no network races here:
       there Esc with a Shift ends the race) */
    if (g_drive.steps < 0x3c) {
        g_pause.pending = 0;
        g_pause.last = 0;
    }
    if (key_down(K_ESC) && g_drive.state == 1 && g_drive.steps > 0x78 && !g_pause.pending) {
        if (g_pause.last + 0xb4 <= g_drive.steps) {
            g_pause.last = g_drive.steps;
            g_pause.at[0] = done + 1;
            g_pause.pending = 1;
            /* (the road's distance asked again, as F3 left it) */
            if (g_rkeys.road < 3)
                g_rkeys.drawn_next = drawn[g_rkeys.road];
        }
    } else if (key_down(K_P) && g_drive.steps > 0x78 && !g_pause.pending
               && !g_opp.ticket_up && !g_opp.warning_up && g_pause.last + 0x78 <= g_drive.steps) {
        g_pause.last = g_drive.steps;
        g_pause.at[1] = done + 1;
        g_pause.pending = 1;
    }
}
