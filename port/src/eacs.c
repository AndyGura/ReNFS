/* EA's sound. The header (the stream reader 0x8c8f3): +4 rate, +8 bytes a sample, +9
   channels, +0xa compression (0 PCM, 2 IMA ADPCM), +0xb the voice, +0xc samples, +0x10
   loop start, +0x14 loop length, +0x18 where the data starts.
   IMA ADPCM as the game's decoders (0xadfdc mono, 0xae084 stereo): high nibble first;
   stereo is one byte a sample, the high nibble the first channel. A stream's block starts
   with 0x14 bytes: samples, the step index of channel 0 and 1, the value of 0 and 1. */
#include "eacs.h"
#include <string.h>

static const short steps[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55,
    60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411,
    1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
    5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818,
    18500, 20350, 22385, 24623, 27086, 29794, 32767
};
static const signed char adjust[8] = { -1, -1, -1, -1, 2, 4, 6, 8 };

static long u32(const unsigned char *p)
{
    return (long)(p[0] | p[1] << 8 | p[2] << 16 | (unsigned long)p[3] << 24);
}

int eacs_header(eacs *h, const unsigned char *p, long len)
{
    if (len < 0x1c || memcmp(p, "EACS", 4) != 0)
        return 0;
    memset(h, 0, sizeof *h);
    h->rate = (int)u32(p + 4);
    h->bytes = p[8];
    h->channels = p[9];
    h->compression = p[0xa];
    h->voice = (signed char)p[0xb];
    h->samples = u32(p + 0xc);
    h->loop = (int)u32(p + 0x10);
    h->loop_len = u32(p + 0x14);
    h->data = u32(p + 0x18);
    if (h->channels < 1 || h->channels > 2 || (h->bytes != 1 && h->bytes != 2))
        return 0;
    return 1;
}

/* one nibble of one channel (0xadfdc: add the difference, clip, then the index) */
static short ima(eacs *h, int c, unsigned n)
{
    int s = steps[h->ima_index[c]], d = s >> 3, v;
    if (n & 4) d += s;
    if (n & 2) d += s >> 1;
    if (n & 1) d += s >> 2;
    v = h->ima_value[c] + (n & 8 ? -d : d);
    h->ima_value[c] = (short)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
    h->ima_index[c] += adjust[n & 7];
    if (h->ima_index[c] < 0) h->ima_index[c] = 0;
    if (h->ima_index[c] > 88) h->ima_index[c] = 88;
    return h->ima_value[c];
}

int eacs_stream_block(eacs *h, const unsigned char *p, long len, short *out, int max)
{
    int n = 0;
    if (h->compression == 2) {
        if (len < 0x14)
            return 0;
        n = (int)u32(p);
        h->ima_index[0] = (int)u32(p + 4);
        h->ima_index[1] = (int)u32(p + 8);
        h->ima_value[0] = (short)u32(p + 0xc);
        h->ima_value[1] = (short)u32(p + 0x10);
        for (int c = 0; c < 2; c++)
            if (h->ima_index[c] < 0 || h->ima_index[c] > 88)
                h->ima_index[c] = 0;
        p += 0x14;
        len -= 0x14;
        if (n > max)
            n = max;
        if (h->channels == 2) {
            if (n > len)
                n = (int)len;
            for (int k = 0; k < n; k++) {
                out[2 * k] = ima(h, 0, p[k] >> 4);
                out[2 * k + 1] = ima(h, 1, p[k] & 15);
            }
        } else {
            if (n > 2 * len)
                n = (int)(2 * len);
            for (int k = 0; k < n; k++)
                out[k] = ima(h, 0, k & 1 ? p[k >> 1] & 15 : p[k >> 1] >> 4);
        }
        return n;
    }
    if (h->compression != 0)
        return 0;
    n = (int)(len / (h->bytes * h->channels));
    if (n > max)
        n = max;
    for (int i = 0; i < n * h->channels; i++)
        out[i] = h->bytes == 1 ? (short)((signed char)p[i] * 256)
                               : (short)(p[2 * i] | p[2 * i + 1] << 8);
    return n;
}

int eacs_sound(eacs *h, const unsigned char *p, long len, short *out, int max)
{
    int n = h->samples < max ? (int)h->samples : max;

    if (h->compression != 2)
        return eacs_stream_block(h, p, len, out, n);
    h->ima_index[0] = h->ima_index[1] = 0;
    h->ima_value[0] = h->ima_value[1] = 0;
    if (h->channels == 2) {
        if (n > len)
            n = (int)len;
        for (int k = 0; k < n; k++) {
            out[2 * k] = ima(h, 0, p[k] >> 4);
            out[2 * k + 1] = ima(h, 1, p[k] & 15);
        }
    } else {
        if (n > 2 * len)
            n = (int)(2 * len);
        for (int k = 0; k < n; k++)
            out[k] = ima(h, 0, k & 1 ? p[k >> 1] & 15 : p[k >> 1] >> 4);
    }
    return n;
}
