/* The standings shown after a race (NOTES §4.21): fe_standings_segment 0x6a9d0,
   fe_standings_track 0x6b938, fe_standings_best 0x69f8c, fe_standings_final 0x6f070,
   a circuit's fe_standings_loop1 0x6cb24 with its laps 0x6e1ac. */
#include "standings.h"
#include "cardraw.h"
#include "config.h"
#include "drive.h"
#include "exe.h"
#include "fe.h"
#include "files.h"
#include "font.h"
#include "fsh.h"
#include "mainmenu.h"
#include "menu.h"
#include "session.h"
#include "sound.h"
#include "video.h"
#include <stdio.h>
#include <string.h>

static fsh_file g_art;
static int g_said;             /* 0x14fb38: the commentary heard once (its second words
                                  after) */
/* what the page shows: the records just taken, the segment; the place in the ten best
   (20 none), the top speed's record taken */
static int g_took, g_seg, g_place, g_speed;

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

/* a record of the track's (CONFIG.DAT +0xb5 ..) */
static int record(int i)
{
    return CFG_RECORDS + config_word(CFG_TRACK) * CFG_TRACK_RECORDS + i * CFG_RECORD;
}

/* the race kind beside a record: German "  z", "  d", English "t.t.", "h.h." (the
   segment's speed row "h.h", the track's speed row "t.t." for both) */
static void record_kind(int at, int y, int speed_row)
{
    int k = config_long(at + 0x23);

    if (k == 0)
        fe_text(g_german ? "  z" : "t.t.", 0x23f, y);
    else if (k == 1)
        fe_text(g_german ? "  d" : speed_row == 1 ? "h.h" : speed_row == 2 ? "t.t." : "h.h.",
                0x23f, y);
}

/* everything but the Done button: the players' column, the rival's (or the record's in
   a time trial), the segment's best time and top speed records (white when just taken) */
static void draw_segment(void)
{
    int took = g_took, seg = g_seg;
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
    fe_text_cut(me->name, 0x131, 0x5a, 0x96);
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
    fe_text_cut((const char *)g_config + at, 7, 0x136, 200);
    fmt_time(s, config_long(at + 0x1b));
    fe_text(s, 0x131, 0x136);
    fe_text(car_name(config_long(at + 0xf)), 0x1d6, 0x136);
    record_kind(at, 0x136, 0);
    at = record(seg + 3);
    fe_style(took == 2 || took == 3 ? 3 : 1);
    fe_text_cut((const char *)g_config + at, 7, 0x17c, 200);
    fmt_speed(s, config_long(at + 0x1f));
    fe_text(s, 0x131, 0x17c);
    fe_text(car_name(config_long(at + 0xf)), 0x1d6, 0x17c);
    record_kind(at, 0x17c, 1);
}

/* a page with its Done button (the loop the standings share): the mouse put on Done,
   the art's palette faded up over what the screen holds, the page drawn on black and
   faded up with the commentary; Done lit while the mouse is on it; until Done, Enter,
   Space, Esc, the right button or two minutes */
typedef struct {
    const char *file;          /* STATS/... */
    void (*draw)(void);        /* everything but Done */
    const char *say;           /* the commentary, or none */
    int toggle;                /* the arrows move the mouse between Done and the middle
                                  (else only onto Done) */
    int dond_see_through;
    int black_first;           /* the screen black at once (else the art's palette faded
                                  up over what it holds) */
    const char *lit, *dark;    /* Done's pictures (donl, dond) */
} page;

