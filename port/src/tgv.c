/* EA's TGV movies: blocks of u32 tag + u32 size (the 8 included). "1SNh" / "1SNd" the
   sound (an EA stream), "kVGT" a key frame, "fVGT" a delta frame. tgv_frame 0x8d4ac
   sends them to tgv_key 0x8d190 and tgv_delta 0x8cf00.
   Checked: tools/tgv.py, written the same way, equals ffmpeg's decoder on all 26. */
#include "tgv.h"
#include "files.h"
#include "unpack.h"
#include <stdlib.h>
#include <string.h>

static unsigned u16(const unsigned char *p) { return p[0] | p[1] << 8; }
static unsigned long u32(const unsigned char *p) { return u16(p) | (unsigned long)u16(p + 2) << 16; }

int tgv_open(tgv *t, const char *rel)
{
    memset(t, 0, sizeof *t);
    t->d = file_load(rel, &t->len);
    return t->d != 0;
}

void tgv_close(tgv *t)
{
    free(t->d);
    free(t->cur);
    free(t->prev);
    memset(t, 0, sizeof *t);
}

/* bits_signed 0x8cd60 / bits_unsigned 0xa6ec8: fields of width bits, least significant
   bit first */
typedef struct {
    const unsigned char *p, *end;
    unsigned long long acc;
    int have;
} lsb;

static unsigned field(lsb *b, int width)
{
    unsigned v;
    while (b->have < width) {
        b->acc |= (unsigned long long)(b->p < b->end ? *b->p : 0) << b->have;
        b->p++;
        b->have += 8;
    }
    v = (unsigned)(b->acc & ((1ull << width) - 1));
    b->acc >>= width;
    b->have -= width;
    return v;
}

/* tgv_key 0x8d190: +8 width, +0xa height, +0xc, +0xe colours, +0x10 flags (bit 0: the
   picture as it is, else packed), +0x14 the palette (3 bytes a colour, 8 bits), then
   the picture */
static int key(tgv *t, const unsigned char *b, long size)
{
    int w = (int)u16(b + 8), h = (int)u16(b + 0xa), n = (int)u16(b + 0xe);
    unsigned long flags = u32(b + 0x10);
    const unsigned char *pic;
    long left;
    if (n > 256)
        n = 256;
    if (w <= 0 || h <= 0 || w > 640 || h > 480 || 0x14 + 3 * n > size)
        return 0;
    if (w != t->w || h != t->h) {          /* a new size: new buffers */
        free(t->cur);
        free(t->prev);
        t->w = w;
        t->h = h;
        t->cur = calloc((size_t)w * h, 1);
        t->prev = calloc((size_t)w * h, 1);
        if (!t->cur || !t->prev)
            return 0;
    }
    memcpy(t->pal, b + 0x14, 3 * n);
    t->new_palette = 1;
    pic = b + 0x14 + 3 * n;
    left = size - 0x14 - 3 * n;
    if (flags & 1) {
        memcpy(t->cur, pic, left < (long)w * h ? left : (long)w * h);
    } else {
        long got;
        unsigned char *u = unpack(pic, left, &got);
        if (!u)
            return 0;
        memcpy(t->cur, u, got < (long)w * h ? got : (long)w * h);
        free(u);
    }
    return 1;
}

/* tgv_delta 0x8cf00: the pictures swap; +8 vectors, +0xa raw blocks, +0xc packed
   blocks, +0xe bits an index; from +0x14 the vectors (pairs of 10-bit signed x, y), the
   raw 4x4 blocks (16 bytes), the packed (4 colours, u32 of sixteen 2-bit codes top
   first), the indices. tgv_build_frame 0xa6f6c: below the vectors' number, the 4x4 at
   this place + that vector in the previous picture; else a block, raw ones first. */
static int delta(tgv *t, const unsigned char *b, long size)
{
    int w = t->w, h = t->h, nmv, nraw, npacked, vb;
    const unsigned char *p = b + 0x14, *end = b + size, *raw, *packed;
    short mv[2 * 4096];
    unsigned char *swap;
    lsb bits;
    if (!t->cur)
        return 0;
    swap = t->prev; t->prev = t->cur; t->cur = swap;
    nmv = (int)u16(b + 8);
    nraw = (int)u16(b + 0xa);
    npacked = (int)u16(b + 0xc);
    vb = (int)u16(b + 0xe);
    if (nmv > 4096 || vb < 1 || vb > 24)
        return 0;
    bits = (lsb){ p, end, 0, 0 };
    for (int i = 0; i < 2 * nmv; i++) {
        unsigned v = field(&bits, 10);
        mv[i] = (short)(v & 0x200 ? (int)v - 0x400 : (int)v);
    }
    p += ((nmv * 20 + 31) & ~31) >> 3;
    raw = p;
    p += 16 * nraw;
    packed = p;
    p += 8 * npacked;
    if (p > end)
        return 0;
    bits = (lsb){ p, end, 0, 0 };
    for (int by = 0; by + 4 <= h; by += 4) {
        for (int bx = 0; bx + 4 <= w; bx += 4) {
            int i = (int)field(&bits, vb);
            unsigned char *dst = t->cur + by * w + bx;
            if (i < nmv) {
                int sx = bx + mv[2 * i], sy = by + mv[2 * i + 1];
                if (sx < 0 || sy < 0 || sx + 4 > w || sy + 4 > h)
                    continue;              /* outside: leave it (none on the CD) */
                for (int r = 0; r < 4; r++)
                    memcpy(dst + r * w, t->prev + (sy + r) * w + sx, 4);
            } else if ((i -= nmv) < nraw) {
                for (int r = 0; r < 4; r++)
                    memcpy(dst + r * w, raw + 16 * i + 4 * r, 4);
            } else if ((i -= nraw) < npacked) {
                const unsigned char *c = packed + 8 * i;
                unsigned long v = u32(c + 4);
                for (int k = 0; k < 16; k++)
                    dst[(k >> 2) * w + (k & 3)] = c[(v >> (30 - 2 * k)) & 3];
            }
        }
    }
    return 1;
}

int tgv_next_frame(tgv *t)
{
    while (t->pos + 8 <= t->len) {
        const unsigned char *b = t->d + t->pos;
        long size = (long)u32(b + 4);
        if (size < 8 || t->pos + size > t->len)
            break;
        t->pos += size;
        if (!memcmp(b, "kVGT", 4) && key(t, b, size))
            return 1;
        if (!memcmp(b, "fVGT", 4) && delta(t, b, size))
            return 1;
    }
    return 0;
}

int tgv_next_sound(tgv *t, short *out, int max)
{
    while (t->sound_pos + 8 <= t->len) {
        const unsigned char *b = t->d + t->sound_pos;
        long size = (long)u32(b + 4);
        if (size < 8 || t->sound_pos + size > t->len)
            break;
        t->sound_pos += size;
        if (!memcmp(b, "1SNh", 4)) {
            t->has_sound = eacs_header(&t->sound, b + 8, size - 8);
            if (t->has_sound && size > 0x28)
                return eacs_stream_block(&t->sound, b + 0x28, size - 0x28, out, max);
        } else if (!memcmp(b, "1SNd", 4) && t->has_sound) {
            return eacs_stream_block(&t->sound, b + 8, size - 8, out, max);
        } else if (!memcmp(b, "1SNe", 4))
            break;
    }
    return -1;
}
