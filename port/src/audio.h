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
void audio_stream_close(int s);

#endif
