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
#include "track.h"
#include "video.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

hud_status g_hud;
hud_picture g_hud_screen;
hud_picture g_hud_map_back;
hud_picture g_hud_view;
hud_picture g_hud_mirror;
hud_picture g_hud_map;

fsh_file g_mask;                     /* MASKLO.FSH (0x145604) */
static unsigned char *g_mask_pal;    /* its "!pal", 6 bits (0x145608) */
static int g_redraw;                 /* 0x12b8d4: the whole dashboard is due */
static int g_wheel_shown;            /* 0x12b8d0: the wheel's picture on the screen (0x63
                                        none), turned left < 0 */
static int g_radar_shown;            /* 0x12b938: the radar's level on the screen */
static int g_bar_at;                 /* 0x12b944: the status bar drawn again after this
                                        step */
static int g_bar_black;              /* 0x12b94c: its strip to be blacked first */
static int g_map_back_taken;         /* 0x12ba0c: the map's background taken */
static int g_cop_flash;              /* 0x12ba04: the police's dot, 0..10 */

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
    g_hud_view.w = 320;                    /* 0x143e30, 0x143e34 */
    g_hud_view.h = 200;
    g_hud_view.pix = calloc(320 * 200, 1);
    g_hud_map.w = 0x3a;
    g_hud_map.h = 0x36;
    g_hud_map.pix = calloc(0x3a * 0x36, 1);
    g_redraw = 1;
    g_bar_black = 1;
    g_map_back_taken = 0;
    return g_hud_map_back.pix && g_hud_view.pix && g_hud_map.pix;
}

void hud_free(void)
{
    fsh_free(&g_mask);
    free(g_hud_map_back.pix);
    free(g_hud_view.pix);
    free(g_hud_map.pix);
    g_hud_map_back.pix = g_hud_view.pix = g_hud_map.pix = 0;
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
    /* every number shown marked for drawing again (0x2d150) */
    g_radar_shown = -99;
    g_bar_at = 0x7d000;
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
    g_map_back_taken = 1;
    window_set(&g_hud_screen);
    status_bar();
    g_wheel_shown = 0x63;
    g_redraw = 0;
}

void hud_redraw_due(void)
{
    g_redraw = 1;
}

static int u16(const unsigned char *p)
{
    return p[0] | p[1] << 8;
}

/* a picture through a BAMF list into the window (0x8b8f0): each span from its place in
   the picture to its place in the window (by 64 KB bank), going on at the start of the
   next row of both where the window's row ends. Only the 1:1 lists so far */
static void bamf_copy(const hud_picture *from, const unsigned char *list)
{
    const unsigned char *p = list + 0x14;
    int banks, b, n, i, sr, sc, dr, dc, len, k;
    unsigned so, d;

    if (!list || memcmp(list, "BAMF", 4) != 0
        || ((unsigned)u16(list + 0xc) | (unsigned)u16(list + 0xe) << 16) != 0x10000)
        return;
    banks = u16(list + 8);
    for (b = 0; b < banks; b++) {
        n = u16(p);
        p += 2;
        for (i = 0; i < n; i++, p += 8) {
            so = (unsigned)u16(p) | (unsigned)u16(p + 2) << 16;
            d = (unsigned)b << 16 | (unsigned)u16(p + 4);
            len = u16(p + 6);
            sr = (int)(so / (unsigned)from->w);
            sc = (int)(so % (unsigned)from->w);
            dr = (int)(d / (unsigned)g_win.w);
            dc = (int)(d % (unsigned)g_win.w);
            while (len > 0 && sr < from->h && dr < g_win.h) {
                k = dc + len > g_win.w ? g_win.w - dc : len;
                memmove(g_win.pix + (long)dr * g_win.w + dc, from->pix + (long)sr * from->w + sc, k);
                len -= k;
                sr++, dr++;
                sc = dc = 0;
            }
        }
    }
}

/* how far the wheel is turned, 0 .. 6 (5 limits at 320x200, 3 never) */
static int wheel_turn(int steer)
{
    int a = steer < 0 ? -steer : steer;

    if (a < 0x74ccc)
        return a < 0x26666 ? 0 : a < 0x66666 ? 1 : 2;
    return a < 0x154ccc ? 4 : a < 0x1c6666 ? 5 : 6;
}

