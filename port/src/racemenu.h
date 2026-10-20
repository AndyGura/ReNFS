/* The menus between and after the races (NOTES §4.21). */
#ifndef RENFS_RACEMENU_H
#define RENFS_RACEMENU_H

/* race_menu_checkpoint 0x1a5d8: between an open road's segments (check/chkmenu.qfs):
   Continue (or Esc) 1, Quit ("End game ?") 0; the standings again; the replay, the
   highlights and the game's options not yet */
int race_menu_checkpoint(void);

/* race_menu_finish 0x1b6a0: after a race's last segment, a circuit, or a race not
   finished (check/finmenu.qfs): Continue (or Esc, or two minutes) back to Control
   Central; the standings again; the replay, the highlights and the options not yet */
int race_menu_finish(void);

/* fe_cop_ticket_movie 0x3823c: arrested, one of the police's three films (COP1..3,
   German GCOP1..3) */
void race_film_police(void);
/* fe_winner_movie 0x37ff0: after an open road's last segment or a circuit, the player
   of the least total: a head to head's or a single race's film by the car (WIN#1..6),
   a tournament's by the track, its last WIN#7; the track marked raced (and won) */
void race_film_winner(void);

#endif
