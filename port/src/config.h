/* The front end's state and the records: CONFIG.DAT, 0x5f52 bytes kept at 0x156c14
   (NOTES §3.16). */
#ifndef RENFS_CONFIG_H
#define RENFS_CONFIG_H

#define CONFIG_SIZE 0x5f52

extern unsigned char g_config[CONFIG_SIZE];

/* cfg_load_config_dat 0x39c40: the CD's GAMEDATA/CONFIG/CONFIG.DAT; 0 if it is missing
   or not its size */
int config_load(void);
/* a word of it (the menus' choices are 16-bit, at odd places) */
int config_word(int at);

#define CFG_MENU_MUSIC 0x73    /* 0 none, 1 all twelve in turn, 2 and 4-9 rock, 3 and 10-15 techno */
#define CFG_SFX_VOLUME 0x79    /* 0..127, the menus' sounds (fe_play_sound 0x72510) */

#endif
