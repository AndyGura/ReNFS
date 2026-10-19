/* The road near the camera, into the view each frame (NOTES §4.7). */
#include "road.h"

road_state g_road;

#define NEAR_PLANE 62259       /* 0xf6aa8, >> the far shift: 30 */
#define LIMIT_X 5376           /* 0xf6ab0: the near sections held inside these */
#define LIMIT_Y 5376           /* 0xf6ab4 */

void road_clip_direction(int section)
{
    int k = g_road.sec[section & g_road.sec_mask].points, z;
    g_view.clip_x = g_road.view[k].x - g_road.view[0].x;
    g_view.clip_y = g_road.view[k].y - g_road.view[0].y;
    g_view.clip_z = z = g_road.view[k].z - g_road.view[0].z;
    if (z <= 0)
        z = (int)-(unsigned)z;
    if (z < 0x1999)
        g_view.clip_z = 0x1999;
}

static void translate(int n, const vec3 *src, int shift, const vec3 *t, vec3 *dst)
{
    for (; n > 0; n--, src++, dst++) {
        dst->x = (src->x >> shift) + t->x;
        dst->y = (src->y >> shift) + t->y;
        dst->z = (src->z >> shift) + t->z;
    }
}

int road_transform(const vec3 *pos, const int m[9], int node)
{
    road_state *r = &g_road;
    int shift = g_view.far_shift, near = NEAR_PLANE >> shift, n = r->count, i, first;
    vec3 t;

    t.x = (int)-(unsigned)pos->x >> shift;
    t.y = (int)-(unsigned)pos->y >> shift;
    t.z = (int)-(unsigned)pos->z >> shift;
    translate(n, r->world, shift, &t, r->near);
    if (r->xsets > 0)
        translate(r->xcount, r->xworld, shift, &t, r->xmoved);
    for (i = 0; i < n; i++)
        rotate(&r->near[i], m, &r->view[i]);
    if (r->xsets > 0)
        for (i = 0; i < r->xcount; i++)
            rotate(&r->xmoved[i], m, &r->xview[i]);
    for (i = 0; i < n; i++)
        r->near[i] = r->view[i];
    road_clip_direction(node / 4);

    /* the nearest sections' points held inside the limits (a quarter across when very
       near) and off the near plane; the others off eight times the near plane */
    first = n - (r->drawn - 3) * 11;
    for (i = 0; i < first; i++) {
        vec3 *p = &r->near[i];
        int lim = p->z < 2 * near ? LIMIT_X / 4 : LIMIT_X;
        if (lim < p->x)
            p->x = lim;
        else if (-lim > p->x)
            p->x = -lim;
        if (LIMIT_Y < p->y)
            p->y = LIMIT_Y;
        else if (-LIMIT_Y > p->y)
            p->y = -LIMIT_Y;
        if (near > p->z)
            move_to_near(p, near);
    }
    for (i = first > 0 ? first : 0; i < n; i++)
        if (near * 8 > r->near[i].z)
            move_to_near(&r->near[i], near * 8);
    project_points(n, r->near, r->screen, 16);
    if (r->xsets > 0)
        project_points(r->xcount, r->xview, r->xscreen, 16);
    return near;
}
