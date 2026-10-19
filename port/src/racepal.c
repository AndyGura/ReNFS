/* The race's palette and what its pictures are turned into. */
#include "racepal.h"
#include "font.h"
#include "video.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PIECES 50          /* the original stops dead past these */
#define MAX_REMAPS 100

static struct { int start, count; const unsigned char *pal; } g_pieces[MAX_PIECES];
static int g_npieces;          /* 0xee3b0; the list at 0x13c7f0 */
static struct { unsigned char *bank; const unsigned char *pal; } g_remaps[MAX_REMAPS];
static int g_nremaps;          /* 0xee3ac; the list at 0x13ce48 */

static int g_tint[5];              /* 0x13d58c, 0x13d594, 0x13d598, 0x13d590, 0x13d588 */

void pal_set_tint(int r, int g, int b, int towards, int bright)
{
    g_tint[0] = r;
    g_tint[1] = g;
    g_tint[2] = b;
    g_tint[3] = towards;
    g_tint[4] = bright;
}

/* x * f >> 16, rounded by the last bit out */
static int mul16(int x, int f)
{
    long long p = (long long)x * f;
    return (int)(p >> 16) + (int)(p >> 15 & 1);
}

void pal_tint(int from, int to, unsigned char *pal)
{
    int i, k, c[3], top;
    for (i = from; i < to; i++, pal += 3) {
        for (k = 0; k < 3; k++) {
            if (g_tint[k] < 0)
                g_tint[k] = 0;
            c[k] = pal[k] * g_tint[k] / 256;
        }
        if (g_tint[3] > 0x10000)
            g_tint[3] = 0x10000;
        top = c[0] > c[1] ? c[0] : c[1];
        if (c[2] >= top)
            top = c[2];
        for (k = 0; k < 3; k++)
            c[k] += mul16(top - c[k], g_tint[3]);
        for (k = 0; k < 3; k++) {
            c[k] = mul16(c[k], g_tint[4]);
            if (c[k] < 0)
                c[k] = 0;
            if (c[k] > 0x3f)
                c[k] = 0x3f;
            pal[k] = (unsigned char)c[k];
        }
    }
}

void pal_queue_piece(int start, int count, const unsigned char *pal)
{
    if (g_npieces == MAX_PIECES) {
        fprintf(stderr, "renfs: too many palette pieces\n");
        exit(1);
    }
    g_pieces[g_npieces].start = start;
    g_pieces[g_npieces].count = count;
    g_pieces[g_npieces].pal = pal;
    g_npieces++;
}

/* setpalette with 6-bit colours: the game's copy holds them x 4 */
void pal_apply_pieces(void)
{
    unsigned char p8[768];
    int i, k;
    for (i = 0; i < g_npieces; i++) {
        for (k = 0; k < 3 * g_pieces[i].count; k++)
            p8[k] = (unsigned char)((g_pieces[i].pal[k] & 0x3f) << 2);
        video_palette(p8, g_pieces[i].start, g_pieces[i].count);
    }
    g_npieces = 0;
}

void pal_queue_remap(unsigned char *bank, const unsigned char *pal)
{
    if (g_nremaps == MAX_REMAPS) {
        fprintf(stderr, "renfs: too many pictures to re-colour\n");
        exit(1);
    }
    g_remaps[g_nremaps].bank = bank;
    g_remaps[g_nremaps].pal = pal;
    g_nremaps++;
}

static unsigned rd32(const unsigned char *p)
{
    return (unsigned)p[0] | (unsigned)p[1] << 8 | (unsigned)p[2] << 16 | (unsigned)p[3] << 24;
}

void pal_remap(unsigned char *bank, const unsigned char *pal)
{
    unsigned char map[256];
    int i, n, k;
    /* 0x95d20: 6 bits to 8 rounded (0xa2f10), then the nearest of each (0xaa1f0) */
    for (i = 0; i < 255; i++) {
        unsigned r = (pal[3 * i] * 255u + 32) / 63, g = (pal[3 * i + 1] * 255u + 32) / 63,
                 b = (pal[3 * i + 2] * 255u + 32) / 63;
        map[i] = (unsigned char)nearest_colour((unsigned long)(r & 0xff) << 16 | (g & 0xff) << 8 | (b & 0xff));
    }
    map[255] = 255;
    for (i = (int)rd32(bank + 8) - 1; i >= 0; i--) {
        unsigned char *s = bank + rd32(bank + 20 + 8 * i);
        if (bank[16 + 8 * i] == '!')
            continue;
        n = (s[4] | s[5] << 8) * (s[6] | s[7] << 8);
        for (k = 0; k < n; k++)
            s[16 + k] = map[s[16 + k]];
    }
}

void pal_apply_remaps(void)
{
    int i;
    for (i = 0; i < g_nremaps; i++)
        pal_remap(g_remaps[i].bank, g_remaps[i].pal);
    g_nremaps = 0;
}

void race_colours(const unsigned char *marks, int n, int mode)
{
    int i;

    memset(g_colour_off, 0, 255);
    g_colour_off[0x47] = 1;
    if (mode < 2)
        g_colour_off[0x5f] = 1;
    for (i = 0xfa; i < 0x100; i++)
        g_colour_off[i] = 1;
    for (i = 0; i < n; i++)
        if (marks[i])
            g_colour_off[marks[i]] = 1;
    inv_build();
}
