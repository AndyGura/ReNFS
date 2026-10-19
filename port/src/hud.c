/* The race's dashboard and HUD at 320x200 (NOTES §4.11). The game draws them with EA's
   shape library into windows: the screen, or an off-screen picture (the 3D view, the
   map). A picture is copied as it is, skipping colour 255 (see-through), or taken back
   from the window (0x9d8d0 with the records 0xf79a4, 0xf79cc, 0xf7a50), cut to the
   window's clip. */
#include "hud.h"
#include "config.h"
#include "dash.h"
#include "exe.h"
#include "files.h"
#include "fsh.h"
#include "polyfill.h"
#include "racepal.h"
#include "raster.h"
#include "video.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

hud_status g_hud;
hud_picture g_hud_screen;
hud_picture g_hud_map_back;

fsh_file g_mask;                     /* MASKLO.FSH (0x145604) */
static unsigned char *g_mask_pal;    /* its "!pal", 6 bits (0x145608) */
static int g_redraw;                 /* 0x12b8d4: the whole dashboard is due */

/* the window drawn into (0xf889c): a picture and its clip */
static hud_picture g_win;
static int g_cx0, g_cy0, g_cx1, g_cy1;

enum { COPY, SEE_THROUGH, TAKE };

static void window_set(const hud_picture *p)
{
    g_win = *p;
    g_cx0 = g_cy0 = 0;
    g_cx1 = p->w;
    g_cy1 = p->h;
    raster_window(p->pix, p->w, 0, 0, p->w, p->h);
}

/* the clip (0x812ec), inside the window */
static void clip_set(int x0, int x1, int y0, int y1)
{
    g_cx0 = x0 < 0 ? 0 : x0;
    g_cy0 = y0 < 0 ? 0 : y0;
    g_cx1 = x1 > g_win.w ? g_win.w : x1;
    g_cy1 = y1 > g_win.h ? g_win.h : y1;
    raster_window(g_win.pix, g_win.w, g_cx0, g_cy0, g_cx1, g_cy1);
}

static int s16(const unsigned char *p)
{
    return (short)(p[0] | p[1] << 8);
}

/* a picture of w x h at (x, y) in the window, cut to the clip (0x9d8d0, 8 bits to 8) */
static void blit_pixels(unsigned char *pix, int w, int h, int x, int y, int how)
{
    int pitch = w, sx = 0, sy = 0, i, r;

    if (g_cy0 - y > 0) {
        sy = g_cy0 - y;
        h -= sy;
        y = g_cy0;
    }
    if (y + h - g_cy1 > 0)
        h -= y + h - g_cy1;
    if (g_cx0 - x > 0) {
        sx = g_cx0 - x;
        w -= sx;
        x = g_cx0;
    }
    if (x + w - g_cx1 > 0)
        w -= x + w - g_cx1;
    if (w <= 0 || h <= 0)
        return;
    for (r = 0; r < h; r++) {
        unsigned char *src = pix + (long)(sy + r) * pitch + sx;
        unsigned char *dst = g_win.pix + (long)(y + r) * g_win.w + x;
        if (how == COPY)
            memmove(dst, src, w);
        else if (how == TAKE)
            memmove(src, dst, w);
        else
            for (i = 0; i < w; i++)
                if (src[i] != 0xff)
                    dst[i] = src[i];
    }
}

static void blit(const unsigned char *head, int x, int y, int how)
{
    blit_pixels((unsigned char *)head + 16, s16(head + 4), s16(head + 6), x, y, how);
}

/* a picture at its own place (0x81230 copied, 0x81580 see-through) */
static void blit_own(const unsigned char *head, int how)
{
    blit(head, s16(head + 0xc), s16(head + 0xe), how);
}

static const unsigned char *named(const fsh_file *f, const char *name)
{
    shape s;

    if (!fsh_find(f, name, &s)) {
        fprintf(stderr, "renfs: the HUD has no picture %.4s\n", name);
        return 0;
    }
    return s.head;
}

/* a picture by the name in one of the program's tables of names */
static const unsigned char *table_named(unsigned table, int i)
{
    return named(&g_mask, (const char *)exe_at((unsigned)exe_i32(table + 4 * i), 4));
}

