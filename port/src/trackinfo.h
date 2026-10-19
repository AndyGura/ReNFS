/* A track's sky and horizon settings, and the tint of its colours (load_track_info 0x49dc0,
   NOTES §3.11): from SIMDATA/MISC/<track>INFO, or the table built into the game. */
#ifndef RENFS_TRACKINFO_H
#define RENFS_TRACKINFO_H

typedef struct {
    int vert_near, vert_far;          /* 0x12ef44, 0x12ef48 (16.16) */
    int z_near, z_far;                /* 0x12ef4c, 0x12ef50 */
    int radius_near, height_near;     /* 0x12ef54, 0x12ef58 */
    int radius_far, height_far;       /* 0x12ef5c, 0x12ef60 */
    int colour_top, colour_bottom;    /* 0x12ef64, 0x12ef68: a << 10 | b << 5 | c */
    int ring_type;                    /* 0x12ef6c */
    int ring_y, ring_height;          /* 0x12ef70, 0x12ef74 (scaled to 200 lines at 320x200) */
    /* three tints of five: red, green, blue scales (256 = 1), towards the grey (65536 =
       all the way), brightness (65536 = 1); the family picks one (0x12ef78 + 4g + 12k) */
    int tint[3][5];
    int last;                         /* 0x12efb4 */
    int last_used;                    /* 0x12efc4 */
} track_info;

extern track_info g_info;

/* the track `name` ("cy1"); family: 1 the CD's usual textures (ETRACKFM), 0 the other set
   (NTRACKFM _T) which reads the file; high: the 640x480 view. Sets the tint of the race's
   palettes (pal_set_tint) from the family's. 0 if the track is unknown */
int trackinfo_load(const char *name, int family, int high);

#endif
