/* The controls screen (fe_controls_setup 0x22bd0, NOTES §4.2): what steers, what
   accelerates and brakes, what shifts (the keyboard, a joystick or the mouse) and the
   keys of each, the camera's, the handbrake's and the horn's. Space or a click on a row's
   name changes its control, Enter or a click on its keys asks for new ones; two
   controls on one key are refused ("Control conflict") and asked for again. Done keeps
   it all, Cancel and Esc put the controls back. From the options screen and the race
   menus' options. */
#include "ctrlopt.h"
#include "config.h"
#include "exe.h"
#include "fe.h"
#include "files.h"
#include "fsh.h"
#include "menu.h"
#include "sound.h"
#include "video.h"
#include <stdio.h>
#include <string.h>

enum { STEERING, PEDALS, SHIFT, CAMERA, HANDBRAKE, HORN, CALIBRATE, DONE, CANCEL };

/* the controls (CONFIG.DAT +0x5f36 .., 0x15cb4a in the game): the three controls' kinds
   (0 the keys; the steering 1 the joystick across, 4 the mouse; the pedals 2 the
   joystick up and down, 3 its buttons, 5 the mouse's; the gears 3, 5 the same buttons),
   then the keys, as the menus read them (a character, or an extended key's code << 8) */
#define CONTROLS 0x5f36
#define C_STEERING 0x5f36
#define C_PEDALS 0x5f38
#define C_SHIFT 0x5f3a
#define C_LEFT 0x5f3c          /* right +0x5f3e */
#define C_FASTER 0x5f40        /* brake +0x5f42 */
#define C_UP 0x5f44            /* down +0x5f46 */
#define C_HANDBRAKE 0x5f48
#define C_CAMERA 0x5f4a
#define C_HORN 0x5f4c
#define CONTROLS_SIZE 26       /* to the map's key +0x5f4e */

/* a joystick (0x12f1d8, 0x12f1c0: calibrated, the last row) and the mouse (0xec408) to
   drive with: the port reads neither in a race yet */
static const int g_joystick = 0, g_mouse_driving = 0;

/* the mouse put on a row by the keys (0x229c0, 0x229d2) */
static const int row_x[9] = { 200, 200, 200, 200, 200, 200, 200, 50, 565 };
static const int row_y[9] = { 118, 157, 196, 237, 277, 317, 356, 450, 450 };
/* the rows' controls, written at 400 */
static const int text_y[6] = { 0x68, 0x90, 0xb8, 0xe0, 0x108, 0x130 };

static fsh_file g_art;
static char g_rows[6][80];     /* what each row shows (0x600 + 0x50 a row) */

static void art(const char *name)
{
    shape s;
    if (fsh_find(&g_art, name, &s))
        fe_draw_shape(&s, 0);
}

static int cw(int at)
{
    return config_word(at);
}

/* 0x73068: a key's name, "" when it has none */
static const char *key_name(int k)
{
    static const struct { int k; const char *en, *de; } names[] = {
        { 0x20, "Space", "Leertaste" }, { 0x0d, "Enter", "Enter" },
        { 0x5200, "Insert", "Einfugen" }, { 0x5300, "Delete", "L\x94schen" },
        { 0x4700, "Home", "Pos 1" }, { 0x4f00, "End", "Ende" }, { 0x4900, "Pgup", "Nachste" },
        { 0x5100, "Pgdown", "Vorherige" }, { 0x4800, "Up", "Hoch" }, { 0x5000, "Down", "Runter" },
        { 0x4b00, "Left", "Links" }, { 0x4d00, "Right", "Rechts" },
        { 8, "Backspace", "Backspace" }, { 9, "Tab", "Tab" }, { 0x4c00, "Center", "Center" },
    };
    static char one[2];

    for (size_t i = 0; i < sizeof names / sizeof names[0]; i++)
        if (names[i].k == k)
            return g_german ? names[i].de : names[i].en;
    if (k >= 0x21 && k <= 0x7e) {         /* in capitals (in German as they are) */
        one[0] = (char)(!g_german && k >= 'a' && k <= 'z' ? k - 0x20 : k);
        return one;
    }
    if (g_german && (k == 0x81 || k == 0x9a || k == 0x99 || k == 0x94 || k == 0x8e
                     || k == 0x84 || k == 0xe1)) {
        one[0] = (char)k;
        return one;
    }
    return "";
}