/* a gauge's needle: from its centre (the last point) to t/n of the way from one point
   to the next, n 1024 or 10 (20 for km/h), the needle EA's smoothed line in the car's
   colour (0x8b195) a pixel wide (0xea71c, 0xea720 less one at 320x200) */
static void needle(int (*pt)[2], int count, int i, int t, int n, int number)
{
    int cx = pt[count - 1][0], cy = pt[count - 1][1], ex, ey;

    if (n == 1024) {
        ex = pt[i][0] + ((pt[i + 1][0] - pt[i][0]) * t >> 10);
        ey = pt[i][1] + ((pt[i + 1][1] - pt[i][1]) * t >> 10);
    } else {
        ex = pt[i][0] + (pt[i + 1][0] - pt[i][0]) * t / n;
        ey = pt[i][1] + (pt[i + 1][1] - pt[i][1]) * t / n;
    }
    poly_wide_line(cx << 16, cy << 16, ex << 16, ey << 16, g_needle_colour,
                   (const unsigned char (*)[256])g_needle_blend,
                   exe_i32(0xea71c + 20 * number) - 0x10000,
                   exe_i32(0xea720 + 20 * number) - 0x10000, g_hud_view.w, g_hud_view.h);
}

/* the speed in three digits, the leading noughts blank (cars 3 and 11) */
static void speed_digits(int v, int number)
{
    int d[3], i, x = number == 3 ? 0x99 : 0x58, y = number == 3 ? 0x98 : 0x62;
    int step = number == 3 ? 4 : 7, leading = 1;

    for (i = 0; i < 3; i++) {
        d[i] = v >= 10 ? v % 10 : v;
        v = v >= 10 ? v / 10 : 0;
    }
    for (i = 2; i >= 0; i--, x += step) {
        const unsigned char *pic;

        if (d[i] == 0 && i > 0 && leading)
            pic = g_dash.digit[10];
        else {
            pic = g_dash.digit[d[i]];
            leading = 0;
        }
        blit(pic, x, y, number == 0xb ? SEE_THROUGH : COPY);
    }
}

/* the gauges' needles (0x2fc40): the engine's turns, the speed */
static void needles(const hud_car *c)
{
    int rpm = c->rpm, v;
    long long p;

    if (rpm > c->redline + 0x3d4)
        rpm = c->redline + 0x3d4;
    needle(g_dash.tach_pt, g_dash.tach_n, rpm >> 10, rpm & 0x3ff, 1024, c->number);

    p = (long long)(c->speed < 0 ? -c->speed : c->speed) * (g_hud.kmh ? 0x39999 : 0x23d70);
    v = ((int)(p >> 16) + (int)((p >> 15) & 1)) >> 16;
    if (g_dash.digital)
        speed_digits(v, c->number);
    else if (v > 0)
        needle(g_dash.sped_pt, g_dash.sped_n, v / (g_hud.kmh ? 20 : 10), v % (g_hud.kmh ? 20 : 10),
               g_hud.kmh ? 20 : 10, c->number);
    else
        needle(g_dash.sped_pt, g_dash.sped_n, 0, 0, 10, c->number);
}

/* the radar detector's lights (0x30214): the big one at place 2 while it hears
   anything, lit or dark as it flashes; the five small ones at places 3..7, as many lit as
   the level, drawn when it changes */
static void radar(void)
{
    int n = g_hud.radar > 5 ? 5 : g_hud.radar, i;
    const unsigned char *big = named(&g_dash.file, g_hud.radar_flash ? "bglt" : "bgdk");
    const unsigned char *lit = named(&g_dash.file, "smlt"), *dark = named(&g_dash.file, "smdk");

    if (g_hud.radar)
        blit(big, g_dash.place[2][0], g_dash.place[2][1], COPY);
    if (g_hud.radar != g_radar_shown) {
        if (!g_hud.radar)
            blit(named(&g_dash.file, "bgdk"), g_dash.place[2][0], g_dash.place[2][1], COPY);
        for (i = 0; i < 5; i++)
            if (g_hud.radar && i < n)
                blit(lit, g_dash.place[3 + i][0], g_dash.place[3 + i][1], COPY);
            else if (!g_hud.radar || n <= 4)
                blit(dark, g_dash.place[3 + i][0], g_dash.place[3 + i][1], COPY);
    }
    g_radar_shown = g_hud.radar;
}

/* the start's lights across the status bar (0x2dbbc), a second (60 steps) each: three
   red then two yellow lit in turn, all five green from step 300 */
