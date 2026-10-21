/* The cars' showcase (fe_showcase 0x17724, from Control Central's car pictures; NOTES
   §4.2): a car's main page (FRONTEND/SHOW/<car>.QFS: History, Video, General,
   Performance, Mechanical, the next or previous car, Done), the photo pages
   (fe_showcase_history 0x16290: History, General with its two frames of five slides) and
   the pictures with their figures (fe_showcase_specs 0x15008: Performance, Mechanical,
   three each), each with its spoken commentary. The tables (the keys' moves, the
   pointer's places, the cars' names) are the original's, read from its program. */
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "exe.h"
#include "fe.h"
#include "files.h"
#include "fsh.h"
#include "mainmenu.h"
#include "menu.h"
#include "racemenu.h"
#include "showcase.h"
#include "sound.h"
#include "video.h"

static fsh_file g_show;
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

/* the car's name in its files: 0x14fb8 (rx7 .. 512tr diablo) the main page's, 0x14f2c
   (rx7 .. 512 dia) the other pages' */
static const char *car_name(unsigned table)
{
    int car = cw(CFG_CAR);
    const char *s;

    if (car < 0 || car > 7)
        car = 0;
    s = (const char *)exe_at((unsigned)exe_i32(table + 4 * (unsigned)car), 1);
    return s ? s : "";
}

/* the page's file (FRONTEND/SHOW, GSHOW in German: 0x1565b4) and its palette */
static int load(const char *name, const char *kind)
{
    char path[256];

    snprintf(path, sizeof path, "FRONTEND/%s/%s%s.qfs", g_german ? "GSHOW" : "SHOW", name, kind);
    if (!fsh_load(&g_show, path) || !fsh_palette(&g_show, 0, g_pal)) {
        fprintf(stderr, "renfs: %s is missing\n", path);
        fsh_free(&g_show);
        return 0;
    }
    return 1;
}