/* what a row shows: a control's two keys, or what it is; one key's name */
static void row_text(int r)
{
    static const char *const kind_en[6] = { "Keyboard", "Left/Right", "Up/Down",
                                            "Button1/Button2", "Left/Right", "Button1/Button2" };
    static const char *const kind_de[6] = { "Keyboard", "Links/Rechts", "Hoch/Runter",
                                            "Taste1/Taste2", "Links/Rechts", "Taste1/Taste2" };
    static const int single[3] = { C_CAMERA, C_HANDBRAKE, C_HORN };
    char a[16];

    if (r < 3) {
        int kind = cw(C_STEERING + 2 * r);
        if (kind == 0) {
            snprintf(a, sizeof a, "%s", key_name(cw(C_LEFT + 4 * r) & 0xffff));
            snprintf(g_rows[r], sizeof g_rows[r], "%s/%s", a,
                     key_name(cw(C_LEFT + 4 * r + 2) & 0xffff));
        } else
            snprintf(g_rows[r], sizeof g_rows[r], "%s",
                     (g_german ? kind_de : kind_en)[kind >= 0 && kind < 6 ? kind : 0]);
    } else
        snprintf(g_rows[r], sizeof g_rows[r], "%s", key_name(cw(single[r - 3]) & 0xffff));
}

/* the pictures of the three controls' kinds, dark and lit (0x229e4 ..): the keyboard,
   the joystick, the mouse */
static const char *kind_art(int r, int lit)
{
    static const char *const names[2][3][3] = {
        { { "stkd", "stjd", "stmd" }, { "ackd", "acjd", "acmd" }, { "shkd", "shjd", "shmd" } },
        { { "stkl", "stjl", "stml" }, { "ackl", "acjl", "acml" }, { "shkl", "shjl", "shml" } },
    };
    int kind = cw(C_STEERING + 2 * r);
    return names[lit][r][kind <= 0 || kind > 5 ? 0 : kind < 4 ? 1 : 2];
}

/* the whole screen: bgnd, the three kinds, each row's controls, then the row chosen lit
   (its name, its kind and its controls again over them) */
static void draw(int sel)
{
    static const char *const label[9] = { "strl", "accl", "shfl", "caml", "hndl", "hrnl",
                                          "call", "donl", "canl" };
    static const char *const keyboard[3] = { "cakl", "hbkl", "hnkl" };

    fe_style(0);
    art("bgnd");
    for (int r = 0; r < 3; r++)
        art(kind_art(r, 0));
    for (int r = 0; r < 6; r++) {
        row_text(r);
        fe_text(g_rows[r], 400, text_y[r]);
    }
    fe_style(1);
    art(label[sel]);
    if (sel < 3)
        art(kind_art(sel, 1));
    else if (sel < 6)
        art(keyboard[sel - 3]);
    if (sel < 6)
        fe_text(g_rows[sel], 400, text_y[sel]);
}

/* the background under a row's controls put back (the window of 240 by 40 at 400 kept
   from bgnd, 0x93c) */
static void clear_row(int r)
{
    shape s;
    if (!fsh_find(&g_art, "bgnd", &s))
        return;
    for (int y = text_y[r]; y < text_y[r] + 40 && y < s.h; y++)
        memcpy(g_fb + y * g_fb_w + 400, s.pixels + y * s.w + 400, 240);
}

/* 0x7390c: two controls of the same kind, or two of the nine on one key */
static int conflict(void)
{
    int st = cw(C_STEERING), pe = cw(C_PEDALS), sh = cw(C_SHIFT), k[9], hit = 0;

    if ((st && pe && st == pe) || (st && sh && st == sh) || (sh && pe && sh == pe))
        hit = 1;
    for (int r = 0; r < 3; r++) {
        int kind = cw(C_STEERING + 2 * r);
        k[2 * r] = kind ? 2 * r : cw(C_LEFT + 4 * r);
        k[2 * r + 1] = kind ? 2 * r + 1 : cw(C_LEFT + 4 * r + 2);
    }
    k[6] = cw(C_HANDBRAKE);
    k[7] = cw(C_CAMERA);
    k[8] = cw(C_HORN);
    for (int i = 0; i < 9; i++)
        for (int j = i + 1; j < 9; j++)
            if (k[i] == k[j])
                hit = 1;
    return hit;
}

