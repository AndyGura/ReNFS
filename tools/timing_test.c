/* For tools/timing_emu.py: the race's timing's globals and tables, in the order of
   timing_emu.py's VARS and ARRAYS, and a car's size. (With drive_test.c for the cars.) */
#include "timing.h"
#include "cardraw.h"
#include "drive.h"
#include "track.h"

int timing_test_car_size(void)
{
    return (int)sizeof(race_car);
}

/* a car's lap, node and progress (for dash_emu.py, whose cars are cars_emu.py's) */
void timing_test_car_set(race_car *c, int lap, int node, int progress)
{
    c->lap = lap;
    c->node = node;
    c->progress = progress;
}

/* the globals, in VARS' order */
int *timing_test_var(int k)
{
    int *const v[] = {
        &g_drive.steps, &g_drive.state, &g_drive.kind, &g_drive.segment, &g_drive.laps,
        &g_drive.last_node, &g_race_type, &g_field, &g_humans, &g_player, &g_racers,
        &g_ncars, &g_timing.humans, &g_timing.lap_limit, &g_timing.table, &g_track.loop,
        &g_timing.passed, &g_timing.rival_passed, &g_timing.split, &g_timing.split_shown,
        &g_timing.result, &g_timing.stopped, &g_drive.best_lap, &g_drive.new_best,
        &g_timing.place_shown, &g_drive.viewed,
    };

    return k >= 0 && k < (int)(sizeof v / sizeof v[0]) ? v[k] : 0;
}

/* the tables, in ARRAYS' order */
int *timing_test_array(int k)
{
    switch (k) {
    case 0: return g_timing.split_at;
    case 1: return g_timing.mine;
    case 2: return g_timing.theirs;
    case 3: return g_timing.segment_times;
    case 4: return g_timing.finish;
    case 5: return g_timing.place;
    case 6: return &g_drive.lap_times[0][0];
    case 7: return g_timing.by_road;
    case 8: return g_timing.by_race;
    case 9: return g_drive.top_speed;
    }
    return 0;
}

unsigned char *timing_test_places(void)
{
    return g_timing.places;
}
