/* EA's fonts (.FFN, NOTES §3.7) and how the menus draw text on a 256-colour screen. */
#ifndef RENFS_FONT_H
#define RENFS_FONT_H

typedef struct {
    unsigned char *d;
    long len;
    int count, x_org, y_org, line_a, line_b, row_bytes;
    const unsigned char *chars, *bitmap;
} font;

/* setfont 0x91ed0 */
int font_load(font *f, const char *rel);
void font_free(font *f);

/* the current text colour (textcolour 0x95e10 with 0xa87ec): for each of the 16 levels
   of a letter's pixels, how much of the colour (0..255) and the colour times that much,
   0xAARRGGBB. (0xa857c also makes a table of the nearest entries over each colour, but
   the letters use it only on screens of 7 bits a pixel, never in this game) */
typedef struct {
    unsigned long ramp[16];
} text_style;

extern text_style g_text;          /* the current one (0x1605e4) */
void text_colour(int fg);

/* the entries a colour may not be matched to (0xf77c8, there 0 for these): the
   program's 254 and 255 (the mouse pointer's), the race's its own (racepal.c); never
   put back after a race, as in the original */
extern unsigned char g_colour_off[256];
/* nearestcolour 0x80c80: the entry nearest 0xRRGGBB. Through the screen's inverse
   palette (.INV) when it has one, else the closest of the allowed entries */
int nearest_colour(unsigned long rgb);
extern const unsigned char *g_inv;     /* 0xf77c4: 32 KB, or 0 */
/* 0x8227c: the table made from the palette in the card (read back, 6 bits x 4): each
   cell of 32x32x32 gets the allowed entry nearest its middle (0x819c0, the exact
   inverse colour map). pal_fade_up makes it at its end, so the menus always have one */
void inv_build(void);

/* the letters only inside x0 <= x < x1, y0 <= y < y1 (a window's edges), and again
   anywhere on the screen */
void text_clip(int x0, int y0, int x1, int y1);
void text_clip_off(void);

/* the text at x, y (0x94420: a9b38 then 0x94234), drawn into the screen */
void text_draw(const font *f, const char *s, int x, int y);
/* its width (0x84794 through 0xa3e30): from the leftmost to the rightmost pixel or the
   pen's end */
int text_width(const font *f, const char *s);

#endif
