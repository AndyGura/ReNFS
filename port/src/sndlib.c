/* EA's sound library as the race uses it (NOTES §4.18). */
#include "sndlib.h"
#include "audio.h"
#include "collide.h"
#include "exe.h"
#include <string.h>

snd_lib g_snd;

static unsigned u32(const unsigned char *p) { return p[0] | p[1] << 8 | p[2] << 16 | (unsigned)p[3] << 24; }
static int i32(const unsigned char *p) { return (int)u32(p); }

/* a sound's header (EACS) and its data in its bank: the record's +4, the header's +0x18
   (file offsets, made pointers as 0x958f5 puts the bank in) */
typedef struct {
    const unsigned char *h, *data;
} snd_rec;

static int bank_of(const unsigned char *rec)
{
    for (int b = 0; b < SND_BANKS; b++)
        if (g_snd.bank[b] && rec >= g_snd.bank[b] && rec < g_snd.bank[b] + g_snd.bank_len[b])
            return b;
    return -1;
}

static int rec_parts(const unsigned char *rec, snd_rec *r)
{
    int b = bank_of(rec);
    long hd, dd;
    if (b < 0)
        return 0;
    hd = (long)u32(rec + 4);
    if (hd < 0 || hd + 0x1c > g_snd.bank_len[b])
        return 0;
    r->h = g_snd.bank[b] + hd;
    dd = (long)u32(r->h + 0x18);
    r->data = dd >= 0 && dd < g_snd.bank_len[b] ? g_snd.bank[b] + dd : 0;
    return 1;
}

const unsigned char *snd_header(int n)
{
    snd_rec r;
    if (n < 0 || n >= SND_SOUNDS || !g_snd.slot[n] || !rec_parts(g_snd.slot[n], &r))
        return 0;
    return r.h;
}

/* 0x9eaae: a voice free */
static void voice_free(int v)
{
    snd_voice *s = &g_snd.v[v];
    s->playing = 0;
    s->f4 = -1;
    s->rec = 0;
    s->sound = 0xff;
    s->f18 = 0;
    s->pan = 0x40;
    s->rec_volume = 0x7f;
    s->volume = 0x7f;
}

void snd_init(int on)
{
    memset(&g_snd, 0, sizeof g_snd);
    for (int v = 0; v < SND_VOICES; v++)
        voice_free(v);
    g_snd.master = 0x7f;
    g_snd.on = on;
}

int snd_bank_add(const unsigned char *d, long len)
{
    int b = -1, n;
    if (!d)
        return -7;
    for (n = 0; n < SND_BANKS; n++)
        if (!g_snd.bank[n]) {
            b = n;
            break;
        }
    if (b < 0)
        return -9;
    /* (the file's offsets are made pointers in place; the port keeps the bank as it is) */
    g_snd.bank[b] = d;
    g_snd.bank_len[b] = len;
    for (n = 0; n < SND_SOUNDS && 4 * n + 4 <= len; n++) {
        unsigned at = u32(d + 4 * n);
        if (!at)
            continue;
        if (g_snd.slot[n]) {
            g_snd.bank[b] = 0;     /* (the original leaves it out of its list, too) */
            return -1;
        }
        if ((long)at + 0x48 <= len)
            g_snd.slot[n] = d + at;
    }
    return b;
}

void snd_bank_remove(int b)
{
    if (b == -1) {
        for (b = 0; b < SND_BANKS; b++)
            snd_bank_remove(b);
        return;
    }
    if (b < 0 || b >= SND_BANKS || !g_snd.bank[b])
        return;
    for (int n = 0; n < SND_SOUNDS && 4 * n + 4 <= g_snd.bank_len[b]; n++)
        if (u32(g_snd.bank[b] + 4 * n))
            g_snd.slot[n] = 0;
    g_snd.bank[b] = 0;
}

/* 0x98bf5: a voice the mask allows, from the one after the last given: a free one, else
   the first whose priority is no higher */
static snd_voice *voice_find(unsigned mask, unsigned priority)
{
    int v = g_snd.next, k;
    for (k = 0; k < SND_VOICES; k++, v = (v + 1) & 15)
        if ((mask >> v & 1) && !g_snd.v[v].playing)
            goto found;
    v = g_snd.next;
    for (k = 0; k < SND_VOICES; k++, v = (v + 1) & 15)
        if ((mask >> v & 1) && priority >= g_snd.v[v].priority)
            goto found;
    return 0;
found:
    g_snd.v[v].index = (unsigned char)v;
    g_snd.next = (v + 1) & 15;
    return &g_snd.v[v];
}

/* -spread .. spread - 1 from EA's random numbers (the original's shifts kept: unsigned) */
static int spread(int s)
{
    unsigned r = collide_rand() >> 16;
    return (int)((unsigned)((int)r * s) >> 15) - s;
}

