/* The 3D view's points and quads (NOTES §4.7). Everything in 16.16 with the original's
   roundings: a multiply rounds by the last bit shifted out, a midpoint truncates toward
   zero. */
#include "poly.h"

view3d g_view;
void (*g_paint)(const raster_tex *t, const int uv[8], const int xy[8]) = raster_quad;

/* the distances under which a quad is cut once more, by level (0xeaadc the view, 0xeaae8
   the mirror: 6.4, 4.8, 3.8), before the shift */
static const int cut_view[3] = { 419430, 314572, 249036 };
static const int cut_mirror[3] = { 419430, 314572, 249036 };

static int fixmul(int a, int b)
{
    long long p = (long long)a * b;
    return (int)(unsigned)(p >> 16) + (int)((p >> 15) & 1);
}

/* a / b in 16.16 as the original's two divisions: the whole part, then the remainder */
static int fixdiv(int a, int b)
{
    int q = a / b, r = a % b;
    return (int)((unsigned)q << 16) + (int)(((long long)r << 16) / b);
}

void view_set_focal(int focal)
{
    int i;
    g_view.focal = focal;
    for (i = 0; i < 0x400; i++)
        g_view.recip[i] = (int)(((long long)focal << 16) / (i < 10 ? 10 : i));
}

/* x · r >> shift, rounded by the last bit out. The CPU takes the shift mod 32: none at all
   for a point 256 away or more, and then the carry is the multiply's overflow */
static int scale(int x, int r, int shift)
{
    long long p = (long long)x * r;
    shift &= 31;
    if (!shift)
        return (int)(unsigned)p + (p != (int)p);
    return (int)(unsigned)(p >> shift) + (int)((p >> (shift - 1)) & 1);
}

void project(const vec3 *p, scr2 *out)
{
    int z = p->z, shift = 16, r;
    if (z <= g_view.min_z) {
        out->x = out->y = (int)0x80000000;
        g_view.flags |= 1;
        return;
    }
    if (z >= 0x10000) {
        shift = 24;
        z >>= 8;
    }
    if (z >= 0x10000) {
        z >>= 8;
        shift += 8;
    }
    if (z >= 0x1000) {
        z >>= 4;
        shift += 4;
    }
    if (z >= 0x800) {
        z >>= 2;
        shift += 2;
    }
    if (z >= 0x400) {
        z >>= 1;
        shift++;
    }
    r = g_view.recip[z];
    out->x = g_view.cx + scale(p->x, r, shift);
    out->y = g_view.cy - scale(p->y, r, shift);
}

static vec3 mid(const vec3 *a, const vec3 *b)
{
    vec3 m;
    m.x = (int)((unsigned)a->x + (unsigned)b->x) / 2;
    m.y = (int)((unsigned)a->y + (unsigned)b->y) / 2;
    m.z = (int)((unsigned)a->z + (unsigned)b->z) / 2;
    return m;
}

/* a midpoint on the screen; one nearer than the near plane is moved onto it first, for
   its projection only */
static void project_near(const vec3 *m, int near, scr2 *out)
{
    if (m->z < near) {
        vec3 c = *m;
        int t = fixdiv(near - m->z, g_view.clip_z);
        c.x += fixmul(t, g_view.clip_x);
        c.y += fixmul(t, g_view.clip_y);
        c.z = near;
        project(&c, out);
    } else
        project(m, out);
}

/* a piece painted whole (0x3521c): its part of the picture, mirrored across */
static void corners(const raster_tex *t, int ushift, int level, int uindex, int vindex,
                    const scr2 s[4])
{
    int w = t->w >> ushift, h = t->h >> level, uv[8];
    int u0 = (int)((unsigned)(((1 << ushift) - 1 - uindex) * w) << 16);
    int v0 = (int)((unsigned)(h * ((1 << level) - 1 - vindex)) << 16);
    int u1 = u0 + (int)((unsigned)(w - 1) << 16), v1 = v0 + (int)((unsigned)(h - 1) << 16);
    if (u1 <= u0 || v1 <= v0)
        return;
    uv[0] = u0; uv[1] = v0;
    uv[2] = u1; uv[3] = v0;
    uv[4] = u1; uv[5] = v1;
    uv[6] = u0; uv[7] = v1;
    g_paint(t, uv, &s[0].x);
}

typedef void cut_fn(const raster_tex *, int, int, int, int, const vec3 *const[4],
                    const scr2[4], int);

