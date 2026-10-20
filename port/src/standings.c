/* The standings shown after a race (NOTES §4.21): fe_standings_segment 0x6a9d0. */
#include "standings.h"
#include "cardraw.h"
#include "config.h"
#include "drive.h"
#include "exe.h"
#include "fe.h"
#include "files.h"
#include "fsh.h"
#include "menu.h"
#include "session.h"
#include "sound.h"
#include "video.h"
#include <stdio.h>
#include <string.h>

static fsh_file g_art;

static void art(const char *name, int see_through)
{
    shape s;
    if (fsh_find(&g_art, name, &s))
        fe_draw_shape(&s, see_through);
}

/* the car's name, the menu's order (0xf7454: RX-7 .. DIABLO, PACK CLOCK CLASS WARRIOR) */
static const char *car_name(int n)
{
    const unsigned char *p = n >= 0 && n < 12 ? exe_at(0xf7454 + 4 * n, 4) : 0;
    unsigned a = p ? (unsigned)(p[0] | p[1] << 8 | p[2] << 16 | p[3] << 24) : 0;
    const unsigned char *s = a ? exe_at(a, 16) : 0;

    return s ? (const char *)s : "";
}

/* 0x717d0: the text cut to the first of its lengths from 7 characters on that is wider
   than w (or whole), with its shadow */
static void text_cut(const char *s, int x, int y, int w)
{
    char cut[50];
    int n;

    memset(cut, 0, sizeof cut);
    for (n = 7; n < 50; n++) {
        strncpy(cut, s, n);
        if (text_width(&g_fe_font, cut) > w)
            break;
    }
    fe_text(cut, x, y);
}

/* a record of the track's (CONFIG.DAT +0xb5 ..) */
static int record(int i)
{
    return CFG_RECORDS + config_word(CFG_TRACK) * CFG_TRACK_RECORDS + i * CFG_RECORD;
}

/* the race kind beside a record: German "  z", "  d", English "t.t.", and "h.h." (the
   speed's row "h.h") */
static void record_kind(int at, int y, int speed_row)
{
    int k = config_long(at + 0x23);

    if (k == 0)
        fe_text(g_german ? "  z" : "t.t.", 0x23f, y);
    else if (k == 1)
        fe_text(g_german ? "  d" : speed_row ? "h.h" : "h.h.", 0x23f, y);
}

/* everything but the Done button: the players' column, the rival's (or the record's in
   a time trial), the segment's best time and top speed records (white when just taken) */
static void draw_page(int took, int seg)
{
    static const char *const place_names[3] = { "city", "coas", "alpi" };
    const session_racer *me = &g_session.racer[g_player];
    const session_racer *them = &g_session.racer[g_session.rival];
    int type = config_word(CFG_RACE_TYPE), track = config_word(CFG_TRACK), at, t;
    const char *dnf = g_german ? "Abbruch" : "DNF";
    char s[64];

    art("bgnd", 0);
    if (track >= 0 && track <= 2)
        art(place_names[track], 0);
    fe_style(1);
    /* the current standing */
    if (g_session.result == 1) {
        if (type == 2 || type == 3) {
            if (g_drive.gave_up[g_player])
                fe_text(dnf, 0x131, 0x89);
            else {
                sprintf(s, "%d / 8", g_session.rank + 1);
                fe_text(s, 0x131, 0x89);
            }
            if (g_drive.gave_up[g_session.rival])
                fe_text(dnf, 0x1d6, 0x89);
            else {
                sprintf(s, "%d / 8", g_session.rival_rank + 1);
                fe_text(s, 0x1d6, 0x89);
            }
        } else if (type == 1) {
            if (g_drive.gave_up[g_player])
                fe_text(dnf, 0x131, 0x89);
            else if (me->time < g_session.racer[1 - g_player].time) {
                fe_text("1 / 2", 0x131, 0x89);
                fe_text("2 / 2", 0x1d6, 0x89);
            } else {
                fe_text("2 / 2", 0x131, 0x89);
                fe_text("1 / 2", 0x1d6, 0x89);
            }
        } else if (me->total > config_long(0x658 + track * CFG_TRACK_RECORDS + seg * 4)) {
            /* a time trial: against the segment's record time so far (+0x5a3 ..) */
            fe_text("2 / 2", 0x131, 0x89);
            fe_text("1 / 2", 0x1d6, 0x89);
        } else {
            fe_text("1 / 2", 0x131, 0x89);
            fe_text("2 / 2", 0x1d6, 0x89);
        }
    } else
        fe_text(dnf, 0x131, 0x89);
    sprintf(s, g_german ? "ETAPPE %d" : "SEGMENT %d", seg + 1);
    fe_text(s, 8, 0x5a);
    text_cut(me->name, 0x131, 0x5a, 0x96);
    fe_text(car_name(config_word(CFG_CAR)), 0x131, 0x6e);
    if (type == 0) {
        fe_text(g_german ? "Bestzeit" : "Best Time", 0x1d6, 0x5a);
        fe_text(car_name(config_long(record(6) + 0xf)), 0x1d6, 0x6e);
    } else {
        fe_text(them->name, 0x1d6, 0x5a);
        fe_text(car_name(type == 2 || type == 3 ? g_session.rival_car
                                                : config_word(CFG_OPPONENT)), 0x1d6, 0x6e);
    }
    /* the player's segment time, total, average and top speed */
    fe_style(1);
    fmt_time(s, me->time);
    fe_text(s, 0x131, 0x9b);
    fmt_time(s, g_session.gave_up[g_player] ? 0 : me->total);
    fe_text(s, 0x131, 0xad);
    if (g_drive.gave_up[g_player])
        fmt_average(s, 0, 0);
    else
        fmt_average(s, me->time, me->distance);
    fe_text(s, 0x131, 0xcb);
    fmt_speed(s, g_session.result == 1 && !g_drive.gave_up[g_player] ? me->top : 0);
    fe_text(s, 0x131, 0xdd);
    /* beside it the rival's (at the player's distance), or a time trial's record so far
       (+0x5a3: the segments' times added up, then their top speeds) */
    if (type == 0) {
        at = 0x658 + track * CFG_TRACK_RECORDS;
        t = seg == 0 ? config_long(at) : config_long(at + seg * 4) - config_long(at + seg * 4 - 4);
        fmt_time(s, t);
        fe_text(s, 0x1d6, 0x9b);
        fmt_time(s, config_long(at + seg * 4));
        fe_text(s, 0x1d6, 0xad);
        fmt_average(s, t, me->distance);
        fe_text(s, 0x1d6, 0xcb);
        fmt_speed(s, config_long(at + 0xc + seg * 4));
    } else {
        fmt_time(s, them->time);
        fe_text(s, 0x1d6, 0x9b);
        fmt_time(s, them->total);
        fe_text(s, 0x1d6, 0xad);
        fmt_average(s, them->time, me->distance);
        fe_text(s, 0x1d6, 0xcb);
        fmt_speed(s, them->top);
    }
    fe_text(s, 0x1d6, 0xdd);
    /* the segment's records: its best time, its top speed */
    at = record(seg);
    fe_style(took == 1 || took == 3 ? 3 : 1);
    text_cut((const char *)g_config + at, 7, 0x136, 200);
    fmt_time(s, config_long(at + 0x1b));
    fe_text(s, 0x131, 0x136);
    fe_text(car_name(config_long(at + 0xf)), 0x1d6, 0x136);
    record_kind(at, 0x136, 0);
    at = record(seg + 3);
    fe_style(took == 2 || took == 3 ? 3 : 1);
    text_cut((const char *)g_config + at, 7, 0x17c, 200);
    fmt_speed(s, config_long(at + 0x1f));
    fe_text(s, 0x131, 0x17c);
    fe_text(car_name(config_long(at + 0xf)), 0x1d6, 0x17c);
    record_kind(at, 0x17c, 1);
}

