/* For tools/skid_emu.py: the tyres' marks (port/src/skid.c) set up as a snapshot of the
   original has the world round them, the lines they paint caught. */
#include "skid.h"
#include "cardraw.h"
#include "drive.h"
#include "polyfill.h"
#include "race.h"
#include "racekeys.h"
#include "roaddraw.h"
#include "track.h"
#include "view.h"
#include <string.h>

#define LINES 4096

static race_camera cam;
static int cam_m[9];
static race_car car;
static int lines[LINES][5], nlines;

static void catch_line(int x0, int y0, int x1, int y1, int w)
{
    if (nlines < LINES) {
        lines[nlines][0] = x0;
        lines[nlines][1] = y0;
        lines[nlines][2] = x1;
        lines[nlines][3] = y1;
        lines[nlines][4] = w;
    }
    nlines++;
}

void skid_test_world(unsigned node_mask, int length, int view_res, int window, int steps,
                     int slide, const int *surfaces, const unsigned char *shade)
{
    g_track.node_mask = node_mask;
    g_track.length = length;
    g_view_res = view_res;
    g_rkeys.window = window;
    g_road_steps = steps;
    g_race_slide = slide;
    memcpy(g_surfaces, surfaces, sizeof g_surfaces);
    memcpy(g_shade, shade, sizeof g_shade);
}

void skid_test_view(const int *pos, const int *m, int cx, int cy, int min_z, const int *recip)
{
    cam.pos.x = pos[0];
    cam.pos.y = pos[1];
    cam.pos.z = pos[2];
    memcpy(cam_m, m, sizeof cam_m);
    g_camera = &cam;
    g_camera_m = cam_m;
    g_view.cx = cx;
    g_view.cy = cy;
    g_view.min_z = min_z;
    memcpy(g_view.recip, recip, sizeof g_view.recip);
}

void skid_test_catch(int on)
{
    g_skid_line = on ? catch_line : skid_line;
    nlines = 0;
}

int skid_test_lines(int *out, int max)
{
    int n = nlines < LINES ? nlines : LINES;

    memcpy(out, lines, sizeof(int) * 5 * (n < max ? n : max));
    max = nlines;
    nlines = 0;
    return max;
}

void skid_test_car(int slide_front, int slide_rear, int tyres_front, int tyres_rear, int node,
                   int surface, const int *along, const int *across, const int *centre)
{
    vec3 a, b, c;

    car.slide_front = slide_front;
    car.slide_rear = slide_rear;
    car.tyres[0] = (unsigned char)tyres_front;
    car.tyres[1] = (unsigned char)tyres_rear;
    car.node = node;
    car.surface = surface;
    a.x = along[0], a.y = along[1], a.z = along[2];
    b.x = across[0], b.y = across[1], b.z = across[2];
    c.x = centre[0], c.y = centre[1], c.z = centre[2];
    skid_record_car(&car, &a, &b, &c);
}

void skid_test_row(int cam_node, int row_node, const int *a, const int *b)
{
    scr2 sa, sb;

    sa.x = a[0], sa.y = a[1];
    sb.x = b[0], sb.y = b[1];
    skid_paint_row(cam_node, row_node, &sa, &sb);
}
