/* The menus' sound: the effects of FRONT_MW.BNK and the music (FRONTEND/MUSIC). */
#ifndef RENFS_SOUND_H
#define RENFS_SOUND_H

/* 0x724c4 / 0x725ac: the effects' bank, loaded by each screen and freed as it ends */
void fe_sound_bank_load(void);
void fe_sound_bank_free(void);
/* fe_play_sound 0x72510: the bank's sound n (0x47 a move, 0x48 a choice) */
void fe_play_sound(int n);

/* 0x63ed0: the menus' music by the setting CFG_MENU_MUSIC and a counter (0xed0fc) */
void music_choose(void);
/* 0x72010: FRONTEND/MUSIC/<name>, played and looped; fed at each retrace (0x71b6c) */
void music_start(const char *name);
/* 0x720d8: faded out over a second; 0x72150: waited for, then stopped */
void music_fade_out(void);
void music_wait_end(void);

#endif
