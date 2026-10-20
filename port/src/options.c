/* The options screen (fe_options_screen 0x53a30, NOTES §4.2): the gearbox, ABS and
   traction control switched here, and the way to the audio, graphics, controls, load /
   save and credits screens. Done keeps the changes, Cancel and Esc put the three
   switches back. */
#include "options.h"
#include "audioopt.h"
#include "gfxopt.h"
#include "ctrlopt.h"
#include "loadsave.h"
#include "config.h"
#include "fe.h"
#include "files.h"
#include "fsh.h"
#include "mainmenu.h"
#include "menu.h"
#include "sound.h"
#include "video.h"
#include <stdio.h>
#include <string.h>

/* the rows, top to bottom, then the two buttons */
enum { GEARBOX, ABS, TRACTION, AUDIO, GRAPHICS, CONTROLS, LOAD_SAVE, CREDITS, DONE, CANCEL };

/* the mouse put on a row by the keys (0x53950, 0x53964) */
static const int row_x[10] = { 240, 240, 240, 240, 240, 240, 240, 240, 40, 565 };
static const int row_y[10] = { 118, 151, 185, 220, 253, 287, 324, 363, 450, 450 };

static fsh_file g_art;

static void art(const char *name)
{
    shape s;
    if (fsh_find(&g_art, name, &s))
        fe_draw_shape(&s, 0);
}

/* bgnd, then for the three switches their value (lit on the row chosen, with its name),
   for the other rows only the chosen one's name lit */
static void draw(int sel)
{
    static const char *const label[10] = { "Tral", "Absl", "Trcl", "Audl", "Gral",
                                           "Conl", "Loal", "Crel", "Donl", "Canl" };
    static const char *const gearbox[2][2] = { { "Autd", "Mand" }, { "Autl", "Manl" } };
    static const char *const abs_v[2][3] = { { "On1d", "Of1d", "N/1d" }, { "On1l", "Of1l", "N/1l" } };
    static const char *const tcs_v[2][3] = { { "On2d", "Of2d", "N/2d" }, { "On2l", "Of2l", "N/2l" } };
    int g = config_word(CFG_TRANSMISSION), a = config_word(CFG_ABS), t = config_word(CFG_TCS);

    art("bgnd");
    if (sel == GEARBOX)
        art(label[GEARBOX]);
    art(gearbox[sel == GEARBOX][g & 1]);
    if (sel == ABS)
        art(label[ABS]);
    art(abs_v[sel == ABS][a >= 0 && a <= 2 ? a : 2]);
    if (sel == TRACTION)
        art(label[TRACTION]);
    art(tcs_v[sel == TRACTION][t >= 0 && t <= 2 ? t : 2]);
    if (sel >= AUDIO)
        art(label[sel]);
}

/* leaving by Done or the clock: a loaded game may have brought another race type, and
   the opponent is made to fit it (0x54759) */
static void fit_opponent(void)
{
    static const int tournament_car[10] = { 0, 3, 6, 0, 3, 3, 6, 6, 7, 3 };
    int type = config_word(CFG_RACE_TYPE), track = config_word(CFG_TRACK);
    if (type == 3) {
        config_set_word(CFG_CAR, tournament_car[track >= 0 && track <= 9 ? track : 0]);
        fe_car_setup();
        config_set_word(CFG_OPPONENT, 10);
        config_set_word(CFG_RACE_LENGTH, 1);
        config_set_word(CFG_LAPS, track >= 4 && track <= 7 ? 6 : 8);
    } else if (type == 2)
        config_set_word(CFG_OPPONENT, 8);
    else if (type == 0)
        config_set_word(CFG_OPPONENT, 9);
    else if (config_word(CFG_OPPONENT) > 7)
        config_set_word(CFG_OPPONENT, config_word(CFG_CAR));
}

static void place(int sel)
{
    mouse_place(row_x[sel], row_y[sel]);   /* 0x71ad4 */
    fe_mouse_save();
}

