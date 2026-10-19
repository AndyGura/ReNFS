/* EA's fonts. The letters are 4-bit levels (0 nothing .. 15 the colour), smoothed at the
   edges; on the 256-colour screen each level over each colour already there becomes the
   nearest palette entry, through a table made when the colour is chosen. */
#include "font.h"
#include "files.h"
#include "video.h"
#include <stdlib.h>
#include <string.h>

text_style g_text;
const unsigned char *g_inv;
unsigned char g_colour_off[256] = { [254] = 1, [255] = 1 };
static int g_clip_on, g_clip[4];

void text_clip(int x0, int y0, int x1, int y1)
{
    g_clip[0] = x0;
    g_clip[1] = y0;
    g_clip[2] = x1;
    g_clip[3] = y1;
    g_clip_on = 1;
}

void text_clip_off(void)
{
    g_clip_on = 0;
}

static unsigned u16(const unsigned char *p) { return p[0] | p[1] << 8; }
static unsigned long u32(const unsigned char *p) { return u16(p) | (unsigned long)u16(p + 2) << 16; }

int font_load(font *f, const char *rel)
{
    memset(f, 0, sizeof *f);
    f->d = file_load(rel, &f->len);
    if (!f->d || f->len < 0x20 || memcmp(f->d, "FNTF", 4)) {
        font_free(f);
        return 0;
    }
    f->count = (int)u16(f->d + 0xa);
    f->x_org = (signed char)f->d[0x10];
    f->y_org = (signed char)f->d[0x11];
    f->line_a = (signed char)f->d[0x12];
    f->line_b = (signed char)f->d[0x13];
    if (u32(f->d + 0x14) + 11UL * f->count > (unsigned long)f->len || u32(f->d + 0x1c) + 16 > (unsigned long)f->len) {
        font_free(f);
        return 0;
    }
    f->chars = f->d + u32(f->d + 0x14);
    f->bitmap = f->d + u32(f->d + 0x1c);
    /* the bitmap is a shape: 4 bits a pixel (type 0x7a), rows of whole bytes */
    f->row_bytes = (4 * (int)u16(f->bitmap + 4) + 7) / 8;
    return 1;
}

void font_free(font *f)
{
    free(f->d);
    memset(f, 0, sizeof *f);
}

/* 0x94174: the character's record, 11 bytes: u16 code, u8 width, u8 height, u16 x, u16 y
   in the bitmap, i8 advance, i8 x offset, i8 y offset. At its place from 0x20 if it is
   there, else searched */
static const unsigned char *glyph(const font *f, int c)
{
    int i = c - 0x20;
    if (i >= 0 && i < f->count && (int)u16(f->chars + 11 * i) == c)
        return f->chars + 11 * i;
    for (i = 0; i < f->count; i++)
        if ((int)u16(f->chars + 11 * i) == c)
            return f->chars + 11 * i;
    return 0;
}

int nearest_colour(unsigned long rgb)
{
    const unsigned char *p = video_palette8();
    int r = (int)(rgb >> 16 & 0xff), g = (int)(rgb >> 8 & 0xff), b = (int)(rgb & 0xff);
    long best = 99999;
    int found = 0;
    if (g_inv)
        return g_inv[(r >> 3) << 10 | (g >> 3) << 5 | b >> 3];
    for (int i = 0; i < 256; i++) {
        if (g_colour_off[i])
            continue;
        long d = (long)(g - p[3 * i + 1]) * (g - p[3 * i + 1]) + (long)(r - p[3 * i]) * (r - p[3 * i]) +
                 (long)(b - p[3 * i + 2]) * (b - p[3 * i + 2]);
        if (d < best) {
            best = d;
            found = i;
        }
    }
    return found;
}

/* inverse_colormap 0x819c0 (Spencer W. Thomas's, Graphics Gems II): each allowed colour
   claims the cells nearer to it than to the colours before it, scanning out from its
   own cell along blue, then green, then red, and stopping a scan at the first cell it
   cannot claim after some it could. Exact but for rare cells its regions' edges miss
   (two on one race's palette): the original's table, so the port's */
