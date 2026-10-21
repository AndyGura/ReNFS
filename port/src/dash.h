/* The dashboard (load_dash 0x2e8d4, NOTES §4.7, §4.11): its pictures, its colours, the
   places on it the program reads from its attachment blocks, and the lists of what of
   the 3D view shows through it. */
#ifndef RENFS_DASH_H
#define RENFS_DASH_H

#include "fsh.h"

typedef struct {
    fsh_file file;             /* SIMDATA/DASH/<car>DL.FSH (DH at 640x480) */
    unsigned char *pal;        /* 0x12b5ec: its palette, 6 bits */
    unsigned char *fmm;        /* <car>DL.FMM (0x12b8b4): a wwww of 4 BAMF lists */
    const unsigned char *list[4];  /* 0x12b8a4: the view through the windows, by the
                                      detail (0x143e04): 1:1, then pixels doubled */
    const unsigned char *dash, *tach, *sped;   /* 0x12b4d8, 0x12b4dc, 0x12b4e0 */
    /* by how far the wheel is turned (0x31684): the wheel turned left and right
       (0x12b4e4, 0x12b500), and the parts of it over the two gauges, turned left
       (0x12b51c, 0x12b538) and right (0x12b554, 0x12b570) */
    const unsigned char *wheel[2][7], *over_tach[2][7], *over_sped[2][7];
    const unsigned char *digit[11];    /* 0x12b58c: num0..num9, numb (cars 3 and 11) */
    int digital;               /* 0x12b5bc: the speed in digits */
    /* places on the screen (0x12b604): 0, 1 the mirror's corners; 8, 9 from 2, 3 */
    int place[10][2];
    int tach_box[4], sped_box[4];  /* 0x12b884, 0x12b894: x, y, w, h */
    int tach_n, sped_n;        /* 0x12b5f8, 0x12b5fc */
    int tach_pt[30][2];        /* 0x12b65c: the needle's ends, the last its centre */
    int sped_pt[30][2];        /* 0x12b74c */
    /* the gear lever (0x2e4f0): its gate, the gate without it, the knob "nob1" and the
       stick's parts "lth3" .. "lth1" (0xea98c), how many were looked for until one was
       missing or all four found (0x12b5e8: 4, the stick drawn in parts), the gate's nine
       points (0x12b83c: reverse, neutral, the gears) */
    const unsigned char *gate, *gate_back, *knob[4];  /* 0x12b5c0, 0x12b5c4, 0x12b5c8 */
    int knob_n;
    int gate_pt[9][2];
} dash_state;

extern dash_state g_dash;

/* the car's dashboard (car its name, "F512TR", number +0x48a), at 640x480 (hires) the
   DH files; 0 if missing */
int dash_load(const char *car, int number, int hires);
void dash_free(void);
/* its first 48 colours as a piece of the race's palette (0x2e8bc), and the cockpit's 13
   of the program's at 0xf0 (0xf6d18, load_cockpit_mask 0x5f7c8) */
void dash_queue_colours(void);
void cockpit_queue_colours(void);

#endif