int fe_options_screen(void)
{
    unsigned char pal[768];
    char path[256];
    /* what Cancel puts back */
    int keep_gearbox = config_word(CFG_TRANSMISSION), keep_auto = config_word(CFG_AUTO);
    int keep_abs = config_word(CFG_ABS), keep_tcs = config_word(CFG_TCS);
    int sel = GEARBOX, redraw = 1, first = 1, changed = 0, done = 0;
    int ready = 1, released = 1, loaded = 0, k;
    unsigned held = 0;

    place(GEARBOX);
    snprintf(path, sizeof path, "%s/OPTION/OPTIONS.QFS", art_dir());
    if (!fsh_load(&g_art, path) || !fsh_palette(&g_art, 0, pal)) {
        fprintf(stderr, "renfs: %s is missing\n", path);
        return 0;
    }
    fe_styles_init();
    if (g_mouse_active)
        fe_mouse_read();
    fe_mouse_save();

    while (!done) {
        /* the mouse picks a row by its height, Done and Cancel by their half; elsewhere
           the choice stays */
        int mx, my, over = -1;
        mouse_read(&mx, &my);
        if (my <= 0x85)
            over = GEARBOX;
        else if (my <= 0xa7)
            over = ABS;
        else if (my <= 0xcb)
            over = TRACTION;
        else if (my <= 0xec)
            over = AUDIO;
        else if (my <= 0x10e)
            over = GRAPHICS;
        else if (my <= 0x132)
            over = CONTROLS;
        else if (my <= 0x155)
            over = LOAD_SAVE;
        else if (my <= 0x184)
            over = CREDITS;
        else if (mx <= 0x141 && my <= 0x1e0)
            over = DONE;
        else if (mx >= 0x143 && my <= 0x1df)
            over = CANCEL;
        if (over >= 0 && over != sel) {
            sel = over;
            changed = 1;
        }

        if (redraw) {
            draw(sel);
            mouse_show(1);                 /* mouse_draw 0x52288 */
            g_mouse_active = 1;
            if (first) {
                fe_mouse_save();
                if (g_mouse_active)
                    fe_mouse_read();
                pal_fade_up(pal, 0, 256);
                fe_styles_init();
                first = 0;
            }
            settimeout(12000);             /* 2 minutes */
            redraw = changed = 0;
        }
        if (changed) {                     /* the old row dark, the new one lit, then all
                                              of it drawn again (so only the sound shows) */
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
                fe_play_sound(0x48);
                switch (sel) {
                case GEARBOX:
                    config_set_word(CFG_TRANSMISSION, config_word(CFG_TRANSMISSION) ^ 1);
                    config_set_word(CFG_AUTO, config_word(CFG_TRANSMISSION) == 0);
                    if (!g_mouse_active)
                        place(sel);
                    changed = 1;
                    break;
                case ABS:                  /* not on a car without it */
                    if (config_word(CFG_ABS) != 2) {
                        config_set_word(CFG_ABS, config_word(CFG_ABS) ^ 1);
                        if (!g_mouse_active)
                            place(sel);
                        changed = 1;
                    }
                    break;
                case TRACTION:
                    if (config_word(CFG_TCS) != 2) {
                        config_set_word(CFG_TCS, config_word(CFG_TCS) ^ 1);
                        if (!g_mouse_active)
                            place(sel);
                        changed = 1;
                    }
                    break;
                case AUDIO:                /* fe_audio_options 0x105dc */
                    pal_fade_down(0, 256);
                    fe_audio_options(AUDIO_OPTIONS);
                    place(sel);
                    first = redraw = 1;
                    break;
                case GRAPHICS:             /* fe_graphics_options 0x32d08 */
                    pal_fade_down(0, 256);
                    fe_graphics_options(GFX_OPTIONS);
                    place(sel);
                    first = redraw = 1;
                    break;
                case CONTROLS:             /* fe_controls_setup 0x22bd0 */
                    pal_fade_down(0, 256);
                    fe_controls_setup(0);
                    place(sel);
                    first = redraw = 1;
                    break;
                case LOAD_SAVE:            /* fe_load_save_screen 0x3c0c8(0) */
                    pal_fade_down(0, 256);
                    fe_load_save(LS_OPTIONS);
                    loaded = 1;
                    place(sel);
                    first = redraw = 1;
                    break;
                case CREDITS:              /* fe_credits 0x25600, with credits.asf */
                    fprintf(stderr, "renfs: that options screen is not written yet\n");
                    break;
                case DONE:
                    if (loaded)
                        fit_opponent();
                    done = 1;
                    break;
                case CANCEL:
                    config_set_word(CFG_TRANSMISSION, keep_gearbox);
                    config_set_word(CFG_AUTO, keep_auto);
                    config_set_word(CFG_ABS, keep_abs);
                    config_set_word(CFG_TCS, keep_tcs);
                    music_volume(config_word(CFG_MENU_VOLUME));   /* (0x721ec) */
                    done = 1;
                    break;
                }
            }
            released = ready = 0;
            held = 0;
            ticksince();
        } else if (k && ready) {
            if (k == 0x1b) {               /* as Cancel */
                fe_play_sound(0x48);
                config_set_word(CFG_TRANSMISSION, keep_gearbox);
                config_set_word(CFG_AUTO, keep_auto);
                config_set_word(CFG_ABS, keep_abs);
                config_set_word(CFG_TCS, keep_tcs);
                music_volume(config_word(CFG_MENU_VOLUME));
                done = 1;
            } else if (k == 0x4800) {      /* up: from the buttons to Credits, round to Done */
                fe_mouse_hide();
                    sel = sel == CANCEL ? sel - 2 : sel - 1;
                if (sel < 0)
                    sel = DONE;
                changed = 1;
                place(sel);
            } else if (k == 0x5000) {      /* down: from the buttons to the top */
                fe_mouse_hide();
                    sel = sel >= DONE ? GEARBOX : sel + 1;
                changed = 1;
                place(sel);
            } else if ((k == 0x4b00 || k == 0x4d00) && sel >= DONE) {
                fe_mouse_hide();           /* left and right: between Done and Cancel */
                    sel = sel == DONE ? CANCEL : DONE;
                changed = 1;
                place(sel);
            }
            ready = 0;
            held = 0;
            ticksince();
        } else if (!k)
            ready = 1;

        video_retrace();
        if (g_quit)
            done = 1;
        if (!done && (timedout() || (g_mouse_buttons == 2 && released))) {
            if (loaded)                    /* 2 minutes, or the right button: as Done */
                fit_opponent();
            fe_play_sound(0x48);
            done = 1;
        }
    }
    pal_fade_down(0, 256);
    fsh_free(&g_art);
    settimeout(12000);
    return 0;
}

