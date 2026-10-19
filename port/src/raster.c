/* Painting a textured four-cornered piece of the 3D view (NOTES §4.6): the road, the
   scenery, the cars. The game's filler 0x34ed8 hands a quad to 0x91360, or to its twin
   0x90b30 when the picture has see-through pixels (colour 255, shape type 0x45, marked at
   load by 0x5b270). Both are the same routine with a different row painter: 0xa7a4c,
   0xa7610.

   The corners come in order round the quad, the screen positions as whole pixels, the
   picture positions in 16.16. The picture is stepped straight across each row (no
   perspective: draw_quad 0x35b04 cuts the far pieces smaller instead). */
#include "raster.h"

/* the window (0xf88a4 x0, 0xf88a8 y0, 0xf88ac x1, 0xf88b0 y1: the ends outside) */
static unsigned char *g_base;
static int g_pitch, g_x0, g_y0, g_x1, g_y1;

/* the edge tables, a row each (0x300 at most): left and right x, and u, v at both ends.
   The original keeps them on its stack, so a row no edge reached keeps what was there */
#define ROWS 0x300
static int g_lx[ROWS], g_rx[ROWS], g_lu[ROWS], g_lv[ROWS], g_ru[ROWS], g_rv[ROWS];

void raster_window(unsigned char *base, int pitch, int x0, int y0, int x1, int y1)
{
    g_base = base;
    g_pitch = pitch;
    g_x0 = x0;
    g_y0 = y0;
    g_x1 = x1;
    g_y1 = y1 > ROWS ? ROWS : y1;
}

static int shl16(int v)
{
    return (int)((unsigned)v << 16);
}

/* the rows of an edge from y, n of them, cut to the window: where its first and last row
   start. Returns 0 when none of it is inside */
static int edge_rows(int *y, int *n, int *start, int step)
{
    if (*y >= g_y1 || *y + *n < g_y0)
        return 0;
    if (*y < g_y0) {
        *start += step * (g_y0 - *y);
        *n -= g_y0 - *y;
        *y = g_y0;
    }
    if (*y + *n >= g_y1)
        *n = g_y1 - *y;
    return 1;
}

/* an edge's x into a table, from x0 to x1 over n rows: the left edge (0xa7790) starts half
   way along its first row when it slopes left, the right one (0xa7844) when it slopes
   right, so the quad takes in the pixels its edge passes through. Returns the slope */
static int edge_x(int *table, int y, int n, int x0, int x1, int right)
{
    int slope, x;
    if (y >= g_y1 || y + n < g_y0)
        return 0;
    slope = shl16(x1 - x0) / n;
    x = shl16(x0);
    if (right ? slope > 0 : slope < 0)
        x += slope;
    if (!edge_rows(&y, &n, &x, slope))
        return 0;
    for (; n > 0; n--, x += slope)
        table[y++] = x >> 16;
    return slope;
}

/* u or v along an edge (0xa78e4 left, 0xa79a4 right), half a step in where its x was */
static void edge_uv(int *table, int y, int n, int a0, int a1, int xslope, int right)
{
    int step, a = a0;
    if (y >= g_y1 || y + n < g_y0)
        return;
    step = (a1 - a0) / n;
    if (right ? xslope > 0 : xslope < 0)
        a += step;
    if (!edge_rows(&y, &n, &a, step))
        return;
    for (; n > 0; n--, a += step)
        table[y++] = a;
}

