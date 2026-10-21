/* The dashboard (load_dash 0x2e8d4; NOTES §4.7, §4.11). */
#include "dash.h"
#include "racepal.h"
#include "exe.h"
#include "files.h"
#include "trackfam.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

dash_state g_dash;

static int s16(const unsigned char *p)
{
    return (short)(p[0] | p[1] << 8);
}

static int i32(const unsigned char *p)
{
    return (int)((unsigned)p[0] | (unsigned)p[1] << 8 | (unsigned)p[2] << 16 | (unsigned)p[3] << 24);
}

static const unsigned char *named(const char *name)
{
    shape s;

    if (!fsh_find(&g_dash.file, name, &s)) {
        fprintf(stderr, "renfs: the dashboard has no picture %.4s\n", name);
        return 0;
    }
    return s.head;
}

/* a picture by the name in one of the program's tables of names */
static const unsigned char *table_named(unsigned table, int i)
{
    return named((const char *)exe_at((unsigned)exe_i32(table + 4 * i), 4));
}

/* a picture's attachment block (type 0x7c) along its chain of blocks */
static const unsigned char *attachment(const unsigned char *head)
{
    unsigned next;

    while (head[0] != 0x7c) {
        next = head[1] | head[2] << 8 | head[3] << 16;
        if (!next)
            return 0;
        head += next;
    }
    return head;
}

/* a picture's points (its attachment block's from +8, x and y), each on the screen from
   the picture's centre and the corner given: n of them, or (n < 0) as many as the block
   says (+4), up to -n */
static int points(const unsigned char *pic, int x, int y, int (*pt)[2], int n)
{
    const unsigned char *b = attachment(pic);
    int i;

    if (!b)
        return 0;
    if (n < 0)
        n = b[4] < -n ? b[4] : -n;
    for (i = 0; i < n; i++) {
        pt[i][0] = s16(pic + 8) + i32(b + 8 + 8 * i) + x;
        pt[i][1] = s16(pic + 10) + i32(b + 12 + 8 * i) + y;
    }
    return n;
}