static void page_show(const page *p)
{
    static const int mx_at[2] = { 40, 320 }, my_at[2] = { 450, 240 };
    static const unsigned char black[768];
    unsigned char pal[768];
    char path[256];
    int off = 0, redraw = 1, first = 1, changed = 0, done = 0, ready = 1, released = 1, k;
    unsigned held = 0;

    mouse_place(mx_at[0], my_at[0]);
    snprintf(path, sizeof path, "%s/STATS/%s", art_dir(), p->file);
    if (!fsh_load(&g_art, path) || !fsh_palette(&g_art, 0, pal)) {
        fprintf(stderr, "renfs: %s is missing\n", path);
        fsh_free(&g_art);
        return;
    }
    if (p->black_first) {
        video_palette(pal, 0, 256);
        inv_build();
        fe_styles_init();
        video_palette(black, 0, 256);
    } else {
        pal_fade_up(pal, 0, 256);
        fe_styles_init();
    }

    while (!done) {
        int mx, my, on;

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
                p->draw();
            if (!off)
                art(p->lit ? p->lit : "donl", 0);
            if (first)
                video_palette(black, 0, 256);
            mouse_show(1);
            if (first) {
                fe_mouse_save();
                if (g_mouse_active)
                    fe_mouse_read();
                if (p->say)
                    speech_say(p->say);
                pal_fade_up(pal, 0, 256);
                first = 0;
            }
            settimeout(12000);
            redraw = changed = 0;
        }
        if (changed) {
            fe_play_sound(0x47);
            art(off ? (p->dark ? p->dark : "dond") : (p->lit ? p->lit : "donl"),
                off && p->dond_see_through);
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
            } else if ((k == 0x4800 || k == 0x5000 || k == 0x4b00 || k == 0x4d00)
                       && (p->toggle || off)) {
                fe_mouse_hide();
                off = p->toggle ? off ^ 1 : 0;
                mouse_place(mx_at[off], my_at[off]);
                fe_mouse_save();
                changed = 1;
            }
            ready = 0;
            held = 0;
            ticksince();
        } else if (!k)
            ready = 1;
        video_retrace();
        if (timedout() || (g_mouse_buttons == 2 && released)) {
            fe_play_sound(0x48);
            done = 1;
        }
        if (g_quit)
            done = 1;
    }
    if (p->say)
        speech_stop();
    pal_fade_down(0, 256);
    fsh_free(&g_art);
    fe_mouse_save();
}

void standings_segment(int take_records)
{
    page p = { "STANDSEG.QFS", draw_segment, 0, 0, 0, 0, 0, 0 };

    g_said = 0;
    g_seg = session_segment();
    g_took = take_records ? session_records_segment() : 0;
    /* the commentary on a record taken (con4_B both, con1_A the time, con2_A the
       speed), when the speech is on */
    if (g_took > 0 && config_word(CFG_SPEECH)) {
        g_said = 1;
        p.say = g_took == 3 ? "CON4_B16.EAS" : g_took == 2 ? "CON1_A16.EAS" : "CON2_A16.EAS";
    }
    page_show(&p);
}

/* the player's and the rival's column of the track's standings (or a time trial's
   record's) */
static void draw_track(void)
{
    static const char *const place_names[3] = { "city", "coas", "alpi" };
    const session_racer *me = &g_session.racer[g_player];
    const session_racer *them = &g_session.racer[g_session.rival];
    int type = config_word(CFG_RACE_TYPE), track = config_word(CFG_TRACK), at;
    const char *dnf = g_german ? "Abbruch" : "DNF";
    char s[64];

    art("bgnd", 0);
    if (track >= 0 && track <= 2)
        art(place_names[track], 0);
    fe_style(1);
    /* the standing over the whole track */
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
        } else if (type == 1 && g_drive.gave_up[g_player])
            fe_text(dnf, 0x131, 0x89);
        else if (type == 1 ? me->total < g_session.racer[1 - g_player].total
                           : me->total <= config_long(record(SESSION_TOP_TEN) + 0x1b)) {
            fe_text("1 / 2", 0x131, 0x89);
            fe_text("2 / 2", 0x1d6, 0x89);
        } else {
            fe_text("2 / 2", 0x131, 0x89);
            fe_text("1 / 2", 0x1d6, 0x89);
        }
    } else
        fe_text(dnf, 0x131, 0x89);
    fe_text_cut(me->name, 0x131, 0x5a, 0x96);
    fe_text(car_name(config_word(CFG_CAR)), 0x131, 0x6e);
    if (type == 0) {
        fe_text(g_german ? "Bestzeit" : "Best Time", 0x1d6, 0x5a);
        fe_text(car_name(config_long(record(SESSION_TOP_TEN) + 0xf)), 0x1d6, 0x6e);
    } else if (type == 2 || type == 3) {
        fe_text(them->name, 0x1d6, 0x5a);
        fe_text(car_name(g_session.rival_car), 0x1d6, 0x6e);
    } else {
        fe_text(g_session.racer[1 - g_player].name, 0x1d6, 0x5a);
        fe_text(car_name(config_word(CFG_OPPONENT)), 0x1d6, 0x6e);
    }
    /* the player's total, average and best top speed */
    fe_style(1);
    fmt_time(s, g_session.gave_up[g_player] ? 0 : me->total);
    fe_text(s, 0x131, 0x9d);
    if (g_session.gave_up[g_player])
        fmt_average(s, 0, 0);
    else
        fmt_average(s, me->total, me->total_distance);
    fe_text(s, 0x131, 0xbc);
    fmt_speed(s, g_session.result == 1 && !g_drive.gave_up[g_player] ? me->best_top : 0);
    fe_text(s, 0x131, 0xd0);
    /* beside them the best total's (a time trial) or the rival's (at the player's
       distance) */
    if (type == 0) {
        at = record(SESSION_TOP_TEN);
        fmt_time(s, config_long(at + 0x1b));
        fe_text(s, 0x1d6, 0x9d);
        fmt_average(s, config_long(at + 0x1b), me->total_distance);
        fe_text(s, 0x1d6, 0xbc);
        fmt_speed(s, config_long(at + 0x1f));
    } else {
        fmt_time(s, them->total);
        fe_text(s, 0x1d6, 0x9d);
        fmt_average(s, them->total, me->total_distance);
        fe_text(s, 0x1d6, 0xbc);
        fmt_speed(s, them->best_top);
    }
    fe_text(s, 0x1d6, 0xd0);
    /* the track's best total and top speed records (white when just taken; the speed's
       race kind "t.t." in English whatever it is) */
    at = record(SESSION_TOP_TEN);
    fe_style(g_place == 0 ? 3 : 1);
    fe_text_cut((const char *)g_config + at, 7, 0x11d, 200);
    fmt_time(s, config_long(at + 0x1b));
    fe_text(s, 0x131, 0x11d);
    fe_text(car_name(config_long(at + 0xf)), 0x1d6, 0x11d);
    record_kind(at, 0x11d, 0);
    at = record(SESSION_TOP_SPEED);
    fe_style(g_speed == 1 ? 3 : 1);
    fe_text_cut((const char *)g_config + at, 7, 0x177, 200);
    fmt_speed(s, config_long(at + 0x1f));
    fe_text(s, 0x131, 0x177);
    fe_text(car_name(config_long(at + 0xf)), 0x1d6, 0x177);
    record_kind(at, 0x177, 2);
}