/* the key the BIOS would give for a key pressed, by the US keyboard's layout (EA's
   table 0xf7b74 without Shift), or the German keyboard's in the German game (which
   read its keys through the DOS keyboard driver, 0x8447c) */
static int key_typed(int scan)
{
    static const struct { unsigned char scan, de; } german[] = {
        { 0x15, 'z' }, { 0x2c, 'y' }, { 0x0c, 0xe1 }, { 0x1a, 0x81 }, { 0x27, 0x94 },
        { 0x28, 0x84 }, { 0x2b, '#' }, { 0x35, '-' }, { 0x0d, 0 }, { 0x1b, '+' }, { 0x29, '^' },
    };
    const unsigned char *t = exe_at(0xf7b74, 0x5b);
    int v;

    if (!t || scan <= 0 || scan >= 0x5b)
        return 0;
    for (size_t i = 0; g_german && i < sizeof german / sizeof german[0]; i++)
        if (german[i].scan == scan)
            return german[i].de;
    v = t[scan];
    return v & 0x80 ? (v & 0x7f) << 8 : v;
}

/* 0x733a0: the next key, once every key is let go; 0 for Esc or the right button. The
   keys the race keeps for itself and those without a name are not taken; letters come
   in capitals */
static int new_key(void)
{
    static const unsigned char refused[] = { 0x3e, 0x3f, 0x40, 0x5e, 0x5f, 0x70, 0x7b, 0x7c,
        0x7d, 0x7e, 0x6d, 0x4d, 0x50, 0x28, 0x29, 0x2a, 0x2b, 0x3a, 0x3c, 0x2d, 0x09, 0x21,
        0x22, 0x23, 0x24, 0x25, 0x26 };

    while (fe_read_keys() && !g_quit)
        video_retrace();
    fe_flush_keys();                       /* (0x84358) */
    ticksince();
    while (!g_quit) {
        int k = 0;
        video_retrace();
        if (g_mouse_active)
            fe_mouse_read();
        if (!key_down(0x2a) && !key_down(0x36))   /* nothing while Shift is held */
            k = key_typed(scan_get());
        if (k < 0x100 && memchr(refused, k, sizeof refused))
            k = 0;
        if (k == 0x1b || g_mouse_buttons == 2)
            return 0;
        if (g_german && (k == 0x3c || k == 0x3e || k == 0x5e || k == 0x60))
            k = 0;
        if (k >= 'a' && k <= 'z')
            k -= 0x20;
        if (g_german)
            k = k == 0x84 ? 0x8e : k == 0x94 ? 0x99 : k == 0x81 ? 0x9a : k;
        if (k && *key_name(k))
            return k;
    }
    return 0;
}

/* a refused choice: "Control conflict, please re-enter" two seconds, then the box gone */
static void conflict_shown(void)
{
    art("conf");
    settimeout(200);
    while (!timedout() && !g_quit) {
        video_retrace();
        if (g_mouse_active)
            fe_mouse_read();
    }
    settimeout(12000);
    art("conb");
}

/* Enter on a control of two keys: each asked for in turn ("Enter key for left..."), the
   first shown beside the second's cursor; again until they conflict with nothing */
static void two_keys(int r)
{
    static const char *const ask[3][2] = { { "ensl", "ensr" }, { "enac", "enbr" },
                                           { "enup", "endn" } };
    int at = C_LEFT + 4 * r, again, k;
    char t[40];

    fe_mouse_hide();
    art(kind_art(r, 1));                   /* (the keyboard's, lit) */
    do {
        again = 0;
        clear_row(r);
        art(ask[r][0]);
        fe_button_text("_", 400, text_y[r]);
        if ((k = new_key()) != 0)
            config_set_word(at, k);
        art(ask[r][1]);
        snprintf(t, sizeof t, "%s/_", key_name(cw(at) & 0xffff));
        fe_button_text(t, 400, text_y[r]);
        if ((k = new_key()) != 0)
            config_set_word(at + 2, k);
        row_text(r);
        fe_button_text(g_rows[r], 400, text_y[r]);
        if (conflict()) {
            conflict_shown();
            again = 1;
        }
        fe_mouse_save();
    } while (again && !g_quit);
}