/* 0x9ea7d: the voice's level from the master volume, the record's and the one asked */
static void voice_level(snd_voice *s)
{
    s->level = (signed char)(g_snd.master * (s->rec_volume * s->volume) / 0x3f01);
}

/* 0xa203a: the two sides' volumes of a level at a pan (0xff - pan above 0x7f) */
static void sides(int pan, int level, int *left, int *right)
{
    int l, r;
    if (pan > 0x7f)
        pan = 0xff - pan;
    if (pan < 0x40) {
        l = 0x7f;
        r = pan * 2;
    } else if (pan == 0x40)
        l = r = 0x7f;
    else {
        l = (0x7f - pan) * 0x7e / 0x3e;
        r = 0x7f;
    }
    *left = (int)((unsigned)(level * l) / 0x7f);
    *right = (int)((unsigned)(level * r) / 0x7f);
}

/* 0xa2181: a pitch bend's cents added to the voice's own */
static int bent(int bend, int reach, int cents)
{
    int t = (bend - 0x40) * reach * 100;
    return t / 64 + cents;
}

/* 0xa062c: a sample's rate (frames a second) moved by cents: the program's table of 1200
   steps an octave (0x9eaec), the octave by a shift */
static unsigned long pitch_rate(unsigned rate, int cents)
{
    int x = cents + 0x2000, top = 0x40d0, shift = 9;
    unsigned long long t;
    while (x < top) {
        top -= 0x4b0;
        shift++;
    }
    x -= top;
    if (x < 0 || x >= 1200)
        x = x < 0 ? 0 : 1199;
    t = (unsigned long long)rate * (unsigned)exe_i32(0x9eaec + 4 * x);
    return (unsigned long)(unsigned)(t >> shift);
}

/* 0xa1f11: the voice's sample to the mixer */
static void voice_start(snd_voice *s, const snd_rec *r)
{
    int left, right, frames;
    sides(s->pan, s->level, &left, &right);
    s->freq = pitch_rate(u32(r->h + 4), bent(s->bend, s->reach, s->cents));
    s->pos = 0;
    frames = i32(r->h + 0xc);
    if (r->data)
        audio_voice_start(s->index, r->data, r->h[8], r->h[9], (unsigned long)frames,
                          i32(r->h + 0x10), u32(r->h + 0x14), s->freq << 16, left, right);
}

/* 0x95b4e: one sound on a voice */
static int play_one(const unsigned char *rec, int n, int pan, int volume, int bend)
{
    snd_voice *s = voice_find(u32(rec), rec[0x14]);
    snd_rec r;
    int v;
    if (!s)
        return -9;
    if (!rec_parts(rec, &r) || memcmp(r.h, "EACS", 4))
        return -7;
    if (!g_snd.on)
        return -10;
    s->playing = 1;
    s->rec = rec;
    s->f4 = i32(rec + 8) - 1;
    s->bend = (unsigned char)bend;
    s->reach = rec[0x17];
    s->priority = rec[0x14];
    s->sound = (unsigned char)n;
    s->kind = 1;
    s->flags = rec[0x1c];
    if (i32(rec + 0x10) || i32(rec + 0xc))
        s->cents = i32(rec + 0x10) + spread(i32(rec + 0xc));
    else
        s->cents = 0;
    if (rec[0x1a]) {
        v = spread((signed char)rec[0x1a]) + (signed char)rec[0x19];
        s->rec_volume = (signed char)(v > 0x7f ? 0x7f : v < 0 ? 0 : v);
    } else
        s->rec_volume = (signed char)rec[0x19];
    if (pan == -1) {
        if (rec[0x1d]) {
            /* the first draw only says whether it is past the right; the pan is a second */
            if (spread(rec[0x1d]) + 0x40 > 0x7f)
                s->pan = 0x7f;
            else
                s->pan = (unsigned char)(spread(rec[0x1d]) + 0x40);
        } else
            s->pan = rec[0x18];
    } else
        s->pan = (unsigned char)pan;
    s->volume = (signed char)volume;
    s->f18 = 0;
    voice_level(s);
    voice_start(s, &r);
    return s->index;
}

/* 0xa20a8: a pair's two volumes from the pan (near either edge shared out), and the pan
   they both get; n stands in for the original's leftover register at a pan past 0xff */
