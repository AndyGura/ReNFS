/* What every menu screen shares (0x71208..0x74dcc, NOTES §4.2): the keys, the mouse, the
   text styles, the yes/no dialog. */
#ifndef RENFS_MENU_H
#define RENFS_MENU_H

#include "font.h"
#include "fsh.h"

/* 0x727a0: the key held, as the menus read the keyboard: 0x1b Esc, 0x20 Space, 0x0d
   Enter, 0x4800 up, 0x5000 down, 0x4b00 left, 0x4d00 right, 0x5200 Ins, 0x5300 Del,
   (F1-F4 0x3b00.. when g_fkeys), then the PC scan codes of Shift, Ctrl, Alt, Y, N, J;
   0 if none */
int fe_read_keys(void);
extern int g_fkeys;
/* 0x84358: the keys typed and not yet read thrown away */
void fe_flush_keys(void);
/* ticksince 0x81350: the hundredths since it was last asked */
unsigned ticksince(void);

/* the mouse: 0x156604 it has been used (moved or pressed), 0x156606 its buttons as last
   read (1 left, 2 right) */
extern int g_mouse_active, g_mouse_buttons;
void fe_mouse_load(void);          /* 0x52020: MOUSE.FSH */
void fe_mouse_read(void);          /* 0x522e8: the buttons, into g_mouse_buttons */
void fe_mouse_save(void);          /* mouse_save 0x52348: its place, to see it move */
void fe_mouse_moved(void);         /* 0x71b24: g_mouse_active once it moves or is pressed */
void fe_mouse_press(void);         /* 0x52304: the pointer pressed a fifth of a second */
/* 0x52394: the pointer shown, pressed or not (the replay's bar) */
void fe_mouse_look(int pressed);
void fe_mouse_hide(void);          /* 0x71afc: a key used, the pointer hidden until the
                                      screen is drawn again */

/* drawshapeatpos 0x8129c: a shape at its own place, all of it; see_through: colour 255
   left out (drawshape_see_through 0x815e4) */
void fe_draw_shape(const shape *s, int see_through);

/* the menus' font (fonts/graver18.ffn, set at start-up 0x42250) */
extern font g_fe_font;
int fe_font_load(void);

/* 0x71208: the text styles made from the palette now shown; 0x7135c: one chosen,
   0 beige (normal), 1 yellow (lit), 2 red, 3 white */
void fe_styles_init(void);
void fe_style(int n);
/* 0x71774: text with its shadow (black, 2 pixels right and down); 0x716fc: centred on
   x; 0x713d0: the same over what is on the screen, inside a window of the text's width
   + 10 by 45 */
void fe_text(const char *s, int x, int y);
void fe_button_text(const char *s, int x, int y);
void fe_text_centre(const char *s, int x, int y);

/* 0x74234: a line in red in a darkened box with two red frames, centred, at y 210 (the
   box text_width + 20 by 60), left on the screen */
void fe_message_box(const char *text);
/* 0x74dcc: fe_message_box for so many hundredths (or until a key or a button), a tenth
   of a second more, then what it covered put back */
void fe_timed_message(const char *text, unsigned ticks);
/* the dialogs' box (0x89f3c, 0x8a0cf, 0x8a0f0): the screen under it darkened, two red
   frames 2 and 3 pixels in */
void fe_box(int x, int y, int w, int h);
/* 0x717d0: the text cut to the first of its lengths from 7 characters on that is wider
   than w (or whole), with its shadow */
void fe_text_cut(const char *s, int x, int y, int w);
/* fe_yes_no_dialog 0x73b04: the question in a box over the screen; 1 yes */
int fe_yes_no_dialog(const char *question);
/* "Exit to system ?" (German "Zurück zum system ?") */
int fe_ask_exit(void);

/* 0x701cc: a line typed in (names 1: a player's name; 0: a file name), centred on cx
   at y; 1 Enter (text changed), 0 Esc */
int fe_text_field(char *text, int maxlen, int cx, int y, int names);
/* fe_enter_name 0x743b4: "Please Enter Name" in a box, the name (8 letters at most,
   CONFIG.DAT +0) typed; 1 Enter, 0 Esc */
int fe_enter_name(char *name);
/* 0x15d12a: what the main menu's codes turn on for the race (4 "No mercy", End held on
   head to head; 8 TURBO); 0x15d12e beside it. A new name clears both */
extern unsigned long g_cheat_flags, g_cheat_flags_b;

#endif
