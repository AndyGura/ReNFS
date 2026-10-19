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
/* a car's pictures now (0x55ff0): its colours 0..0xf9 to their nearest; 0xfe its mark's
   entry (if it has one); 0xfa..0xfd kept when lights (the police car's flashing lights in
   a chase); the rest as the previous re-colouring left them */
void car_pics_remap(unsigned char *bank, const unsigned char *pal, int mark, int lights);

/* the shade table the cars' shadows darken through (0x55abc: 0x89f98(0x9999), 0x89ff0):
   each colour of the palette set times f (16.16, 0x9999 = 0.6), to the nearest allowed */
void shade_make(int f);

/* the gauges' needles' colour (0x8a390, from race_colours 0x2e74c: the car's (number
   +0x48a) at 0xea710) and its 16 blends into each colour of the palette set, from 1/16
   of it to all of it, each the nearest allowed */
void needle_colours(int number);
extern unsigned char g_needle_blend[16][256];
extern int g_needle_colour;

/* the HUD's colours (0x55abc, before the shade table): the palette's nearest to some
   fixed ones (0x55790), and the racers' markers' */
void hud_colours(void);
extern int g_hud_colour[17];               /* 0x13d5a4 ..: see hud_colours */
enum { HUD_RED = 0, HUD_WHITE = 8, HUD_BLACK = 9 };
extern int g_hud_slate;                    /* 0x12b9fc */
extern int g_map_frame;                    /* 0x12ba08: the map's frame, dark grey */
extern unsigned char g_marker_colour[8];   /* 0x13d5e8: each racer's marker, by slot */
extern int g_marker_line;                  /* 0x13d5c8: their outline, the nearest black */

#endif
