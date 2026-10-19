/* For tools/opponent_emu.py: opponent.c itself (its parts reached one by one), its
   globals in the order of opponent_emu.py's VARS, its tables, and its pointers set from
   the original's. (With drive_test.c and timing_test.c for the cars and the orders.) */
#include "opponent.c"

int *opp_test_var(int k)
{
    int *const v[] = {
        &g_opp.brake, &g_opp.lead_index, &g_opp.ticks, &g_opp.lane_w, &g_opp.lane_rule,
        &g_opp.off, &g_opp.explode, &g_opp.traffic, &g_opp.wander, &g_opp.bend_drop,
        &g_opp.bend_slow, &g_opp.behind, (int *)&g_opp.random, (int *)&g_opp.mult,
        (int *)&g_opp.last, &g_race_type, &g_drive.police, &g_opp.waiting, &g_opp.sides[0],
        &g_opp.sides[1], &g_opp.rival_car, &g_opp.chasing, &g_opp.come_node,
        &g_opp.long_chases, &g_opp.linked, &g_opp.ticket_up, &g_opp.warning_up,
        &g_opp.msg_time, &g_timing.tickets[0], &g_timing.warnings[0], &g_timing.finish[0],
        &g_timing.result,
    };

    return k >= 0 && k < (int)(sizeof v / sizeof v[0]) ? v[k] : 0;
}

short *opp_test_zone(void)
{
    return &g_opp.zone[0][0];
}

int *opp_test_cfg(void)
{
    return g_opp.cfg;
}

int *opp_test_grid(void)
{
    return g_opp.grid;
}

/* a test: node n's byte at `at` set to v (the road given lanes the other way) */
void opp_test_node_byte(int n, int at, int v)
{
    ((unsigned char *)node_ptr(n))[at] = (unsigned char)v;
}

int *opp_test_drivers(void)
{
    return g_opp.drivers;
}

/* racer k's driver: the record at the original's address (0: none) */
void opp_test_rec(int k, unsigned addr)
{
    g_opp.rec[k] = addr ? exe_at(addr, 0x8d) : 0;
}

/* the leading player and the rival, as cars' indices */
void opp_test_cars(int lead, int rival)
{
    g_opp.lead = g_cars[lead];
    g_opp.rival = g_cars[rival];
}

int opp_test_lead(void)
{
    int k;

    for (k = 0; k < g_ncars; k++)
        if (g_cars[k] == g_opp.lead)
            return k;
    return -1;
}

/* the settings as the port makes them (0x63f50) */
void opp_test_settings(int *out)
{
    int keep[OPP_CFG];

    memcpy(keep, g_opp.cfg, sizeof keep);
    settings();
    memcpy(out, g_opp.cfg, sizeof keep);
    memcpy(g_opp.cfg, keep, sizeof keep);
}

/* the police spots' odds, from the program as opponent_start reads them */
void opp_test_odds(void)
{
    int k;

    for (k = 0; k < 4; k++)
        g_opp.come_odds[k] = exe_i32(0xf7500 + 4 * k);
}

void opp_test_humans_only(int v)
{
    g_opp.humans_only = v;
}
