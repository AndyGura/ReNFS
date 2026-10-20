/* The audio screen (fe_audio_options 0x105dc, NOTES §4.2): the song, the race music's,
   the menus' music's and the sound effects' volumes on three bars, the speech, the
   card's mode and "3D enhanced". From the options screen, the race menus' options and
   the pause menu (where it can change neither the mode nor 3D). Done keeps it all,
   Cancel and Esc put the settings back as they were. */
#include "audioopt.h"
#include "audio.h"
#include "config.h"
#include "fe.h"
#include "files.h"
#include "fsh.h"
#include "menu.h"
#include "sound.h"
#include "video.h"
#include <stdio.h>
#include <string.h>

enum { SONG, RACE_MUSIC, MENU_MUSIC, EFFECTS, SPEECH, MODE, SOUND_3D, DONE, CANCEL };

/* the mouse put on a row by the keys (0x104c0, 0x104d2) */
static const int row_x[9] = { 240, 240, 240, 240, 240, 240, 240, 40, 565 };
static const int row_y[9] = { 114, 158, 201, 244, 287, 329, 372, 450, 450 };

/* the three bars: what each one sets, and where it is (300, y) */
static const int bar_at[3] = { CFG_MUSIC_VOLUME, CFG_MENU_VOLUME, CFG_SFX_VOLUME };
static const int bar_y[3] = { 0x8d, 0xb8, 0xe3 };

static fsh_file g_art;

static void art(const char *name)
{
    shape s;
    if (fsh_find(&g_art, name, &s))
        fe_draw_shape(&s, 0);
}

/* a value's picture from a table of names, by a setting kept inside it */
static void value(const char *const *names, int n, int v)
{
    art(names[v >= 0 && v < n ? v : 0]);
}

/* a bar: its box (lit on the row chosen) in a window of 198 by 29 at (300, y), and in
   it, cut at x 6, the red bar Bor1 at pos - 0xb2, 6 (pos 0 .. 184 shows 2 .. 186 of it) */
static void bar(int b, int lit, int pos)
{
    char name[8];
    shape box, red;
    int y0 = bar_y[b];

    snprintf(name, sizeof name, "%s%d", lit ? "Bol" : "Box", b + 1);
    if (!fsh_find(&g_art, name, &box) || !fsh_find(&g_art, "Bor1", &red))
        return;
    for (int y = 0; y < box.h && y < 29; y++)
        for (int x = 0; x < box.w && x < 198; x++)
            g_fb[(y0 + y) * g_fb_w + 300 + x] = box.pixels[y * box.w + x];
    for (int y = 0; y < red.h && 6 + y < 29; y++)
        for (int x = 0; x < red.w; x++) {
            int wx = pos - 0xb2 + x;
            if (wx >= 6 && wx < 198)
                g_fb[(y0 + 6 + y) * g_fb_w + 300 + wx] = red.pixels[y * red.w + x];
        }
}

static void draw(int sel, int where, const int pos[3])
{
    /* 0x104e4, 0x10530 .. 0x1055c */
    static const char *const song[16] = { "Nony", "Shuy", "Shuo", "Shug", "Ramo", "Toxo",
                                          "Scuo", "Oveo", "Rouo", "Powo", "Filg", "Odyg",
                                          "Aaeg", "Hidg", "Fung", "Chog" };
    static const char *const mode_lit[3] = { "16bl", "8bsl", "8bil" };
    static const char *const mode_dark[3] = { "16bd", "8bsd", "8bid" };
    static const char *const speech_lit[2] = { "Oful", "Onul" };
    static const char *const speech_dark[2] = { "Ofud", "Onud" };
    static const char *const d3_lit[2] = { "Ofdl", "Ondl" };
    static const char *const d3_dark[2] = { "Ofdd", "Ondd" };
    static const char *const label[4] = { "Sonl", "Racl", "Menl", "Soul" };

    art("bgnd");
    for (int b = 0; b < 3; b++)
        bar(b, sel == b + 1, pos[b]);
    value(song, 16, config_word(CFG_MENU_MUSIC));
    if (sel <= EFFECTS)
        art(label[sel]);
    if (sel == SPEECH)
        art("Spel");
    value(sel == SPEECH ? speech_lit : speech_dark, 2, g_config[CFG_SPEECH] & 1);
    if (sel == MODE)
        art("Audl");
    value(sel == MODE ? mode_lit : mode_dark, 3, config_word(CFG_AUDIO_MODE));
    if (sel == SOUND_3D)
        art("3del");
    value(sel == SOUND_3D ? d3_lit : d3_dark, 2, g_config[CFG_3D_SOUND] & 1);
    if (sel == DONE)
        art("Donl");
    if (sel == CANCEL)
        art("Canl");
    if (where == AUDIO_PAUSE) {    /* (or a card that has no modes, 0x15d14e) */
        art("Audm");
        value(mode_dark, 3, config_word(CFG_AUDIO_MODE));
        art("3dem");
    }
}

