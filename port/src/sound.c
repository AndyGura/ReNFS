/* The menus' sound.
   The effects: FRONT_MW.BNK (SIMDATA/SOUNDBNK), 128 offsets of records with an EACS
   header at +0x28 (NOTES §3.8); played in the middle at the volume CFG_SFX_VOLUME.
   The music: an .AS4 stream (1SNh / 1SNd blocks, 1SNl the loop's start, 1SNe the end),
   read ahead half a second at every retrace as 0x71b6c reads it whenever the menus wait. */
#include "sound.h"
#include "audio.h"
#include "config.h"
#include "eacs.h"
#include "files.h"
#include "video.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char *g_bank;
static long g_bank_len;

static unsigned u16(const unsigned char *p) { return p[0] | p[1] << 8; }
static unsigned long u32(const unsigned char *p) { return u16(p) | (unsigned long)u16(p + 2) << 16; }

void fe_sound_bank_load(void)
{
    fe_sound_bank_free();
    g_bank = file_load("SIMDATA/SOUNDBNK/FRONT_MW.BNK", &g_bank_len);
}

void fe_sound_bank_free(void)
{
    free(g_bank);
    g_bank = 0;
}

void fe_play_sound(int n)
{
    static short out[2 * 65536];
    unsigned long rec;
    eacs h;
    int s, frames;
    if (!g_bank || n < 0 || n >= 128 || g_bank_len < 512)
        return;
    rec = u32(g_bank + 4 * n);
    if (!rec || (long)rec + 0x48 > g_bank_len || !eacs_header(&h, g_bank + rec + 0x28, 0x20))
        return;
    if (h.data >= g_bank_len)
        return;
    /* the data is in the file, not in the record */
    frames = eacs_stream_block(&h, g_bank + h.data, g_bank_len - h.data, out,
                               h.samples < 65536 ? (int)h.samples : 65536);
    s = audio_stream_open(h.rate, h.channels);
    if (s < 0)
        return;
    audio_stream_volume(s, config_word(CFG_SFX_VOLUME));
    audio_stream_put(s, out, frames);
    audio_stream_release(s);
}

/* the music */
static struct {
    unsigned char *d;
    long len, pos, loop;           /* loop: the 1SNl's block, -1 none */
    eacs h;
    int has_header, stream, fading, volume;
    unsigned fade_start;
} g_music = { .stream = -1 };

static unsigned long g_music_counter;  /* 0xed0fc */

static void music_stop(void)
{
    audio_stream_close(g_music.stream);
    free(g_music.d);
    memset(&g_music, 0, sizeof g_music);
    g_music.stream = -1;
}

/* the next block of sound, the loop followed; -1 at the end */
static int music_block(short *out, int max)
{
    for (int guard = 0; guard < 4096; guard++) {
        const unsigned char *b;
        long size;
        if (g_music.pos + 8 > g_music.len)
            goto end;
        b = g_music.d + g_music.pos;
        size = (long)u32(b + 4);
        if (size < 8 || g_music.pos + size > g_music.len)
            goto end;
        g_music.pos += size;
        if (!memcmp(b, "1SNh", 4)) {
            g_music.has_header = eacs_header(&g_music.h, b + 8, size - 8);
            if (g_music.has_header && size > 0x28)
                return eacs_stream_block(&g_music.h, b + 0x28, size - 0x28, out, max);
        } else if (!memcmp(b, "1SNd", 4) && g_music.has_header) {
            return eacs_stream_block(&g_music.h, b + 8, size - 8, out, max);
        } else if (!memcmp(b, "1SNl", 4) && size >= 12) {
            g_music.loop = (long)u32(b + 8);
        } else if (!memcmp(b, "1SNe", 4))
            goto end;
        continue;
end:
        if (g_music.loop < 0 || g_music.loop >= g_music.len)
            return -1;
        g_music.pos = g_music.loop;
    }
    return -1;
}

static void music_service(void)
{
    static short out[2 * 32768];
    if (!g_music.d)
        return;
    if (g_music.fading) {
        int v = 127 - (int)(clock_ticks() - g_music.fade_start) * 127 / 100;
        audio_stream_volume(g_music.stream, v);
        if (v <= 0) {
            music_stop();
            return;
        }
    }
    while (!g_music.has_header || audio_stream_queued(g_music.stream) < g_music.h.rate / 2) {
        int n = music_block(out, 32768);
        if (n < 0)
            break;
        if (g_music.stream < 0 && g_music.has_header)
            g_music.stream = audio_stream_open(g_music.h.rate, g_music.h.channels);
        if (g_music.stream < 0)
            break;
        audio_stream_put(g_music.stream, out, n);
    }
}

void music_start(const char *name)
{
    char path[256];
    if (!config_word(CFG_MENU_MUSIC))
        return;
    music_stop();
    snprintf(path, sizeof path, "FRONTEND/MUSIC/%s", name);
    g_music.d = file_load(path, &g_music.len);
    if (!g_music.d) {
        fprintf(stderr, "renfs: %s is missing\n", path);
        return;
    }
    g_music.loop = -1;
    g_retrace_hook = music_service;
    music_service();
}

void music_choose(void)
{
    static const char *const names[12] = {
        "ROCK1.AS4", "ROCK2.AS4", "ROCK3.AS4", "ROCK4.AS4", "ROCK5.AS4", "ROCK6.AS4",
        "TECHNO1.AS4", "TECHNO2.AS4", "TECHNO3.AS4", "TECHNO4.AS4", "TECHNO5.AS4", "TECHNO6.AS4",
    };
    int mode = config_word(CFG_MENU_MUSIC), pick;
    unsigned long c = g_music_counter;
    if (mode < 0 || mode > 15)
        return;
    if (mode == 0)
        pick = 5;
    else if (mode == 1)
        pick = (int)(c % 12);
    else if (mode == 3 || mode >= 10)
        pick = (int)(c % 6) + 6;
    else
        pick = (int)(c % 6);
    /* 5 means none: so ROCK6 is never chosen here (as in the original) */
    if (pick != 5)
        music_start(names[pick]);
}

void music_fade_out(void)
{
    if (g_music.d && !g_music.fading) {
        g_music.fading = 1;
        g_music.fade_start = clock_ticks();
    }
}

void music_wait_end(void)
{
    while (g_music.d && g_music.fading && !g_quit)
        video_retrace();
    music_stop();
}
