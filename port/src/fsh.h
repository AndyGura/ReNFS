/* EA's shape files (FSH, and QFS = a packed FSH; NOTES §3.1). */
#ifndef RENFS_FSH_H
#define RENFS_FSH_H

typedef struct {
    unsigned char *d;      /* the unpacked file */
    long len;
} fsh_file;

typedef struct {
    const unsigned char *head;     /* the shape's first block */
    int type, w, h, cx, cy, x, y;
    const unsigned char *pixels;
} shape;

/* load a .FSH or .QFS from the data folder; 0 if missing or not a shape file */
int fsh_load(fsh_file *f, const char *rel);
void fsh_free(fsh_file *f);
/* the shape of that 4-letter name (any case); 0 if there is none */
int fsh_find(const fsh_file *f, const char *name, shape *s);
/* a palette as the game keeps it (pal_from_shape 0x718a4): 256 x RGB, 8 bits a channel.
   From the shape's own palette block, else from "!pal"; 0 if the file has none */
int fsh_palette(const fsh_file *f, const shape *s, unsigned char pal[768]);

#endif
