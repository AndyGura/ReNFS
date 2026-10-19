/* The race's timing (NOTES §4.12): see timing.h. */
#include <string.h>

#include "timing.h"
#include "racesnd.h"
#include "cardraw.h"
#include "config.h"
#include "exe.h"
#include "track.h"

race_timing g_timing;

static int mulr(int a, int b)
{
    long long p = (long long)a * b;

    return (int)(p >> 16) + (int)((p >> 15) & 1);
}

/* a time of the CONFIG.DAT's records: the track's block of 37, record k (0x156ce4 ..) */
static int record(int k)
{
    const unsigned char *r = g_config + 0xb5 + g_drive.kind * 0xa6b + k * 0x27 + 0x1b;

    return (int)(r[0] | r[1] << 8 | r[2] << 16 | (unsigned)r[3] << 24);
}

/* the track's table of splits (0xf7238): rows of five (a fraction of the time to beat at
   each split, 16.16), three rows a track */
static int split_row(int row, int k)
{
    return exe_i32(0xf7238 + row * 0x14 + k * 4);
}

/* 0x685e4's time trial: the times to beat at each split from the records. (The other
   player's splits kept from the race before, the record's +0x44, come first in the
   original; the port keeps none yet, so these are always used.) */
static void time_trial_times(void)
{
    int t = g_drive.kind, k;

    if (t > 2) {
        /* a circuit: the first lap by its own row and the first lap's time (row 3t + 2),
           the others the rest of the record's time shared out */
        int first = split_row(3 * t + 2, 0), race = record(6 + 10 * g_timing.table);

        for (k = 0; k < g_drive.laps * 5; k++) {
            if (k < 5)
                g_timing.theirs[k] = mulr(split_row(3 * t, k), first);
            else {
                int lap = (race - first) / (g_timing.lap_limit - 1);

                g_timing.theirs[k] = mulr(split_row(3 * t + 1, k % 5), lap)
                                   + (k - 5) / 5 * lap + first;
            }
            g_timing.theirs[k] += 300;
        }
        g_timing.theirs[g_drive.laps * 5] = race + 300;
    } else {
        /* an open road: the segment's record shared out by its row */
        int best = record(g_drive.segment), d;

        for (k = 0; k < 5; k++)
            g_timing.theirs[k] = mulr(split_row(3 * t + g_drive.segment, k), best) + 300;
        g_timing.theirs[5] = best + 300;
        d = g_timing.split_at[4] - g_drive.last_node;
        if (d < 0)
            d = -d;
        if (d < 0x27)
            g_timing.split_at[4] = g_drive.last_node;
    }
}

void timing_start(void)
{
    int p, i, k;

    /* race_start 0x506dc: who is checked at the finish */
    g_timing.result = 0;
    if (g_race_type == 0)
        g_racers = g_timing.humans = 1;
    else if (g_race_type == 1) {
        g_racers = 2;
        g_timing.humans = g_humans;
    } else if (g_race_type <= 3) {
        g_racers = g_field;
        g_timing.humans = g_humans;
    }
    /* 0x685e4: each racer's results cleared (the record's other words, the best times,
       come with the race's end) */
    for (p = 0; p < g_racers && p < DRIVE_PLAYERS; p++) {
        g_timing.finish[p] = 0;
        g_timing.tickets[p] = g_timing.warnings[p] = 0;
        g_timing.place[p] = 0;
        g_drive.top_speed[p] = 0;
        for (k = 0; k < DRIVE_LAPS; k++)
            g_drive.lap_times[p][k] = 0;
    }
    /* the splits: the track's five points for the segment (0xf701c), one set a lap */
    for (i = 0; i < 17; i++)
        for (k = 0; k < 5; k++)
            g_timing.split_at[i * 5 + k] = exe_i32(0xf701c + ((3 * g_drive.kind + g_drive.segment) * 5 + k) * 4)
                                         + i * g_drive.last_node;
    g_timing.passed = 0;
    if (g_race_type) {
        g_timing.rival_passed = 0;
        memset(g_timing.theirs, 0, sizeof g_timing.theirs);
    } else {
        g_timing.rival_passed = TIMING_SPLITS;
        memset(g_timing.theirs, 0, sizeof g_timing.theirs);
        time_trial_times();
    }
    memset(g_timing.mine, 0, sizeof g_timing.mine);
    if (g_drive.segment == 0)
        memset(g_timing.segment_times, 0, sizeof g_timing.segment_times);
    g_timing.split = g_timing.split_shown = 0;
    g_timing.switched = 0;
}

void timing_cars(void)
{
    int i;

    for (i = 0; i < TIMING_CARS; i++)
        g_timing.by_road[i] = g_timing.by_race[i] = i;
}

int timing_state(const race_car *c)
{
    if (g_track.loop)
        return c->lap < 1 ? 1 : c->lap > g_drive.laps ? 2 : 0;
    return c->node < 12 ? 1 : c->node > g_drive.last_node ? 2 : 0;
}

/* a lap's slot in the times (lap_times[p][k], 0x14ea80 + 4k), if there is one */
static void lap_time_set(int p, int k, int t)
{
    if (p >= 0 && p < DRIVE_PLAYERS && k >= 0 && k < DRIVE_LAPS)
        g_drive.lap_times[p][k] = t;
}

