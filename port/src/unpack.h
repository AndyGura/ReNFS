/* EA's packers (the game's unpack 0xa7020; NOTES §3.9). */
#ifndef RENFS_UNPACK_H
#define RENFS_UNPACK_H

/* 1 if d starts with a packer's mark (xxFB) */
int is_packed(const unsigned char *d, long len);
/* the unpacked bytes, allocated (free them); 0 for a kind not handled yet */
unsigned char *unpack(const unsigned char *d, long len, long *out_len);

#endif
