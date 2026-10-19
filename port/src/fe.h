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

/* fe_attract_mode 0x34284: EA.TGV, one of ATTRACT1-3.TGV, TITLE.TGV, each only if no
   key stopped the one before; returns to 640x480, black */
int fe_attract_mode(void);


/* fe_game_mode_screen 0x47274: single player, modem, exit (CONTROL/GAMEMOD2.QFS);
   0 single, 1 exit, 2 nothing chosen in 45 seconds */
int fe_game_mode_screen(void);

/* fe_outer_loop 0x47000: the movies, the title, the game mode screen, the main menu */
void fe_outer_loop(void);

#endif