/* both sides from the top corner to the bottom one: the left going round by -dir */
static void edges(const int uv[8], const int xy[8], int top, int bottom, int dir)
{
    int i, j, n, s;
    for (i = top; i != bottom; i = j) {
        j = (i - dir) & 3;
        n = xy[j * 2 + 1] - xy[i * 2 + 1] + 1;
        if (n > 0) {
            s = edge_x(g_lx, xy[i * 2 + 1], n, xy[i * 2], xy[j * 2], 0);
            edge_uv(g_lu, xy[i * 2 + 1], n, uv[i * 2], uv[j * 2], s, 0);
            edge_uv(g_lv, xy[i * 2 + 1], n, uv[i * 2 + 1], uv[j * 2 + 1], s, 0);
        }
    }
    for (i = top; i != bottom; i = j) {
        j = (i + dir) & 3;
        n = xy[j * 2 + 1] - xy[i * 2 + 1] + 1;
        if (n > 0) {
            s = edge_x(g_rx, xy[i * 2 + 1], n, xy[i * 2], xy[j * 2], 1);
            edge_uv(g_ru, xy[i * 2 + 1], n, uv[i * 2], uv[j * 2], s, 1);
            edge_uv(g_rv, xy[i * 2 + 1], n, uv[i * 2 + 1], uv[j * 2 + 1], s, 1);
        }
    }
}

/* one row (0xa7a4c, 0xa7610 skipping colour 255): the picture's whole part and fraction
   stepped apart, as the original's carries do */
static void span(const raster_tex *t, unsigned char *dst, int u, int du, int v, int dv,
                 int n, int see_through)
{
    const unsigned char *p = t->pixels + (v >> 16) * t->rowbytes + (u >> 16);
    int step = (dv >> 16) * t->rowbytes + (du >> 16);
    unsigned uf = u & 0xffff, vf = v & 0xffff;
    for (; n > 0; n--, dst++) {
        unsigned char c = p >= t->lo && p < t->hi ? *p : 0;
        if (!see_through || c != 255)
            *dst = c;
        uf += du & 0xffff;
        vf += dv & 0xffff;
        p += step + (int)(uf >> 16) + (vf >> 16 ? t->rowbytes : 0);
        uf &= 0xffff;
        vf &= 0xffff;
    }
}

void raster_quad(const raster_tex *t, const int uv[8], const int xy[8], int see_through)
{
    int top = 0, bottom = 2, ymin = xy[1], ymax = xy[5], i, y, dir, s_next, s_prev;
    int retried = 0;

    for (i = 0; i < 4; i++) {
        if (xy[i * 2 + 1] < ymin) {
            ymin = xy[i * 2 + 1];
            top = i;
        } else if (xy[i * 2 + 1] > ymax) {
            ymax = xy[i * 2 + 1];
            bottom = i;
        }
    }
    if (ymin >= g_y1 || ymax < g_y0)
        return;

    /* which way round the corners go: the edge after the top one further right */
    i = (top + 1) & 3;
    s_next = shl16(xy[i * 2] - xy[top * 2]) / (xy[i * 2 + 1] - xy[top * 2 + 1] + 1);
    i = (top - 1) & 3;
    s_prev = shl16(xy[i * 2] - xy[top * 2]) / (xy[i * 2 + 1] - xy[top * 2 + 1] + 1);
    dir = s_next >= s_prev ? 1 : -1;
    edges(uv, xy, top, bottom, dir);

    if (ymin < g_y0)
        ymin = g_y0;
    if (ymax >= g_y1)
        ymax = g_y1 - 1;
    for (y = ymin; y <= ymax; y++) {
        int l = g_lx[y], r = g_rx[y], n = r - l + 1, u, v, du, dv;
        if (n <= 0) {
            /* a row the wrong way round: once, the sides swapped and this row again;
               a second time ends the quad (0x91917) */
            if (retried)
                return;
            retried = 1;
            dir = -dir;
            edges(uv, xy, top, bottom, dir);
            y--;
            continue;
        }
        if (g_x1 <= l || g_x0 > r)
            continue;
        u = g_lu[y];
        v = g_lv[y];
        du = (g_ru[y] - u) / n;
        dv = (g_rv[y] - v) / n;
        if (l < g_x0) {
            u += du * (g_x0 - l);
            v += dv * (g_x0 - l);
            n -= g_x0 - l;
            l = g_x0;
        }
        if (r >= g_x1)
            n -= r - g_x1 + 1;
        span(t, g_base + y * g_pitch + l, u, du, v, dv, n, see_through);
    }
}
