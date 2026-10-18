/* The front end (the menus at 640x480; NOTES §4.2). */
#ifndef RENFS_FE_H
#define RENFS_FE_H

/* pal_fade_up 0x71918 / pal_fade_down 0x7198c: 17 retraces each */
void pal_fade_up(const unsigned char *pal, int first, int count);
void pal_fade_down(int first, int count);

/* 0x74b20: a whole-screen picture ("bgnd" and "!pal" of art/<name>) faded in; with
   wait, held until a key or a timeout and faded out. 0 if the file is missing */
int fe_show_picture(const char *name, int wait);

/* fe_title_screen 0x46f20: TITLE.QFS for 5 seconds or until a key */
void fe_title_screen(void);

#endif
