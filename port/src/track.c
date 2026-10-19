/* A track's TRI file (NOTES §3.3). The original keeps the file open and reads each
   stored section as the car nears it; the port has the whole file in memory and decodes
   the same sections into the same ring, so what the drawing reads is the same. */
#include "track.h"
#include "files.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

track_state g_track;

#define HEAD_LEN 0x1621c

static int rd32(const unsigned char *p)
{
    return (int)((unsigned)p[0] | (unsigned)p[1] << 8 | (unsigned)p[2] << 16 | (unsigned)p[3] << 24);
}

static int rd16(const unsigned char *p)
{
    return (short)(p[0] | p[1] << 8);
}

static void wr32(unsigned char *p, int v)
{
    p[0] = (unsigned char)v;
    p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16);
    p[3] = (unsigned char)(v >> 24);
}

const unsigned char *node_ptr(int node)
{
    return g_track.nodes + (unsigned)(node & g_track.node_mask) * 0x24;
}

int node_i32(int node, int at)
{
    return rd32(node_ptr(node) + at);
}

int track_load(const char *name)
{
    track_state *t = &g_track;
    char path[64];
    long objlen, slicelen;
    unsigned char *f;

    track_free();
    snprintf(path, sizeof path, "SIMDATA/MISC/%s.tri", name);
    f = file_load(path, &t->len);
    if (!f || t->len < HEAD_LEN + 12 || rd32(f) != 0x11) {
        fprintf(stderr, "renfs: %s is missing or not a track\n", path);
        free(f);
        return 0;
    }
    t->file = t->head = f;
    t->nodes = f + 0x98c;
    t->speeds = f + 0x15b0c;
    objlen = rd32(f + HEAD_LEN + 4) - 12;              /* "SJBO", its size with the head */
    t->objects = f + HEAD_LEN + 12;
    t->placements = t->objects + rd32(f + 0x16214) * 16;
    t->slices = t->objects + objlen;
    slicelen = rd32(f + 0x24);
    if (memcmp(f + HEAD_LEN, "SJBO", 4) != 0 || t->slices + slicelen > f + t->len) {
        fprintf(stderr, "renfs: %s is not a track\n", path);
        track_free();
        return 0;
    }
    for (t->nplacements = 0; t->nplacements < 1000 &&
         rd32(t->placements + t->nplacements * 16) != -1; t->nplacements++)
        ;
    t->fallen = calloc((size_t)rd32(f + 0x16218) + 1, 2);   /* (0x1c630) */
    if (!t->fallen) {
        track_free();
        return 0;
    }

    if (rd16(f + 4) == 0) {                            /* an open road */
        t->sec_mask = t->node_mask = 0xffffffffu;
        t->length = 0x8000;
        t->nodes_len = 0x20000;
    } else {
        t->length = rd16(f + 4);
        t->sec_mask = (unsigned)t->length - 1;
        t->nodes_len = t->length * 4;
        t->node_mask = (unsigned)t->nodes_len - 1;
    }
    t->loop = rd16(f + 4) != 0;
    t->ring_ahead = 30;
    t->ring_span = t->ring_ahead + 5;
    t->ring_head = t->ring_next = 0;
    memset(t->ring, 0, sizeof t->ring);
    t->last_node = t->last_kinds = -1;
    track_keep_sections(0);
    /* node 0 takes two words of the head */
    memcpy(t->nodes + 0x14, f + 0x18, 2);
    memcpy(t->nodes + 0x18, f + 0x20, 2);
    return 1;
}

void track_free(void)
{
    free(g_track.file);
    free(g_track.fallen);
    g_track.file = 0;
    g_track.fallen = 0;
}

const unsigned char *track_section(int section)
{
    return g_track.ring + ((section & g_track.sec_mask) * SECTION_BYTES) % TRACK_RING;
}

/* a stored section into the ring (0x4eb00): its 12 bytes, then each row's points from
   its node: point 0 from the node, 1 and 6 from point 0, the others from the one before,
   in 1/128 units */
static void decode_section(unsigned s, unsigned char *dst)
{
    const unsigned char *rec = g_track.slices + rd32(g_track.head + 0x2c + (s & g_track.sec_mask) * 4);
    const unsigned char *d = rec + 0x18;
    int row, i;
    memcpy(dst, rec + 0xc, 12);
    for (row = 0; row < 4; row++, d += 66) {
        int node = (int)(s * 4 + row);
        int x0 = node_i32(node, 8) + rd16(d) * 0x200;
        int y0 = node_i32(node, 12) + rd16(d + 2) * 0x200;
        int z0 = node_i32(node, 16) + rd16(d + 4) * 0x200;
        int x = x0, y = y0, z = z0;
        unsigned char *o = dst + 12 + row * 0x84;
        wr32(o, x0);
        wr32(o + 4, y0);
        wr32(o + 8, z0);
        for (i = 1; i < 11; i++) {
            if (i == 1 || i == 6) {
                x = x0;
                y = y0;
                z = z0;
            }
            x += rd16(d + i * 6) * 0x200;
            y += rd16(d + i * 6 + 2) * 0x200;
            z += rd16(d + i * 6 + 4) * 0x200;
            wr32(o + i * 12, x);
            wr32(o + i * 12 + 4, y);
            wr32(o + i * 12 + 8, z);
        }
    }
}

/* 0x4ed74: nothing before the start of an open road */
static void load_section(int section)
{
    unsigned s = (unsigned)section & g_track.sec_mask;
    unsigned at;
    if ((int)s < 0 && !g_track.loop)
        return;
    at = (s * SECTION_BYTES) % TRACK_RING;
    if (memcmp(g_track.slices + rd32(g_track.head + 0x2c + s * 4), "TRKD", 4) != 0)
        return;                                        /* the original waits for the CD */
    decode_section(s, g_track.ring + at);
}

/* how far a is from b round a circuit, as the original folds it */
static int fold(int d, int len)
{
    if (d > len / 2)
        return len - d;
    if (d < -len / 2)
        return d + len;
    return d;
}

void track_keep_sections(int section)
{
    track_state *t = &g_track;
    int d = fold(t->ring_head - section, t->length);
    int e = section - (t->ring_head - t->ring_ahead);
    if (e <= 0)
        e = -e;
    e = fold(e, t->length);
    if (e > t->ring_span) {                            /* far off: start again there */
        t->ring_head = (int)((unsigned)(t->ring_ahead + section - t->ring_span) & t->sec_mask);
        d = t->ring_ahead - t->ring_span;
    }
    while (d < t->ring_ahead) {
        t->ring_next = (int)((unsigned)(t->ring_head + 1) & t->sec_mask);
        load_section(t->ring_head);
        t->ring_head = (int)((unsigned)(t->ring_head + 1) & t->sec_mask);
        d = fold(t->ring_head - section, t->length);
    }
    while (d > t->ring_ahead) {
        t->ring_next = (int)((unsigned)(t->ring_head - 1) & t->sec_mask);
        load_section(t->ring_head - 1 - t->ring_span);
        t->ring_head = (int)((unsigned)(t->ring_head - 1) & t->sec_mask);
        d = fold(t->ring_head - section, t->length);
    }
}
