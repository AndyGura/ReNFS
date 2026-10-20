/* The port's own, not the original's: the race drawn at every retrace (60 a second at
   640x480, 70 at 320x200) with the cars, the camera and the dials moved between the race's
   steps (33.3 a second, NOTES §4.3), instead of the same picture until the next step.
   The steps are the original's, untouched: only what a frame is drawn from is put in
   between the last two steps, and put back after the frame (NOTES §4.25). */
#ifndef RENFS_SMOOTH_H
#define RENFS_SMOOTH_H

#include "view.h"

extern int g_smooth;           /* on (F11 in a race; kept in the home folder's renfs.cfg) */

/* the setting from the home folder (on if it has none), or RENFS_SMOOTH=0|1 */
void smooth_load(void);
/* F11 in a race: on or off, kept */
void smooth_keys(void);
/* before a race step: what is drawn now becomes the step's "before" */
void smooth_keep(const race_camera *cam);
/* the next frame drawn as it was before the last step (0), as it is (0x10000) or in
   between; nothing moved when off, or for what jumped (a car put back, a replay's jump, the
   camera on another car or mode) */
void smooth_apply(race_camera *cam, int t);
/* after the frame: everything as the step left it */
void smooth_restore(race_camera *cam);
/* the dials' revs and steering between the steps (the player's car's) */
int smooth_rpm(int now);
int smooth_steer(int now);
/* no "before" for the next frame (a new race, a jump) */
void smooth_reset(void);

#endif