/* the pictures and places the race's dashboard draws with */
static int dash_places(const char *car, int number, int hires)
{
    char path[64];
    long len;
    int i, k;

    g_dash.dash = named("dash");
    g_dash.tach = named("tach");
    g_dash.sped = named("sped");
    if (!g_dash.dash || !g_dash.tach || !g_dash.sped)
        return 0;
    if (!hires)
        for (i = 0; i < 7; i++)
            for (k = 0; k < 2; k++) {
                g_dash.wheel[k][i] = table_named(k ? 0xea8d4 : 0xea8b8, i);
                g_dash.over_tach[k][i] = table_named(k ? 0xea928 : 0xea8f0, i);
                g_dash.over_sped[k][i] = table_named(k ? 0xea944 : 0xea90c, i);
                if (!g_dash.wheel[k][i] || !g_dash.over_tach[k][i] || !g_dash.over_sped[k][i])
                    return 0;
            }
    else {
        /* at 640x480 six each way (0xea828 .., 0x2eb68); a picture missing is the last
           one found before it of the wheel turned that way (0x12b980, 0x12b984), the
           parts over the gauges too */
        int last[2] = { 0, 0 };

        for (i = 0; i < 6; i++)
            for (k = 0; k < 2; k++) {
                shape s;
                const char *n = (const char *)exe_at((unsigned)exe_i32((k ? 0xea840 : 0xea828) + 4 * i), 4);

                if (fsh_find(&g_dash.file, n, &s))
                    last[k] = i;
                g_dash.wheel[k][i] = table_named(k ? 0xea840 : 0xea828, last[k]);
                if (!g_dash.wheel[k][i])
                    return 0;
                n = (const char *)exe_at((unsigned)exe_i32((k ? 0xea888 : 0xea858) + 4 * i), 4);
                g_dash.over_tach[k][i] = fsh_find(&g_dash.file, n, &s) ? s.head
                    : table_named(k ? 0xea888 : 0xea858, last[k]);
                n = (const char *)exe_at((unsigned)exe_i32((k ? 0xea8a0 : 0xea870) + 4 * i), 4);
                g_dash.over_sped[k][i] = fsh_find(&g_dash.file, n, &s) ? s.head
                    : table_named(k ? 0xea8a0 : 0xea870, last[k]);
            }
    }
    /* the gear lever's pictures (0x2ed90) */
    g_dash.gate = named("gate");
    g_dash.gate_back = named("gbak");
    if (!g_dash.gate || !g_dash.gate_back)
        return 0;
    for (i = 0; i < 4; i++) {
        shape s;

        g_dash.knob_n = i + 1;
        if (!fsh_find(&g_dash.file, (const char *)exe_at((unsigned)exe_i32(0xea98c + 4 * i), 4), &s))
            break;
        g_dash.knob[i] = s.head;
    }
    if (!g_dash.knob[0])
        return 0;

    g_dash.digital = number == 3 || number == 0xb;
    if (g_dash.digital)
        for (i = 0; i < 11; i++)
            if (!(g_dash.digit[i] = table_named(0xea960, i)))
                return 0;

    /* the gauges' boxes: their pictures' places and sizes */
    g_dash.tach_box[0] = s16(g_dash.tach + 0xc);
    g_dash.tach_box[1] = s16(g_dash.tach + 0xe);
    g_dash.tach_box[2] = s16(g_dash.tach + 4);
    g_dash.tach_box[3] = s16(g_dash.tach + 6);
    g_dash.sped_box[0] = s16(g_dash.sped + 0xc);
    g_dash.sped_box[1] = s16(g_dash.sped + 0xe);
    g_dash.sped_box[2] = s16(g_dash.sped + 4);
    g_dash.sped_box[3] = s16(g_dash.sped + 6);

    /* the dashboard's ten places: 8 and 9 kept from 2 and 3 */
    if (points(g_dash.dash, s16(g_dash.dash + 0xc), s16(g_dash.dash + 0xe), g_dash.place, 10) < 10)
        return 0;
    for (i = 0; i < 2; i++)
        for (k = 0; k < 2; k++)
            g_dash.place[8 + i][k] -= g_dash.place[2 + i][k];

    /* the needles' points, round each gauge from its box's corner */
    g_dash.tach_n = points(g_dash.tach, g_dash.tach_box[0], g_dash.tach_box[1], g_dash.tach_pt, -30);
    g_dash.sped_n = points(g_dash.sped, g_dash.sped_box[0], g_dash.sped_box[1], g_dash.sped_pt, -30);
    /* the gate's nine points, from its corner (0x2f123) */
    if (points(g_dash.gate, s16(g_dash.gate + 0xc), s16(g_dash.gate + 0xe), g_dash.gate_pt, 9) < 9)
        return 0;
    /* the speedometer's point 12 half way between 11 and 13 */
    for (k = 0; k < 2; k++)
            g_dash.sped_pt[12][k] = (g_dash.sped_pt[11][k] + g_dash.sped_pt[13][k]) / 2;

    snprintf(path, sizeof path, "SIMDATA/DASH/%s%s.FMM", car, hires ? "DH" : "DL");
    g_dash.fmm = file_load(path, &len);
    if (ww_count(g_dash.fmm) < 4) {
        fprintf(stderr, "renfs: %s is missing\n", path);
        return 0;
    }
    for (i = 0; i < 4; i++)
        g_dash.list[i] = ww_entry(g_dash.fmm, i);
    return 1;
}

int dash_load(const char *car, int number, int hires)
{
    char path[64];
    shape s;
    unsigned char *head;
    int i;

    dash_free();
    snprintf(path, sizeof path, "SIMDATA/DASH/%s%s.FSH", car, hires ? "DH" : "DL");
    if (!fsh_load(&g_dash.file, path)) {
        fprintf(stderr, "renfs: %s is missing\n", path);
        return 0;
    }
    /* "!pal", 8-bit ones made 6-bit; else the palette block after "dash" */
    if (fsh_find(&g_dash.file, "!pal", &s)) {
        head = (unsigned char *)s.head;
        g_dash.pal = head + 0x10;
        if (head[0] == 0x24)
            for (i = 0; i < 0x300; i++)
                g_dash.pal[i] >>= 2;
    } else if (fsh_find(&g_dash.file, "dash", &s)) {
        head = (unsigned char *)s.head;
        g_dash.pal = head + (int)((unsigned)(head[1] | head[2] << 8 | head[3] << 16)) + 0x10;
    } else {
        fprintf(stderr, "renfs: %s has no palette\n", path);
        dash_free();
        return 0;
    }
    if (!dash_places(car, number, hires)) {
        dash_free();
        return 0;
    }
    return 1;
}

void dash_free(void)
{
    fsh_free(&g_dash.file);
    free(g_dash.fmm);
    memset(&g_dash, 0, sizeof g_dash);
}

void dash_queue_colours(void)
{
    pal_queue_piece(0, 0x30, g_dash.pal);
}

void cockpit_queue_colours(void)
{
    pal_queue_piece(0xf0, 13, exe_at(0xf6d18, 39));
}
