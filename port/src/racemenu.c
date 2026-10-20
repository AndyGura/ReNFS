/* The menus between and after the races (NOTES §4.21): race_menu_checkpoint 0x1a5d8,
   race_menu_finish 0x1b6a0. */
#include "racemenu.h"
#include "cardraw.h"
#include "config.h"
#include "drive.h"
#include "movie.h"
#include "session.h"
#include "fe.h"
#include "files.h"
#include "fsh.h"
#include "mainmenu.h"
#include "menu.h"
#include "sound.h"
#include "standings.h"
#include "video.h"
#include <stdio.h>
#include <time.h>

enum { CONTINUE, REPLAY, HIGHLIGHTS, OPTIONS, STANDINGS, CHAT, QUIT, NONE };

static fsh_file g_art;

static void art(const char *name)
{
    shape s;
    if (fsh_find(&g_art, name, &s))
        fe_draw_shape(&s, 0);
}

/* the mouse put on a row by the keys (0x1a550, 0x1a55e) */
static void place(int sel)
{
    static const int y[7] = { 128, 173, 219, 266, 312, 358, 404 };
    mouse_place(320, y[sel]);
    fe_mouse_save();
}

static int load(unsigned char *pal, const char *name)
{
    char path[256];

    snprintf(path, sizeof path, "%s/CHECK/%s", art_dir(), name);
    if (!fsh_load(&g_art, path) || !fsh_palette(&g_art, 0, pal)) {
        fprintf(stderr, "renfs: %s is missing\n", path);
        fsh_free(&g_art);
        return 0;
    }
    return 1;
}

int race_menu_checkpoint(void)
{
    static const char *const lit[7] = { "conl", "repl", "higl", "gmol", "gmsl", "chal", "quil" };
    static const char *const dark[7] = { "cond", "repd", "higd", "gmod", "gmsd", "chad", "quid" };
    unsigned char pal[768];
    int sel = CONTINUE, old = CONTINUE, redraw = 1, first = 1, changed = 0, done = 0;
    int answer = 1, ready = 1, released = 1, k;
    unsigned held = 0;

    place(CONTINUE);
    if (!load(pal, "CHKMENU.QFS"))
        return 1;

    while (!done) {
        int mx, my;

        /* the mouse picks a row by its height (the chat only in a network race) */
        mouse_read(&mx, &my);
        k = my >= 0 && my <= 0x91 ? CONTINUE : my >= 0x9c && my <= 0xbe ? REPLAY
            : my >= 0xcb && my <= 0xec ? HIGHLIGHTS : my >= 0xf7 && my <= 0x11b ? OPTIONS
            : my >= 0x125 && my <= 0x149 ? STANDINGS : my >= 0x180 && my <= 0x1e0 ? QUIT
            : NONE;
        if (k != NONE && k != sel) {
            old = sel;
            sel = k;
            changed = 1;
        }

        if (redraw) {
            art("bgnd");
            art(lit[sel]);
            mouse_show(1);
            if (first) {
                fe_mouse_save();
                if (g_mouse_active)
                    fe_mouse_read();
                fe_styles_init();
                pal_fade_up(pal, 0, 256);
                first = 0;
            }
            settimeout(12000);
            redraw = changed = 0;
        }
        if (changed) {                     /* the old row dark, the new one lit */
            fe_play_sound(0x47);
            art(dark[old]);
            art(lit[sel]);
            changed = 0;
            redraw = 1;                    /* (all drawn again next time, the pointer
                                              shown again) */
        }

        if (!ready) {
            held += ticksince();
            if (held > 25)
                ready = 1;
        }
        if (g_mouse_active)
            fe_mouse_read();
        if (!g_mouse_buttons)
            released = 1;
        fe_mouse_moved();
        k = fe_read_keys();
        if (k == 0x20 || k == 0x0d || (g_mouse_buttons == 1 && released)) {
            fe_mouse_press();
            if (ready) {
                fe_play_sound(0x48);
                switch (sel) {
                case CONTINUE:
                    answer = 1;
                    done = 1;
                    break;
                case REPLAY:               /* the race again from REPLAY.RPL (+0xaf 1, 2
                                              the highlights): not yet */
                case HIGHLIGHTS:
                case OPTIONS:              /* 0x12760: the game's options (chkopts.qfs) */
                    fprintf(stderr, "renfs: that screen is not written yet\n");
                    break;
                case STANDINGS:
                    /* the segment's standings again (a race's and a tournament's, the
                       standings so far after them: 0x6f070, not yet), then this menu
                       drawn again */
                    pal_fade_down(0, 256);
                    standings_segment(0);
                    place(sel);
                    first = redraw = 1;
                    break;
                case QUIT:
                    if (fe_yes_no_dialog(g_german ? "Spiel beenden ?" : "End game ?")) {
                        answer = 0;
                        done = 1;
                    }
                    break;
                }
                settimeout(12000);
            }
            released = ready = 0;
            held = 0;
            ticksince();
        } else if (k && ready) {
            if (k == 0x1b) {               /* Esc: as Continue */
                fe_play_sound(0x48);
                done = 1;
            } else if (k == 0x4800 || k == 0x5000) {
                /* up and down, round, past the chat */
                fe_mouse_hide();
                old = sel;
                sel = k == 0x4800 ? (sel == CONTINUE ? QUIT : sel - 1) : (sel + 1) % 7;
                if (sel == CHAT)
                    sel = k == 0x4800 ? STANDINGS : QUIT;
                place(sel);
                changed = 1;
            }
            ready = 0;
            held = 0;
            ticksince();
        } else if (!k)
            ready = 1;
        video_retrace();
        if (g_quit) {
            answer = 0;
            done = 1;
        }
    }
    /* (on the way to the next segment the music fades: the flag is the stack's, set only
       by Quit, which keeps the music for the menu) */
    if (answer)
        music_fade_out();
    fe_sound_bank_free();
    pal_fade_down(0, 256);
    if (answer)
        music_wait_end();
    fsh_free(&g_art);
    fe_mouse_save();
    return answer;
}