static void start_lights(void)
{
    int k = g_hud.steps / 60, i;

    if (g_bar_black) {
        raster_fill(0, 0, 320, 16, g_marker_line);
        g_bar_black = 0;
    }
    for (i = 0; i < 5; i++) {
        const char *name = g_hud.steps >= 300 ? "sgrn"
                         : i < 3 ? (i > k ? "srdd" : "srdl") : (i > k ? "syld" : "syll");

        /* (each light's sound as it comes on, 0x5257c: not yet) */
        blit(named(&g_mask, name), 0x41 * i, 0, COPY);
    }
    g_bar_at = g_hud.steps + 10;
}

/* the status bar's numbers (0x30d8c): before the start its lights; then the speed, the
   gear, the place, the time and the laps or miles (not yet: they wait for the race's
   timing) */
static void bar_numbers(void)
{
    if (!g_hud.shown || g_hud.steps <= 360) {
        if (g_hud.steps < 360)
            start_lights();
        return;
    }
    g_bar_black = 1;
}

void hud_frame(const hud_car *c)
{
    int turn = wheel_turn(c->steer), shown = c->steer < 0 ? -turn : turn;
    /* the wheel's pictures turned left: steering left, or right on a mirrored track */
    int left = g_hud.mirrored ? c->steer >= 0 : c->steer < 0;

    /* (0x13c17c: nothing; the tickets 0xea81c, 0xea820: 0x30424; 0x306e8: the
       dashboard's pictures loaded again: not yet) */
    if (g_redraw)
        hud_redraw();
    window_set(&g_hud_screen);
    bamf_copy(&g_hud_view, g_dash.list[g_hud.detail & 3]);
    if (g_hud.mirror && g_hud_mirror.pix)          /* 0x5b040 */
        blit_pixels(g_hud_mirror.pix, g_hud_mirror.w, g_hud_mirror.h,
                    g_dash.place[0][0], g_dash.place[0][1], COPY);
    clip_set(0, 0x102, 0, g_hud.running ? 0xc8 : 0xb4);
    if (shown != g_wheel_shown)
        blit_own(g_dash.wheel[!left][turn], SEE_THROUGH);
    g_wheel_shown = shown;

    /* the gauges in the view's picture, then their boxes to the screen */
    window_set(&g_hud_view);
    blit_own(g_dash.tach, COPY);
    blit_own(g_dash.sped, c->number == 0xb ? SEE_THROUGH : COPY);
    needles(c);
    blit_own(g_dash.over_tach[!left][turn], SEE_THROUGH);
    blit_own(g_dash.over_sped[!left][turn], SEE_THROUGH);
    window_set(&g_hud_screen);
    clip_set(g_dash.tach_box[0], g_dash.tach_box[0] + g_dash.tach_box[2],
             g_dash.tach_box[1], g_dash.tach_box[1] + g_dash.tach_box[3]);
    blit_pixels(g_hud_view.pix, g_hud_view.w, g_hud_view.h, 0, 0, COPY);
    clip_set(g_dash.sped_box[0], g_dash.sped_box[0] + g_dash.sped_box[2],
             g_dash.sped_box[1], g_dash.sped_box[1] + g_dash.sped_box[3]);
    blit_pixels(g_hud_view.pix, g_hud_view.w, g_hud_view.h, 0, 0, COPY);

    window_set(&g_hud_screen);
    /* (0x2e4f0: the gear lever's gate, in the map's box when the map is off: not yet) */
    radar();
    /* 0x2f644: the status bar again some steps after the start's lights (and the keys
       that switch it, the map and the view: not yet) */
    if (g_hud.steps > g_bar_at)
        status_bar();
    bar_numbers();
    if (g_hud.map && g_hud_map.pix)
        blit_pixels(g_hud_map.pix, g_hud_map.w, g_hud_map.h, 0x103, 0x7e, COPY);
    /* (0x5c5d0: the replay's buttons, not yet) */
}

/* the map (0x31cb4), 58 x 54 at 320x200: positions in 1/2^20 of the world's units round
   the viewed car's node, turned by its heading, x the other way on a mirrored track */
#define MAP_SHIFT 20
#define MAP_X 0x1d                     /* its centre: 1 + 0x1c */
#define MAP_Y 0x1d                     /* the road's; the dots' 1 + 0x19 */
#define MAP_DOT_Y 0x1a