/* the commentary on the track's standings: the records taken, and the player leading,
   at a tournament's end too; once heard, its second words */
static const char *track_comment(int take)
{
    int both = g_speed == 1 && g_place == 0, rec = g_speed == 1 || g_place == 0;
    int speech = config_word(CFG_SPEECH), raced = 0, i, again = g_said;
    const char *s = 0;

    if (g_session.rank == 0 && config_word(CFG_RACE_TYPE) != 0) {
        int most = 1;

        for (i = 0; i < 8; i++)
            raced += g_session.raced[i];
        for (i = 0; i < 8; i++)
            if (g_session.points[i] > g_session.points[g_player])
                most = 0;
        if (((g_vegas_ok && raced + g_session.raced[8] == 9) || (!g_vegas_ok && raced == 8))
            && most) {
            /* the tournament over, and won */
            if (speech && rec)
                s = again ? (both ? "CON8_H16.EAS" : g_speed == 1 ? "CON6_F16.EAS" : "CON7_H16.EAS")
                          : (both ? "CON8_D16.EAS" : g_speed == 1 ? "CON6_C16.EAS" : "CON7_D16.EAS");
            else if (take)
                s = again ? "CON5_D16.EAS" : "CON5_B16.EAS";
        } else if (speech && rec)
            s = again ? (both ? "CON8_G16.EAS" : g_speed == 1 ? "CON6_E16.EAS" : "CON7_G16.EAS")
                      : (both ? "CON8_A16.EAS" : g_speed == 1 ? "CON6_A16.EAS" : "CON7_A16.EAS");
        else if (take)
            s = again ? "CON5_C16.EAS" : "CON5_A16.EAS";
    } else if (speech && rec)
        s = again ? (both ? "CON4_C16.EAS" : g_speed == 1 ? "CON1_C16.EAS" : "CON2_D16.EAS")
                  : (both ? "CON4_F16.EAS" : g_speed == 1 ? "CON1_B16.EAS" : "CON2_B16.EAS");
    if (s)
        g_said = 1;
    return s;
}

/* the ten best totals of the track (a circuit's of its race length), the player's place
   white; under them the track's top speed */
static void draw_best(void)
{
    static const char *const names[9] = { "city", "coas", "alpi", "rust", "autu", "burt",
                                          "mont", "trop", "neod" };
    int track = config_word(CFG_TRACK), first = SESSION_TOP_TEN, i, at, y;
    char s[64];

    art("bgnd", 0);
    if (track >= 0 && track < 9)
        art(names[track], 0);
    if (track > 2)
        first += config_word(CFG_RACE_LENGTH) * 10;
    fe_style(1);
    for (i = 0; i < 10; i++) {
        at = record(first + i);
        y = 0x82 + i * 0x15;
        if (i == g_place)
            fe_style(3);
        fe_text_cut((const char *)g_config + at, 7, y, 0x11d);
        fmt_time(s, config_long(at + 0x1b));
        fe_text(s, 0x131, y);
        fe_text(car_name(config_long(at + 0xf)), 0x1d6, y);
        record_kind(at, y, 0);
        if (i == g_place)
            fe_style(1);
    }
    at = record(SESSION_TOP_SPEED);
    fe_style(g_speed ? 3 : 1);
    fe_text_cut((const char *)g_config + at, 7, 0x177, 0x11d);
    fmt_speed(s, config_long(at + 0x1f));
    fe_text(s, 0x131, 0x177);
    fe_text(car_name(config_long(at + 0xf)), 0x1d6, 0x177);
    record_kind(at, 0x177, 0);
}

