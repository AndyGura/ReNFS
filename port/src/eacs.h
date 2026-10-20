/* EA's sound: the "EACS" header and its samples (NOTES §3.8). */
#ifndef RENFS_EACS_H
#define RENFS_EACS_H

typedef struct {
    int rate, bytes, channels, compression, voice;
    long samples, loop, loop_len, data;
    short ima_value[2];        /* a stream's IMA decoders, carried from block to block */
    int ima_index[2];
} eacs;

/* the header at p ("EACS"); 0 if it is not one */
int eacs_header(eacs *h, const unsigned char *p, long len);
/* a stream's data block (a 1SNh's data, after its header, or a 1SNd's): the samples into
   out (16-bit, channels interleaved, room for max frames); the frames decoded */
int eacs_stream_block(eacs *h, const unsigned char *p, long len, short *out, int max);

/* a whole sound (an .EAS's or a bank's, its data from h->data): IMA from a decoder at
   0 with no block header; the frames decoded */
int eacs_sound(eacs *h, const unsigned char *p, long len, short *out, int max);

#endif
