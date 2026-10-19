/* A race (race_main 0x6356c, NOTES §4.3): for now its loading and the first frame from
   the starting grid, drawn by the port alone with the car standing. */
#ifndef RENFS_RACE_H
#define RENFS_RACE_H

/* the track `name` ("cy1") from the grid in the 512TR, the view from the driver's eyes
   at 320x200, drawn every retrace until Esc; 0 if something is missing */
int race_test(const char *name);

#endif