/* fe_standings_best 0x69f8c (stats/standbst.qfs): the commentary when the player is
   among the ten (con4_G/E once said before, con4_D/A: with the top speed or without) */
static void standings_best(void)
{
    page p = { "STANDBST.QFS", draw_best, 0, 1, 1, 0, 0, 0 };

    if (config_word(CFG_SPEECH) && g_place < 10)
        p.say = g_said ? (g_speed ? "CON4_G16.EAS" : "CON4_E16.EAS")
                       : (g_speed ? "CON4_D16.EAS" : "CON4_A16.EAS");
    page_show(&p);
}

void standings_track(int take_records)
{
    page p = { "STANDTRK.QFS", draw_track, 0, 1, 0, 0, 0, 0 };

    g_place = 20;
    g_speed = 20;
    if (take_records) {
        g_place = session_top_ten();
        g_speed = session_records_top_speed();
    }
    p.say = track_comment(take_records);
    page_show(&p);
    if (take_records)
        standings_best();
}

/* the racers' standings: so far in the race (its segment or laps, each racer's total and
   an open road's segment time, by the totals), or the tournament's points (each racer's
   and this race's, by the points) */
static int g_now;

static void draw_final(void)
{
    static const char *const names[9] = { "city", "coas", "alpi", "rust", "autu", "burn",
                                          "mont", "trop", "neod" };
    int track = config_word(CFG_TRACK), order[8], i, y, again;
    char s[64];

    art("bgnd", 0);
    if (g_now) {
        if (track >= 0 && track < 9)
            art(names[track], 0);
        fe_style(2);
        if (track < 3) {
            art("segt", 0);
            sprintf(s, g_german ? "ETAPPE %d" : "SEGMENT %d", g_seg + 1);
        } else
            sprintf(s, g_german ? "%d Rundenrennen" : "%d Lap race", config_word(CFG_LAPS));
        fe_button_text(s, 8, 0x5a);
        session_order_by_totals(order);
        for (i = 0; i < 8; i++) {
            const session_racer *r = &g_session.racer[order[i]];

            y = 0xa6 + i * 0x1d;
            fe_style(1);
            fmt_time(s, g_session.gave_up[order[i]] ? 0 : r->total);
            fe_button_text(s, 0x135, y);
            if (track < 3) {
                fmt_time(s, r->time);
                fe_button_text(s, 0x1ef, y);
            }
            fe_style(3);
            fe_button_text(r->name, 0x7b, y);
        }
        return;
    }
    /* the tournament's: by the points, the most first */
    for (i = 0; i < 8; i++)
        order[i] = i;
    do {
        again = 0;
        for (i = 0; i < 7; i++)
            if (g_session.points[order[i]] < g_session.points[order[i + 1]]) {
                int t = order[i];

                order[i] = order[i + 1];
                order[i + 1] = t;
                again = 1;
            }
    } while (again);
    for (i = 0; i < 8; i++) {
        y = 0xa6 + i * 0x1d;
        fe_style(1);
        sprintf(s, "%d", g_session.points[order[i]]);
        fe_button_text(s, 0x1ef, y);
        if (g_session.result == 1)
            sprintf(s, "%d", g_session.race_points[order[i]]);
        else
            strcpy(s, "0");
        fe_button_text(s, 0x13e, y);
        fe_style(3);
        fe_button_text(g_session.racer[order[i]].name, 0x7b, y);
    }
}

/* fe_standings_final 0x6f070: the race's standings so far (stats/sgmtstnd.qfs), or the
   tournament's (trmtstnd.qfs; finstnd.qfs when every track is raced: nine, or eight
   with Lost Vegas not open, +0x5f50) */
static void standings_final(int now)
{
    page p = { "SGMTSTND.QFS", draw_final, 0, 1, 1, 1, "Donl", "Dond" };
    int raced = 0, i;

    for (i = 0; i < 9; i++)
        raced += g_session.raced[i];
    if (!now)
        p.file = raced == 9 || (raced == 8 && !config_word(CFG_UNLOCKED)) ? "FINSTND.QFS"
                                                                         : "TRMTSTND.QFS";
    g_now = now;
    g_seg = config_word(CFG_SEGMENT) ? config_word(CFG_SEGMENT) - 1 : 2;
    page_show(&p);
}

