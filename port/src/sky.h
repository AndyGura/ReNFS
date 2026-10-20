/* The sky behind the road (NOTES §4.7): on the open roads a picture ("horz") turning
   with the camera and stretched across the view, the sky's and the ground's colours
   above and below it (0x4a9d0, 0x4a8b4); on the circuits a ring of panels of it round the
   track (0x4ace8). */
#ifndef RENFS_SKY_H
#define RENFS_SKY_H

#include "roaddraw.h"
#include "view.h"

/* the tables that stretch the picture across the view (0x49a78): the view `width`
   pixels wide over `half_fov` degrees each side, `scale` picture pixels across it, the
   rows squashed by `squash` (16.16). The view's settings give them (0x14154) */
void sky_tables(int width, int half_fov, int scale, int squash);

/* the sky of the view (0x4a9d0): the camera's heading `yaw` and roll (2^24 a turn), its
   place and matrix, the view's window `vw` (its squash and lens width) and the part
   painted `win`; plain: the colours only, no picture (0x12d9d6). The open roads' picture
   turning with the camera, or on the circuits the ring round the track (0x4ace8) */
void sky_draw(int yaw, int roll, const vec3 *pos, const int m[9], const view_window *vw,
              const road_window *win, int plain);

/* the mirror's horizon (0x5adbc), into the window's picture of width x height: on the
   road types without one (node 3 behind the camera's: 4, 7, 9, 8) one colour; else the
   "horz" picture in tiles across, every other one turned round on tracks that say so, by
   the camera's yaw and pitch (2^24 a turn), lens_h the view's layout's height (+0x20) and
   dy its window's centre moved */
void sky_behind(int yaw, int pitch, int node, int lens_h, int dy, int width, int height);

#endif