void timing_finish(race_car *c)
{
    int p = c->player, s, i;

    if (p < 0 || p >= DRIVE_PLAYERS || g_timing.finish[p])
        return;
    s = drive_race_progress(c);
    if (s == 1 && p < g_timing.humans) {
        /* turned back past the start */
        lap_time_set(p, c->lap + 1, -999);
        g_timing.finish[p] = -999;
    } else if (s == 2) {
        int t = g_drive.steps - 300;

        lap_time_set(p, c->lap - 1, t);
        g_timing.finish[p] = t;
        /* the player's finishing place: one more than those in before (in a race played,
           not a replay's) */
        if (g_race_type && p == g_player) {
            int place = 0;

            for (i = 0; i < g_field && i < DRIVE_PLAYERS; i++)
                if (g_timing.finish[i] && g_timing.finish[i] < t)
                    place++;
            g_timing.place[p] = place + 1;
        }
    }
}

void timing_step(void)
{
    int i, all = 1, any = 0;

    if (g_drive.state != 1)
        return;
    for (i = 0; i < g_racers && i < g_ncars; i++)
        if (g_cars[i] && g_cars[i]->part & 4)
            timing_finish(g_cars[i]);
    /* over once every human is in (or turned back) and stopped */
    for (i = 0; i < g_timing.humans; i++) {
        if (!g_timing.finish[i] || g_cars[i]->speed_abs > 0x8000)
            all = 0;
        else if (g_humans > 1 && i == g_player) {
            int j;

            /* two players, the first in and stopped: the view goes to the one still
               racing, its engine heard (0x53214; the camera itself turned, 0x13530
               with mode 3: not yet) */
            g_timing.stopped = 1;
            if (!g_timing.switched)
                for (j = 0; j < g_humans; j++)
                    if (!g_timing.finish[j]) {
                        g_drive.viewed = j;
                        g_timing.switched = 1;
                        rsnd_engine_on();
                        break;
                    }
        }
    }
    if (!all)
        return;
    /* (a network race waits for the others here) */
    for (i = 0; i < g_timing.humans; i++)
        if (g_timing.finish[i] > 0)
            any = 1;
    g_timing.result = any ? 1 : 0x10;
}

/* a before b in the order: further along (k), or as far and further across */
static int behind(const race_car *a, const race_car *b, int by_race)
{
    int ka = by_race ? a->progress : a->node, kb = by_race ? b->progress : b->node;

    return ka > kb || (ka == kb && a->dist > b->dist);
}

/* one order sorted again by swapping neighbours until none moves (0x518c0) */
static void sort(int *order, int by_race)
{
    int moved, i;

    do {
        moved = 0;
        for (i = 0; i < g_ncars - 1; i++)
            if (behind(g_cars[order[i]], g_cars[order[i + 1]], by_race)) {
                int t = order[i];

                order[i] = order[i + 1];
                order[i + 1] = t;
                moved = 1;
            }
    } while (moved);
}

void timing_sort(void)
{
    sort(g_timing.by_road, 0);
    sort(g_timing.by_race, 1);
}

void timing_splits(void)
{
    int a, b = 0, before = g_timing.split;

    /* the player's progress and the rival's: the other car when there are two, else the
       leader or, the player leading, the second (race_update_cars) */
    if (g_humans > 1 || g_field - g_humans == 1) {
        a = g_cars[g_player]->progress;
        b = g_cars[g_player == 0]->progress;
    } else if (g_field - g_humans > 1) {
        int lead = g_timing.by_race[g_ncars - 1];

        if (lead == g_player)
            lead = g_timing.by_race[g_ncars - 2];
        a = g_cars[g_player]->progress;
        b = g_cars[lead]->progress;
    } else
        a = g_cars[g_player]->progress;

    /* 0x68d7c: a split passed, by each (one a step); the rival's only in a race */
    if (g_timing.passed < TIMING_SPLITS && a > g_timing.split_at[g_timing.passed])
        g_timing.mine[g_timing.passed++] = g_drive.steps;
    if (g_race_type && g_timing.rival_passed < TIMING_SPLITS
        && b > g_timing.split_at[g_timing.rival_passed])
        g_timing.theirs[g_timing.rival_passed++] = g_drive.steps;

    /* 0x68d00: the gap at the last split both passed; shown for 90 steps once it
       changes */
    if (g_timing.passed && g_timing.rival_passed) {
        int k = g_timing.passed <= g_timing.rival_passed ? g_timing.passed : g_timing.rival_passed;

        g_timing.split = g_timing.theirs[k - 1] - g_timing.mine[k - 1];
    }
    if (g_timing.split != before)
        g_timing.split_shown = 90;
    if (g_timing.split_shown > 0)
        g_timing.split_shown--;
}

int timing_place(int viewed)
{
    int place = g_timing.place_shown, i;

    if (timing_state(g_cars[viewed]) == 2) {
        /* finished: one more than those in before it */
        place = 1;
        for (i = 0; i < g_field && i < DRIVE_PLAYERS; i++)
            if (g_timing.finish[i] && g_timing.finish[i] < g_timing.finish[viewed])
                place++;
        return place;
    }
    if (g_race_type == 0)
        place = g_timing.split > 0 ? 1 : 2;
    else {
        /* from the race order: each car's place for the map, and its record's */
        int mine = 0, other = 0, n = g_ncars;

        for (i = 0; i < n; i++) {
            int k = g_timing.by_race[i];

            g_timing.places[k] = (unsigned char)(n - i);
            if (k == viewed)
                mine = i;
            else if (k == (viewed == 0))
                other = i;
            if (k < DRIVE_PLAYERS)
                g_timing.place[k] = n - i;
        }
        place = g_race_type == 1 ? (mine > other ? 1 : 2) : n - mine;
    }
    if (viewed < DRIVE_PLAYERS)
        g_timing.place[viewed] = place;
    return place;
}
