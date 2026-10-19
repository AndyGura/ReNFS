/* The screen, the clock and the keys: what the game had from the VGA card, the PC's
   timer and the keyboard. */
#ifndef RENFS_VIDEO_H
#define RENFS_VIDEO_H

#define FB_MAX_W 640
#define FB_MAX_H 480

extern unsigned char g_fb[FB_MAX_W * FB_MAX_H];  /* palette numbers, g_fb_w a row */
extern int g_fb_w, g_fb_h;
extern int g_quit;                                /* the window was closed */

int video_init(const char *shot, int shot_frames);
void video_done(void);
/* initgraphics: 640x480 (the front end), 320x200 (a race in low detail), 320x240 (the
   movies, VGA mode X) */
void video_mode(int w, int h);
/* setpalette 0x84080: colours first.. from an 8-bit palette; the card keeps 6 bits */
void video_palette(const unsigned char *pal, int first, int count);
/* the palette read back from the card (0x98aec): its 6 bits widened, v << 2 | v >> 4 */
void video_get_palette(unsigned char *pal, int first, int count);
/* wait for the next vertical retrace (0xba9c2): the frame is shown then */
void video_retrace(void);

/* the system clock, 100 a second (the game's timer, NOTES §4.3) */
unsigned clock_ticks(void);
/* settimeout 0x8136b / timedout 0x81389: a wait of n hundredths */
void settimeout(unsigned n);
int timedout(void);

/* the next key pressed, as the front end reads them (0x727a0): 0x1b Esc, 0x0d Enter,
   0x20 space, letters and digits as they are; 0 if none */
int key_get(void);

#endif
