/* EA's polygon filler (0x8bc71, NOTES §4.10): a polygon's rows found edge by edge, cut to
   the raster window, then painted by a row painter. The cars' shadows go through it. */
#ifndef RENFS_POLYFILL_H
#define RENFS_POLYFILL_H

/* n rows from row y: left[i]..right[i] each, both ends in */
typedef void (*poly_rows)(const int *left, const int *right, int y, int n, int colour);

/* the polygon of n corners (x, y pairs, whole pixels) in order round it; a convex one
   (the walk the other way round takes the first side it leaves for all its rows after,
   0x8c276). A point or one column (0x8bb3c's lines) is left out */
void poly_fill(int n, const int *xy, poly_rows rows, int colour);

/* the shadow's rows (0xa5dd4): each pixel through the shade table */
void poly_shade_rows(const int *left, const int *right, int y, int n, int colour);

/* the shade table (0xf8118): each colour's darker one */
extern unsigned char g_shade[256];

#endif