#define CELLS 32
static struct {
    unsigned dist[CELLS * CELLS * CELLS];     /* the squared distance of the colour held */
    unsigned char *map;
    int index;                                /* the colour claiming */
    int rc, gc, bc;                           /* its cell */
    long rinc, ginc0, binc0;                  /* the distance's steps from there */
    long cdist;
    int at;                                   /* its cell's place */
    /* the red loop's row, the green loop's column (0x15d968.., 0x15d984..) */
    long rdist, gdist;
    int rat, gat;
    int g_here, g_min, g_max;                 /* 0x15d9c8, 0x15d9c4, 0x15d9c0 */
    long g_inc;                               /* 0x15d9bc */
    int b_here, b_min, b_max;                 /* 0x15d9d8, 0x15d9d4, 0x15d9d0 */
    long b_inc;                               /* 0x15d9cc */
} g_ic;

#define XSQR 64                               /* (256 / CELLS)^2 */

/* 0x82000 */
static int blue_loop(int restart)
{
    int detect = 0, b, at;
    long bdist, bxx;

    if (restart) {
        g_ic.b_here = g_ic.bc;
        g_ic.b_min = 0;
        g_ic.b_max = CELLS - 1;
        g_ic.b_inc = g_ic.binc0;
    }
    for (b = g_ic.b_here, bdist = g_ic.gdist, bxx = g_ic.b_inc, at = g_ic.gat; b <= g_ic.b_max;
         b++, at++, bdist += bxx, bxx += 2 * XSQR)
        if (g_ic.dist[at] > (unsigned)bdist) {
            if (b > g_ic.b_here) {
                g_ic.b_here = b;
                g_ic.gat = at;
                g_ic.gdist = bdist;
                g_ic.b_inc = bxx;
            }
            detect = 1;
            break;
        }
    for (; b <= g_ic.b_max && g_ic.dist[at] > (unsigned)bdist; b++, at++, bdist += bxx, bxx += 2 * XSQR) {
        g_ic.dist[at] = (unsigned)bdist;
        g_ic.map[at] = (unsigned char)g_ic.index;
    }
    b = g_ic.b_here - 1;
    bxx = g_ic.b_inc - 2 * XSQR;
    bdist = g_ic.gdist - bxx;
    at = g_ic.gat - 1;
    if (!detect)
        for (; b >= g_ic.b_min; b--, at--, bxx -= 2 * XSQR, bdist -= bxx)
            if (g_ic.dist[at] > (unsigned)bdist) {
                g_ic.b_here = b;
                g_ic.gat = at;
                g_ic.gdist = bdist;
                g_ic.b_inc = bxx;
                detect = 1;
                break;
            }
    for (; b >= g_ic.b_min && g_ic.dist[at] > (unsigned)bdist; b--, at--, bxx -= 2 * XSQR, bdist -= bxx) {
        g_ic.dist[at] = (unsigned)bdist;
        g_ic.map[at] = (unsigned char)g_ic.index;
    }
    return detect;
}

/* 0x81d5c: the blue loop moves gat / gdist to where it found the colour's run, and
   the column goes on from there; gcat / gcdist go on from where it was */
