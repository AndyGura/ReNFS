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
/* the palette as last given, 8 bits a channel (what nearestcolour 0x80c80 searches) */
const unsigned char *video_palette8(void);
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

/* the next key pressed, by its PC scan code (set 1), whatever the keyboard's layout;
   0 if none */
int scan_get(void);

/* the keyboard's state (0x84742): is the key of that PC scan code (set 1) held? */
int key_down(int scan);

/* the mouse (0x848af): its place in the 640x480 picture; the buttons held (1 left,
   2 right) */
int mouse_read(int *x, int *y);
/* 0x84885: the mouse kept inside x0..x1, y0..y1 (the whole screen: 0, 0, 640, 480) */
void mouse_limits(int x0, int y0, int x1, int y1);
/* mouse_set 0x71ad4: the mouse put there */
void mouse_place(int x, int y);
/* the pointer's picture (palette numbers, 255 see-through), drawn over the screen at
   the mouse while shown */
void mouse_pointer(const unsigned char *pixels, int w, int h);
void mouse_show(int on);

/* called at every retrace (the music is fed from it) */
extern void (*g_retrace_hook)(void);

#endif
