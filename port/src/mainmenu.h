/* The main menu, "Control Central" (fe_main_menu 0x42c30). */
#ifndef RENFS_MAINMENU_H
#define RENFS_MAINMENU_H

/* the cars, the opponent, the race type and the track chosen (into CONFIG.DAT's place);
   0 back to the game mode screen, 1 quit, 2 nothing done in 2 minutes (the title) */
int fe_main_menu(void);
/* car_setup 0x73a80: ABS and traction control as the player's car has them */
void fe_car_setup(void);

/* what the codes and the tournaments have opened: the mirrored tracks (0xec454), the
   rally (0xec458), Lost Vegas offered (0xec460), the Warrior (0xf7494) */
extern int g_mirror_ok, g_rally_ok, g_vegas_ok, g_warrior_ok;
/* the track's version chosen: mirrored (0x12dd28), the rally (0x12dd2c) */
extern int g_mirror, g_rally;
/* 0x42a84: after a tournament won, what CONFIG.DAT's +0x5f50 opens (1 Lost Vegas, the
   Warrior and the mirrored tracks; 2 the rally too) */
void fe_unlock(void);

#endif