static int green_loop(int restart)
{
    int detect = 0, first, g, gcat;
    long gcdist, gxx;

    if (restart) {
        g_ic.g_here = g_ic.gc;
        g_ic.g_min = 0;
        g_ic.g_max = CELLS - 1;
        g_ic.g_inc = g_ic.ginc0;
    }
    for (g = g_ic.g_here, gcdist = g_ic.gdist = g_ic.rdist, gxx = g_ic.g_inc,
         gcat = g_ic.gat = g_ic.rat, first = 1; g <= g_ic.g_max;
         g++, g_ic.gat += CELLS, gcat += CELLS, g_ic.gdist += gxx, gcdist += gxx, gxx += 2 * XSQR,
         first = 0) {
        if (blue_loop(first)) {
            if (!detect) {
                if (g > g_ic.g_here) {
                    g_ic.g_here = g;
                    g_ic.rat = gcat;
                    g_ic.rdist = gcdist;
                    g_ic.g_inc = gxx;
                }
                detect = 1;
            }
        } else if (detect)
            break;
    }
    for (g = g_ic.g_here - 1, gxx = g_ic.g_inc - 2 * XSQR, gcdist = g_ic.gdist = g_ic.rdist - gxx,
         gcat = g_ic.gat = g_ic.rat - CELLS, first = 1; g >= g_ic.g_min;
         g--, g_ic.gat -= CELLS, gcat -= CELLS, gxx -= 2 * XSQR, g_ic.gdist -= gxx, gcdist -= gxx,
         first = 0) {
        if (blue_loop(first)) {
            if (!detect) {
                g_ic.g_here = g;
                g_ic.rat = gcat;
                g_ic.rdist = gcdist;
                g_ic.g_inc = gxx;
                detect = 1;
            }
        } else if (detect)
            break;
    }
    return detect;
}

/* 0x81bd8: up the red planes from the colour's, then down; the green loop moves rat /
   rdist along the plane */
static void red_loop(void)
{
    int detect = 0, first, r;
    long rxx;

    for (r = g_ic.rc, g_ic.rdist = g_ic.cdist, rxx = g_ic.rinc, g_ic.rat = g_ic.at, first = 1; r < CELLS;
         r++, g_ic.rat += CELLS * CELLS, g_ic.rdist += rxx, rxx += 2 * XSQR, first = 0) {
        if (green_loop(first))
            detect = 1;
        else if (detect)
            break;
    }
    for (r = g_ic.rc - 1, rxx = g_ic.rinc - 2 * XSQR, g_ic.rdist = g_ic.cdist - rxx,
         g_ic.rat = g_ic.at - CELLS * CELLS, first = 1; r >= 0;
         r--, g_ic.rat -= CELLS * CELLS, rxx -= 2 * XSQR, g_ic.rdist -= rxx, first = 0) {
        if (green_loop(first))
            detect = 1;
        else if (detect)
            break;
    }
}

void inv_build(void)
{
    static unsigned char table[CELLS * CELLS * CELLS];
    const unsigned char *p8 = video_palette8();
    int i;

    for (i = 0; i < CELLS * CELLS * CELLS; i++)
        g_ic.dist[i] = 0xffffffffu;
    g_ic.map = table;
    for (i = 0; i < 256; i++) {
        /* the colour as the card reads it back: 6 bits x 4 */
        int r = (p8[3 * i] >> 2) * 4, g = (p8[3 * i + 1] >> 2) * 4, b = (p8[3 * i + 2] >> 2) * 4;
        long rd, gd, bd;

        if (g_colour_off[i])
            continue;
        g_ic.index = i;
        g_ic.rc = r >> 3;
        g_ic.gc = g >> 3;
        g_ic.bc = b >> 3;
        rd = r - (g_ic.rc * 8 + 4);
        gd = g - (g_ic.gc * 8 + 4);
        bd = b - (g_ic.bc * 8 + 4);
        g_ic.cdist = rd * rd + gd * gd + bd * bd;
        g_ic.rinc = 2 * ((g_ic.rc + 1) * XSQR - r * 8);
        g_ic.ginc0 = 2 * ((g_ic.gc + 1) * XSQR - g * 8);
        g_ic.binc0 = 2 * ((g_ic.bc + 1) * XSQR - b * 8);
        g_ic.at = g_ic.rc * CELLS * CELLS + g_ic.gc * CELLS + g_ic.bc;
        red_loop();
    }
    g_inv = table;
}

void text_colour(int fg)
{
    const unsigned char *p = video_palette8() + 3 * fg;
    for (int k = 0; k < 16; k++)
        g_text.ramp[k] = (unsigned long)(k * 255 / 15) << 24 | (unsigned long)(p[0] * k / 15) << 16 |
                         (unsigned long)(p[1] * k / 15) << 8 | (unsigned long)(p[2] * k / 15);
}

