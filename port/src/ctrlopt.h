#ifndef RENFS_CTRLOPT_H
#define RENFS_CTRLOPT_H

/* fe_controls_setup 0x22bd0: the controls chosen and the keys redefined (CONFIG.DAT
   +0x5f36 ..); from the options screen (OPTION/CNTRLSET.QFS) or, race_menu set, the race
   menus' options (CHECK/CHCONSET.QFS) */
int fe_controls_setup(int race_menu);

#endif
