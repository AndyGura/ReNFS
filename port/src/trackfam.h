/* A track's family file (.FAM, NOTES §3.2): the road's textures, the roadside pictures,
   the horizon and the track's 3D models, as load_track_textures 0x5b34c sets them up. */
#ifndef RENFS_TRACKFAM_H
#define RENFS_TRACKFAM_H

#include "model.h"

#define ROAD_TEXTURES 144              /* 0x140cd8: 8 sizes each, the nearest first */
#define ROADSIDE_PICTURES 64           /* 0x141ed8: 4 slots each */
#define TRACK_MODELS 16                /* 0x146f14, 0x138 bytes each */

typedef struct {
    unsigned char *file;               /* the family, in memory (0x145c50[0]) */
    long len;
    unsigned char *banks, *pictures, *horizon;   /* entries 0, 1, 2 (0x143de4, 0x143de0, 0x143e50) */
    unsigned char *texture[ROAD_TEXTURES][8];    /* shapes (their first block) or 0 */
    unsigned char *picture[ROADSIDE_PICTURES][4];
    unsigned char *road_pal;           /* the road's palette, 6 bits, tinted: 0x90.. of the race's */
    unsigned char *horizon_pal;
    int nmodels;                       /* the 3D objects (entry 3, load_track_models 0x66118) */
    model models[TRACK_MODELS];        /* 0x146f24 + 0x138 × i: each made drawable */
} track_fam;

extern track_fam g_fam;

/* the family of track `name` ("cy1"): family 1 = SIMDATA/ETRACKFM/<name>_001.FAM (G.. in
   German), else NTRACKFM's _T01 (or _M01, _R01 by the race's kind: 2, 3); its textures
   found and the re-colouring queued (racepal.h), the 3D objects made drawable (turned
   over on a mirrored track, g_road_mirror). 0 if missing */
int trackfam_load(const char *name, int family, int kind);
void trackfam_free(void);

/* a "wwww" container's entries */
int ww_count(const unsigned char *c);
unsigned char *ww_entry(unsigned char *c, int i);
/* the shape of that name in an "SHPI" bank, exactly; 0 if none */
unsigned char *shpi_find(unsigned char *bank, const char *name);

#endif
