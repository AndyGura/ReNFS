/* What a race leaves for the screens after it (NOTES §4.21). */
#include "session.h"
#include "cardraw.h"
#include "config.h"
#include "drive.h"
#include "exe.h"
#include "files.h"
#include "timing.h"
#include <stdio.h>
#include <string.h>

race_session g_session;

void session_new(void)
{
    int i;

    memcpy(g_session.kept_keys, g_config + 0x5f36, sizeof g_session.kept_keys);
    /* tourn_clear_records 0x745f4: everything a racer's but the name, the flags, and
       the segment back to the first */
    for (i = 0; i < SESSION_RACERS; i++) {
        session_racer *r = &g_session.racer[i];
        char name[9];

        memcpy(name, r->name, sizeof name);
        memset(r, 0, sizeof *r);
        memcpy(r->name, name, sizeof name);
        g_session.gave_up[i] = 0;
    }
    config_set_word(CFG_SEGMENT, 0);
    memset(g_session.points, 0, sizeof g_session.points);
    memset(g_session.raced, 0, sizeof g_session.raced);
    memset(g_session.won, 0, sizeof g_session.won);
    /* 0x74748: the computer's drivers' names (8 bytes each from 0xe43e0), then the
       player's */
    for (i = 1; i < SESSION_RACERS; i++) {
        const unsigned char *p = exe_at(0xe43e0 + 8 * (i - 1), 8);

        if (p)
            snprintf(g_session.racer[i].name, sizeof g_session.racer[i].name, "%.8s",
                     (const char *)p);
    }
    snprintf(g_session.racer[0].name, sizeof g_session.racer[0].name, "%.8s",
             (const char *)g_config);
}

/* 0x36c3c: the game's car number to the menu's (the words at 0x36270) */
static int menu_car(int number)
{
    const unsigned char *p = exe_at(0x36270 + 2 * (number & 15), 2);

    return p ? (short)(p[0] | p[1] << 8) : 0;
}

/* the racers' distance, a time worked out for those still on the road, the order
   (0x68ab0) */
static void finish_order(void)
{
    int i, j, k, d = g_drive.last_node * 0x30 / 10;

    for (i = 0; i < g_field && i < SESSION_RACERS; i++)
        g_session.gave_up[i] |= g_drive.gave_up[i];
    for (i = 0; i < 9; i++)
        g_session.order[i] = i;
    /* the players: the distance, the race's (an open road's segment, a circuit's lap) */
    for (i = 0; i < g_timing.humans; i++)
        g_drive.distance[i] = d;
    /* the others not in: their time as their pace so far would have made it */
    for (i = g_timing.humans; i < g_racers && i < DRIVE_PLAYERS; i++) {
        if (!g_timing.finish[i]) {
            const race_car *c = g_cars[i];

            g_timing.finish[i] = (g_drive.last_node * g_drive.laps * g_drive.steps - 300)
                                 / ((c->lap - 1) * g_drive.last_node + c->node + 1);
            g_drive.distance[i] = d;
        }
    }
    /* the order by finishing time: a racer without one (or turned back) goes behind
       the next */
    for (k = 0; k < g_racers; k++)
        for (j = 1; j < g_racers && j < 9; j++) {
            int a = g_session.order[j - 1], b = g_session.order[j];
            int ta = a < DRIVE_PLAYERS ? g_timing.finish[a] : 0;
            int tb = b < DRIVE_PLAYERS ? g_timing.finish[b] : 0;

            if (ta <= 0 || (tb < ta && ta > 0)) {
                g_session.order[j - 1] = b;
                g_session.order[j] = a;
            }
        }
    /* (the player's time into the segments' times; the splits into its record
       (0x14eac4), read by nothing) */
    if (g_drive.segment >= 0 && g_drive.segment < 3)
        g_timing.segment_times[g_drive.segment] = g_timing.finish[g_player];
}

