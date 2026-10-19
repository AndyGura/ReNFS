/* The dashboard (load_dash 0x2e8d4, NOTES §4.7): only its colours so far. */
#ifndef RENFS_DASH_H
#define RENFS_DASH_H

#include "fsh.h"

typedef struct {
    fsh_file file;             /* SIMDATA/DASH/<car>DL.FSH (DH at 640x480) */
    unsigned char *pal;        /* 0x12b5ec: its palette, 6 bits */
} dash_state;

extern dash_state g_dash;

/* the car's dashboard (car its name, "F512TR"); 0 if missing */
int dash_load(const char *car, int hires);
void dash_free(void);
/* its first 48 colours as a piece of the race's palette (0x2e8bc), and the cockpit's 13
   of the program's at 0xf0 (0xf6d18, load_cockpit_mask 0x5f7c8) */
void dash_queue_colours(void);
void cockpit_queue_colours(void);

#endif
