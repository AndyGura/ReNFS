/* The main menu, "Control Central" (fe_main_menu 0x42c30). */
#ifndef RENFS_MAINMENU_H
#define RENFS_MAINMENU_H

/* the cars, the opponent, the race type and the track chosen (into CONFIG.DAT's place);
   0 back to the game mode screen, 1 quit, 2 nothing done in 2 minutes (the title) */
int fe_main_menu(void);

#endif