static void pair_sides(int pan, int volume, int n, int *p, int *a, int *b)
{
    int e = n;
    if (pan >= 0 && pan < 0x80) {
        *p = pan;
        e = pan > 0x7a ? 0x7f - pan : pan;
        *a = e < 5 ? (e + 5) * volume / 10 : volume;
        *b = volume - *a;
    } else if (pan >= 0x80 && pan < 0x100) {
        *p = 0xff - pan;
        e = *p > 0x7a ? 0x7f - *p : *p;
        *b = e < 5 ? (e + 5) * volume / 10 : volume;
        *a = volume - *b;
    } else {
        *p = 0x40;
        *a = volume;
        *b = 0;
    }
    if (e < 5) {
        *a = exe_i32(0x12412c + 4 * e) * *a / 100;
        *b = exe_i32(0x12412c + 4 * e) * *b / 100;
    }
}

int snd_play(int n, int pan, int volume, int bend)
{
    const unsigned char *rec;
    int p, a, b, v, w;
    if (n < 0 || n >= SND_SOUNDS || !(rec = g_snd.slot[n]))
        return -8;
    if (!(rec[0x1c] & 1))
        return play_one(rec, n, pan, volume, bend);
    /* a pair: this sound and the next, the pan shared out between them */
    pair_sides(pan, volume, n, &p, &a, &b);
    v = play_one(rec, n, p, a & 0xff, bend);
    if (v < 0)
        return v;
    if (n + 1 >= SND_SOUNDS || !g_snd.slot[n + 1])
        return -8;
    w = play_one(g_snd.slot[n + 1], n + 1, p, b & 0xff, bend);
    if (w < 0) {
        snd_stop(n);           /* (the original stops the voice numbered as the sound) */
        return w;
    }
    return w << 16 | v;
}

static snd_voice *live(int v)
{
    if (v < 0 || v >= SND_VOICES || !g_snd.v[v].playing)
        return 0;
    return &g_snd.v[v];
}

static void voice_sides(snd_voice *s)
{
    int left, right;
    sides(s->pan, s->level, &left, &right);
    audio_voice_volume(s->index, left, right);
}

int snd_volume(int v, int volume)
{
    snd_voice *s;
    if (v < 0 || v >= SND_VOICES)
        return -8;
    if (!g_snd.on)
        return -10;
    if (volume > 0x7f || volume < 0 || !(s = live(v)))
        return -8;
    s->volume = (signed char)volume;
    voice_level(s);
    voice_sides(s);
    return 0;
}

int snd_pan(int v, int pan)
{
    snd_voice *s;
    if (v < 0 || v >= SND_VOICES)
        return -8;
    if (!g_snd.on)
        return -10;
    if (pan > 0xff || pan < 0 || !(s = live(v)))
        return -8;
    s->pan = (unsigned char)pan;
    voice_level(s);
    voice_sides(s);
    return 0;
}

int snd_bend(int v, int bend)
{
    snd_voice *s;
    snd_rec r;
    if (v < 0 || v >= SND_VOICES)
        return -8;
    if (!g_snd.on)
        return -10;
    s = &g_snd.v[v];
    if (s->kind != 1 || bend < 0 || bend > 0x7f || !s->playing)
        return -8;
    if (s->rec && rec_parts(s->rec, &r)) {
        s->freq = pitch_rate(u32(r.h + 4), bent(bend, s->reach, s->cents));
        audio_voice_rate(v, s->freq << 16);
    }
    return 0;
}

int snd_idle(int v)
{
    if (v < 0 || v >= SND_VOICES)
        return -8;
    if (!g_snd.on)
        return -10;
    return g_snd.v[v].playing == 1 ? 0 : 1;
}

int snd_stop(int v)
{
    if (v < 0 || v >= SND_VOICES)
        return -8;
    if (!g_snd.on)
        return -10;
    if (!g_snd.v[v].playing)
        return -8;
    audio_voice_stop(v);
    voice_free(v);
    return 0;
}

void snd_time(unsigned now)
{
    unsigned ticks = now - g_snd.last;
    g_snd.last = now;
    if (g_snd.keep || !ticks)
        return;
    for (int v = 0; v < SND_VOICES; v++) {
        snd_voice *s = &g_snd.v[v];
        snd_rec r;
        long long frames;
        if (s->playing != 1 || s->kind != 1 || !s->rec || !rec_parts(s->rec, &r))
            continue;
        /* a sound that loops plays until it is stopped */
        if (u32(r.h + 0x14))
            continue;
        s->pos += ((unsigned long long)s->freq << 32) / 100 * ticks;
        frames = i32(r.h + 0xc);
        if ((long long)(s->pos >> 32) >= frames) {
            audio_voice_stop(v);
            voice_free(v);
        }
    }
}

void snd_hold(int v, int priority, int volume)
{
    snd_voice *s = &g_snd.v[v];
    s->playing = 1;
    s->kind = 2;
    s->priority = (unsigned char)priority;
    s->volume = (signed char)volume;
    s->index = (unsigned char)v;
}