static void place(int sel)
{
    mouse_place(row_x[sel], row_y[sel]);   /* 0x71ad4 */
    fe_mouse_save();
}

/* a bar's place from its volume, 0 .. 184 */
static int bar_pos(int v)
{
    return v * 0xb8 / 0x7f;
}

/* a bar moved to pos: its volume pos * 127 / 184 (1 kept 1), heard at once */
static void bar_set(int b, int pos)
{
    int v = pos == 1 ? 1 : pos * 0x7f / 0xb8;
    config_set_word(bar_at[b], v);
    if (b == 2)
        fe_sfx_volume(v);          /* 0x721c0 */
    else
        music_volume(v);           /* 0x721ec */
}

/* a click on a bar's box (x 0x131 .. 0x1ec, 11 lines high): the bar to the pointer */
static int bar_click(int b, int *pos)
{
    static const int top[3] = { 0x93, 0xbe, 0xe9 };
    int mx, my, p;

    mouse_read(&mx, &my);
    if (mx < 0x131 || mx > 0x1ec || my < top[b] || my > top[b] + 0x11)
        return 0;
    p = mx - 0x132;
    p = p < 0 ? 0 : p > 0xb8 ? 0xb8 : p;
    pos[b] = p;
    if (config_word(bar_at[b]) == p * 0x7f / 0xb8)
        return 0;
    if (fe_sound_muted()) {        /* F5's silence ends (the effects heard again) */
        fe_sound_unmute();
        fe_sfx_volume(config_word(CFG_SFX_VOLUME));
        if (b == 2)
            music_volume(config_word(CFG_MENU_VOLUME));
    }
    bar_set(b, p);
    return 1;
}