/* Enter on the camera's, the handbrake's or the horn's key ("Enter key...") */
static void one_key(int r)
{
    static const int at[3] = { C_CAMERA, C_HANDBRAKE, C_HORN };
    int again, k;

    fe_mouse_hide();
    do {
        again = 0;
        clear_row(r);
        art("enhb");
        fe_button_text("_", 400, text_y[r]);
        if ((k = new_key()) != 0)
            config_set_word(at[r - 3], k);
        row_text(r);
        fe_button_text(g_rows[r], 400, text_y[r]);
        if (conflict()) {
            conflict_shown();
            again = 1;
        }
        fe_mouse_save();
    } while (again && !g_quit);
}

/* Space or a click on a control's name: the next kind the machine has (0x240c1,
   0x2440d, 0x247f2; with neither joystick nor mouse to drive with, always the keys);
   back to the keys, a conflict asks for new ones at once (1) */
static int next_kind(int r)
{
    int kind = cw(C_STEERING + 2 * r), pe = cw(C_PEDALS), sh = cw(C_SHIFT), to = -1;

    if (r == STEERING) {
        if (kind == 0)
            to = g_joystick ? 1 : g_mouse_driving ? 4 : 0;
        else if (kind == 1)
            to = g_mouse_driving ? 4 : 0;
        else if (kind == 4)
            to = 0;
    } else if (r == PEDALS) {
        if (kind == 0)
            to = g_joystick ? 2 : sh == 5 ? -1 : g_mouse_driving ? 5 : 0;
        else if (kind == 2)
            to = sh == 3 ? (g_mouse_driving ? 5 : 0) : g_joystick ? 3 : g_mouse_driving ? 5 : 0;
        else if (kind == 3)
            to = sh != 5 && g_mouse_driving ? 5 : 0;
        else if (kind == 5)
            to = 0;
    } else {
        if (kind == 0)
            to = pe == 3 ? (g_mouse_driving ? 5 : 0) : g_joystick ? 3 : pe == 5 ? -1
                : g_mouse_driving ? 5 : 0;
        else if (kind == 3)
            to = pe != 5 && g_mouse_driving ? 5 : 0;
        else if (kind == 5)
            to = 0;
    }
    if (to < 0)
        return 0;
    config_set_word(C_STEERING + 2 * r, to);
    return to == 0 && conflict();
}

static void place(int sel)
{
    mouse_place(row_x[sel], row_y[sel]);   /* 0x71ad4 (its place kept, 0x52348) */
    fe_mouse_save();
}