int race_menu_finish(void)
{
    static const char *const lit[6] = { "conl", "repl", "higl", "gmol", "gmsl", "chal" };
    static const char *const dark[6] = { "cond", "repd", "higd", "gmod", "gmsd", "chad" };
    unsigned char pal[768];
    int sel = CONTINUE, old = CONTINUE, redraw = 1, first = 1, changed = 0, done = 0;
    int ready = 1, released = 1, k;
    unsigned held = 0;

    place(CONTINUE);
    if (!load(pal, "FINMENU.QFS"))
        return 1;

    while (!done) {
        int mx, my;

        /* the mouse picks a row by its height (the chat only in a network race, where
           the replay and the highlights are not offered) */
        mouse_read(&mx, &my);
        k = my >= 0 && my <= 0x91 ? CONTINUE : my >= 0x9c && my <= 0xbe ? REPLAY
            : my >= 0xcb && my <= 0xec ? HIGHLIGHTS : my >= 0xf7 && my <= 0x11b ? OPTIONS
            : my >= 0x125 && my <= 0x149 ? STANDINGS : NONE;
        if (k != NONE && k != sel) {
            old = sel;
            sel = k;
            changed = 1;
        }

        if (redraw) {
            art("bgnd");
            art(lit[sel]);
            mouse_show(1);
            if (first) {
                fe_mouse_save();
                if (g_mouse_active)
                    fe_mouse_read();
                pal_fade_up(pal, 0, 256);
                fe_styles_init();
                first = 0;
            }
            settimeout(12000);
            redraw = changed = 0;
        }
        if (changed) {
            fe_play_sound(0x47);
            art(dark[old]);
            art(lit[sel]);
            changed = 0;
            redraw = 1;                    /* (all drawn again next time, the pointer
                                              shown again) */
        }

        if (!ready) {
            held += ticksince();
            if (held > 25)
                ready = 1;
        }
        if (g_mouse_active)
            fe_mouse_read();
        if (!g_mouse_buttons)
            released = 1;
        fe_mouse_moved();
        k = fe_read_keys();
        if (k == 0x20 || k == 0x0d || (g_mouse_buttons == 1 && released)) {
            fe_mouse_press();
            if (ready) {
                fe_play_sound(0x48);
                switch (sel) {
                case CONTINUE:
                    done = 1;
                    break;
                case REPLAY:               /* the race again from REPLAY.RPL (+0xaf 1, 2
                                              the highlights), the menu's music and art
                                              after it: not yet */
                case HIGHLIGHTS:
                case OPTIONS:              /* 0x195d8: the game's options: not yet */
                    fprintf(stderr, "renfs: that screen is not written yet\n");
                    break;
                case STANDINGS:
                    /* the standings again, as after a race not finished */
                    pal_fade_down(0, 256);
                    standings_after(0, 1);
                    place(sel);
                    first = redraw = 1;
                    break;
                }
                settimeout(12000);
            }
            released = ready = 0;
            held = 0;
            ticksince();
        } else if (k && ready) {
            if (k == 0x1b) {               /* Esc: as Continue */
                fe_play_sound(0x48);
                done = 1;
            } else if (k == 0x4800 || k == 0x5000) {
                /* up and down, round, past the chat */
                fe_mouse_hide();
                old = sel;
                sel = k == 0x4800 ? (sel == CONTINUE ? CHAT : sel - 1) : (sel + 1) % 6;
                if (sel == CHAT)
                    sel = k == 0x4800 ? STANDINGS : CONTINUE;
                place(sel);
                changed = 1;
            }
            ready = 0;
            held = 0;
            ticksince();
        } else if (!k)
            ready = 1;
        video_retrace();
        /* two minutes, or the right button, end it too */
        if (!first && (timedout() || (g_mouse_buttons == 2 && released)))
            done = 1;
        if (g_quit)
            done = 1;
    }
    /* (the menu's music goes on into Control Central) */
    fsh_free(&g_art);
    fe_sound_bank_free();
    pal_fade_down(0, 256);
    fe_mouse_save();
    return 1;
}