int fe_audio_options(int where)
{
    unsigned char pal[768], keep[0xac];
    char path[256];
    int sel = SONG, old = SONG, redraw = 1, first = 1, changed = 0, done = 0;
    int ready = 1, released = 1, race_service = 1, song_due = 0, k;
    int mode = config_word(CFG_AUDIO_MODE), pos[3];
    unsigned held = 0, song_at = 0;

    memcpy(keep, g_config + 9, sizeof keep);
    /* a race's song (or in the pause menu any) heard while the screen is up */
    if (config_word(CFG_MENU_MUSIC) > 3 || where == AUDIO_PAUSE) {
        music_fade_out();
        music_wait_end();
        music_level_from(CFG_MUSIC_VOLUME);
        race_music_start();        /* 0x63d88 */
    }
    for (int b = 0; b < 3; b++)
        pos[b] = bar_pos(fe_sound_muted() ? fe_sound_kept(bar_at[b]) : config_word(bar_at[b]));
    place(SONG);
    snprintf(path, sizeof path, "%s/OPTION/AUDIO.FSH", art_dir());
    if (!fsh_load(&g_art, path) || !fsh_palette(&g_art, 0, pal)) {
        fprintf(stderr, "renfs: %s is missing\n", path);
        return 0;
    }
    if (g_mouse_active)
        fe_mouse_read();
    fe_mouse_save();

    while (!done) {
        int mx, my, over = -1;

        /* a new song a second after it was chosen */
        if (song_due && clock_ticks() - song_at > 100) {
            song_due = 0;
            if (config_word(CFG_MENU_MUSIC))
                race_music_start();
        }
        /* the effects' bar chosen: sound 0x4a over and over */
        if (sel != EFFECTS && old == EFFECTS)
            fe_test_sound_stop();
        else if (sel == EFFECTS && fe_test_sound_done())
            fe_play_sound(0x4a);

        /* the mouse picks a row by its height; over the bars the music is heard at that
           bar's volume (the menus' music's bar: the menus' service, else the race's) */
        mouse_read(&mx, &my);
        if (my <= 0x89)
            over = SONG;
        else if (my <= 0xb3) {
            race_service = 1;
            music_volume(config_word(CFG_MUSIC_VOLUME));
            over = RACE_MUSIC;
        } else if (my <= 0xde) {
            race_service = 0;
            music_volume(config_word(CFG_MENU_VOLUME));
            over = MENU_MUSIC;
        } else if (my <= 0x109) {
            race_service = 1;
            music_volume(config_word(CFG_MUSIC_VOLUME));
            over = EFFECTS;
        } else if (my <= 0x135)
            over = SPEECH;
        else if (my <= 0x15e && where != AUDIO_PAUSE)
            over = MODE;
        else if (my >= 0x15f && my <= 0x18d && where != AUDIO_PAUSE)
            over = SOUND_3D;
        else if (mx <= 0x141 && my >= 0x18e && my <= 0x1e0)
            over = DONE;
        else if (mx >= 0x143 && my >= 0x18e && my <= 0x1df)
            over = CANCEL;
        if (over >= 0 && over != sel) {
            old = sel;
            sel = over;
            changed = 1;
        }
        music_level_from(race_service ? CFG_MUSIC_VOLUME : CFG_MENU_VOLUME);

        if (redraw) {
            draw(sel, where, pos);
            mouse_show(1);                 /* mouse_draw 0x52288 */
            g_mouse_active = 1;
            if (first) {
                if (g_mouse_active)
                    fe_mouse_read();
                pal_fade_up(pal, 0, 256);
                fe_styles_init();
                first = 0;
            }
            settimeout(12000);
            redraw = changed = 0;
        }
        if (changed) {                     /* drawn again (the pointer shown again) */
            fe_play_sound(0x47);
            if (sel < RACE_MUSIC || sel > EFFECTS) {
                if (old >= RACE_MUSIC && old <= EFFECTS)
                    ready = released = 0;  /* (off a bar: the keys wait) */
                if (old == EFFECTS)
                    fe_test_sound_stop();
                old = sel;
            }
            settimeout(12000);
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
                switch (sel) {
                case SONG:                 /* the next of the sixteen, heard in a second */
                    music_stop_now();
                    config_set_word(CFG_MENU_MUSIC, (config_word(CFG_MENU_MUSIC) + 1) % 16);
                    song_due = 1;
                    song_at = clock_ticks();
                    changed = 1;
                    break;
                case RACE_MUSIC:
                case MENU_MUSIC:
                case EFFECTS:
                    if (bar_click(sel - RACE_MUSIC, pos))
                        changed = 1;
                    break;
                case SPEECH:
                case SOUND_3D:
                    fe_play_sound(0x48);
                    g_config[sel == SPEECH ? CFG_SPEECH : CFG_3D_SOUND] ^= 1;
                    redraw = 1;
                    released = ready = 0;
                    place(sel);
                    break;
                case MODE:                 /* the card set up again in its next mode */
                    fe_play_sound(0x48);
                    config_set_word(CFG_AUDIO_MODE, (config_word(CFG_AUDIO_MODE) + 1) % 3);
                    mode = config_word(CFG_AUDIO_MODE);
                    fe_message_box(g_german ? "Wechsle audio-modus" : "Changing audio mode");
                    music_stop_now();
                    fe_sound_bank_free();
                    audio_mode(mode);      /* 0x71db8 */
                    if (config_word(CFG_MENU_MUSIC))
                        race_music_start();
                    fe_sound_bank_load();
                    place(sel);
                    redraw = 1;
                    released = ready = 0;
                    break;
                case DONE:
                    fe_play_sound(0x48);
                    done = 1;
                    break;
                case CANCEL:
                    fe_play_sound(0x48);
                    memcpy(g_config + 9, keep, sizeof keep);
                    music_volume(config_word(CFG_MENU_VOLUME));
                    done = 1;
                    break;
                }
            }
            held = 0;
            ticksince();
        } else if (k && ready) {
            if (k == 0x1b) {               /* as Cancel */
                if (sel == EFFECTS)
                    fe_test_sound_stop();
                fe_play_sound(0x48);
                memcpy(g_config + 9, keep, sizeof keep);
                music_volume(config_word(CFG_MENU_VOLUME));
                done = 1;
            } else if (k == 0x4800) {      /* up: from Cancel as from Done, round to Done */
                fe_mouse_hide();
                old = sel;
                sel = (sel == CANCEL ? DONE : sel) - 1;
                if (sel < 0)
                    sel = DONE;
                if (where == AUDIO_PAUSE && sel == SOUND_3D)
                    sel = SPEECH;
                place(sel);
                changed = 1;
                ready = 0;
            } else if (k == 0x5000) {      /* down: from the buttons to the top */
                fe_mouse_hide();
                old = sel;
                sel = sel == CANCEL ? SONG : (sel + 1) % 8;
                if (where == AUDIO_PAUSE && sel == MODE)
                    sel = DONE;
                place(sel);
                changed = 1;
                ready = 0;
            } else if (k == 0x4b00 || k == 0x4d00) {
                int b = sel - RACE_MUSIC, step = k == 0x4b00 ? -1 : 1;
                if (b >= 0 && b < 3) {     /* a bar: a step along it */
                    fe_mouse_hide();
                    if ((step < 0 && pos[b] > 0) || (step > 0 && pos[b] < 0xb8))
                        pos[b] += step;
                    bar_set(b, pos[b]);
                    changed = 1;
                    place(sel);
                } else if (sel == DONE || sel == CANCEL) {
                    fe_mouse_hide();       /* between Done and Cancel */
                    old = sel;
                    sel = sel == DONE ? CANCEL : DONE;
                    changed = 1;
                    place(sel);
                }
                ready = 0;
            }
            held = 0;
            ticksince();
        } else if (!k)
            ready = 1;

        video_retrace();
        if (g_quit)
            done = 1;
        if (!done && !first && (timedout() || (g_mouse_buttons == 2 && released))) {
            if (sel == EFFECTS)            /* 2 minutes, or the right button: as Done */
                fe_test_sound_stop();
            fe_play_sound(0x48);
            done = 1;
        }
    }
    pal_fade_down(0, 256);
    fsh_free(&g_art);
    fe_test_sound_stop();
    music_level_from(CFG_MENU_VOLUME);
    if (config_word(CFG_AUDIO_MODE) != mode) {
        /* Cancel put another mode back: the card set up again */
        music_fade_out();
        fe_sound_bank_free();
        music_wait_end();
        audio_mode(config_word(CFG_AUDIO_MODE));
        fe_sound_bank_load();
        if (where == AUDIO_PAUSE)
            music_start("PAUSE.ASF");
        else
            music_random();        /* 0x63e2c */
    } else if (where == AUDIO_PAUSE) {
        music_fade_out();
        music_wait_end();
        music_start("PAUSE.ASF");
    }
    music_volume(config_word(CFG_MENU_VOLUME));
    settimeout(12000);
    fe_mouse_save();
    return 0;
}