static void draw(const char *name, int see_through)
{
    shape s;

    if (fsh_find(&g_show, name, &s))
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

/* the next or the previous car: in a tournament inside the track's class (the main
   page's middle class has tracks 1, 4 and 5, the other pages' 1 and 4) */
static void step_car(int next, int main_page)
{
    int car = cw(CFG_CAR), track = cw(CFG_TRACK);

    car += next ? 1 : -1;
    if (cw(CFG_RACE_TYPE) == 3) {
        int lo, hi;

        if (track == 0 || track == 3)
            lo = 0, hi = 2;
        else if (track == 1 || track == 4 || (main_page && track == 5))
            lo = 3, hi = 5;
        else
            lo = 6, hi = 7;
        if (next && car > hi)
            car = lo;
        else if (!next && car < lo)
            car = hi;
    } else if (next)
        car %= 8;
    else if (car < 0)
        car = 7;
    config_set_word(CFG_CAR, car);
    fe_car_setup();                        /* car_setup 0x73a80 */
}

/* the spoken commentary: <car><kind>16.eas from FRONTEND/SPEECH (0x72370, 0x72438) */
static void commentary(const char *kind)
{
    char name[32];

    snprintf(name, sizeof name, "%s%s16.eas", car_name(0x14f2c), kind);
    speech_say(name);
}

/* a page's other car: the commentary stopped (0x72454, 0x72490), the screen faded, the
   new car's file */
static void page_car(int next, const char *kind)
{
    step_car(next, 0);
    speech_stop();
    pal_fade_down(0, 256);
    fsh_free(&g_show);
    load(car_name(0x14f2c), kind);
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

/* fe_showcase_history 0x16290: History ("his": its text and the buttons) or General
   ("gen": the figures, a small frame (2) and a large one (1) of five slides each, with
   arrows). Choices: 0 frame 2, 1 frame 1, 2 Done, 3 the previous car, 4 the next, 5 none;
   the keys' moves 0x14f14 (up), 0x14f20 (down), the pointer's places 0x14efc, 0x14f08 */
enum { H_FRAME2, H_FRAME1, H_DONE, H_PREV, H_NEXT, H_NONE };

static void photo_slide(int *slide, int step)
{
    *slide = step > 0 ? (*slide + 1) % 5 : *slide - 1 < 0 ? 4 : *slide - 1;
}

static int page_photos(const char *kind)
{
    static const char *const lit_name[5] = { "fr2l", "fr1l", "donl", "prel", "nexl" };
    int gen = !strcmp(kind, "gen");
    int lit = gen ? H_FRAME2 : H_DONE, arrow = 0, slide[2] = { 0, 0 };
    int redraw = 1, first = 1, changed = 0, done = 0, ready = 1, released = 1, k;
    unsigned held = 0;
    char name[8];

    if (!g_mouse_active) {
        lit = H_DONE;
        place(0x14efc, 0x14f08, H_DONE);
    }
    if (!load(car_name(0x14f2c), kind))
        return 0;
    if (g_mouse_active)
        fe_mouse_read();
    fe_mouse_save();

    while (!done) {
        /* the mouse over a frame (General only), its arrows, a button */
        int over = H_NONE, over_arrow = arrow;

        if (gen && in_rect(0x174, 0x254, 0x6f, 0xe7)) {
            over = H_FRAME2;
            over_arrow = in_rect(0x174, 0x18d, 0x9e, 0xb9) ? 1 : in_rect(0x23a, 0x253, 0x9e, 0xba) ? 2 : 0;
        } else if (gen && in_rect(0xc1, 0x25f, 0xef, 0x1a6)) {
            over = H_FRAME1;
            over_arrow = in_rect(0xc3, 0xdc, 0x140, 0x155) ? 1 : in_rect(0x244, 0x25d, 0x140, 0x155) ? 2 : 0;
        } else if (in_rect(0, 0x82, 0x1ae, 0x1e0))
            over = H_DONE;
        else if (in_rect(200, 0x104, 0x1ae, 0x1e0))
            over = H_PREV;
        else if (in_rect(0x168, 0x1a4, 0x1ae, 0x1e0))
            over = H_NEXT;
        if (over != lit) {
            lit = over;
            changed = 1;
        }
        if (over_arrow != arrow) {
            arrow = over_arrow;
            changed = 1;
        }

        if (redraw) {
            draw("bgnd", 0);
            if (gen) {
                snprintf(name, sizeof name, "f2s%d", slide[0]);
                draw(name, 0);
                snprintf(name, sizeof name, "f1s%d", slide[1]);
                draw(name, 0);
            }
            if (lit <= H_FRAME1) {
                draw(lit_name[lit], 1);
                if (arrow)
                    draw(lit == H_FRAME2 ? (arrow == 1 ? "f2lf" : "f2rt")
                                         : (arrow == 1 ? "f1lf" : "f1rt"), 1);
            } else if (lit < H_NONE)
                draw(lit_name[lit], 0);
            mouse_show(1);                 /* mouse_draw 0x52288 */
            g_mouse_active = 1;
            if (first) {
                fe_mouse_save();
                if (g_mouse_active)
                    fe_mouse_read();
                pal_fade_up(g_pal, 0, 256);
                commentary(kind);
                first = 0;
            }
            settimeout(12000);
            redraw = 0;
        }
        if (changed) {                     /* (the old choice dark, the new one lit: drawn
                                              over by the redraw before they are seen) */
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
                case H_FRAME2:             /* a click on an arrow: the next or last slide */
                    if (in_rect(0x23a, 0x253, 0x9e, 0xb9))
                        photo_slide(&slide[0], 1), redraw = 1;
                    else if (in_rect(0x174, 0x18d, 0x9e, 0xb9))
                        photo_slide(&slide[0], -1), redraw = 1;
                    break;
                case H_FRAME1:
                    if (in_rect(0x244, 0x25d, 0x140, 0x155))
                        photo_slide(&slide[1], 1), redraw = 1;
                    else if (in_rect(0xc3, 0xdc, 0x140, 0x155))
                        photo_slide(&slide[1], -1), redraw = 1;
                    break;
                case H_DONE:
                    done = 1;
                    break;
                case H_PREV:
                case H_NEXT:
                    page_car(lit == H_NEXT, kind);
                    redraw = first = 1;
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
            } else if (k == 0x4800 || k == 0x5000) {
                /* up, down: from none to Done; between the frames and the buttons on
                   General only */
                if (lit == H_NONE || gen) {
                    fe_mouse_hide();
                    lit = lit == H_NONE ? H_DONE : tab(k == 0x4800 ? 0x14f14 : 0x14f20, lit);
                    place(0x14efc, 0x14f08, lit);
                    changed = 1;
                }
            } else if (k == 0x4b00 || k == 0x4d00) {
                /* left, right: a frame's slides, else along the buttons, round */
                int right = k == 0x4d00;

                fe_mouse_hide();
                if (lit == H_NONE) {
                    lit = H_DONE;
                    changed = 1;
                } else if (lit <= H_FRAME1) {
                    fe_play_sound(0x48);
                    photo_slide(&slide[lit], right ? 1 : -1);
                    redraw = 1;
                } else {
                    lit += right ? 1 : -1;
                    if (lit > H_NEXT)
                        lit = H_DONE;
                    else if (lit < H_DONE)
                        lit = H_NEXT;
                    changed = 1;
                }
                place(0x14efc, 0x14f08, lit);
            }
            ticksince();
            ready = 0;
            held = 0;
        } else if (!k)
            ready = 1;

        video_retrace();                   /* (fe_service_music 0x71c20) */
        if (g_quit)
            done = 1;
        if (!done && timed_out(first, released))
            done = 1;
    }
    speech_stop();
    pal_fade_down(0, 256);
    fsh_free(&g_show);
    fe_mouse_save();
    return done;
}

/* fe_showcase_specs 0x15008: Performance ("per": a picture and its figures, three of
   each, two in German) or Mechanical ("mec": three pictures). Choices: 0 the frame, 1
   Done, 2 the previous car, 3 the next, 4 none; the keys' moves 0x14ea4 (up), 0x14eae
   (down), the pointer's places 0x14e90, 0x14e9a; the pictures 0x14ed8 and 0x14ef0
   (acc1 bra1 han1, accl brak hand), 0x14ee4 (engn driv chas) */
enum { S_FRAME, S_DONE, S_PREV, S_NEXT, S_NONE };

static void spec_slide(int *slide, int step, int per)
{
    if (step > 0)
        *slide = per && g_german ? *slide ^ 1 : (*slide + 1) % 3;
    else {
        *slide = *slide - 1 < 0 ? 2 : *slide - 1;
        if (per && g_german && *slide == 2)
            *slide = 1;
    }
}

static const char *spec_picture(unsigned table, int slide)
{
    const char *s = (const char *)exe_at((unsigned)exe_i32(table + 4 * (unsigned)slide), 1);

    return s ? s : "";
}

static int page_specs(const char *kind)
{
    static const char *const lit_name[4] = { "frml", "donl", "prel", "nexl" };
    int mec = !strcmp(kind, "mec"), per = !mec;
    int lit = S_FRAME, arrow = 0, slide = 0;
    int redraw = 1, first = 1, changed = 0, done = 0, ready = 1, released = 1, k;
    unsigned held = 0;
    /* the frame, its arrows, Done: Mechanical's, Performance's */
    const int frame[2][4] = { { 0xc, 0x268, 0x61, 0x191 }, { 0x22, 0x254, 0x68, 0x14d } };
    const int left[2][4] = { { 0xe, 0x2d, 0xe1, 0x10e }, { 0x23, 0x3e, 0xc8, 0xeb } };
    const int right[2][4] = { { 0x244, 0x266, 0xe1, 0x10e }, { 0x235, 0x253, 0xc8, 0xeb } };
    const int done_x1[2] = { 0x82, 0x69 };
    const int *f = frame[per], *l = left[per], *r = right[per];

    if (!g_mouse_active) {
        lit = S_DONE;
        place(0x14e90, 0x14e9a, S_DONE);
    }
    if (!load(car_name(0x14eb8), kind))
        return 0;
    if (g_mouse_active)
        fe_mouse_read();
    fe_mouse_save();

    while (!done) {
        int over = S_NONE, over_arrow = arrow;

        if (in_rect(f[0], f[1], f[2], f[3])) {
            over = S_FRAME;
            over_arrow = in_rect(l[0], l[1], l[2], l[3]) ? 1 : in_rect(r[0], r[1], r[2], r[3]) ? 2 : 0;
        } else if (in_rect(0, done_x1[per], 0x1ae, 0x1e0))
            over = S_DONE;
        else if (in_rect(200, 0x104, 0x1ae, 0x1e0))
            over = S_PREV;
        else if (in_rect(0x168, 0x1a4, 0x1ae, 0x1e0))
            over = S_NEXT;
        if (over != lit) {
            lit = over;
            changed = 1;
        }
        if (over_arrow != arrow) {
            arrow = over_arrow;
            changed = 1;
        }

        if (redraw) {
            draw("bgnd", 0);
            if (mec)
                draw(spec_picture(0x14ee4, slide), 0);
            else {
                draw(spec_picture(0x14ed8, slide), 0);
                draw(spec_picture(0x14ef0, slide), 0);
            }
            if (lit == S_FRAME) {
                draw("frml", 1);
                if (arrow)
                    draw(arrow == 1 ? "left" : "righ", 1);
            } else if (lit < S_NONE)
                draw(lit_name[lit], 0);
            mouse_show(1);                 /* mouse_draw 0x52288 */
            g_mouse_active = 1;
            if (first) {
                fe_mouse_save();
                if (g_mouse_active)
                    fe_mouse_read();
                pal_fade_up(g_pal, 0, 256);
                commentary(kind);
                first = 0;
            }
            settimeout(12000);
            redraw = 0;
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
                case S_FRAME:
                    if (in_rect(r[0], r[1], r[2], r[3]))
                        spec_slide(&slide, 1, per), redraw = 1;
                    else if (in_rect(l[0], l[1], l[2], l[3]))
                        spec_slide(&slide, -1, per), redraw = 1;
                    break;
                case S_DONE:
                    done = 1;
                    break;
                case S_PREV:
                case S_NEXT:
                    page_car(lit == S_NEXT, kind);
                    redraw = first = 1;
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
            } else if (k == 0x4800 || k == 0x5000) {
                fe_mouse_hide();
                lit = tab(k == 0x4800 ? 0x14ea4 : 0x14eae, lit);
                place(0x14e90, 0x14e9a, lit);
                changed = 1;
            } else if (k == 0x4b00 || k == 0x4d00) {
                int to_right = k == 0x4d00;

                fe_mouse_hide();
                if (lit == S_NONE) {
                    lit = S_DONE;
                    changed = 1;
                } else if (lit == S_FRAME) {
                    fe_play_sound(0x48);
                    spec_slide(&slide, to_right ? 1 : -1, per);
                    redraw = 1;
                } else {
                    lit += to_right ? 1 : -1;
                    if (lit > S_NEXT)
                        lit = S_DONE;
                    else if (lit < S_DONE)
                        lit = S_NEXT;
                    changed = 1;
                }
                place(0x14e90, 0x14e9a, lit);
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
    speech_stop();
    pal_fade_down(0, 256);
    fsh_free(&g_show);
    fe_mouse_save();
    return done;
}

/* the main page. Choices: 0 History, 1 Video, 2 General, 3 Performance, 4 Mechanical,
   5 the next car, 6 the previous, 7 Done, 8 none; the keys' moves 0x14f4c (up), 0x14f5e
   (down), 0x14f70 (left), 0x14f82 (right), the pointer's places 0x14f94, 0x14fa6 */
enum { M_HISTORY, M_VIDEO, M_GENERAL, M_PERFORMANCE, M_MECHANICAL, M_NEXT, M_PREV, M_DONE, M_NONE };

/* a page of its own, the main page's file let go meanwhile and loaded again after */
static void main_page(int which)
{
    fsh_free(&g_show);
    pal_fade_down(0, 256);
    if (which == M_HISTORY)
        page_photos("his");
    else if (which == M_GENERAL)
        page_photos("gen");
    else
        page_specs(which == M_PERFORMANCE ? "per" : "mec");
    load(car_name(0x14fb8), "");
}

int fe_showcase(void)
{
    static const char *const lit_name[8] = { "hisl", "vidl", "genl", "perl", "engl", "nexl",
                                             "prel", "donl" };
    static const unsigned char black[768];
    int lit = M_VIDEO, redraw = 1, first = 1, changed = 0, done = 0, ready = 1, released = 1, k;
    unsigned held = 0;
    char name[32];

    if (!g_mouse_active)
        place(0x14f94, 0x14fa6, M_VIDEO);
    if (!load(car_name(0x14fb8), ""))
        return 0;
    /* the screen cleared (0x72600) and the page's palette set at once (0x83fe4) */
    memset(g_fb, 0, (size_t)g_fb_w * g_fb_h);
    video_palette(g_pal, 0, 256);
    inv_build();                           /* 0x8227c */
    fe_styles_init();
    if (g_mouse_active)
        fe_mouse_read();
    fe_mouse_save();

    while (!done) {
        int over = M_NONE;

        if (in_rect(0x134, 0x1bc, 0xd1, 0x141))
            over = M_VIDEO;
        else if (in_rect(0x1c3, 0x267, 0xf6, 0x16e))
            over = M_HISTORY;
        else if (in_rect(0x86, 0x12c, 0xa4, 0x119))
            over = M_GENERAL;
        else if (in_rect(0x134, 0x212, 0x4e, 0xcb))
            over = M_PERFORMANCE;
        else if (in_rect(0x52, 0x14f, 0x146, 0x19c))
            over = M_MECHANICAL;
        else if (in_rect(200, 0x104, 0x1ae, 0x1e0))
            over = M_PREV;
        else if (in_rect(0x168, 0x1a4, 0x1ae, 0x1e0))
            over = M_NEXT;
        else if (in_rect(0, 0x82, 0x1ae, 0x1e0))
            over = M_DONE;
        if (over != lit) {
            lit = over;
            changed = 1;
        }

        if (redraw) {
            draw("bgnd", 0);
            /* the choice lit (the video's only with the films, 0x15d120: always here) */
            if (lit < M_NONE)
                draw(lit_name[lit], lit <= M_MECHANICAL);
            if (first)                     /* (0x84080) black until it fades up */
                video_palette(black, 0, 256);
            mouse_show(1);                 /* mouse_draw 0x52288 */
            g_mouse_active = 1;
            if (first) {
                fe_mouse_save();
                if (g_mouse_active)
                    fe_mouse_read();
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
                case M_HISTORY:
                case M_GENERAL:
                case M_PERFORMANCE:
                case M_MECHANICAL:
                    main_page(lit);
                    place(0x14f94, 0x14fa6, lit);
                    redraw = first = 1;
                    break;
                case M_VIDEO:              /* the car's film (0x341a4), its music again */
                    music_fade_out();
                    pal_fade_down(0, 256);
                    music_wait_end();
                    snprintf(name, sizeof name, "%s.tgv", car_name(0x14fb8));
                    fe_film(name);
                    music_start("showcase.asf");
                    redraw = first = 1;
                    place(0x14f94, 0x14fa6, lit);
                    break;
                case M_NEXT:
                case M_PREV:
                    step_car(lit == M_NEXT, 1);
                    pal_fade_down(0, 256);
                    fsh_free(&g_show);
                    load(car_name(0x14fb8), "");
                    redraw = first = 1;
                    break;
                case M_DONE:
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
            } else if (k == 0x4800 || k == 0x5000 || k == 0x4b00 || k == 0x4d00) {
                fe_mouse_hide();
                lit = tab(k == 0x4800 ? 0x14f4c : k == 0x5000 ? 0x14f5e : k == 0x4b00 ? 0x14f70
                                                                                    : 0x14f82, lit);
                place(0x14f94, 0x14fa6, lit);
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
    music_fade_out();
    pal_fade_down(0, 256);
    music_wait_end();
    fsh_free(&g_show);
    fe_mouse_save();
    return done;
}