/* 0xb1430: one pixel of a letter, a part of the colour over what is there: each channel
   under it times (256 - a) / 256, rounded, plus the colour's part, at most 255; then the
   nearest entry. The whole colour is just its nearest entry */
static void blend_pixel(int x, int y, unsigned long argb)
{
    unsigned a = (unsigned)(argb >> 24), f = 0x10000 - a * 256;
    const unsigned char *p;
    int c[3];
    if (x < 0 || y < 0 || x >= g_fb_w || y >= g_fb_h)
        return;
    if (g_clip_on && (x < g_clip[0] || y < g_clip[1] || x >= g_clip[2] || y >= g_clip[3]))
        return;
    if (a == 255) {
        g_fb[y * g_fb_w + x] = (unsigned char)nearest_colour(argb & 0xffffff);
        return;
    }
    p = video_palette8() + 3 * g_fb[y * g_fb_w + x];
    for (int i = 0; i < 3; i++) {
        c[i] = (int)((p[i] * f + 0x8000) >> 16) + (int)(argb >> (16 - 8 * i) & 0xff);
        if (c[i] > 255)
            c[i] = 255;
    }
    g_fb[y * g_fb_w + x] = (unsigned char)nearest_colour((unsigned long)c[0] << 16 | c[1] << 8 | c[2]);
}

/* 0xa8080: a letter's box two pixels (a byte) at a time; the pixels of level 0 are left */
static void draw_glyph(const font *f, const unsigned char *g, int x, int y)
{
    int w = g[2], h = g[3], bx = (int)u16(g + 4), by = (int)u16(g + 6);
    const unsigned char *src = f->bitmap + 16 + (long)by * f->row_bytes + bx / 2;
    for (int j = 0; j < h; j++, src += f->row_bytes) {
        for (int i = 0; i < w; i += 2) {
            int hi = src[i >> 1] >> 4, lo = src[i >> 1] & 15;
            if (hi)
                blend_pixel(x + i, y + j, g_text.ramp[hi]);
            if (lo)
                blend_pixel(x + i + 1, y + j, g_text.ramp[lo]);
        }
    }
}

/* 0x94234 with a routine for each letter */
static void text_walk(const font *f, const char *s, int x, int y,
                      void (*each)(const font *, const unsigned char *, int, int, void *), void *arg,
                      int *end_x)
{
    const unsigned char *sp = glyph(f, ' ');
    int space = sp ? (signed char)sp[8] : 0;
    int pen = x + f->x_org, base = y + f->y_org;
    for (const unsigned char *c = (const unsigned char *)s; *c; c++) {
        const unsigned char *g;
        if (*c <= 0x20) {
            if (*c == '\n') {
                base += f->line_a + f->line_b;
                pen = 0;
            } else
                pen += space;
            continue;
        }
        g = glyph(f, *c);
        if (!g)
            continue;
        each(f, g, pen + (signed char)g[9], base + (signed char)g[10], arg);
        pen += (signed char)g[8];
    }
    if (end_x)
        *end_x = pen - f->x_org;
}

static void draw_each(const font *f, const unsigned char *g, int x, int y, void *arg)
{
    (void)arg;
    draw_glyph(f, g, x, y);
}

void text_draw(const font *f, const char *s, int x, int y)
{
    text_walk(f, s, x, y, draw_each, 0, 0);
}

static void measure_each(const font *f, const unsigned char *g, int x, int y, void *arg)
{
    int *box = arg;
    (void)f;
    (void)y;
    if (x < box[0])
        box[0] = x;
    if (x + g[2] > box[1])
        box[1] = x + g[2];
}

int text_width(const font *f, const char *s)
{
    int box[2] = { 0, 0 }, end;
    char buf[257];
    strncpy(buf, s, 256);                  /* 0xa3e30 measures at most 256 characters */
    buf[256] = 0;
    text_walk(f, buf, 0, 0, measure_each, box, &end);
    return (box[1] > end ? box[1] : end) - box[0];
}
