/* The sound card. SDL's device at 44.1 kHz, 16-bit stereo; each stream is converted to
   that by an SDL_AudioStream and the streams are added together, clipped. */
#include "audio.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STREAMS 8
#define RATE 44100

static SDL_AudioDeviceID g_dev;
static FILE *g_wav;            /* RENFS_SNDWAV: what is heard, written to a file */
static unsigned long g_wav_bytes;

static void put32(unsigned char *p, unsigned long v)
{
    for (int b = 0; b < 4; b++)
        p[b] = (unsigned char)(v >> (8 * b));
}

/* a WAV header for what is written so far: 16-bit stereo at RATE */
static void wav_header(void)
{
    unsigned char h[44] = { 'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E',
                            'f', 'm', 't', ' ', 16, 0, 0, 0, 1, 0, 2, 0 };
    put32(h + 4, 36 + g_wav_bytes);
    put32(h + 24, RATE);
    put32(h + 28, RATE * 4UL);
    h[32] = 4;
    h[34] = 16;
    memcpy(h + 36, "data", 4);
    put32(h + 40, g_wav_bytes);
    fseek(g_wav, 0, SEEK_SET);
    fwrite(h, 1, 44, g_wav);
    fseek(g_wav, 0, SEEK_END);
}

static struct {
    SDL_AudioStream *conv;
    int channels, rate;
    int volume;                /* 0..127 */
    int release;               /* closed by the mixer once played out */
} g_st[STREAMS];

static void mix(void *unused, Uint8 *out, int len)
{
    static short buf[4096];
    int frames = len / 4;
    int acc[4096];
    (void)unused;
    if (frames > 2048)
        frames = 2048;
    memset(acc, 0, sizeof acc);
    for (int s = 0; s < STREAMS; s++) {
        int got;
        if (!g_st[s].conv)
            continue;
        got = SDL_AudioStreamGet(g_st[s].conv, buf, frames * 4);
        if (got <= 0) {
            if (g_st[s].release) {
                SDL_FreeAudioStream(g_st[s].conv);
                g_st[s].conv = 0;
            }
            continue;
        }
        for (int i = 0; i < got / 2; i++)
            acc[i] += buf[i] * g_st[s].volume / 127;
        if (g_st[s].release && SDL_AudioStreamAvailable(g_st[s].conv) == 0) {
            SDL_FreeAudioStream(g_st[s].conv);
            g_st[s].conv = 0;
        }
    }
    for (int i = 0; i < frames * 2; i++) {
        int v = acc[i];
        ((short *)out)[i] = (short)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
    }
    if (len > frames * 4)
        memset(out + frames * 4, 0, len - frames * 4);
    if (g_wav) {
        fwrite(out, 1, len, g_wav);
        g_wav_bytes += len;
        wav_header();              /* kept right, should the program be stopped */
    }
}

int audio_init(void)
{
    SDL_AudioSpec want = { 0 }, got;
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
        return 0;
    want.freq = RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    want.callback = mix;
    g_dev = SDL_OpenAudioDevice(0, 0, &want, &got, 0);
    if (!g_dev)
        return 0;
    if (getenv("RENFS_SNDWAV") && (g_wav = fopen(getenv("RENFS_SNDWAV"), "wb")))
        wav_header();
    SDL_PauseAudioDevice(g_dev, 0);
    return 1;
}

void audio_done(void)
{
    if (!g_dev)
        return;
    SDL_CloseAudioDevice(g_dev);
    if (g_wav) {
        wav_header();
        fclose(g_wav);
        g_wav = 0;
    }
    for (int s = 0; s < STREAMS; s++)
        if (g_st[s].conv)
            SDL_FreeAudioStream(g_st[s].conv);
    g_dev = 0;
}

int audio_stream_open(int rate, int channels)
{
    if (!g_dev)
        return -1;
    for (int s = 0; s < STREAMS; s++) {
        if (g_st[s].conv)
            continue;
        SDL_AudioStream *c = SDL_NewAudioStream(AUDIO_S16SYS, (Uint8)channels, rate,
                                                AUDIO_S16SYS, 2, RATE);
        if (!c)
            return -1;
        SDL_LockAudioDevice(g_dev);
        g_st[s].conv = c;
        g_st[s].channels = channels;
        g_st[s].rate = rate;
        g_st[s].volume = 127;
        g_st[s].release = 0;
        SDL_UnlockAudioDevice(g_dev);
        return s;
    }
    return -1;
}

void audio_stream_put(int s, const short *samples, int frames)
{
    if (s < 0 || !g_st[s].conv || frames <= 0)
        return;
    SDL_LockAudioDevice(g_dev);
    SDL_AudioStreamPut(g_st[s].conv, samples, frames * g_st[s].channels * 2);
    SDL_UnlockAudioDevice(g_dev);
}

int audio_stream_queued(int s)
{
    long q;
    if (s < 0 || !g_st[s].conv)
        return 0;
    SDL_LockAudioDevice(g_dev);
    /* what the converter still holds, back at the stream's own rate */
    q = (long)(SDL_AudioStreamAvailable(g_st[s].conv) / 4) * g_st[s].rate / RATE;
    SDL_UnlockAudioDevice(g_dev);
    return q > 0 ? (int)q : 0;
}

void audio_stream_volume(int s, int volume)
{
    if (s < 0 || !g_st[s].conv)
        return;
    SDL_LockAudioDevice(g_dev);
    g_st[s].volume = volume < 0 ? 0 : volume > 127 ? 127 : volume;
    SDL_UnlockAudioDevice(g_dev);
}

void audio_stream_release(int s)
{
    if (s < 0 || !g_st[s].conv)
        return;
    SDL_LockAudioDevice(g_dev);
    SDL_AudioStreamFlush(g_st[s].conv);
    g_st[s].release = 1;
    SDL_UnlockAudioDevice(g_dev);
}

void audio_stream_close(int s)
{
    if (s < 0 || !g_st[s].conv)
        return;
    SDL_LockAudioDevice(g_dev);
    SDL_FreeAudioStream(g_st[s].conv);
    g_st[s].conv = 0;
    SDL_UnlockAudioDevice(g_dev);
}
