/* The options screen (fe_options_screen 0x53a30). */
#ifndef RENFS_OPTIONS_H
#define RENFS_OPTIONS_H

/* gearbox, ABS, traction control, and the screens behind it; 0 */
int fe_options_screen(void);

/* race_menu_options 0x12760: the race menus' options (finish: the finish menu's, whose
   load / save screen is another); 0 */
int race_menu_options(int finish);
#endif
