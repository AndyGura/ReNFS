/* The track's screen (fe_track_screen 0x76380, from Control Central's track picture; NOTES
   §4.2): FRONTEND/ART/TRACKSEL/<track>SEL.QFS with the track's spoken description, the
   lengths of the road and its segments (an open road's three maps, one lit) or a circuit's
   and its laps, the time of day, the previous and next track, Done; and its Best Times
   (fe_track_best 0x75288, <track>BST.QFS): the track's ten best totals (a circuit's of its
   race length) and its top speed. The tables (the keys' moves, the pointer's places, the
   files' and cars' names, the lengths' texts) are the original's, read from its program. */
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "exe.h"
#include "fe.h"
#include "files.h"
#include "fsh.h"
#include "font.h"
#include "mainmenu.h"
#include "menu.h"
#include "replay.h"
#include "session.h"
#include "sound.h"
#include "trackrec.h"
#include "video.h"

static fsh_file g_sel;
static unsigned char g_pal[768];

static int cw(int off)
{
    return config_word(off);
}

/* a short of the program's tables */
static int tab(unsigned addr, int i)
{
    const unsigned char *p = exe_at(addr + 2 * (unsigned)i, 2);

    return p ? (short)(p[0] | p[1] << 8) : 0;
}

/* the string a table of the program's points at */
static const char *str(unsigned table, int i)
{
    const char *s = (const char *)exe_at((unsigned)exe_i32(table + 4 * (unsigned)i), 1);

    return s ? s : "";
}

static int track(void)
{
    int t = cw(CFG_TRACK);

    return t < 0 || t > 8 ? 0 : t;
}

/* the track's file, from the names 0x750cc (citysel ..) or 0x74fa8 (citybst ..), and its
   palette; the inverse table (TRACKSEL/<name>.INV) is built again from the palette */
static int load(unsigned names)
{
    char path[256];

    snprintf(path, sizeof path, "%s/TRACKSEL/%s.qfs", art_dir(), str(names, track()));
    if (!fsh_load(&g_sel, path) || !fsh_palette(&g_sel, 0, g_pal)) {
        fprintf(stderr, "renfs: %s is missing\n", path);
        fsh_free(&g_sel);
        return 0;
    }
    /* the screen cleared (0x72600) and the palette set at once (0x83fe4) */
    memset(g_fb, 0, (size_t)g_fb_w * g_fb_h);
    video_palette(g_pal, 0, 256);
    inv_build();                           /* 0x8227c */
    fe_styles_init();
    return 1;
}

static void draw(const char *name, int see_through)
{
    shape s;

    if (fsh_find(&g_sel, name, &s))
        fe_draw_shape(&s, see_through);
}

static int in_rect(int x0, int x1, int y0, int y1)
{
    int x, y;

    mouse_read(&x, &y);
    return x >= x0 && x <= x1 && y >= y0 && y <= y1;
}

/* mouse_set 0x71ad4: the pointer at a choice's place (tables of x and y), remembered */
static void place(unsigned xs, unsigned ys, int i)
{
    mouse_place(tab(xs, i), tab(ys, i));
    fe_mouse_save();
}

/* a circuit's laps by the race length (+0xb3): 4, 8, 16, or 2, 6, 12 on tracks 4..7 */
static void set_laps(void)
{
    int t = cw(CFG_TRACK), mid = t >= 4 && t <= 7, len = cw(CFG_RACE_LENGTH);

    config_set_word(CFG_LAPS, len == 0 ? (mid ? 2 : 4) : len == 1 ? (mid ? 6 : 8) : (mid ? 12 : 16));
}

/* the previous or the next track (0..7, Lost Vegas 8 when offered, 0xec460); in a
   tournament its car (0xec440) against the field (opponent 10) */
static void step_track(int next)
{
    int t = cw(CFG_TRACK);

    if (next)
        t = (t + 1) % (g_vegas_ok ? 9 : 8);
    else if (--t < 0)
        t = g_vegas_ok ? 8 : 7;
    config_set_word(CFG_TRACK, t);
    if (cw(CFG_RACE_TYPE) == 3) {
        config_set_word(CFG_OPPONENT, 10);
        config_set_word(CFG_CAR, tab(0xec440, t));
        fe_car_setup();                    /* car_setup 0x73a80 */
    }
    set_laps();
}

/* what ends a page: two minutes, or the right button */
static int timed_out(int first, int released)
{
    if (!first && (timedout() || (g_mouse_buttons == 2 && released))) {
        fe_play_sound(0x48);
        return 1;
    }
    return 0;
}

/* a record's car (0x74fcc: RX-7 .. DIABLO; the Warrior's and the others' past them from
   the menu's list 0xf7454, where the original would read past its table) and its race
   kind: German "  z" or "  d", English "t.t." or "h.h." */
