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
    for (int i = 0; i < 254; i++) {        /* 254 and 255 are the mouse pointer's */
        long d = (long)(g - p[3 * i + 1]) * (g - p[3 * i + 1]) + (long)(r - p[3 * i]) * (r - p[3 * i]) +
                 (long)(b - p[3 * i + 2]) * (b - p[3 * i + 2]);
        if (d < best) {
            best = d;
            found = i;
        }
    }
    return found;
}

void inv_build(void)
{
    static unsigned char table[32768];
    const unsigned char *p8 = video_palette8();
    int pal[254][3];
    for (int i = 0; i < 254; i++)
        for (int c = 0; c < 3; c++)
            pal[i][c] = (p8[3 * i + c] >> 2) * 4;
    for (int cell = 0; cell < 32768; cell++) {
        int r = (cell >> 10) * 8 + 4, g = (cell >> 5 & 31) * 8 + 4, b = (cell & 31) * 8 + 4;
        long best = 0x7fffffff;
        int found = 0;
        for (int i = 0; i < 254; i++) {     /* the first nearest wins */
            long d = (long)(r - pal[i][0]) * (r - pal[i][0]) + (long)(g - pal[i][1]) * (g - pal[i][1]) +
                     (long)(b - pal[i][2]) * (b - pal[i][2]);
            if (d < best) {
                best = d;
                found = i;
            }
        }
        table[cell] = (unsigned char)found;
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