/* a circuit's pages (fe_standings_loop1 0x6cb24 and its laps 0x6e1ac): Done at the left
   (sel 0), a second button at the right (sel 1), 2 the pointer on neither; the mouse
   put on a button, the art's palette faded up over what the screen holds, the page drawn
   on black and faded up with the commentary; left and right go from button to button,
   up and down only from neither onto Done */
typedef struct {
    const char *file;          /* STATS/... */
    void (*draw)(void);        /* everything but the buttons */
    const char *lit[2], *dark[2];
} two_page;

static const int two_x[2] = { 40, 600 }, two_y[2] = { 450, 450 };   /* 0x691bc, 0x691c4 */

static int two_open(const two_page *p, unsigned char *pal)
{
    char path[256];

    snprintf(path, sizeof path, "%s/STATS/%s", art_dir(), p->file);
    if (!fsh_load(&g_art, path) || !fsh_palette(&g_art, 0, pal)) {
        fprintf(stderr, "renfs: %s is missing\n", path);
        fsh_free(&g_art);
        return 0;
    }
    pal_fade_up(pal, 0, 256);
    fe_styles_init();
    return 1;
}

/* until a button is chosen (Enter, Space or a click: 0 or 1), or Esc, the right button or
   two minutes (-1); fresh 0: a key or button still held from the page before is let go
   first */