int fe_controls_setup(int race_menu)
{
    unsigned char pal[768], black[768], keep[CONTROLS_SIZE];
    char path[256];
    int sel = STEERING, redraw = 1, first = 1, changed = 0, done = 0, ready = 1, released = 1;
    int k;
    unsigned held = 0;

    memcpy(keep, g_config + CONTROLS, sizeof keep);
    memset(black, 0, sizeof black);
    memset(g_fb, 0, (size_t)g_fb_w * g_fb_h);     /* (0x72600) */
    place(sel);
    snprintf(path, sizeof path, "%s/%s", art_dir(),
             race_menu ? "CHECK/CHCONSET.QFS" : "OPTION/CNTRLSET.QFS");
    if (!fsh_load(&g_art, path) || !fsh_palette(&g_art, 0, pal)) {
        fprintf(stderr, "renfs: %s is missing\n", path);
        return 0;
    }
    pal_fade_up(pal, 0, 256);              /* (the screen still clear) */
    fe_styles_init();
    if (g_mouse_active)
        fe_mouse_read();
    fe_mouse_save();

    while (!done) {
        int mx, my, over = -1, on_keys;

        /* the mouse picks a row by its height (the joystick's only with one); over the
           keys (x 390 on, y 100..330) a click asks for new ones */
        mouse_read(&mx, &my);
        on_keys = mx >= 0x186 && my >= 100 && my <= 0x14a;
        if (my <= 0x86)
            over = STEERING;
        else if (my >= 0x8a && my <= 0xae)
            over = PEDALS;
        else if (my >= 0xb2 && my <= 0xd6)
            over = SHIFT;
        else if (my >= 0xda && my <= 0xfe)
            over = CAMERA;
        else if (my >= 0x102 && my <= 0x127)
            over = HANDBRAKE;
        else if (my >= 300 && my <= 0x14e)
            over = HORN;
        else if (g_joystick && my >= 0x152 && my <= 0x18b)
            over = CALIBRATE;
        else if (mx <= 0x140 && my >= 0x195 && my <= 0x1e0)
            over = DONE;
        else if (mx >= 0x145 && my >= 0x195 && my <= 0x1df)
            over = CANCEL;
        if (over >= 0 && over != sel) {
            sel = over;
            changed = 1;
        }

        if (redraw) {
            draw(sel);
            if (first)                     /* black until it fades up */
                video_palette(black, 0, 256);
            mouse_show(1);                 /* mouse_draw 0x52288 */
            g_mouse_active = 1;
            if (first) {
                pal_fade_up(pal, 0, 256);
                fe_styles_init();
                first = 0;
            }
            settimeout(12000);
            redraw = 0;
        }
        if (changed) {                     /* the rows drawn again, the whole screen after */
            fe_play_sound(0x47);
            redraw = 1;
            changed = 0;
        }

        if (!ready) {
            held += ticksince();
            if (held > 25)
                ready = 1;
        }
        if (g_mouse_active)
            fe_mouse_read();
        if (!g_mouse_buttons)
            released = 1;
        fe_mouse_moved();
        k = fe_read_keys();

        if (k == 0x20 || k == 0x0d || (g_mouse_buttons == 1 && released)) {
            fe_mouse_press();
            if (ready) {
                int ask;

                fe_play_sound(0x48);
                switch (sel) {
                case STEERING:
                case PEDALS:
                case SHIFT:
                    /* Space or the name: the next kind; Enter or the keys: new keys */
                    ask = on_keys;
                    if (!on_keys && k != 0x0d)
                        ask = next_kind(sel);
                    settimeout(12000);
                    if (cw(C_STEERING + 2 * sel) == 0 && (ask || k == 0x0d))
                        two_keys(sel);
                    redraw = 1;
                    break;
                case CAMERA:
                case HANDBRAKE:
                case HORN:
                    fe_style(1);
                    if (on_keys || k == 0x0d) {
                        one_key(sel);
                        redraw = 1;
                    }
                    break;
                case CALIBRATE:            /* fe_joystick_calibrate 0x4c6b0: no joystick */
                    break;
                case CANCEL:
                    memcpy(g_config + CONTROLS, keep, sizeof keep);
                    /* fall through */
                case DONE:
                    done = 1;
                    break;
                }
                settimeout(12000);
                while (fe_read_keys() && !g_quit)
                    video_retrace();
                if (!g_mouse_active)
                    place(sel);
            }
            released = ready = 0;
            held = 0;
            ticksince();
        } else if (k && ready) {
            if (k == 0x1b) {               /* as Cancel */
                memcpy(g_config + CONTROLS, keep, sizeof keep);
                fe_play_sound(0x48);
                done = 1;
            } else if (k == 0x4800) {      /* up: from Cancel as from Done, round to Done */
                fe_mouse_hide();
                sel = (sel == CANCEL ? DONE : sel) - 1;
                if (!g_joystick && sel == CALIBRATE)
                    sel = HORN;
                if (sel < 0)
                    sel = DONE;
                place(sel);
                changed = 1;
            } else if (k == 0x5000) {      /* down: from the buttons to the top */
                fe_mouse_hide();
                sel = sel == CANCEL ? STEERING : (sel + 1) % 8;
                if (!g_joystick && sel == CALIBRATE)
                    sel = DONE;
                place(sel);
                changed = 1;
            } else if ((k == 0x4b00 || k == 0x4d00) && (sel == DONE || sel == CANCEL)) {
                fe_mouse_hide();           /* between Done and Cancel */
                sel = sel == DONE ? CANCEL : DONE;
                place(sel);
                changed = 1;
            }
            ready = 0;
            held = 0;
            ticksince();
        } else if (!k)
            ready = 1;

        video_retrace();
        if (g_quit)
            done = 1;
        if (!done && !first && (timedout() || (g_mouse_buttons == 2 && released))) {
            fe_play_sound(0x48);           /* 2 minutes, or the right button: as Done */
            done = 1;
        }
    }
    pal_fade_down(0, 256);
    fsh_free(&g_art);
    fe_mouse_save();
    return 1;
}
