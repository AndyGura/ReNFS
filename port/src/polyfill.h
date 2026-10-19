/* EA's polygon filler (0x8bc71, NOTES §4.10): a polygon's rows found edge by edge, cut to
   the raster window, then painted by a row painter. The cars' shadows go through it. */
#ifndef RENFS_POLYFILL_H
#define RENFS_POLYFILL_H

/* n rows from row y: left[i]..right[i] each, both ends in */
typedef void (*poly_rows)(const int *left, const int *right, int y, int n, int colour);

/* the polygon of n corners (x, y pairs, whole pixels) in order round it; a convex one
   (the walk the other way round takes the first side it leaves for all its rows after,
   0x8c276). A point or one column is a line in the colour */
void poly_fill(int n, const int *xy, poly_rows rows, int colour);

/* a line from (x0, y0) to (x1, y1), both ends in, cut to the window (0x8bb3c) */
void poly_line(int x0, int y0, int x1, int y1, int colour);

/* EA's smoothed wide line (0x8b195): from (x0, y0) to (x1, y1) (16.16), w0 wide at the
   start and w1 at the end, the edges' pixels blended into what is there through
   blend[k] (k + 1 sixteenths of the colour); kept inside width x height and the window */
void poly_wide_line(int x0, int y0, int x1, int y1, int colour,
                    const unsigned char (*blend)[256], int w0, int w1, int width, int height);

/* rows of one colour (0xa6e80, 0x8bc71's) */
void poly_solid_rows(const int *left, const int *right, int y, int n, int colour);

/* the shadow's rows (0xa5dd4): each pixel through the shade table */
void poly_shade_rows(const int *left, const int *right, int y, int n, int colour);

/* the shade table (0xf8118): each colour's darker one */
extern unsigned char g_shade[256];

#endif
