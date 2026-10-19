/* The race's camera (camera_update 0x136b0, NOTES §4.7): where the frame is seen from,
   once a frame before the drawing. Only the driver's eyes (modes 0 and 1) so far. */
#ifndef RENFS_CAMERA_H
#define RENFS_CAMERA_H

#include "view.h"

/* the cameras' settings, 0x58 bytes a mode (0xe80f0) */
typedef struct {
    int kind;                  /* +0 */
    vec3 eye;                  /* +4: the eye from the car (in the car: the driver's) */
    int fill[18];
} camera_mode;

#define CAMERA_MODES 14

extern camera_mode g_cam_modes[CAMERA_MODES];
extern int g_cam_pitch_bias;   /* 0x12ba20: the dashboard's (0 so far) */

/* the settings as the program has them, the driver's eye height the car's (its spec's
   +0x36c: modes 0 and 10, 0x147b2) */
void camera_init(int eye_height);
/* the camera on car c in mode `mode` (camera_set_car 0x13530 / 0x13450) */
void camera_set_car(race_camera *cam, const race_car *c, int mode);
/* the camera this frame (0x136b0) */
void camera_update(race_camera *cam);

#endif
