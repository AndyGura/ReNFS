/* A race (race_run 0x37228, race_main 0x6356c, NOTES §4.3): its loading, its steps and
   frames until it ends; started from Control Central (race_from_menu 0x3770c) or straight
   from the command line. */
#ifndef RENFS_RACE_H
#define RENFS_RACE_H

/* what Control Central's typed codes bring to the race */
typedef struct {
    int mirror;                /* 0x12dd28: SKCART's mirrored tracks */
    int rally;                 /* 0x12dd2c: EARALLYE's (Rusty Springs' tr5) */
    int nitro;                 /* 0x12cca0: NITRO */
} race_codes;

extern race_codes g_race_codes;

/* race_run 0x37228: the race CONFIG.DAT's settings ask for (the track, segment, type,
   car, opponent and laps as Control Central left them), from the loading picture to its
   end. 1 finished, 4 arrested, 0x10 everyone turned back, 2 left from the pause menu, 0
   the window closed or something missing */
int race_run(void);
/* race_from_menu 0x3770c: Control Central's Race button: the race, then back to the
   menu (the results and standings between are still to come) */
void race_from_menu(void);
/* --race: the track `name` ("cy1", "tr1", "tr5" the rally) with the rest of the saved
   settings; -1 if unknown */
int race_test(const char *name);

#endif
