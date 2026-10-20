/* For tools/camera_emu.py: what the view key (camera_next, port/src/camera.c) reads, set
   as the original's globals have it. */
#include "camera.h"
#include "cardraw.h"
#include "drive.h"
#include "opponent.h"
#include "timing.h"

/* 0xf6b74, 0x12dd54, 0x145c90, 0x12da34, 0x12dd08, 0x12da9e, 0xe8f68 */
void camera_test_set(int state, int viewed, int stopped, int police, int chase, int rival,
                     int look_first)
{
    g_drive.state = state;
    g_drive.viewed = viewed;
    g_timing.stopped = stopped;
    g_drive.police = police;
    g_drive.chase = chase;
    g_opp.rival_car = rival;
    g_cam_look_first = look_first;
}

int camera_test_look_first(void)
{
    return g_cam_look_first;
}
