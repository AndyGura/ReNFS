/* The joystick (NOTES §4.2, §4.6): found at start-up (0x4d1fc), its calibration kept in
   the home folder (joystick.cfg, the original's 0x5c bytes, 0x4d4a0 / 0x4d5ec) or made
   on the calibration screen (fe_joystick_calibrate 0x4c6b0), read as the menus' keys
   (0x4dfe0) and in a race as steering, pedals and buttons (0x4e0b8). */
#ifndef RENFS_JOYSTICK_H
#define RENFS_JOYSTICK_H

/* the calibration (0x12f294.., in this order in joystick.cfg): for the stick across and
   up and down its least, middle and most counts, the two halves' scales (16.16) and the
   menus' thresholds; the throttle's (the fourth axis) least, middle, most and scales,
   the brake's (the third) least, most and scale */
enum {
    JC_X_MIN, JC_X_MID, JC_X_MAX, JC_X_NEG, JC_X_POS, JC_X_LO, JC_X_HI,
    JC_Y_MIN, JC_Y_MID, JC_Y_MAX, JC_Y_NEG, JC_Y_POS, JC_Y_LO, JC_Y_HI,
    JC_T_MIN, JC_T_MID, JC_T_MAX, JC_T_NEG, JC_T_POS, JC_T_SCALE,
    JC_B_MIN, JC_B_MAX, JC_B_SCALE, JC_COUNT
};

typedef struct {
    int present;               /* 0x12f1c0: one was found at start-up */
    int has_x, has_y;          /* 0x12f1c4, 0x12f1c8 */
    int has_throttle;          /* 0x12f1cc: a fourth axis (the slider on the screen) */
    int has_pedals;            /* 0x12f1d0: a third axis too: the pedals two axes */
    int on;                    /* 0x12f1d8: calibrated, to be used */
    int x, y, throttle, brake; /* 0x12f278 0x12f27c 0x12f280 0x12f284: the counts read */
    int buttons;               /* 0x12f290: the buttons held (1, 2, 4, 8) */
    int held;                  /* 0x12f274: the same, as the race last read them */
    int extra;                 /* (the port's) a pad's right and left shoulder (1, 2): a
                                  gear up and down */
    int steer;                 /* 0x12f260: -0x20 .. 0x1f */
    int pedal;                 /* 0x12f264: -0x1e (faster) .. 0x1e (brake) */
    int cal[JC_COUNT];         /* 0x12f294 .. 0x12f2ec */
} joystick;

extern joystick g_joy;

/* 0x4d1fc at start-up: a joystick found, its calibration loaded and checked with the
   stick let go, else the calibration screen */
void joy_startup(void);
/* 0x4e3e8: the counts and buttons read; 0 if there is no joystick */
int joy_poll(void);
/* 0x4e0b8 (every timer tick of a race): the steering, pedal and buttons, or none */
void joy_race_read(void);
/* 0x4dfe0 in the menus' key readers: button 1 Space, 2 Enter, else the stick past half
   way an arrow key's code; 0 if none */
int joy_menu_key(void);
/* fe_joystick_calibrate 0x4c6b0(from_menu): the stick moved through its corners and
   back, a button; Esc leaves the joystick off. race_menu: the race menus' green picture
   (0x15d126) */
void fe_joystick_calibrate(int from_menu, int race_menu);

#endif
