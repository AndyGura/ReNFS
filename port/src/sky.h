/* The sky behind the road (NOTES §4.7): on the open roads a picture ("horz") turning
   with the camera and stretched across the view, the sky's and the ground's colours
   above and below it (0x4a9d0, 0x4a8b4); the circuits' ring of pictures is to come. */
#ifndef RENFS_SKY_H
#define RENFS_SKY_H

#include "roaddraw.h"

/* the tables that stretch the picture across the view (0x49a78): the view `width`
   pixels wide over `half_fov` degrees each side, `scale` picture pixels across it, the
   rows squashed by `squash` (16.16). The view's settings give them (0x14154) */
void sky_tables(int width, int half_fov, int scale, int squash);

/* the sky of the view: the camera's heading `yaw` (2^24 a turn) and its matrix; plain:
   the colours only, no picture (0x12d9d6) */
void sky_draw(int yaw, const int m[9], const road_window *win, int plain);

#endif