/* 0x74aec: Watcom's random numbers seeded from the clock's seconds (srand 0x91df4), the
   first (rand 0x91dd0) / 0x2aaa: 0, 1 or 2 (3 counts as 0) */
static int film_number(void)
{
    unsigned s = (unsigned)time(0) * 0x41c64e6d + 0x3039;
    int n = (int)(s >> 16 & 0x7fff) / 0x2aaa;

    return n > 2 ? 0 : n;
}

/* 0x341a4: a film between the screens, at 320x240 from black to black, the keys typed
   before it thrown away (with the films off, 0x15d120, a picture instead: never here) */
static void film(const char *name)
{
    static const unsigned char black[768];

    fe_flush_keys();
    video_palette(black, 0, 256);
    movie_play(name);
    video_palette(black, 0, 256);
    video_mode(640, 480);
    video_palette(black, 0, 256);
}

void race_film_police(void)
{
    char name[16];

    snprintf(name, sizeof name, g_german ? "GCOP%d.TGV" : "COP%d.TGV", film_number() + 1);
    film(name);
}

void race_film_winner(void)
{
    const session_racer *me = &g_session.racer[g_player];
    int won = me->total >= 1 && !g_drive.gave_up[g_player], track = config_word(CFG_TRACK);
    int type = config_word(CFG_RACE_TYPE), i, n, car;
    char name[16];

    /* won: no other racer of a smaller total (a total of 0, a segment missed, counts as
       smaller) */
    if (won)
        for (i = 0; i < g_field && i < SESSION_RACERS; i++)
            if (i != g_player && g_session.racer[i].total < me->total)
                won = 0;
    if (track >= 0 && track < 9)
        g_session.raced[track] = 1;
    if (!won)
        return;
    if (type == 3) {
        /* a tournament: the track's own film; the last of eight (or nine) won, the
           tournament's */
        if (track >= 0 && track < 9)
            g_session.won[track] = 1;
        for (i = n = 0; i < 9; i++)
            n += g_session.won[i];
        if ((n == 9 && g_vegas_ok) || (n == 8 && !g_vegas_ok))
            film("WIN#7.TGV");
        else
            film(track == 0 ? "WIN#2.TGV" : track == 1 ? "WIN#4.TGV" : track == 2 ? "WIN#6.TGV"
                 : track == 3 ? "WIN#1.TGV" : track == 4 || track == 5 ? "WIN#3.TGV"
                 : "WIN#5.TGV");
        return;
    }
    if (type == 0)
        return;
    /* head to head or a single race: one of two by the player's car */
    do
        n = film_number();
    while (n == 2);
    car = config_word(CFG_CAR);
    snprintf(name, sizeof name, "WIN#%d.TGV", car < 3 ? n + 1 : car < 6 ? n + 3 : n + 5);
    film(name);
}