/* a times b, 16.16, rounded by the last bit out */
static int mulr(int a, int b)
{
    long long p = (long long)a * b;
    return (int)(p >> 16) + (int)((p >> 15) & 1);
}

static int rd32(const unsigned char *p)
{
    return (int)((unsigned)p[0] | (unsigned)p[1] << 8 | (unsigned)p[2] << 16 | (unsigned)p[3] << 24);
}

static const unsigned char *map_node(int node)
{
    return g_track.nodes + (long)(node & (int)g_track.node_mask) * 0x24;
}

typedef struct {
    int px, pz, c, s, mir;
} map_view;

static void map_point(const map_view *v, int node, int *x, int *y)
{
    const unsigned char *r = map_node(node);
    int dx = (rd32(r + 8) >> MAP_SHIFT) - v->px, dz = (rd32(r + 0x10) >> MAP_SHIFT) - v->pz;

    *x = (mulr(v->c, dx) - mulr(v->s, dz)) * v->mir;
    *y = mulr(v->s, dx) + mulr(v->c, dz);
}

/* EA's arctangent (0x8bb7c): a turn of 1024, from the y axis towards x; the smaller over
   the larger as a 32-bit fraction, rounded to 1/256, into the table of an eighth of a
   turn (0x12572c) */
static int atan_1024(int x, int y)
{
    unsigned ax = (unsigned)(x < 0 ? -x : x), ay = (unsigned)(y < 0 ? -y : y), r;
    int f = (y < 0 ? 8 : 0) | (x < 0 ? 0x10 : 0), t;

    if (ax == ay)
        t = 0x80;
    else {
        if (ax > ay) {
            unsigned w = ax;
            ax = ay;
            ay = w;
            f |= 4;
        }
        r = (unsigned)(((unsigned long long)ax << 32) / ay);
        t = *exe_at(0x12572c + (r >> 24) + (r >> 23 & 1), 1);
    }
    switch (f) {
    case 4:
        return 0x100 - t;
    case 8:
        return 0x200 - t;
    case 0xc:
        return t + 0x100;
    case 0x10:
        return -t;
    case 0x14:
        return t - 0x100;
    case 0x18:
        return t - 0x200;
    case 0x1c:
        return -t - 0x100;
    }
    return t;
}

/* a racer's dot (0x32ac0): a square of 4 in its colour under the "pos1" ring, kept off
   the map's top and bottom rows */
static void map_dot(int x, int y, int colour)
{
    clip_set(1, 0x39, 6, 0x30);
    raster_fill(x - 2, y + 1, 4, 4, colour);
    blit(named(&g_mask, "pos1"), x - 3, y, SEE_THROUGH);
    clip_set(1, 0x39, 1, 0x35);
}

/* the nodes between two racers, the shorter way (round a circuit too) */
static int map_apart(int d)
{
    int a = abs(d), b = abs(d + g_track.nodes_len), c = abs(g_track.nodes_len - d);

    return a < b && a < c ? a : b < c ? b : c;
}

