#ifndef RENFS_GFXOPT_H
#define RENFS_GFXOPT_H

/* fe_graphics_options 0x32d08: from the options screen (OPTION/GRAPHICS.QFS), from the
   race menus' Game options (CHECK/GRAPHICS.FSH, as 0x15d126 is set during a race), and
   from the pause menu (the resolution greyed, it cannot change during a race) */
enum { GFX_OPTIONS, GFX_RACE_MENU, GFX_PAUSE };
int fe_graphics_options(int where);

#endif