/* race_menu_options 0x12760: the race menus' "Game options" (CHECK/CHKOPTS.QFS), the way
   to the audio, graphics, controls and load / save screens. Cancel and Esc put back what
   those screens changed (CONFIG.DAT +9 .. +0xb4); the settings are saved as it ends */
enum { R_AUDIO, R_GRAPHICS, R_CONTROLS, R_LOAD_SAVE, R_DONE, R_CANCEL };

int race_menu_options(int finish)
{
    static const int rx[6] = { 100, 100, 100, 100, 40, 565 };   /* 0x12700, 0x1270c */
    static const int ry[6] = { 119, 196, 271, 345, 450, 450 };
    static const char *const lit[6] = { "audl", "gral", "conl", "loal", "donl", "canl" };
    unsigned char pal[768], keep[0xac];
    char path[256];
    int sel = R_AUDIO, redraw = 1, first = 1, changed = 0, done = 0;
    int ready = 1, released = 1, k;
    unsigned held = 0;

    memcpy(keep, g_config + 9, sizeof keep);
    mouse_place(rx[sel], ry[sel]);
    snprintf(path, sizeof path, "%s/CHECK/CHKOPTS.QFS", art_dir());
    if (!fsh_load(&g_art, path) || !fsh_palette(&g_art, 0, pal)) {
        fprintf(stderr, "renfs: %s is missing\n", path);
        return 0;
    }
    if (g_mouse_active)
        fe_mouse_read();
    fe_mouse_save();

    while (!done) {
        int mx, my, over = -1;
        mouse_read(&mx, &my);
        if (my <= 0x9b)
            over = R_AUDIO;
        else if (my >= 0x9d && my <= 0xe9)
            over = R_GRAPHICS;
        else if (my >= 0xeb && my <= 0x136)
            over = R_CONTROLS;
        else if (my >= 0x138 && my <= 0x1a4)
            over = R_LOAD_SAVE;    /* (not in a network game) */
        else if (mx <= 0x141 && my >= 0x1a6 && my <= 0x1e0)
            over = R_DONE;
        else if (mx >= 0x143 && my >= 0x1a6 && my <= 0x1df)
            over = R_CANCEL;
        if (over >= 0 && over != sel) {
            sel = over;
            changed = 1;
        }

        if (redraw) {
            art("bgnd");
            art(lit[sel]);
            mouse_show(1);                 /* mouse_draw 0x52288 */
            g_mouse_active = 1;
            if (first) {
                fe_mouse_save();
                if (g_mouse_active)
                    fe_mouse_read();
                pal_fade_up(pal, 0, 256);
                first = 0;
            }
            settimeout(12000);
            redraw = changed = 0;
        }
        if (changed) {                     /* the old row dark, the new one lit */
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
                fe_play_sound(0x48);
                switch (sel) {
                case R_AUDIO:
                    pal_fade_down(0, 256);
                    fe_audio_options(AUDIO_RACE_MENU);
                    if (!g_mouse_active)
                        mouse_place(rx[sel], ry[sel]);
                    first = redraw = 1;
                    break;
                case R_GRAPHICS:           /* fe_graphics_options 0x32d08(0) */
                    pal_fade_down(0, 256);
                    fe_graphics_options(GFX_RACE_MENU);
                    if (!g_mouse_active)
                        mouse_place(rx[sel], ry[sel]);
                    first = redraw = 1;
                    break;
                case R_CONTROLS:           /* fe_controls_setup 0x22bd0(1) */
                    pal_fade_down(0, 256);
                    fe_controls_setup(1);
                    if (!g_mouse_active)
                        mouse_place(rx[sel], ry[sel]);
                    first = redraw = 1;
                    break;
                case R_LOAD_SAVE:          /* 0x3c0c8(1) at a checkpoint, 0x3d4c4(1) after */
                    pal_fade_down(0, 256);
                    fe_load_save(finish ? LS_FINISH : LS_CHECKPOINT);
                    if (!g_mouse_active)
                        mouse_place(rx[sel], ry[sel]);
                    first = redraw = 1;
                    break;
                case R_DONE:
                    done = 1;
                    break;
                case R_CANCEL:
                    memcpy(g_config + 9, keep, sizeof keep);
                    music_volume(config_word(CFG_MENU_VOLUME));   /* (0x721ec) */
                    done = 1;
                    break;
                }
            }
            released = ready = 0;
            held = 0;
            ticksince();
        } else if (k && ready) {
            if (k == 0x1b) {               /* as Cancel */
                fe_play_sound(0x48);
                memcpy(g_config + 9, keep, sizeof keep);
                music_volume(config_word(CFG_MENU_VOLUME));
                done = 1;
            } else if (k == 0x4800 || k == 0x5000) {
                /* up round to Done, down round from Done to the top (from Cancel to
                   Graphics) */
                fe_mouse_hide();
                sel = k == 0x4800 ? (sel == R_AUDIO ? R_DONE : sel - 1) : (sel + 1) % 5;
                mouse_place(rx[sel], ry[sel]);
                changed = 1;
            } else if ((k == 0x4b00 || k == 0x4d00) && sel >= R_DONE) {
                fe_mouse_hide();
                sel = sel == R_DONE ? R_CANCEL : R_DONE;
                mouse_place(rx[sel], ry[sel]);
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
        if (!done && (timedout() || (g_mouse_buttons == 2 && released))) {
            fe_play_sound(0x48);           /* 2 minutes, or the right button: as Done */
            done = 1;
        }
    }
    pal_fade_down(0, 256);
    fsh_free(&g_art);
    settimeout(12000);
    config_save();                         /* cfg_save_config_dat 0x3a134 */
    fe_mouse_save();
    return 0;
}