static int two_loop(const two_page *p, const unsigned char *pal, int sel, int fresh,
                    const char *(*comment)(void))
{
    static const unsigned char black[768];
    int prev = sel, redraw = 1, first = 1, changed = 0, ready = fresh, released = fresh, k;
    unsigned held = 0;
    const char *say;

    for (;;) {
        int mx, my;

        mouse_read(&mx, &my);
        if (mx >= 0 && mx <= 0x82 && my >= 0x1b3 && my <= 0x1e0) {
            if (sel != 0) {
                prev = sel;
                sel = 0;
                changed = 1;
            }
        } else if (mx >= 0x190 && mx <= 0x280 && my >= 0x1b3 && my <= 0x1e0) {
            if (sel != 1) {
                prev = sel;
                sel = 1;
                changed = 1;
            }
        } else if (sel != 2) {
            prev = sel;
            sel = 2;
            changed = 1;
        }
        if (redraw) {
            if (first)
                p->draw();
            if (sel < 2)
                art(p->lit[sel], 0);
            if (first)
                video_palette(black, 0, 256);
            mouse_show(1);
            if (first) {
                fe_mouse_save();
                if (g_mouse_active)
                    fe_mouse_read();
                say = comment ? comment() : 0;
                if (say)
                    speech_say(say);
                pal_fade_up(pal, 0, 256);
                first = 0;
            }
            settimeout(12000);
            redraw = changed = 0;
        }
        if (changed) {
            fe_play_sound(0x47);
            if (prev < 2)
                art(p->dark[prev], 0);
            if (sel < 2)
                art(p->lit[sel], 0);
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
        if (k == 0x20 || k == 0x0d || (g_mouse_buttons == 1 && released)) {
            fe_mouse_press();
            if (ready) {
                fe_play_sound(0x48);
                if (sel < 2)
                    return sel;
            }
            released = ready = 0;
            held = 0;
            ticksince();
        } else if (k && ready) {
            if (k == 0x1b) {
                fe_play_sound(0x48);
                return -1;
            }
            if (((k == 0x4800 || k == 0x5000) && sel == 2) || k == 0x4b00 || k == 0x4d00) {
                fe_mouse_hide();
                if (k == 0x4b00 || k == 0x4d00) {
                    prev = sel;
                    sel = sel == 2 ? 0 : sel ^ 1;
                } else
                    sel = 0;
                mouse_place(two_x[sel], two_y[sel]);
                fe_mouse_save();
                changed = 1;
            }
            ready = 0;
            held = 0;
            ticksince();
        } else if (!k)
            ready = 1;
        video_retrace();
        if (timedout() || (g_mouse_buttons == 2 && released)) {
            fe_play_sound(0x48);
            return -1;
        }
        if (g_quit)
            return -1;
    }
}

static const char *const g_circuit_names[6] = { "rust", "autu", "burn", "mont", "trop", "neod" };
/* the circuit's page: the player first (the race won), the records not to be commented
   on (not taken, or said before the laps were looked at), a con5 said; the best lap's
   record taken, the lap page's visits, the best lap (-1 none) */
static int g_won, g_again, g_take, g_spoke, g_lap_rec, g_count, g_best;

static void draw_circuit(void)
{
    const session_racer *me = &g_session.racer[g_player];
    int type = config_word(CFG_RACE_TYPE), track = config_word(CFG_TRACK), order[8];
    int at = record(SESSION_TOP_TEN + config_word(CFG_RACE_LENGTH) * 10);
    const session_racer *them;
    const char *dnf = g_german ? "Abbruch" : "DNF";
    char s[64];

    art("bgnd", 0);
    if (track >= 3 && track <= 8)
        art(g_circuit_names[track - 3], 0);
    fe_style(1);
    /* the standing */
    g_won = 0;
    if (g_session.result != 1)
        fe_text(dnf, 0x131, 0x89);
    else if (type == 2 || type == 3) {
        session_order_by_totals(order);
        g_won = g_session.place == 0;
        if (g_drive.gave_up[g_player])
            fe_text(dnf, 0x131, 0x89);
        else {
            sprintf(s, "%d / 8", g_session.place + 1);
            fe_text(s, 0x131, 0x89);
        }
        if (g_drive.gave_up[g_session.rival])
            fe_text(dnf, 0x1d6, 0x89);
        else {
            sprintf(s, "%d / 8", g_session.rival_place + 1);
            fe_text(s, 0x1d6, 0x89);
        }
    } else if (type == 1 && g_drive.gave_up[g_player])
        fe_text(dnf, 0x131, 0x89);
    else if (type == 1 ? me->total < g_session.racer[1 - g_player].total
                       : me->total <= config_long(at + 0x1b)) {
        g_won = type == 1;
        fe_text("1 / 2", 0x131, 0x89);
        fe_text("2 / 2", 0x1d6, 0x89);
    } else {
        fe_text("2 / 2", 0x131, 0x89);
        fe_text("1 / 2", 0x1d6, 0x89);
    }
    sprintf(s, g_german ? "%d Rundenrennen" : "%d Lap race", config_word(CFG_LAPS));
    fe_text(s, 8, 0x5f);
    fe_text_cut(me->name, 0x131, 0x5a, 0x96);
    fe_text(car_name(config_word(CFG_CAR)), 0x131, 0x6e);
    if (type == 0) {
        fe_text(g_german ? "Bestzeit" : "Best Time", 0x1d6, 0x5a);
        fe_text(car_name(config_long(at + 0xf)), 0x1d6, 0x6e);
    } else if (type == 2 || type == 3) {
        fe_text(g_session.racer[g_session.rival].name, 0x1d6, 0x5a);
        fe_text(car_name(g_session.rival_car), 0x1d6, 0x6e);
    } else {
        fe_text(g_session.racer[1 - g_player].name, 0x1d6, 0x5a);
        fe_text(car_name(config_word(CFG_OPPONENT)), 0x1d6, 0x6e);
    }
    /* the player's total, average and best top speed */
    fe_style(1);
    fmt_time(s, g_session.gave_up[g_player] ? 0 : me->total);
    fe_text(s, 0x131, 0x9d);
    if (g_session.gave_up[g_player])
        fmt_average(s, 0, 0);
    else
        fmt_average(s, me->total, me->total_distance);
    fe_text(s, 0x131, 0xbc);
    fmt_speed(s, g_session.result == 1 && !g_drive.gave_up[g_player] ? me->best_top : 0);
    fe_text(s, 0x131, 0xd0);
    /* beside them the best total's of this race length (a time trial), the other racer's
       or the best total's but the player's (at the player's distance) */
    if (type == 0) {
        fmt_time(s, config_long(at + 0x1b));
        fe_text(s, 0x1d6, 0x9d);
        fmt_average(s, config_long(at + 0x1b), me->total_distance);
        fe_text(s, 0x1d6, 0xbc);
        fmt_speed(s, config_long(at + 0x1f));
    } else if (g_session.result == 1 && !g_drive.gave_up[g_player]) {
        them = &g_session.racer[type == 1 ? 1 - g_player : order[g_won]];
        fmt_time(s, them->total);
        fe_text(s, 0x1d6, 0x9d);
        fmt_average(s, them->total, me->total_distance);
        fe_text(s, 0x1d6, 0xbc);
        fmt_speed(s, them->best_top);
    } else {
        fmt_time(s, 0);
        fe_text(s, 0x1d6, 0x9d);
        fmt_average(s, 0, 0);
        fe_text(s, 0x1d6, 0xbc);
        fmt_speed(s, 0);
    }
    fe_text(s, 0x1d6, 0xd0);
    /* the best total of this race length and the track's top speed (white when just
       taken) */
    fe_style(g_place == 0 ? 3 : 1);
    fe_text_cut((const char *)g_config + at, 7, 0x11d, 200);
    fmt_time(s, config_long(at + 0x1b));
    fe_text(s, 0x131, 0x11d);
    fe_text(car_name(config_long(at + 0xf)), 0x1d6, 0x11d);
    record_kind(at, 0x11d, 0);
    at = record(SESSION_TOP_SPEED);
    fe_style(g_speed == 1 ? 3 : 1);
    fe_text_cut((const char *)g_config + at, 7, 0x177, 200);
    fmt_speed(s, config_long(at + 0x1f));
    fe_text(s, 0x131, 0x177);
    fe_text(car_name(config_long(at + 0xf)), 0x1d6, 0x177);
    record_kind(at, 0x177, 0);
}

/* the commentary: a race won (or a tournament's), with the records taken; at a
   tournament's end with the most points; a record taken otherwise */
static const char *circuit_comment(void)
{
    int type = config_word(CFG_RACE_TYPE), speech = config_word(CFG_SPEECH), raced = 0, i;
    int both = g_speed == 1 && g_place == 0, rec = g_speed == 1 || g_place == 0, end;

    if (g_won || type == 3) {
        for (i = 0; i < 8; i++)
            raced += g_session.raced[i];
        end = (g_vegas_ok && raced + g_session.raced[8] == 9) || (!g_vegas_ok && raced == 8);
        g_said = 1;
        if (g_again || !(end ? session_player_most() : g_won))
            return 0;
        if (speech && rec)
            return end ? (both ? "CON8_D16.EAS" : g_speed == 1 ? "CON6_C16.EAS" : "CON7_D16.EAS")
                       : (both ? "CON8_A16.EAS" : g_speed == 1 ? "CON6_A16.EAS" : "CON7_A16.EAS");
        if (!g_take)
            return 0;
        g_spoke = 1;
        return end ? "CON5_B16.EAS" : "CON5_A16.EAS";
    }
    if (speech && rec && !g_again) {
        g_said = 1;
        return both ? "CON4_F16.EAS" : g_speed == 1 ? "CON1_B16.EAS" : "CON2_B16.EAS";
    }
    return 0;
}

/* the commentary started on the page (and so to be stopped) */
static int circuit_spoken(void)
{
    return config_word(CFG_SPEECH) && (g_speed == 1 || g_place == 0 || g_spoke) && !g_again;
}

/* "%d.%d", miles an hour and the tenth of the metres a second (0x692a8) */
static void fmt_mph(char *s, int v)
{
    if (v <= 0x9fff6)
        strcpy(s, "N/A");
    else
        sprintf(s, "%d.%d", (v >> 16) * 224 / 100 + 1, (int)(((unsigned)v & 0xffff) * 10 >> 16));
}

/* a braking distance from its steps (0x69318: from 60 mph, 0x693a8: from 80) */
static void fmt_braking(char *s, int v, int scale)
{
    int q = v * scale / 12000;

    if (v < 100 || v == 999)
        strcpy(s, "N/A");
    else if (g_german)
        sprintf(s, "%d M", q);
    else
        sprintf(s, "%d Ft", q * 33 / 10);
}

static void lap_row(int i, int y, int x0, int x1, int x2)
{
    const session_racer *me = &g_session.racer[g_player];
    char s[64];

    if (i == g_best)
        fe_style(3);
    sprintf(s, "%d", i + 1);
    fe_text(s, x0, y);
    fmt_time(s, me->laps[i]);
    fe_text(s, x1, y);
    fmt_average(s, me->laps[i], me->distance);
    fe_text(s, x2, y);
    if (i == g_best)
        fe_style(1);
}

/* the laps (two columns from nine on), the best white; Rusty Springs' measurements in a
   time trial; the track's lap record */
static void draw_laps(void)
{
    const session_racer *me = &g_session.racer[g_player];
    int type = config_word(CFG_RACE_TYPE), track = config_word(CFG_TRACK), laps = config_word(CFG_LAPS);
    int n = laps > 8 ? (laps + 1) / 2 : laps, i, at;
    char s[64], a[32], b[32];

    art("bgnd", 0);
    if (track >= 3 && track <= 8) {
        art(g_circuit_names[track - 3], 0);
        if (track != 3 || type != 0)
            art("no5w", 0);
    }
    fe_style(1);
    if (track == 3 && type == 0 && !g_drive.rally) {
        if (g_german) {
            fmt_speed(s, me->best_top);
            fe_text(s, 0x131, 0x172);
            fmt_time(s, me->stats[0]);
            fe_text(s, 0x131, 0x186);
            fmt_braking(s, me->stats[2], 3000);
            fe_text(s, 0x131, 0x19a);
        } else {
            fmt_speed(s, me->best_top);
            fe_text(s, 0xc8, 0x171);
            fmt_time(s, me->stats[0]);
            fe_text(s, 0xc8, 0x186);
            fmt_time(s, me->stats[1]);
            fe_text(s, 0xc8, 0x19a);
            fmt_time(a, me->stats[4]);
            fmt_mph(b, me->stats[5]);
            sprintf(s, "%s / %s", a, b);
            fe_text(s, 0x1f0, 0x171);
            fmt_braking(s, me->stats[2], 3000);
            fe_text(s, 0x1ef, 0x186);
            fmt_braking(s, me->stats[3], 0xf11);
            fe_text(s, 0x1ef, 0x19a);
        }
    }
    for (i = 0; i < n; i++)
        lap_row(i, 0x7d + i * 0x14, 7, 0x68, 0xc7);
    for (i = n; i < laps; i++)
        lap_row(i, 0x7d + (i - n) * 0x14, 0x148, 0x1a6, 0x205);
    at = record(0);
    fe_style(g_lap_rec == 1 ? 3 : 1);
    fe_text_cut((const char *)g_config + at, 7, 0x144, 200);
    fmt_time(s, config_long(at + 0x1b));
    fe_text(s, 0x131, 0x144);
    fe_text(car_name(config_long(at + 0xf)), 0x1d6, 0x144);
    record_kind(at, 0x144, 0);
}

static const char *laps_comment(void)
{
    if (!g_said)
        g_said = 1;
    return "CON2_C16.EAS";
}

/* fe_standings_loop2 0x6e1ac (stats/standlo2.qfs): Done (1) or Finish, back to the race's
   page (0, Esc and the time too); con2_C the first time, with the lap record taken */
static int standings_laps(void)
{
    static const two_page p = { "STANDLO2.QFS", draw_laps, { "donl", "finl" }, { "dond", "find" } };
    const session_racer *me = &g_session.racer[g_player];
    int said = config_word(CFG_SPEECH) && g_lap_rec == 1 && g_count == 1, c, i;
    unsigned char pal[768];

    g_best = 0;
    for (i = 0; i < 20; i++)
        if (me->laps[i] < me->laps[g_best] && me->laps[i] > 0)
            g_best = i;
    if (me->laps[g_best] <= 0)
        g_best = -1;
    mouse_place(two_x[1], two_y[1]);
    if (!two_open(&p, pal))
        return 0;
    c = two_loop(&p, pal, 1, 1, said ? laps_comment : 0);
    if (said)
        speech_stop();
    pal_fade_down(0, 256);
    fsh_free(&g_art);
    fe_mouse_save();
    return c == 0;
}

/* fe_standings_loop1 0x6cb24 (stats/standlo1.qfs): a circuit's race, the records taken
   (the best total of the race length, the top speed, the best lap) when it was finished;
   Laps shows the laps, then this again; at the end the ten best totals */
static void standings_loop1(int take_records)
{
    static const two_page p = { "STANDLO1.QFS", draw_circuit, { "donl", "lapl" }, { "dond", "lapd" } };
    unsigned char pal[768];
    int sel = 0, fresh = 1, c;

    g_place = g_speed = g_lap_rec = 20;
    g_again = !take_records;
    g_take = take_records;
    g_spoke = g_count = 0;
    g_said = 0;
    if (take_records) {
        g_place = session_top_ten();
        g_speed = session_records_top_speed();
        g_lap_rec = session_records_best_lap();
    }
    mouse_place(two_x[0], two_y[0]);
    if (!two_open(&p, pal))
        return;
    for (;;) {
        c = two_loop(&p, pal, sel, fresh, circuit_comment);
        if (c != 1)
            break;
        if (circuit_spoken()) {
            speech_stop();
            g_again = 1;
        }
        fsh_free(&g_art);
        pal_fade_down(0, 256);
        g_count++;
        if (standings_laps()) {
            if (take_records)
                standings_best();
            fe_mouse_save();
            return;
        }
        if (!two_open(&p, pal))
            return;
        if (!g_mouse_active)
            mouse_place(two_x[1], two_y[1]);
        sel = 1;
        fresh = 0;
    }
    if (circuit_spoken())
        speech_stop();
    pal_fade_down(0, 256);
    fsh_free(&g_art);
    if (take_records)
        standings_best();
    fe_mouse_save();
}

void standings_after(int finished, int last)
{
    int type = config_word(CFG_RACE_TYPE), many = type == 2 || type == 3;

    session_test_load();
    if (config_word(CFG_TRACK) < 3) {
        standings_segment(1);
        if (many)
            standings_final(1);
        if (last) {
            standings_track(finished);
            if (type == 3) {
                if (finished)
                    session_points();
                standings_final(0);
            }
        }
    } else {
        standings_loop1(finished);
        if (many)
            standings_final(1);
        if (type == 3) {
            if (finished)
                session_points();
            standings_final(0);
        }
    }
}
