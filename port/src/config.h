/* The front end's state and the records: CONFIG.DAT, 0x5f52 bytes kept at 0x156c14
   (NOTES §3.16). */
#ifndef RENFS_CONFIG_H
#define RENFS_CONFIG_H

#define CONFIG_SIZE 0x5f52

extern unsigned char g_config[CONFIG_SIZE];

/* cfg_load_config_dat 0x39c40: the port's copy (home/config.dat), else the built-in
   settings and records (cfg_defaults 0x38ea8); 0 when those were made (a first start) */
int config_load(void);
/* cfg_save_config_dat 0x3a134: written whole into home/config.dat (never the game's
   folder) */
void config_save(void);
/* a word of it (the menus' choices are 16-bit, at odd places) */
int config_word(int at);
void config_set_word(int at, int v);

#define CFG_MENU_MUSIC 0x73    /* 0 none, 1 all twelve in turn, 2 and 4-9 rock, 3 and 10-15 techno */
#define CFG_SPEECH 0x7b        /* the race's spoken words on (0x722b4) */
#define CFG_MUSIC_VOLUME 0x75  /* 0..127, the race's music (0x12da6b's, 0x52640) */
#define CFG_SFX_VOLUME 0x79    /* 0..127, the menus' sounds (fe_play_sound 0x72510) */

/* the main menu's choices (fe_main_menu 0x42c30) */
#define CFG_RACE_TYPE 0x6d     /* 0 time trial, 1 head to head, 2 single race, 3 tournament */
#define CFG_AUTO 0x8d          /* 1 with the automatic gearbox (the options set it) */
#define CFG_TCS 0x8f           /* traction control: 0 on, 1 off, 2 the car has none */
#define CFG_OPPONENT 0x91      /* 0-7 the cars, 8 traffic (single race), 9 the clock (time
                                  trial), 10 the tournament, 11 the Warrior */
#define CFG_SEGMENT 0x93      /* an open road's segment, 0..2 */
#define CFG_TRACK 0x95         /* 0 city .. 7 transtropolis, 8 Lost Vegas, 9 (moon, never) */
#define CFG_TRANSMISSION 0x97  /* 0 automatic, 1 manual */
#define CFG_ABS 0x99           /* 0 on, 1 off, 2 the car has none */
#define CFG_CAR 0x9b           /* the player's car, 0-7, 11 the Warrior */
#define CFG_LAPS 0xb1          /* from the race length and the track */
#define CFG_RACE_LENGTH 0xb3   /* 0, 1, 2 */
#define CFG_UNLOCKED 0x5f50    /* ROCKYROAD: 1 mirrored tracks, Warrior, Lost Vegas; 2 also rally */

#endif