int hud_load(void)
{
    shape s;
    int i;

    hud_free();
    if (!fsh_load(&g_mask, g_german ? "SIMDATA/MISC/MASKLOG.FSH" : "SIMDATA/MISC/MASKLO.FSH")) {
        fprintf(stderr, "renfs: SIMDATA/MISC/MASKLO.FSH is missing\n");
        return 0;
    }
    if (!fsh_find(&g_mask, "!pal", &s)) {
        hud_free();
        return 0;
    }
    g_mask_pal = (unsigned char *)s.head + 0x10;
    if (s.type == 0x24)
        for (i = 0; i < 0x300; i++)
            g_mask_pal[i] >>= 2;
    pal_queue_remap(g_mask.d, g_mask_pal);
    cockpit_queue_colours();
    g_hud_screen.w = 320;
    g_hud_screen.h = 200;
    g_hud_screen.pix = g_fb;
    g_hud_map_back.w = 0x39;
    g_hud_map_back.h = 0x34;
    g_hud_map_back.pix = calloc(0x39 * 0x34, 1);
    return g_hud_map_back.pix != 0;
}

void hud_free(void)
{
    fsh_free(&g_mask);
    free(g_hud_map_back.pix);
    g_hud_map_back.pix = 0;
}

/* a time (1/60 s) as minutes, seconds and tenths in grey digits (0x2d6b4 with 0x2d3ac:
   300 taken off first, never below 0; 0x2d47c each group) */
static void digits(int v, int x, int y, int step, int n)
{
    int d[8], i;

    for (i = 0; i < n; i++) {
        d[i] = v >= 10 ? v % 10 : v;
        v = v >= 10 ? v / 10 : 0;
    }
    for (i = n - 1; i >= 0; i--, x += step)
        blit(table_named(0xf6c84, d[i]), x, y, COPY);
}

static void time_draw(int t, int x, int y, int step, int gap)
{
    int left = t > 300 ? t - 300 : 0, min, sec;

    min = left / 3600;
    left -= min * 3600;
    sec = left / 60;
    left -= sec * 60;
    digits(min, x, y, step, 2);
    x += 2 * step + gap;
    digits(sec, x, y, step, 2);
    digits(left / 6, x + 2 * step + gap, y, step, 1);
}

/* a record's time of the CONFIG.DAT's records (the track's 9 blocks of 37) */
static int record_time(int track, int k)
{
    const unsigned char *r = g_config + 0xb5 + track * 0xa6b + k * 0x27 + 0x1b;

    return (int)(r[0] | r[1] << 8 | r[2] << 16 | (unsigned)r[3] << 24);
}

/* the status bar (0x2f2a4): its picture, what is timed, the record to beat, laps or
   miles, the kind of race; a black strip for the first 360 steps */
static void status_bar(void)
{
    if (!g_hud.shown || g_hud.steps < 360) {
        raster_fill(0, 0, 320, 16, g_marker_line);
        return;
    }
    blit_own(named(&g_mask, "stat"), COPY);
    if (g_hud.mode == 1) {
        blit_own(named(&g_mask, "rtet"), COPY);
        time_draw(record_time(g_hud.track, 6 + (g_hud.circuit ? 10 * g_hud.table : 0)) + 300,
                  0x11, 3, 6, 2);
        blit_own(named(&g_mask, "splt"), COPY);
        blit_own(table_named(0xf6cb0, g_hud.ahead >= 0 ? 12 : 13), COPY);
    } else if (g_hud.mode == 0) {
        blit_own(named(&g_mask, "segt"), COPY);
        if (g_hud.splits > 0) {
            blit_own(named(&g_mask, "splt"), COPY);
            blit_own(table_named(0xf6cb0, g_hud.ahead >= 0 ? 12 : 13), COPY);
        } else
            blit_own(named(&g_mask, g_hud.circuit ? "lap#" : "mile"), COPY);
        time_draw((g_hud.circuit ? g_hud.best_lap : record_time(g_hud.track, g_hud.segment)) + 300,
                  0x11, 3, 6, 2);
    }
    /* (mode 2, the acceleration test: 0x145780 and 0x2d8c0, not yet) */
    if (g_hud.mode < 2)
        blit_own(named(&g_mask, g_hud.race_type <= 1 ? "head" : "sing"), COPY);
    /* (0x2d150: every number shown marked for drawing again) */
}

void hud_redraw(void)
{
    static const int corners[8] = { 0, 0, 0x38, 0, 0x38, 0x3b, 0, 0x3b };
    shape dash;

    window_set(&g_hud_screen);
    clip_set(0, g_win.w, 0, g_win.h);          /* 0x2d314, as the race runs */
    if (fsh_find(&g_dash.file, "dash", &dash))
        blit(dash.head, 0, 0, SEE_THROUGH);
    /* the map's box: the dashboard under it, taken and darkened twice (0x84248) */
    blit_pixels(g_hud_map_back.pix, g_hud_map_back.w, g_hud_map_back.h, 0x104, 0x7f, TAKE);
    window_set(&g_hud_map_back);
    poly_fill(4, corners, poly_shade_rows, 0);
    poly_fill(4, corners, poly_shade_rows, 0);
    window_set(&g_hud_screen);
    status_bar();
    g_redraw = 0;
}