void session_race_end(void)
{
    int rank[SESSION_RACERS] = { 0 };
    int n = g_field < SESSION_RACERS ? g_field : SESSION_RACERS, i, j;
    session_racer *me;

    finish_order();
    /* (0x68dcc only in a race played, +0xaf 0) each racer's time and top speed, the
       times added up; all cleared when the race was not finished */
    if (g_timing.result == 1)
        for (i = 0; i < n; i++) {
            session_racer *r = &g_session.racer[i];

            r->time = g_timing.finish[i];
            r->top = g_drive.top_speed[i];
            r->total = r->time < 1 ? 0 : r->total + r->time;
            if (r->best_top < r->top)
                r->best_top = r->top;
        }
    else
        for (i = 0; i < SESSION_RACERS; i++) {
            session_racer *r = &g_session.racer[i];

            r->time = r->total = r->top = r->best_top = 0;
        }
    /* where the player came, by time (a network race: the first other human) */
    g_session.rival_place = 8;
    for (i = 0; i < n; i++)
        if (g_session.order[i] == g_player)
            g_session.place = i;
    /* the ranks by the totals: how many have less */
    for (i = 0; i < n; i++)
        for (j = 0; j < n; j++)
            if (g_session.racer[j].total < g_session.racer[i].total)
                rank[i]++;
    g_session.rank = rank[g_player];
    for (i = 0; i < n; i++)
        g_session.last_order[i] = (short)g_session.order[i];
    /* the rival: the other racer of the least total */
    g_session.rival = g_player == 0;
    for (i = 0; i < n; i++)
        if (g_session.racer[i].total < g_session.racer[g_session.rival].total && i != g_player)
            g_session.rival = i;
    g_session.rival_rank = rank[g_session.rival];
    g_session.rival_car = g_session.rival < g_ncars && g_cars[g_session.rival]
        ? menu_car(g_cars[g_session.rival]->number) : 0;
    for (i = 0; i < n; i++)
        if (g_session.order[i] == g_session.rival) {
            g_session.rival_place = i;
            break;
        }
    me = &g_session.racer[g_player];
    /* a circuit: the player's laps, each from the times as they were finished */
    if (config_word(CFG_TRACK) > 2) {
        memset(me->laps, 0, sizeof me->laps);
        if (g_timing.result != 0x10)
            for (i = 0; i < config_word(CFG_LAPS) && i + 1 < DRIVE_LAPS && i < 20; i++) {
                const int *t = g_drive.lap_times[g_player];

                me->laps[i] = i < 1 ? t[1] : t[i + 1] - t[i] < 0x3c ? 0 : t[i + 1] - t[i];
            }
    }
    me->tickets = g_timing.tickets[g_player];
    me->warnings = g_timing.warnings[g_player];
    me->distance = g_drive.distance[g_player];
    me->stats[0] = g_drive.stats[g_player][0];
    me->stats[1] = g_drive.stats[g_player][1];
    me->stats[2] = g_drive.stats[g_player][3];
    me->stats[3] = g_drive.stats[g_player][2];
    me->stats[4] = g_drive.stats[g_player][5];
    me->stats[5] = g_drive.stats[g_player][4];
}

int session_segment(void)
{
    int s = config_word(CFG_SEGMENT);

    return s == 0 ? 2 : s - 1;
}

/* a record's place in the settings */
static int record_at(int track, int i)
{
    return CFG_RECORDS + track * CFG_TRACK_RECORDS + i * CFG_RECORD;
}

/* the player's name, time and top speed, car, gearbox and race type into a record */
static void record_take(int at)
{
    const session_racer *r = &g_session.racer[g_player];
    const unsigned char *k = g_session.kept - 9;   /* (CONFIG.DAT's places) */

    memcpy(g_config + at, r->name, strlen(r->name) + 1);
    config_set_long(at + 0x1b, r->time);
    config_set_long(at + 0x1f, r->top);
    config_set_long(at + 0xf, (short)(k[0x9b] | k[0x9c] << 8));
    config_set_long(at + 0x13, (short)(k[0x97] | k[0x98] << 8));
    config_set_long(at + 0x23, (short)(k[0x6d] | k[0x6e] << 8));
    config_set_long(at + 0x17, 1);
}

int session_records_segment(void)
{
    const session_racer *r = &g_session.racer[g_player];
    int seg = session_segment(), track = config_word(CFG_TRACK), took = 0, at;

    if (config_word(CFG_CAR) == 11)
        return 0;
    at = record_at(track, seg);
    if (r->time < config_long(at + 0x1b) && r->time > 0) {
        record_take(at);
        took = 1;
    }
    at = record_at(track, seg + 3);
    if (config_long(at + 0x1f) < r->top) {
        record_take(at);
        took += 2;
    }
    return took;
}

void fmt_time(char *s, int t)
{
    if (t >= 3600 && t != 999999 && t != 99999)
        sprintf(s, "%d:%02d.%d", t / 3600, t % 3600 / 60, t % 3600 % 60 / 6);
    else if (t > 0 && t != 999999 && t != 99999)
        sprintf(s, "%d.%d", t / 60, t % 60 / 6);
    else
        strcpy(s, "N/A");
}

/* the tenth is the metres a second's, not the miles' or kilometres' an hour */
void fmt_speed(char *s, int v)
{
    int tenth = (int)(((unsigned)v & 0xffff) * 10 >> 16), i = v >> 16;

    if (v <= 0x9fff6)
        strcpy(s, "N/A");
    else if (g_german)
        sprintf(s, "%d.%d%c", i * 360 / 100 + 1, tenth, 0x9b);
    else
        sprintf(s, "%d.%dmph", i * 224 / 100 + 1, tenth);
}

void fmt_average(char *s, int t, int d)
{
    int v;

    if (t <= 0 || d <= 0 || t == 99999 || t == 999999) {
        strcpy(s, "N/A");
        return;
    }
    v = d * (g_german ? 2160 : 1344) / t;
    if (g_german)
        sprintf(s, "%d.%d%c", v / 10, v % 10, 0x9b);
    else
        sprintf(s, "%d.%dmph", v / 10, v % 10);
}