void hud_map(const hud_map_state *m)
{
    int track = g_hud.track, range = (short)exe_i32(0xea694 + 4 * track);
    int count = g_hud.race_type > 1 ? 8 : g_hud.race_type == 0 ? 1 : 2;
    int i, k, node, x, y, x0, y0, x1, y1, a, c4, s4, t;
    int seg_end = track < 3 ? (short)(exe_at(0xeaab4 + 2 * (track * 3 + g_hud.segment), 2)[0]
                                      | exe_at(0xeaab4 + 2 * (track * 3 + g_hud.segment), 2)[1] << 8)
                            : 0;
    const unsigned char *r = map_node(m->node);
    map_view v;

    if (!g_hud.map)
        return;
    v.mir = g_hud.mirrored ? -1 : 1;
    v.px = rd32(r + 8) >> MAP_SHIFT;
    v.pz = rd32(r + 0x10) >> MAP_SHIFT;
    a = (int)((unsigned)(short)(r[0x18] | r[0x19] << 8) << 10) >> 14;
    v.c = fix_cos(a);
    v.s = fix_sin(a);

    window_set(&g_hud_map);
    if (m->driver) {
        if (g_map_back_taken)
            blit_pixels(g_hud_map_back.pix, g_hud_map_back.w, g_hud_map_back.h, 1, 1, COPY);
    } else
        blit(named(&g_mask, "mpbd"), 1, 1, SEE_THROUGH);
    /* the frame (0x8a0f0) */
    raster_fill(0, 0, 0x3a, 1, g_map_frame);
    raster_fill(0x39, 1, 1, 0x34, g_map_frame);
    raster_fill(0, 1, 1, 0x34, g_map_frame);
    raster_fill(0, 0x35, 0x3a, 1, g_map_frame);
    clip_set(1, 0x39, 1, 0x35);

    /* the road: from `range` nodes behind, then 16 apart (City-like circuit: 13.5),
       open roads kept to their first node and 60 past the segment's end */
    node = (short)(m->node - range);
    if (node < 0)
        node = track < 3 ? 0 : (short)(node + g_track.nodes_len);
    map_point(&v, node, &x0, &y0);
    for (i = (short)exe_i32(0xea6b8 + 4 * track); i <= exe_i32(0xea6dc + 4 * track); i++) {
        node = (short)(track == 3 ? (i * 0x6c >> 3) + (short)m->node : m->node + i * 16);
        if (track < 3) {
            if (node < 0)
                node = 0;
            else if (node > seg_end + 0x3c)
                node = (short)(seg_end + 0x3c);
        } else if (node < 0)
            node = (short)(node + g_track.nodes_len);
        map_point(&v, node, &x, &y);
        poly_line(MAP_X + x0, MAP_Y - y0, MAP_X + x, MAP_Y - y, g_hud_colour[HUD_WHITE]);
        x0 = x;
        y0 = y;
    }

    /* the segment's end (the circuits' start): a short red bar across the road */
    map_point(&v, seg_end, &x0, &y0);
    map_point(&v, seg_end + 8, &x1, &y1);
    t = y1 - y0;
    a = atan_1024(x1 - x0, t + (t >> 2));
    c4 = mulr(4, -fix_cos(a));
    s4 = mulr(4, fix_sin(a));
    t = s4 - (s4 >> 2) + (s4 >> 4);
    poly_line(x0 + 1 + (c4 >> 1) + 0x1c, MAP_Y - (y0 + (t >> 1)),
              x0 + 1 - (c4 - (c4 >> 1)) + 0x1c, MAP_Y - (y0 - (t - (t >> 1))),
              g_hud_colour[HUD_RED]);

    /* the racers, the last first: a dot on the map, else an arrow at its edge, above
       when ahead of the player (behind: below) */
    for (k = count - 1; k >= 0; k--) {
        int apart = (short)map_apart((short)(m->node - m->nodes[k]));
        int colour = g_marker_colour[k], bx, ahead;

        if (apart <= range) {
            map_point(&v, m->nodes[k], &x, &y);
            if ((x < 0 ? -x : x) < 0x1c && (y < 0 ? -y : y) < 0x17) {
                map_dot(MAP_X + x, MAP_DOT_Y - y, colour);
                continue;
            }
        }
        bx = g_hud.race_type == 1 ? 1 + 0x2e : 1 + 0x31 - 7 * k;
        ahead = g_hud.race_type == 1 ? m->place == 1 : m->places[k] > m->place;
        if (ahead) {
            int tri[6] = { bx + 1, 0x31, bx + 5, 0x31, bx + 3, 0x33 };
            poly_fill(3, tri, poly_solid_rows, colour);
            blit(named(&g_mask, "aro1"), bx, 0x30, SEE_THROUGH);
        } else {
            int tri[6] = { bx + 1, 4, bx + 5, 4, bx + 3, 2 };
            poly_fill(3, tri, poly_solid_rows, colour);
            blit(named(&g_mask, "aru1"), bx, 0, SEE_THROUGH);
        }
    }

    /* the police, near enough: its dot flashing */
    if (m->cop) {
        int apart = (short)(m->node - m->cop_node);

        apart = (short)(apart < 0 ? -apart : apart);
        if (apart < 0x140) {
            map_point(&v, m->cop_node, &x, &y);
            if ((x < 0 ? -x : x) < 0x1c && (y < 0 ? -y : y) < 0x1d) {
                if (++g_cop_flash > 10)
                    g_cop_flash = 0;
                map_dot(MAP_X + x, MAP_DOT_Y - y, g_marker_colour[g_cop_flash > 5 ? 0 : 6]);
            }
        }
    }
}
