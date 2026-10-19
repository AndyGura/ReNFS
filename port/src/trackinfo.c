/* SIMDATA/MISC/<track>INFO: text, a value a line, '#' and empty lines skipped. Read only
   with the other texture set; with the usual one the game takes its built-in table. */
#include "trackinfo.h"
#include "exe.h"
#include "files.h"
#include "racepal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

track_info g_info;

#define BUILT_IN 0xec5dc               /* 33 ints a track, in the order of `names` */
#define POWERS 0xec3e0                 /* 1, 10, 100 .. */

static const char *const names[] = {
    "cl1", "cl2", "cl3", "al1", "al2", "al3", "cy1", "cy2", "cy3",
    "tr1", "tr2", "tr3", "tr4", "tr5", "tr6", "tr7"
};

/* the file read as DOS's text mode reads it, a line at a time (0x38c50) */
typedef struct {
    const char *p, *end;
    char line[0x80];
} reader;

static int read_line(reader *r)
{
    int n = 0;
    if (r->p >= r->end)
        return 0;                      /* (the line before stays: the game reads it again) */
    while (r->p < r->end && n < 0x7e) {
        char c = *r->p++;
        if (c == '\r' && r->p < r->end && *r->p == '\n')
            continue;
        r->line[n++] = c;
        if (c == '\n')
            break;
    }
    r->line[n] = 0;
    return 1;
}

static char *next_value(reader *r)
{
    while (read_line(r))
        if (r->line[0] != '#' && r->line[0] != '\n' && r->line[0] != '\r')
            break;
    return r->line;
}

/* 0x41b68: a decimal number into 16.16, four digits kept after the fraction's zeros */
static int fixed(char *s)
{
    int sign = 1, whole = 0, digits, zeros;
    unsigned frac;
    char *dot, *d;
    while (*s == ' ')
        s++;
    if (*s == '-') {
        sign = -1;
        s++;
    }
    dot = strchr(s, '.');
    if (!dot)
        return (int)((unsigned)(atoi(s) * sign) << 16);
    *dot = 0;
    if (dot != s)
        whole = (int)((unsigned)atoi(s) << 16);
    d = dot + 1;
    digits = 1;
    for (s = d + 1; *s >= '0' && *s <= '9'; s++)
        digits++;
    for (zeros = 0; *d == '0'; d++)
        zeros++;
    if (digits > zeros + 4) {
        digits = zeros + 4;
        d[4] = 0;
    }
    frac = ((unsigned)atoi(d) << 16) / (unsigned)exe_i32(POWERS + 4 * digits);
    return (int)((frac + (unsigned)whole) * (unsigned)sign);
}

/* "a b c" on one line: a << 10 | b << 5 | c */
static int colour(const char *s)
{
    int v = 0, i;
    for (i = 0; i < 3; i++) {
        while (*s == ' ' || *s == '\t' || *s == '\n')
            s++;
        v |= atoi(s) << (10 - 5 * i);
        while (*s && *s != ' ' && *s != '\t' && *s != '\n')
            s++;
    }
    return v;
}

static int from_file(const char *name)
{
    track_info *t = &g_info;
    char path[64];
    long len;
    unsigned char *f;
    reader r;
    int g, k;

    snprintf(path, sizeof path, "SIMDATA/MISC/%sInfo", name);
    f = file_load(path, &len);
    if (!f)
        return 0;
    r.p = (const char *)f;
    r.end = r.p + len;
    r.line[0] = 0;
    t->vert_near = fixed(next_value(&r));
    t->vert_far = fixed(next_value(&r));
    t->z_near = fixed(next_value(&r));
    t->z_far = fixed(next_value(&r));
    t->height_near = fixed(next_value(&r));
    t->height_far = fixed(next_value(&r));
    t->radius_near = fixed(next_value(&r));
    t->radius_far = fixed(next_value(&r));
    t->colour_top = colour(next_value(&r));
    t->colour_bottom = colour(next_value(&r));
    t->ring_type = atoi(next_value(&r));
    t->ring_y = atoi(next_value(&r));
    t->ring_height = atoi(next_value(&r));
    for (g = 0; g < 3; g++)
        for (k = 0; k < 5; k++)
            t->tint[g][k] = atoi(next_value(&r)) << (k == 4 ? 8 : 0);
    t->last = atoi(next_value(&r));
    if (t->last)
        t->ring_type = 1;
    free(f);
    return 1;
}

static int built_in(const char *name)
{
    track_info *t = &g_info;
    unsigned i, a;
    int g, k;
    for (i = 0; i < sizeof names / sizeof names[0]; i++)
        if (strcasecmp(name, names[i]) == 0)
            break;
    if (i == sizeof names / sizeof names[0])
        return 0;
    a = BUILT_IN + i * 0x84;
    t->vert_near = (int)((unsigned)exe_i32(a) << 16);
    t->vert_far = (int)((unsigned)exe_i32(a + 4) << 16);
    t->z_near = (int)((unsigned)exe_i32(a + 8) << 16);
    t->z_far = (int)((unsigned)exe_i32(a + 0xc) << 16);
    t->height_near = (int)((unsigned)exe_i32(a + 0x10) << 16);
    t->height_far = (int)((unsigned)exe_i32(a + 0x14) << 16);
    t->radius_near = (int)((unsigned)exe_i32(a + 0x18) << 16);
    t->radius_far = (int)((unsigned)exe_i32(a + 0x1c) << 16);
    t->colour_top = exe_i32(a + 0x20) << 10 | exe_i32(a + 0x24) << 5 | exe_i32(a + 0x28);
    t->colour_bottom = exe_i32(a + 0x2c) << 10 | exe_i32(a + 0x30) << 5 | exe_i32(a + 0x34);
    t->ring_type = exe_i32(a + 0x38);
    t->ring_y = exe_i32(a + 0x3c);
    t->ring_height = exe_i32(a + 0x40);
    for (g = 0; g < 3; g++)
        for (k = 0; k < 5; k++)
            t->tint[g][k] = exe_i32(a + 0x44 + 20 * g + 4 * k) << (k == 4 ? 8 : 0);
    t->last = exe_i32(a + 0x80);
    return 1;
}

int trackinfo_load(const char *name, int family, int high)
{
    track_info *t = &g_info;
    memset(t, 0, sizeof *t);
    /* the game's switch 0xec5d8 is 1: the file when there is one */
    if (family == 1 || !from_file(name))
        if (!built_in(name)) {
            fprintf(stderr, "renfs: no sky settings for the track %s\n", name);
            return 0;
        }
    if (!high) {
        t->ring_height = t->ring_height * 200 / 480;
        t->ring_y = t->ring_y * 200 / 480;
    }
    t->last_used = name[0] == 't' || family != 1 ? t->last : 0;
    if (family >= 0 && family < 3)
        pal_set_tint(t->tint[family][0], t->tint[family][1], t->tint[family][2],
                     t->tint[family][3], t->tint[family][4]);
    return 1;
}
