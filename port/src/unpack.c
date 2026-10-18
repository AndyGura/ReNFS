/* EA's packers. unpack 0xa7020 picks the kind by the first byte (& 0xfe): 0x10 RefPack
   (unpack_refpack 0xaf2c8), 0x30/0x32/0x34 Huffman, 0x46 pairs. The menus' pictures are
   RefPack; the others come with the cars' files. */
#include "unpack.h"
#include <stdlib.h>
#include <string.h>

int is_packed(const unsigned char *d, long len)
{
    return len >= 5 && d[1] == 0xfb;
}

/* RefPack: 0x10FB (0x11FB: a packed size first), the unpacked size big-endian in 24
   bits, then LZ77 commands of 1 to 4 bytes, each some bytes as they are and a copy from
   what is already out */
static unsigned char *refpack(const unsigned char *d, long len, long *out_len)
{
    long p = 2, size, o = 0;
    unsigned char *out;
    if (d[0] & 1)
        p += 3;
    size = (long)d[p] << 16 | d[p + 1] << 8 | d[p + 2];
    p += 3;
    out = malloc(size + 1);
    if (!out)
        return 0;
    while (p < len) {
        unsigned b0 = d[p], lit, n, back;
        if (b0 < 0x80) {                       /* 0-3 bytes, copy 3-10 from 1024 back */
            unsigned b1 = d[p + 1];
            p += 2;
            lit = b0 & 3;
            n = ((b0 >> 2) & 7) + 3;
            back = ((b0 & 0x60) << 3) + b1 + 1;
        } else if (b0 < 0xc0) {                /* 0-3 bytes, copy 4-67 from 16384 back */
            unsigned b1 = d[p + 1], b2 = d[p + 2];
            p += 3;
            lit = b1 >> 6;
            n = (b0 & 0x3f) + 4;
            back = ((b1 & 0x3f) << 8) + b2 + 1;
        } else if (b0 < 0xe0) {                /* 0-3 bytes, copy 5-1028 from 131072 back */
            unsigned b1 = d[p + 1], b2 = d[p + 2], b3 = d[p + 3];
            p += 4;
            lit = b0 & 3;
            n = ((b0 & 0x0c) << 6) + b3 + 5;
            back = ((b0 & 0x10) << 12) + (b1 << 8) + b2 + 1;
        } else if (b0 < 0xfc) {                /* 4-112 bytes as they are */
            lit = ((b0 & 0x1f) << 2) + 4;
            p++;
            n = 0;
            back = 0;
        } else {                               /* the end, with 0-3 bytes */
            lit = b0 & 3;
            p++;
            if (o + lit > size || p + lit > len)
                break;
            memcpy(out + o, d + p, lit);
            o += lit;
            break;
        }
        if (o + lit + n > size || p + lit > len || back > (unsigned long)(o + lit))
            break;                             /* a damaged file: stop */
        memcpy(out + o, d + p, lit);
        o += lit;
        p += lit;
        for (unsigned k = 0; k < n; k++, o++)  /* byte by byte: the copy may overlap */
            out[o] = out[o - back];
    }
    *out_len = o;
    return out;
}

/* a stream of bits read from the top of each byte */
typedef struct {
    const unsigned char *d;
    long len, p;                               /* p counts bits */
} bits;

static unsigned get(bits *b, int n)
{
    unsigned v = 0;
    while (n--) {
        long at = b->p >> 3;
        unsigned byte = at < b->len ? b->d[at] : 0;
        v = v << 1 | ((byte >> (7 - (b->p & 7))) & 1);
        b->p++;
    }
    return v;
}

/* 1xx = 4..7; else z zeros, a one and z + 2 bits: 2^(z+2) + those bits */
static unsigned number(bits *b)
{
    int z = 1;
    if (get(b, 1))
        return 4 | get(b, 2);
    while (!get(b, 1) && z < 24)
        z++;
    return (1u << (z + 2)) | get(b, z + 2);
}

/* the Huffman packing, unpack_huffman 0xaf440: 16 bits the kind (bit 8: 24 bits more to
   skip), 24 bits the size, 8 bits the escape; the codes' lengths (canonical), then the
   symbols as steps over the byte values not yet used; then the data. 32FB adds each byte
   to the sum before it afterwards, 34FB does that twice. */
