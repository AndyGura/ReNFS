/* The race's colours (NOTES §4.9): every picture of a race is drawn with one palette made
   of pieces (the cars', the road's, the dashboard's ..), and the pictures loaded with
   palettes of their own are turned into it through the nearest colours. */
#ifndef RENFS_RACEPAL_H
#define RENFS_RACEPAL_H

/* the tint every loaded palette gets (0x560bc): red, green, blue scales (256 = 1), towards
   each colour's brightest channel (65536 = all the way), brightness (65536 = 1) */
void pal_set_tint(int r, int g, int b, int towards, int bright);
/* entries from..to-1 of a palette of 6-bit RGB triples, in place (0x560ec) */
void pal_tint(int from, int to, unsigned char *pal);

/* a piece of the race's palette, set at the next pal_apply_pieces (0x55ea0, 0x55f58) */
void pal_queue_piece(int start, int count, const unsigned char *pal);
void pal_apply_pieces(void);
/* the pictures of an "SHPI" bank, made with the 6-bit palette pal, to be turned into the
   race's colours at pal_apply_remaps (0x55e68, 0x55fa8) */
void pal_queue_remap(unsigned char *bank, const unsigned char *pal);
void pal_apply_remaps(void);
/* the race's colours matched (0x2e74c, after pal_apply_pieces): every entry but 0x47,
   0x5f (only one player: mode 0x12da20 < 2), 0xfa..0xff and the cars' marks (marks[i],
   n cars; 0 none) may be matched to, and the inverse table made from the palette set */
void race_colours(const unsigned char *marks, int n, int mode);
/* one bank now (0x55ee0): each colour to its nearest (nearest_colour), 255 kept */
void pal_remap(unsigned char *bank, const unsigned char *pal);

#endif
