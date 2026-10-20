#ifndef RENFS_LOADSAVE_H
#define RENFS_LOADSAVE_H

/* the load and save screen: from the options screen (fe_load_save_screen 0x3c0c8(0),
   OPTION/LOADSAVE.QFS), from the checkpoint menu's options (0x3c0c8(1)) or the finish
   menu's (fe_load_save_screen2 0x3d4c4(1), both CHECK/LOADSAVE.QFS) */
enum { LS_OPTIONS, LS_CHECKPOINT, LS_FINISH };
int fe_load_save(int where);

#endif