static void record_row(int at, int y)
{
    int car = config_long(at + 0xf), kind = config_long(at + 0x23);

    if (car >= 0 && car < 8)
        fe_text(str(0x74fcc, car), 0x1e0, y);
    else if (car >= 8 && car < 12)
        fe_text(str(0xf7454, car), 0x1e0, y);
    if (kind == 0)
        fe_text(g_german ? "  z" : "t.t.", 0x24e, y);
    else if (kind == 1)
        fe_text(g_german ? "  d" : "h.h.", 0x24e, y);
}

/* fe_track_best 0x75288: the ten best and the top speed. Choices: 0 Done (back to
   Control Central), 1 the previous track, 2 the next, 3 the description (back to the
   track's screen), 4 none; the keys' moves 0x74f80 (left), 0x74f8a (right), the
   pointer's places 0x74f94, 0x74f9e. 1 when Done */
enum { B_DONE, B_PREV, B_NEXT, B_DESC, B_NONE };

static int best_times(void)
{
    static const char *const lit_name[4] = { "donl", "larl", "rarl", "desl" };
    static const char *const dark_name[4] = { "dond", "lard", "rard", "desd" };
    static const unsigned char black[768];
    int lit = B_DESC, was = B_DESC, redraw = 1, first = 1, changed = 0, done = 0, ready = 1;
    int released = 1, ret = 0, i, k, at, y;
    unsigned held = 0;
    char s[32];

    if (!g_mouse_active)
        place(0x74f94, 0x74f9e, B_DESC);
    if (!load(0x74fa8))
        return 0;
    if (g_mouse_active)
        fe_mouse_read();
    fe_mouse_save();

    while (!done) {
        int over = B_NONE;

        if (in_rect(0, 0x82, 0x1b3, 0x1e0))
            over = B_DONE;
        else if (in_rect(200, 0xfa, 0x1b3, 0x1e0))
            over = B_PREV;
        else if (in_rect(0x172, 0x1a4, 0x1b3, 0x1e0))
            over = B_NEXT;
        else if (in_rect(0x1e0, 0x280, 0x1b3, 0x1df))
            over = B_DESC;
        if (over != lit) {
            was = lit;
            lit = over;
            changed = 1;
        }

        if (redraw) {
            /* the page whole the first time, then only the choice lit */
            if (first) {
                draw("bgnd", 0);
                fe_style(1);
                for (i = 0; i < 10; i++) {
                    at = CFG_RECORDS + track() * CFG_TRACK_RECORDS + SESSION_TOP_TEN * CFG_RECORD
                       + (track() > 2 ? cw(CFG_RACE_LENGTH) * 10 * CFG_RECORD : 0) + i * CFG_RECORD;
                    y = 0x73 + i * 0x16;
                    fe_text_cut((const char *)g_config + at, 8, y, 0x10e);
                    fmt_time(s, config_long(at + 0x1b));
                    fe_text(s, 0x118, y);
                    record_row(at, y);
                }
                fe_style(1);
                at = CFG_RECORDS + track() * CFG_TRACK_RECORDS + SESSION_TOP_SPEED * CFG_RECORD;
                if (config_long(at + 0x17)) {
                    fe_text_cut((const char *)g_config + at, 8, 0x17c, 0x10e);
                    fmt_speed(s, config_long(at + 0x1f));
                    fe_text(s, 0x118, 0x17c);
                    record_row(at, 0x17c);
                }
            }
            if (lit < B_NONE)
                draw(lit_name[lit], 0);
            if (first)                     /* black until it fades up */
                video_palette(black, 0, 256);
            mouse_show(1);                 /* mouse_draw 0x52288 */
            if (first) {
                fe_mouse_save();
                fe_mouse_read();
                pal_fade_up(g_pal, 0, 256);
                inv_build();
                fe_styles_init();
                first = 0;
            }
            settimeout(12000);
            redraw = 0;
        }
        if (changed) {
            /* the choice left dark, the new one lit (the description's see-through) */
            fe_play_sound(0x47);
            video_retrace();
            redraw = 1;
            if (was < B_NONE)
                draw(dark_name[was], was == B_DESC);
            if (lit < B_NONE)
                draw(lit_name[lit], lit == B_DESC);
            changed = 0;
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
                switch (lit) {
                case B_DONE:
                    ret = done = 1;
                    break;
                case B_PREV:
                case B_NEXT:
                    step_track(lit == B_NEXT);
                    pal_fade_down(0, 256);
                    fsh_free(&g_sel);
                    load(0x74fa8);
                    redraw = first = 1;
                    break;
                case B_DESC:
                    done = 1;
                    break;
                }
            }
            ticksince();
            released = ready = 0;
            held = 0;
        } else if (k && ready) {
            if (k == 0x1b) {
                fe_play_sound(0x48);
                done = 1;
            } else if (k == 0x4b00 || k == 0x4d00) {
                fe_mouse_hide();
                was = lit;
                lit = tab(k == 0x4b00 ? 0x74f80 : 0x74f8a, lit);
                place(0x74f94, 0x74f9e, lit);
                changed = 1;
            } else if ((k == 0x4800 || k == 0x5000) && lit == B_NONE) {
                fe_mouse_hide();
                lit = was = B_DONE;
                place(0x74f94, 0x74f9e, lit);
                changed = 1;
            }
            ticksince();
            ready = 0;
            held = 0;
        } else if (!k)
            ready = 1;

        video_retrace();
        if (g_quit)
            done = 1;
        if (!done && timed_out(first, released))
            done = 1;
    }
    if (ret) {                             /* back to Control Central */
        music_fade_out();
        pal_fade_down(0, 256);
        music_wait_end();
    } else
        pal_fade_down(0, 256);
    fsh_free(&g_sel);
    fe_mouse_save();
    return ret;
}