void standings_segment(int take_records)
{
    static const unsigned char black[768];
    unsigned char pal[768];
    char path[256];
    int took = 0, seg = session_segment(), off = 0, redraw = 1, first = 1, changed = 0;
    int done = 0, ready = 1, released = 1, k;
    unsigned held = 0;

    if (take_records)
        took = session_records_segment();
    /* (0x72600: two pages shown) the mouse on Done, the art, its palette faded up over
       what the screen holds */
    mouse_place(40, 450);
    snprintf(path, sizeof path, "%s/STATS/STANDSEG.QFS", art_dir());
    if (!fsh_load(&g_art, path) || !fsh_palette(&g_art, 0, pal)) {
        fprintf(stderr, "renfs: %s is missing\n", path);
        fsh_free(&g_art);
        return;
    }
    pal_fade_up(pal, 0, 256);
    fe_styles_init();

    while (!done) {
        int mx, my, on;

        /* Done lit while the mouse is on it */
        mouse_read(&mx, &my);
        on = mx >= 0 && mx <= 0x82 && my >= 0x1b3 && my <= 0x1e0;
        if (on && off) {
            off = 0;
            changed = 1;
        } else if (!on && !off) {
            off = 1;
            changed = 1;
        }
        if (redraw) {
            if (first)
                draw_page(took, seg);
            if (!off)
                art("donl", 0);
            if (first)
                video_palette(black, 0, 256);
            mouse_show(1);
            if (first) {
                fe_mouse_save();
                if (g_mouse_active)
                    fe_mouse_read();
                /* the commentary on a record taken (con4_B both, con1_A the time,
                   con2_A the speed), when the speech is on */
                if (took > 0)
                    speech_say(took == 3 ? "CON4_B16.EAS" : took == 2 ? "CON1_A16.EAS"
                                                                     : "CON2_A16.EAS");
                pal_fade_up(pal, 0, 256);
                first = 0;
            }
            settimeout(12000);
            redraw = changed = 0;
        }
        if (changed) {
            fe_play_sound(0x47);
            art(off ? "dond" : "donl", 0);
            settimeout(12000);
            redraw = 1;
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
        if (k == 0x20 || k == 0x0d || (g_mouse_buttons == 1 && released && !off)) {
            fe_mouse_press();
            if (ready) {
                fe_play_sound(0x48);
                break;
            }
            released = 0;
            held = 0;
            ticksince();
        } else if (k && ready) {
            if (k == 0x1b) {
                fe_play_sound(0x48);
                done = 1;
            } else if ((k == 0x4800 || k == 0x5000 || k == 0x4b00 || k == 0x4d00) && off) {
                /* the keys put the mouse on Done */
                off = 0;
                fe_mouse_hide();
                mouse_place(40, 450);
                fe_mouse_save();
                changed = 1;
            }
            ready = 0;
            held = 0;
            ticksince();
        } else if (!k)
            ready = 1;
        video_retrace();
        /* two minutes, or the right button, end it too */
        if (timedout() || (g_mouse_buttons == 2 && released)) {
            fe_play_sound(0x48);
            done = 1;
        }
        if (g_quit)
            done = 1;
    }
    if (config_word(CFG_SPEECH) && took > 0)
        speech_stop();
    pal_fade_down(0, 256);
    fsh_free(&g_art);
    fe_mouse_save();
}