static unsigned char *huffman(const unsigned char *d, long len, long *out_len)
{
    bits b = { d, len, 0 };
    unsigned kind = get(&b, 16), size, escape;
    int counts[33] = { 0 }, first[33] = { 0 }, index[33] = { 0 }, maxlen = 0;
    unsigned char syms[256], used[256] = { 0 }, *out;
    unsigned code = 0, total = 0, o = 0;
    int s = 255;

    if (kind & 0x100)
        get(&b, 24);
    size = get(&b, 24);
    escape = get(&b, 8);
    for (int L = 1; L <= 32; L++) {            /* the codes of each length */
        unsigned n;
        code <<= 1;
        n = number(&b) - 4;
        counts[L] = (int)n;
        first[L] = (int)code;
        code += n;
        total += n;
        maxlen = L;
        if (n && code == 1u << L)
            break;
        if (total > 256)
            return 0;
    }
    if (total > 256)
        return 0;
    for (unsigned i = 0; i < total; i++) {     /* the symbols, in code order */
        unsigned step = number(&b) - 3;
        int tries = 0;
        while (step && tries++ < 1024) {
            s = (s + 1) & 255;
            if (!used[s])
                step--;
        }
        used[s] = 1;
        syms[i] = (unsigned char)s;
    }
    for (int L = 1; L <= maxlen; L++)
        index[L] = index[L - 1] + counts[L - 1];
    out = malloc(size + 1);
    if (!out)
        return 0;
    while (b.p < len * 8 + 64) {
        unsigned c = 0, sym;
        int L = 0;
        for (;;) {
            c = c << 1 | get(&b, 1);
            if (++L > maxlen)
                goto done;                     /* a damaged file */
            if (counts[L] && c - (unsigned)first[L] < (unsigned)counts[L])
                break;
        }
        sym = syms[index[L] + c - first[L]];
        if (sym != escape) {
            if (o < size)
                out[o++] = (unsigned char)sym;
            continue;
        }
        {
            unsigned n = number(&b) - 4;
            if (n) {                           /* the last byte n times more */
                while (n-- && o < size && o > 0) {
                    out[o] = out[o - 1];
                    o++;
                }
            } else if (get(&b, 1)) {
                break;                         /* the end */
            } else if (o < size) {
                out[o++] = (unsigned char)get(&b, 8);
            }
        }
    }
done:
    kind &= 0xfeff;
    for (int pass = 0; pass < (kind == 0x34fb ? 2 : kind == 0x32fb ? 1 : 0); pass++) {
        unsigned char t = 0;
        for (unsigned i = 0; i < o; i++)
            out[i] = t = (unsigned char)(t + out[i]);
    }
    *out_len = o;
    return out;
}

/* the pair packing 46FB (unpack_pairs 0xb0674; 47FB has 3 bytes more first): 24 bits the
   size, the escape, the number of pairs, then (code, first, second) each; a code stands
   for its two bytes, each of which may be a code again */
typedef struct {
    unsigned char *out;
    unsigned o, size;
    short a[256], b[256];                      /* -1: not a code */
} pairs_t;

static void put(pairs_t *P, unsigned c, int depth)
{
    while (P->a[c] >= 0 && depth < 256) {
        put(P, (unsigned)P->a[c], depth + 1);
        c = (unsigned)P->b[c];
    }
    if (P->o < P->size)
        P->out[P->o++] = (unsigned char)c;
}

static unsigned char *pairs(const unsigned char *d, long len, long *out_len)
{
    pairs_t P;
    long p = d[0] == 0x47 ? 5 : 2;
    unsigned escape, n;
    P.size = (unsigned)d[p] << 16 | d[p + 1] << 8 | d[p + 2];
    escape = d[p + 3];
    n = d[p + 4];
    p += 5;
    memset(P.a, 0xff, sizeof P.a);
    memset(P.b, 0xff, sizeof P.b);
    for (unsigned i = 0; i < n && p + 3 <= len; i++, p += 3) {
        P.a[d[p]] = d[p + 1];
        P.b[d[p]] = d[p + 2];
    }
    P.out = malloc(P.size + 1);
    P.o = 0;
    if (!P.out)
        return 0;
    while (p < len) {
        unsigned c = d[p++];
        if (c == escape) {
            c = p < len ? d[p++] : 0;
            if (c == 0)
                break;
            if (P.o < P.size)
                P.out[P.o++] = (unsigned char)c;
        } else
            put(&P, c, 0);
    }
    *out_len = P.o;
    return P.out;
}

unsigned char *unpack(const unsigned char *d, long len, long *out_len)
{
    if (!is_packed(d, len))
        return 0;
    switch (d[0] & 0xfe) {
    case 0x10:
        return refpack(d, len, out_len);
    case 0x30: case 0x32: case 0x34:
        return huffman(d, len, out_len);
    case 0x46:
        return pairs(d, len, out_len);
    default:                                   /* 4AFB: not on the CD */
        return 0;
    }
}