/* The track's screen. Choices: 0 Done, 1 the previous track, 2 the next, 3 Best Times,
   4..6 an open road's segments, 7 a circuit's laps, 8 none, 9 the time of day; the keys'
   moves on the open roads 0x75054 (up), 0x75068 (down), 0x7507c (left), 0x75090
   (right), on the circuits 0x75004, 0x75018, 0x7502c, 0x75040; the pointer's places
   0x750a4, 0x750b8 */
enum { T_DONE, T_PREV, T_NEXT, T_BEST, T_SEG1, T_SEG2, T_SEG3, T_LAPS, T_NONE, T_TIME };

/* the time of day can be chosen but on Lost Vegas and the mirrored and rally tracks */
static int time_shown(void)
{
    return cw(CFG_TRACK) != 8 && !g_rally && !g_mirror;
}

static int time_of_day(void)
{
    int v = rec_get(REC_TOD + 4 * track());

    return v < 0 || v > 2 ? 1 : v;
}

int fe_track_screen(void)
{
    static const char *const lit_name[8] = { "donl", "prel", "nexl", "besl", "ssg1", "ssg2",
                                             "ssg3", "lapl" };
    static const unsigned char black[768];
    unsigned char saved[0xac];
    int lit = T_DONE, redraw = 1, first = 1, changed = 0, done = 0, ready = 1, released = 1;
    int segment = 1, best = 0, k, t, circuit;
    unsigned held = 0;
    char s[32];

    memcpy(saved, g_config + 9, sizeof saved);     /* for Esc */
    if (!g_mouse_active)
        place(0x750a4, 0x750b8, T_DONE);
    if (!load(0x750cc))
        return 0;
    if (g_mouse_active)
        fe_mouse_read();
    fe_mouse_save();

    while (!done) {
        int over;

        t = track();
        circuit = t > 2;
        /* the mouse over a choice (a segment's map by the track's places, 0x75234,
           0x75246, 0x751c8, 0x751fe; the circuits' are off the screen) */
        if (in_rect(0, 0x82, 0x1b3, 0x1e0))
            over = T_DONE;
        else if (in_rect(200, 0xfa, 0x1b3, 0x1e0))
            over = T_PREV;
        else if (in_rect(0x172, 0x1a4, 0x1b3, 0x1e0))
            over = T_NEXT;
        else if (in_rect(0x1e0, 0x280, 0x1b3, 0x1df))
            over = T_BEST;
        else if (in_rect(tab(0x75234, t), tab(0x75246, t), tab(0x751c8, 3 * t), tab(0x751fe, 3 * t)))
            over = T_SEG1;
        else if (in_rect(tab(0x75234, t), tab(0x75246, t), tab(0x751c8, 3 * t + 1),
                         tab(0x751fe, 3 * t + 1)))
            over = T_SEG2;
        else if (in_rect(tab(0x75234, t), tab(0x75246, t), tab(0x751c8, 3 * t + 2),
                         tab(0x751fe, 3 * t + 2)))
            over = T_SEG3;
        else if (in_rect(0, 0x140, 0x15e, 0x183) && circuit)
            over = T_LAPS;
        else if (in_rect(0, 0x140, 0x184, 0x1a0) && time_shown())
            over = T_TIME;
        else
            over = T_NONE;
        if (over != lit) {
            lit = over;
            changed = 1;
        }

        if (redraw) {
            draw("bgnd", 0);
            if (time_shown())              /* morning, midday or evening (0x74ff8) */
                draw(str(0x74ff8, time_of_day()), 1);
            /* the lengths: a circuit's and its laps (lit with them), or the segment's and
               the road's (miles 0x75168 and 0x75114, kilometres in German 0x751b0 and
               0x75180) */
            fe_style(0);
            if (circuit) {
                fe_text(str(g_german ? 0x751b0 : 0x75168, t - 3), 0xc8, 0x153);
                if (lit == T_LAPS)
                    fe_style(1);
                snprintf(s, sizeof s, "%d", cw(CFG_LAPS));
                fe_text(s, 0xdc, 0x16b);
            } else {
                fe_text(str(g_german ? 0x75180 : 0x75114, t * 4 + segment), 0xc8, 0x153);
                fe_text(str(g_german ? 0x75180 : 0x75114, t * 4), 0xaf, 0x16a);
                /* the segment's map lit, its picture */
                snprintf(s, sizeof s, "bsg%d", segment);
                draw(s, 0);
                snprintf(s, sizeof s, "ssg%d", segment);
                draw(s, 0);
            }
            if (lit < T_NONE)
                draw(lit_name[lit], 1);
            else if (lit == T_TIME && t != 8) {
                draw("timl", 1);
                draw(str(0x74fec, time_of_day()), 1);
            }
            if (first)                     /* black until it fades up */
                video_palette(black, 0, 256);
            mouse_show(1);                 /* mouse_draw 0x52288 */
            if (first) {
                fe_mouse_save();
                if (g_mouse_active)
                    fe_mouse_read();
                speech_say(str(0x750f0, t));   /* the track described (0x72370) */
                pal_fade_up(g_pal, 0, 256);
                inv_build();
                fe_styles_init();
                first = 0;
            }
            settimeout(12000);
            redraw = changed = 0;
        }
        if (changed) {
            redraw = 1;
            fe_play_sound(0x47);
            video_retrace();
            changed = 0;
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
                switch (lit) {
                case T_DONE:
                    done = 1;
                    break;
                case T_PREV:
                case T_NEXT:
                    step_track(lit == T_NEXT);
                    speech_stop();         /* 0x72454, 0x72490 */
                    pal_fade_down(0, 256);
                    fsh_free(&g_sel);
                    load(0x750cc);
                    redraw = first = 1;
                    break;
                case T_BEST:
                    speech_stop();
                    fsh_free(&g_sel);
                    pal_fade_down(0, 256);
                    if (best_times()) {
                        best = done = 1;
                        break;
                    }
                    load(0x750cc);
                    if (!g_mouse_active)
                        place(0x750a4, 0x750b8, lit);
                    redraw = first = 1;
                    break;
                case T_SEG1:
                case T_SEG2:
                case T_SEG3:
                    segment = lit - T_SEG1 + 1;
                    changed = 1;
                    break;
                case T_LAPS:               /* (not in a tournament) */
                    if (cw(CFG_RACE_TYPE) != 3) {
                        config_set_word(CFG_RACE_LENGTH, (cw(CFG_RACE_LENGTH) + 1) % 3);
                        set_laps();
                    }
                    changed = 1;
                    break;
                case T_TIME:               /* midday or the track's other (0x75144) */
                    if (t != 8) {
                        rec_put(REC_TOD + 4 * t,
                                time_of_day() == 1 ? exe_i32(0x75144 + 4 * (unsigned)t) : 1);
                        changed = 1;
                    }
                    break;
                }
            }
            ticksince();
            settimeout(12000);
            released = ready = 0;
            held = 0;
        } else if (k && ready) {
            if (k == 0x1b) {
                memcpy(g_config + 9, saved, sizeof saved);
                fe_play_sound(0x48);
                done = 1;
            } else if (k == 0x4800 || k == 0x5000 || k == 0x4b00 || k == 0x4d00) {
                fe_mouse_hide();
                if (k == 0x4800)
                    lit = tab(circuit ? 0x75004 : 0x75054, lit);
                else if (k == 0x5000)
                    lit = tab(circuit ? 0x75018 : 0x75068, lit);
                else if (k == 0x4b00)
                    lit = tab(circuit ? 0x7502c : 0x7507c, lit);
                else
                    lit = tab(circuit ? 0x75040 : 0x75090, lit);
                if ((k == 0x4800 || k == 0x5000) && lit == T_TIME && !time_shown())
                    lit = k == 0x5000 ? T_DONE : circuit ? T_LAPS : T_SEG1;
                place(0x750a4, 0x750b8, lit);
                changed = 1;
            }
            ticksince();
            ready = 0;
            held = 0;
        } else if (!k)
            ready = 1;

        video_retrace();
        if (g_quit)
            done = 1;
        if (!done && timed_out(first, released))
            done = 1;
    }
    if (!best) {
        speech_stop();
        music_fade_out();
        pal_fade_down(0, 256);
        music_wait_end();
        fsh_free(&g_sel);
    }
    fe_mouse_save();
    return 1;
}
