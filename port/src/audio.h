/* The sound card: streams of samples mixed into SDL's device. */
#ifndef RENFS_AUDIO_H
#define RENFS_AUDIO_H

int audio_init(void);          /* 0: no sound (none asked for, or no device) */
void audio_done(void);

/* a stream at its own rate (1 or 2 channels, 16-bit): -1 if none is free */
int audio_stream_open(int rate, int channels);
void audio_stream_put(int s, const short *samples, int frames);
/* frames put in but not yet heard */
int audio_stream_queued(int s);
/* 0..127 (127 as it is) */
void audio_stream_volume(int s, int volume);
/* no more to come: the stream is closed once it has played out (a sound effect) */
void audio_stream_release(int s);
void audio_stream_close(int s);

/* voice v (0..15) plays frames of a sample in memory (bytes 1: 8-bit signed, 2: 16-bit;
   1 or 2 channels) at freq frames a second (16.16), over and over from loop for loop_len
   frames (loop_len 0: once), at left and right 0..127. The sample stays the
   caller's. */
void audio_voice_start(int v, const void *d, int bytes, int channels, unsigned long frames,
                       long loop, unsigned long loop_len, unsigned long freq, int left, int right);
void audio_voice_rate(int v, unsigned long freq);
void audio_voice_volume(int v, int left, int right);
void audio_voice_stop(int v);

#endif