static void child(cut_fn *f, const raster_tex *t, int ushift, int level, int uindex,
                  int vindex, const vec3 *a, const vec3 *b, const vec3 *c, const vec3 *d,
                  scr2 sa, scr2 sb, scr2 sc, scr2 sd, int near)
{
    const vec3 *p[4] = { a, b, c, d };
    scr2 s[4] = { sa, sb, sc, sd };
    f(t, ushift, level, uindex, vindex, p, s, near);
}

/* the view's (0x35348): cut while the far side (P1, P2) is near enough; the side P0-P3
   first. A piece wholly left, right or above the window is not painted */
void quad_view(const raster_tex *t, int ushift, int level, int uindex, int vindex,
               const vec3 *const p[4], const scr2 s[4], int near)
{
    int far;
    vec3 m01, m23, m03, m12, c;
    scr2 s01, s23, s03, s12, sc;

    if (near > p[3]->z && near > p[0]->z)
        return;
    far = level < 3 ? cut_view[level] >> g_view.far_shift : 0;
    if ((far > p[1]->z || far > p[2]->z) && level < g_view.max_level) {
        m01 = mid(p[0], p[1]);
        project_near(&m01, near, &s01);
        m23 = mid(p[2], p[3]);
        project_near(&m23, near, &s23);
        m03 = mid(p[0], p[3]);
        project_near(&m03, near, &s03);
        c = mid(&m23, &m01);
        project_near(&c, near, &sc);
        child(quad_view, t, ushift + 1, level + 1, 2 * uindex + 1, 2 * vindex + 1,
              p[0], &m01, &c, &m03, s[0], s01, sc, s03, near);
        child(quad_view, t, ushift + 1, level + 1, 2 * uindex + 1, 2 * vindex,
              &m03, &c, &m23, p[3], s03, sc, s23, s[3], near);
        if (m23.z < near && near > m01.z)
            return;
        m12 = mid(p[1], p[2]);
        project_near(&m12, near, &s12);
        child(quad_view, t, ushift + 1, level + 1, 2 * uindex, 2 * vindex + 1,
              &m01, p[1], &m12, &c, s01, s[1], s12, sc, near);
        child(quad_view, t, ushift + 1, level + 1, 2 * uindex, 2 * vindex,
              &c, &m12, p[2], &m23, sc, s12, s[2], s23, near);
    } else if (!(raster_outcode(4, &s[0].x) & 7))
        corners(t, ushift, level, uindex, vindex, s);
}

/* the mirror's (draw_quad 0x35b04): cut while P3 is near enough; the side P1-P2 first */
void quad_mirror(const raster_tex *t, int ushift, int level, int uindex, int vindex,
                 const vec3 *const p[4], const scr2 s[4], int near)
{
    int far;
    vec3 m01, m23, m30, m12, c;
    scr2 s01, s23, s30, s12, sc;

    if (near > p[2]->z && near > p[1]->z)
        return;
    far = level < 3 ? cut_mirror[level] >> g_view.far_shift : 0;
    if (far > p[3]->z && level < g_view.max_level) {
        m01 = mid(p[0], p[1]);
        project_near(&m01, near, &s01);
        m23 = mid(p[2], p[3]);
        project_near(&m23, near, &s23);
        c = mid(&m01, &m23);
        project_near(&c, near, &sc);
        m12 = mid(p[1], p[2]);
        project_near(&m12, near, &s12);
        child(quad_mirror, t, ushift + 1, level + 1, 2 * uindex + 1, 2 * vindex + 1,
              &m01, p[1], &m12, &c, s01, s[1], s12, sc, near);
        child(quad_mirror, t, ushift + 1, level + 1, 2 * uindex + 1, 2 * vindex,
              &c, &m12, p[2], &m23, sc, s12, s[2], s23, near);
        if (m23.z < near && near > m01.z)
            return;
        m30 = mid(p[0], p[3]);
        project_near(&m30, near, &s30);
        child(quad_mirror, t, ushift + 1, level + 1, 2 * uindex, 2 * vindex + 1,
              p[0], &m01, &c, &m30, s[0], s01, sc, s30, near);
        child(quad_mirror, t, ushift + 1, level + 1, 2 * uindex, 2 * vindex,
              &m30, &c, &m23, p[3], s30, sc, s23, s[3], near);
    } else
        corners(t, ushift, level, uindex, vindex, s);
}
