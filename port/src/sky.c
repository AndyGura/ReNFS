/* The sky behind the road (NOTES §4.7). */
#include "sky.h"
#include "exe.h"
#include "config.h"
#include "racepal.h"
#include "track.h"
#include "trackfam.h"
#include "trackinfo.h"
#include "view.h"

int g_sky_steps[0x400];   /* 0x12df40: a column's x in the picture, from the middle */
unsigned char g_sky_rows[480];  /* 0x12dd60: a screen row's row of the picture */
static int g_columns;          /* 0x12efb8 */
static int g_squash;           /* 0x12efbc */

static int rd16(const unsigned char *p)
{
    return (short)(p[0] | p[1] << 8);
}

/* a / b in 16.16, the whole part and the remainder divided apart (EA's way) */
static int fixdiv(int a, int b)
{
    int q = a / b, r = a % b;
    return (int)((unsigned)q << 16) + (int)((long long)r * 65536 / b);
}

/* a × b in 16.16, rounded by the bit below */
static int fixmul(int a, int b)
{
    long long p = (long long)a * b;
    return (int)(p >> 16) + (int)(p >> 15 & 1);
}

void sky_tables(int width, int half_fov, int scale, int squash)
{
    int half = width / 2, angle = (int)((unsigned)half_fov << 16) / 360, t, focal, per, i;
    unsigned acc, step;

    t = ea_cos(angle);
    t = t ? fixdiv(ea_sin(angle), t) : 0x11;
    focal = fixdiv((int)((unsigned)half << 16), t);
    per = fixdiv(scale / 2, angle);
    if (half_fov <= 10)
        return;
    g_squash = squash;
    step = (unsigned)fixdiv(0x10000, squash);
    for (i = 0, acc = 0; i < 480; i++, acc += step)
        g_sky_rows[i] = (unsigned char)((int)acc >> 16);
    g_columns = width;
    /* each column from the middle by its angle, the left half the right's negated */
    for (i = 0; half + i <= width && half >= 0; i++) {
        int v = fixmul(ea_atan2((int)((unsigned)i << 16), focal), per);
        if (half + i < 0x400)
            g_sky_steps[half + i] = v;
        if (half - i >= 0)
            g_sky_steps[half - i] = -v;
    }
}

/* a pixel of the picture; outside the family's memory 0, where the original read on */
static unsigned char pixel(const unsigned char *pix, long at)
{
    const unsigned char *p = pix + at;
    return p >= g_fam.file && p < g_fam.file + g_fam.len ? *p : 0;
}

/* the picture's rows from screen row `top` (0x49bdc), from picture column `off` at the
   middle: what falls off one end comes round from the other */
static void strip(const unsigned char *shape, int off, int x, int top)
{
    int w = rd16(shape + 4), h = rd16(shape + 6), n = fixmul(h - 1, g_squash), skip = 0;
    int x0, y0, x1, y1, k, rest, i, j, row = 1;
    const unsigned char *pix = shape + 16;

    raster_clip(&x0, &y0, &x1, &y1);
    if (top < y0) {
        skip = (int)((unsigned)y0 - (unsigned)top);
        top = y0;
        n -= skip;
    }
    if (top + n > y1)
        n = y1 - top;
    k = 0;
    if (g_sky_steps[0] + off < 0) {        /* the left end before the picture's start */
        row++;
        while (k < g_columns && g_sky_steps[k] + off < 0)
            k++;
    } else
        while (k < g_columns && g_sky_steps[k] + off < w)
            k++;
    rest = g_columns - k;
    for (i = 0; i < n && skip + i >= 0 && skip + i < 480; i++) {
        unsigned char *dst;
        if (top + i < y0 || top + i >= y1)
            break;                     /* (a horizon far off the screen: the original
                                          runs past its table of rows) */
        dst = raster_row(top + i) + x;
        long at = (long)(g_sky_rows[skip + i] + row) * w + off;
        for (j = 0; j < k; j++)
            dst[j] = pixel(pix, at + g_sky_steps[j]);
        for (j = 0; j < rest; j++)
            dst[k + j] = pixel(pix, at + g_sky_steps[k + j] - w);
    }
}

void sky_draw(int yaw, const int m[9], const road_window *win, int plain)
{
    const unsigned char *shape;
    int a = yaw >> 14, w, h, cy, top, bottom, off;
    vec3 d, v;
    scr2 s;

    /* where the horizon is: a point far ahead along the heading, on the screen */
    d.x = (int)((unsigned)fix_sin(a) << 8) >> g_view.far_shift;
    d.y = 0;
    d.z = (int)((unsigned)fix_cos(a) << 8) >> g_view.far_shift;
    rotate(&d, m, &v);
    project_points(1, &v, &s, 16);

    shape = g_fam.horizon ? shpi_find(g_fam.horizon, "horz") : 0;
    if (!shape)
        return;
    w = rd16(shape + 4);
    h = rd16(shape + 6);
    cy = rd16(shape + 0xa);
    off = (fixmul(g_road_mirror ? -yaw : yaw, 0x50000) >> 16) % w;
    if (plain)
        h = cy = 0;
    top = s.y - (cy * g_squash >> 16);
    bottom = top + ((h - 1) * g_squash >> 16);
    raster_fill(win->x, win->y, win->w, top, shape[16]);
    if (!plain)
        strip(shape, off, win->x, top);
    raster_fill(win->x, bottom, win->w, win->y + win->h - bottom, shape[16 + rd16(shape + 4) * rd16(shape + 6) - 1]);
}

void sky_behind(int yaw, int pitch, int node, int lens_h, int dy, int width, int height)
{
    int type = node_ptr((int)((unsigned)(node - 3) & g_track.node_mask))[7];
    int across, row, step, w, tile, bottom, flip = 0, xy[8];
    const unsigned char *shape;
    raster_tex t;

    if (type == 4 || type == 7 || type == 9) {
        raster_fill(0, 0, width, height, g_hud_colour[12]);
        return;
    }
    if (type == 8) {
        raster_fill(0, 0, width, height, g_hud_colour[3]);
        return;
    }
    across = -(((int)((unsigned)yaw * 2) >> 16) + 0x100) & 0x1ff;
    row = (int)((unsigned)pitch << 2) >> 16;
    if (row > 0x200)
        row -= 0x400;
    if (g_view_res)
        row = row * 480 / 200;
    row += lens_h / 2 + dy;
    shape = g_fam.horizon ? shpi_find(g_fam.horizon, "horz") : 0;
    if (!shape)
        return;
    w = rd16(shape + 4);
    step = 0x1000000 / w;
    if (!g_view_res) {
        step /= 2;
        across /= 2;
    }
    tile = fixmul(step, w);
    bottom = fixmul(step, rd16(shape + 6) - 1);
    row -= fixmul(step, rd16(shape + 0xa));
    if (config_word(CFG_TRACK) == 5)
        row = -17;
    raster_fill(0, 0, width, row, shape[16]);
    while (across > width) {
        across -= tile;
        flip ^= 1;
    }
    shape_tex(shape, &t);
    for (; across > -tile; across -= tile, flip ^= 1) {
        int turned = g_info.last_used && flip;
        xy[0] = xy[6] = turned ? across + tile : across;
        xy[2] = xy[4] = turned ? across : across + tile;
        xy[1] = xy[3] = row;
        xy[5] = xy[7] = row + bottom;
        raster_quad_whole(&t, xy);
    }
    raster_fill(0, row + bottom, width, height - row + bottom,
                shape[16 + rd16(shape + 4) * rd16(shape + 6) - 1]);
}
