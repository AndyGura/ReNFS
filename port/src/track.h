/* A track's TRI file in memory (load_tri 0x4f010, NOTES §3.3), and the ring of its road
   sections decoded round the car (tri_keep_sections 0x4ee30). */
#ifndef RENFS_TRACK_H
#define RENFS_TRACK_H

#define TRACK_RING 0x8700              /* 64 decoded sections of 0x21c bytes */
#define SECTION_BYTES 0x21c

typedef struct {
    unsigned char *file;               /* the whole TRI */
    long len;
    unsigned char *head;               /* "RoadSection", 0x1621c bytes (0x136608) */
    unsigned char *nodes;              /* head + 0x98c, 0x24 bytes each (0x137430) */
    unsigned char *speeds;             /* head + 0x15b0c, 3 bytes a section (0xf74a0) */
    unsigned char *objects;            /* the object kinds, 16 bytes each (0x13bb84) */
    unsigned char *placements;         /* 16 bytes each, to an i32 -1 (0x13bb94) */
    int nplacements;                   /* 0x1365dc */
    unsigned char *fallen;             /* 2 bytes a placement, 0: standing (0xea5d8) */
    unsigned char *slices;             /* "TrackSlices" (0x136604) */
    int loop;                          /* a circuit (0x13bb80) */
    unsigned sec_mask, node_mask;      /* 0x13bb90, 0x13bb88: -1 on an open road */
    int length;                        /* sections round a circuit, else 0x8000 (0x13bb8c) */
    int nodes_len;                     /* 0x12ba00 */
    unsigned char ring[TRACK_RING];    /* 0x1365e8 */
    int ring_head, ring_next;          /* 0x1365ec, 0x1365f0 */
    int ring_span, ring_ahead;         /* 0x1365f4 (35), 0x1365f8 (30) */
    int last_node, last_kinds;         /* 0x136610, 0x136614 */
} track_state;

extern track_state g_track;

/* the track `name` (e.g. "cy1"), from SIMDATA/MISC; 0 if missing */
int track_load(const char *name);
void track_free(void);

/* a road node's fields */
int node_i32(int node, int at);
const unsigned char *node_ptr(int node);

/* the ring kept round section `section` (0x4ee30): sections from 5 behind to 30 ahead */
void track_keep_sections(int section);
/* a decoded section in the ring: 12 bytes, then 4 rows of 11 points x 3 i32 */
const unsigned char *track_section(int section);

#endif
