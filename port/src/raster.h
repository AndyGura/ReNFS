/* Painting textured quads into the 3D view (0x91360, 0x90b30; NOTES §4.6). */
#ifndef RENFS_RASTER_H
#define RENFS_RASTER_H

/* a picture to paint with: 8 bits a pixel, rowbytes a row; reads outside lo..hi (the
   memory the picture was loaded in) give 0 where the original read its neighbours */
typedef struct {
    const unsigned char *pixels, *lo, *hi;
    int rowbytes;
} raster_tex;

/* where the quads go: a picture of palette numbers, pitch bytes a row, and the window
   x0..x1, y0..y1 (the ends outside) */
void raster_window(unsigned char *base, int pitch, int x0, int y0, int x1, int y1);

/* the corners in order round the quad: uv in 16.16 picture pixels, xy in screen pixels.
   see_through: colour 255 is left out (0x90b30) */
void raster_quad(const raster_tex *t, const int uv[8], const int xy[8], int see_through);

#endif
