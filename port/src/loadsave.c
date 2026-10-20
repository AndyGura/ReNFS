/* The load and save screen (fe_load_save_screen 0x3c0c8 and 0x3d4c4, NOTES §4.2): save,
   load and delete a tournament, save, view and delete replays and highlights. A saved
   tournament is the session as the original holds it (*.SAV, 0x5b6 bytes); a saved
   replay a copy of the last race's REPLAY.RPL. Both are kept in the home folder, never
   in the game's; the CD's four replays (REPLAY, REPLAY1..3) are listed with the saved
   ones and can be neither replaced nor deleted. */
#include "loadsave.h"
#include "config.h"
#include "fe.h"
#include "files.h"
#include "fsh.h"
#include "mainmenu.h"
#include "menu.h"
#include "race.h"
#include "session.h"
#include "sound.h"
#include "video.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

enum { SAVE_GAME, LOAD_GAME, DELETE_GAME, SAVE_REPLAY, VIEW_REPLAY, VIEW_HIGHLIGHTS,
       DELETE_REPLAY, DONE };

/* fe_load_save_list 0x3aacc's kinds */
enum { LIST_VIEW, LIST_DELETE_REPLAY, LIST_LOAD, LIST_DELETE_GAME };

static fsh_file g_art;
static int g_check;                /* 0x12cbf4: the race menus' pictures (CHECK/) */
static int g_highlights;           /* 0xec248: the replay to view is for its highlights */

static void art(const fsh_file *f, const char *name, int see_through)
{
    shape s;
    if (fsh_find(f, name, &s))
        fe_draw_shape(&s, see_through);
}

/* a shape at x, y instead of its own place */
static void art_at(const fsh_file *f, const char *name, int x, int y, int see_through)
{
    shape s;
    if (fsh_find(f, name, &s)) {
        s.x = x;
        s.y = y;
        fe_draw_shape(&s, see_through);
    }
}

/* ------------------------------------------------------------------ the files */

/* 0x12c424: the names listed (as DOS gives them: capitals, with the extension) */
#define MAX_FILES 100
static char g_names[MAX_FILES][20];
static int g_count;

static void home_path(char *path, size_t n, const char *name)
{
    char low[20];
    int i;

    for (i = 0; name[i] && i < 19; i++)
        low[i] = name[i] >= 'A' && name[i] <= 'Z' ? (char)(name[i] - 'A' + 'a') : name[i];
    low[i] = 0;
    snprintf(path, n, "%s/%s", files_home(), low);
}

static int is_system(const char *name)       /* 0x3a9c4's four */
{
    return !strcasecmp(name, "REPLAY.RPL") || !strcasecmp(name, "REPLAY1.RPL") ||
           !strcasecmp(name, "REPLAY2.RPL") || !strcasecmp(name, "REPLAY3.RPL");
}

static void add_from(const char *dir, const char *ext)
{
    DIR *d = dir ? opendir(dir) : 0;
    struct dirent *e;

    if (!d)
        return;
    while ((e = readdir(d)) && g_count < MAX_FILES) {
        size_t n = strlen(e->d_name);
        char up[20];
        int i, seen = 0;

        if (n < 5 || n > 12 || strcasecmp(e->d_name + n - 4, ext))
            continue;
        for (i = 0; i <= (int)n; i++)
            up[i] = e->d_name[i] >= 'a' && e->d_name[i] <= 'z' ? (char)(e->d_name[i] - 32)
                                                               : e->d_name[i];
        for (i = 0; i < g_count; i++)
            seen |= !strcmp(g_names[i], up);
        if (!seen)
            memcpy(g_names[g_count++], up, n + 1);
    }
    closedir(d);
}

static int by_name(const void *a, const void *b)
{
    return strcmp((const char *)a, (const char *)b);
}

/* the saved tournaments (*.SAV) or the replays (*.RPL: the home folder's and the CD's),
   in the order of their names (a DOS folder gives them in the order they were written);
   the replays to delete without the CD's four */
static void list_files(int kind)
{
    char cd[1024];
    int i, j;

    memset(g_names, 0, sizeof g_names);
    g_count = 0;
    if (kind == LIST_LOAD || kind == LIST_DELETE_GAME)
        add_from(files_home(), ".SAV");
    else {
        add_from(files_home(), ".RPL");
        if (file_path("GAMEDATA/REPLAY", cd, sizeof cd))
            add_from(cd, ".RPL");
    }
    qsort(g_names, (size_t)g_count, sizeof g_names[0], by_name);
    if (kind == LIST_DELETE_REPLAY) {
        for (i = j = 0; i < g_count; i++)
            if (!is_system(g_names[i]))
                memmove(g_names[j++], g_names[i], sizeof g_names[0]);
        for (i = j; i < g_count; i++)
            g_names[i][0] = 0;
        g_count = j;
    }
}

/* a replay's file: the home folder's (the last race's, those saved), else the CD's */
static FILE *replay_file(const char *name)
{
    char path[1024], rel[128];
    FILE *f;

    home_path(path, sizeof path, name);
    if ((f = fopen(path, "rb")))
        return f;
    snprintf(rel, sizeof rel, "GAMEDATA/REPLAY/%s", name);
    return file_open(rel);
}

static int replay_exists(const char *name)
{
    FILE *f = replay_file(name);

    if (f)
        fclose(f);
    return f != 0;
}

/* replay_copy_as 0x3a228: REPLAY.RPL copied under the new name */
static int replay_copy_as(const char *name)
{
    char path[1024];
    static unsigned char buf[1 << 17];
    FILE *in = replay_file("REPLAY.RPL"), *out;
    size_t n;
    int ok = 1;

    if (!in)
        return 0;
    home_path(path, sizeof path, name);
    if (!(out = fopen(path, "wb"))) {
        fclose(in);
        return 0;
    }
    while ((n = fread(buf, 1, sizeof buf, in)) > 0)
        ok &= fwrite(buf, 1, n, out) == n;
    fclose(in);
    ok &= fclose(out) == 0;
    return ok;
}

/* 0x3a1b0: the session written */
static int game_write(const char *name)
{
    char path[1024];
    unsigned char d[SESSION_FILE];
    FILE *f;
    int ok;

    home_path(path, sizeof path, name);
    session_to_bytes(d);
    if (!(f = fopen(path, "wb")))
        return 0;
    ok = fwrite(d, sizeof d, 1, f) == 1;
    ok &= fclose(f) == 0;
    return ok;
}

/* ------------------------------------------------------------------ the dialogs */

/* a box over the screen (400 or 480 by 100 at y 190), its title, a name typed in it (8
   letters at most, capitals); what it covered put back; 1 Enter */
static int name_box(int x, int w, const char *title, char *name)
{
    static unsigned char saved[480 * 100];
    int ok, y;

    for (y = 0; y < 100; y++)
        memcpy(saved + y * w, g_fb + (0xbe + y) * g_fb_w + x, (size_t)w);
    fe_box(x, 0xbe, w, 100);
    fe_styles_init();
    fe_style(2);
    fe_text_centre(title, x + w / 2, 0xbe + 10);
    fe_style(1);
    video_retrace();
    ok = fe_text_field(name, 8, 0x140, 0xfa, 0);
    for (y = 0; y < 100; y++)
        memcpy(g_fb + (0xbe + y) * g_fb_w + x, saved + y * w, (size_t)w);
    video_retrace();
    return ok;
}

/* save_game 0x3a2b0: "Save game as", the name (CONFIG.DAT +9) and the settings and keys
   kept in the session, "Replace game ?" if there is one so named; "Game saved" */
static void save_game(char *name)
{
    char path[1024];
    FILE *f;
    int saved = 0;

    if (!name_box(0x78, 400, g_german ? "Spiel speichern als" : "Save game as", name)) {
        fe_play_sound(0x48);
        return;
    }
    memset(g_config + 9, 0, 9);
    snprintf((char *)g_config + 9, 9, "%.8s", name);
    memcpy(g_session.kept_keys, g_config + 0x5f36, sizeof g_session.kept_keys);
    g_session.vegas = (short)g_vegas_ok;
    memcpy(g_session.kept, g_config + 9, sizeof g_session.kept);
    snprintf(path, sizeof path, "%s.SAV", name);
    {
        char real[1024];
        home_path(real, sizeof real, path);
        f = fopen(real, "rb");
        if (f) {
            fclose(f);
            if (fe_yes_no_dialog(g_german ? "Spiel ersetzen ?" : "Replace game ?")) {
                remove(real);
                saved = game_write(path);
            }
        } else {
            saved = game_write(path);
            fe_play_sound(0x48);
        }
    }
    if (saved)
        fe_timed_message(g_german ? "Spiel gespeichert" : "Game saved", 100);
}

/* save_replay 0x3a628: "Save replay as" (the name the settings hold, to its dot), the
   name into CONFIG.DAT +0x3b; the CD's four not replaced, another so named only when
   asked; "Replay saved" */
static void save_replay(char *name)
{
    char file[80];
    int saved = 0, i;

    for (i = 0; name[i] != '.' && i < 15; i++)
        ;
    name[i] = 0;
    if (!name_box(0x50, 480, g_german ? "Wiederholung speichern als" : "Save replay as",
                  name)) {
        fe_play_sound(0x48);
        return;
    }
    snprintf(file, sizeof file, "%s.RPL", name);
    memset(g_config + 0x3b, 0, 0x20);
    snprintf((char *)g_config + 0x3b, 0x20, "%.31s", name);
    if (replay_exists(file)) {
        if (is_system(file))
            fe_timed_message(g_german ? "Kann system-weiderholung nicht ersetzen"
                                      : "Cannot replace system replay", 100);
        else if (fe_yes_no_dialog(g_german ? "Wiederholung ersetzen ?" : "Replace replay ?"))
            saved = replay_copy_as(file);
    } else {
        saved = replay_copy_as(file);
        fe_play_sound(0x48);
    }
    if (saved)
        fe_timed_message(g_german ? "Wiederholung gespeichert" : "Replay saved", 100);
}

/* 0x3a0b4: a saved tournament read back; its results cleared but the points, the
   tracks raced and the drivers' police records (0x3ba9c), the settings, keys and Lost
   Vegas it kept, the race type a tournament, the menus' music at its volume */
static int load_game(const char *name)
{
    char path[1024];
    unsigned char d[SESSION_FILE + 1];
    FILE *f;
    int i, n;

    home_path(path, sizeof path, name);
    if (!(f = fopen(path, "rb")))
        return 0;
    n = (int)fread(d, 1, sizeof d, f);
    fclose(f);
    if (n != SESSION_FILE) {           /* "BUG: Loadgame, wrong file size" */
        fprintf(stderr, "renfs: %s is not a saved game\n", path);
        return 0;
    }
    session_from_bytes(d);
    for (i = 0; i < SESSION_RACERS; i++) {
        session_racer *r = &g_session.racer[i];

        r->total = r->total_distance = r->best_top = 0;
        r->time = r->distance = r->top = 0;
        memset(r->laps, 0, sizeof r->laps);
    }
    g_game_on = 1;
    memcpy(g_config + 9, g_session.kept, sizeof g_session.kept);
    memcpy(g_config + 0x5f36, g_session.kept_keys, sizeof g_session.kept_keys);
    g_vegas_ok = g_session.vegas;
    config_set_word(CFG_RACE_TYPE, 3);
    music_volume(config_word(CFG_MENU_VOLUME));    /* (0x721ec) */
    return 1;
}

/* ------------------------------------------------------------------ the file list */

/* fe_load_save_list 0x3aacc: a box opening at 353, 108 (LOADANIM.QFS opn1..opn8, a tenth
   of a second each) with ten of the files, an arrow above and below to scroll them and
   Cancel on its right; the mouse kept inside it. The chosen file loaded, deleted or
   played (1 back when a replay is to be played, or a game loaded on an open road's later
   segment) */
static int file_list(int kind)
{
    enum { DX = 0x161, DY = 0x6c, UP = 0, DOWN = 0xb, CANCEL = 0xd };
    /* 0x38df0, 0x38e0c: the mouse's places, by what is chosen */
    static const short px[14] = { 455, 455, 455, 455, 455, 455, 455, 455, 455, 455, 455,
                                  455, 366, 545 };
    static const short py[14] = { 121, 143, 165, 186, 207, 228, 249, 270, 291, 312, 333,
                                  356, 237, 237 };
    static unsigned char saved[640 * 200];
    fsh_file anim;
    char path[256], want[40];
    int top = 0, sel = CANCEL, back = CANCEL, step = 1, redraw = 1, ready = 0;
    int released = 0, done = 0, result = 0, i, k;
    unsigned held = 0;

    fe_styles_init();
    snprintf(path, sizeof path, "%s/%s/LOADANIM.QFS", art_dir(),
             g_check ? "CHECK" : "OPTION");
    if (!fsh_load(&anim, path)) {
        fprintf(stderr, "renfs: %s is missing\n", path);
        return 0;
    }
    for (i = 0; i < 15 && g_config[0x3b + i] != '.'; i++)
        ;
    g_config[0x3b + i] = 0;
    list_files(kind);
    /* the first file chosen, Cancel only when there is none (0x3ada3, 0x3adaa: stores
       made with two arguments pushed, so sel and back); the file the settings name (the
       tournament's or the replay's) chosen instead, shown at the top when the list is
       long */
    if (g_count)
        sel = back = 1;
    if (kind == LIST_LOAD || kind == LIST_VIEW) {
        if (kind == LIST_LOAD)
            snprintf(want, sizeof want, "%.9s.SAV", (const char *)g_config + 9);
        else
            snprintf(want, sizeof want, "%.32s.RPL", (const char *)g_config + 0x3b);
        for (i = 0; i < g_count; i++)
            if (!strcmp(want, g_names[i])) {
                if (g_count < 10) {
                    top = 0;
                    sel = back = i + 1;
                } else if (i < g_count - 10)
                    top = i;
                else {
                    top = g_count - 10;
                    sel = back = i - top + 1;
                }
            }
    }
    mouse_limits(0x165, 0x6f, 0x22a, 0x16e);
    mouse_place(px[sel], py[sel]);
    ticksince();

    while (!done && !g_quit) {
        int mx, my;

        mouse_read(&mx, &my);
        if (mx >= 0x178 && mx <= 0x217 && my >= 0x70 && my <= 0x84) {
            if (sel != UP)
                back = sel, sel = UP, redraw = 1;
        } else if (mx >= 0x178 && mx <= 0x217 && my >= 0x15c && my <= 0x16e) {
            if (sel != DOWN)
                back = sel, sel = DOWN, redraw = 1;
        } else if (mx >= 0x217 && mx <= 0x22a && my >= 0x8c && my <= 0x154) {
            if (sel != CANCEL)
                back = sel, sel = CANCEL, redraw = 1;
        } else
            for (i = 1; i <= 10; i++)
                if (mx >= 0x178 && mx <= 0x217 && my >= 0x87 + 0x15 * (i - 1) &&
                    my <= 0x9a + 0x15 * (i - 1) && sel != i && i <= g_count - top)
                    back = sel, sel = i, redraw = 1;

        /* drawn: the opening's frames one by one first, the chosen name lit from the
           seventh on */
        while (redraw && !g_quit) {
            int y = 0x1c;

            if (step == 1)
                fe_play_sound(0x49);
            if (step < 8)
                settimeout(10);
            art_at(&anim, "opn8", DX, DY, 0);
            if (sel == UP)
                art_at(&anim, "arul", DX + 0x5b, DY + 7, 0);
            else if (sel == DOWN)
                art_at(&anim, "ardl", DX + 0x5b, DY + 0xf1, 0);
            else if (sel == CANCEL)
                art_at(&anim, "canl", DX + 0xb7, DY + (g_german ? 0x4e : 0x55), 0);
            fe_style(0);
            for (i = 0; i < 10; i++) {
                const char *n = top + i < MAX_FILES ? g_names[top + i] : "";
                char shown[20];
                int j;

                if (!n[0])
                    continue;
                for (j = 0; n[j] && n[j] != '.'; j++)
                    shown[j] = n[j];
                shown[j] = 0;
                if (i + 1 == sel && step > 6)
                    fe_style(1);
                fe_text_cut(shown, DX + 0x23, DY + y, 0x7d);
                fe_style(0);
                y += 0x15;
            }
            if (step < 8) {
                char f[16];
                snprintf(f, sizeof f, "opn%d", step);
                art_at(&anim, f, DX, DY, 1);
            }
            if (step > 8)
                fe_play_sound(0x47);
            if (step < 9)
                step++;
            mouse_show(1);
            g_mouse_active = 1;
            video_retrace();
            if (step < 8)
                while (!timedout() && !g_quit)
                    video_retrace();
            if (step >= 9)
                redraw = 0;
        }

        if (!ready && (held += ticksince()) > 15)
            ready = 1;
        if (g_mouse_active)
            fe_mouse_read();
        if (!g_mouse_buttons)
            released = 1;
        if (!released && (held += ticksince()) > 15)
            released = 1;
        fe_mouse_moved();
        k = fe_read_keys();
        if (g_mouse_buttons == 2 && released) {   /* the right button: Cancel */
            sel = CANCEL;
            ready = 1;
            k = 0x0d;
        }
        if (k == 0x20 || k == 0x0d || (g_mouse_buttons == 1 && released)) {
            fe_mouse_press();
            if (ready) {
                if (sel == UP) {
                    if (top > 0) {
                        fe_play_sound(0x47);
                        top--;
                        redraw = 1;
                    }
                } else if (sel == DOWN) {
                    if (top < g_count - 10) {
                        fe_play_sound(0x47);
                        top++;
                        redraw = 1;
                    }
                } else if (sel == CANCEL)
                    done = 1;
                else if (sel >= 1 && sel <= 10) {
                    const char *name = g_names[top + sel - 1];
                    char q[64];

                    mouse_limits(0, 0, 640, 480);
                    if (name[0]) {
                        fe_play_sound(0x48);
                        if (kind == LIST_DELETE_GAME || kind == LIST_DELETE_REPLAY) {
                            int ask = 1;

                            if (kind == LIST_DELETE_REPLAY && is_system(name)) {
                                fe_timed_message(g_german ? "Kann system-weiderholung nicht l\x94schen"
                                                          : "Cannot delete system replay", 100);
                                redraw = 1;
                                ask = 0;
                            }
                            if (kind == LIST_DELETE_GAME)
                                snprintf(q, sizeof q, "%s", g_german ? "Spiel l\x94schen ?" : "Delete game ?");
                            else
                                snprintf(q, sizeof q, "%s", g_german ? "Weiderholung l\x94schen ?" : "Delete replay ?");
                            if (ask && fe_yes_no_dialog(q)) {
                                home_path(path, sizeof path, name);
                                remove(path);
                                list_files(kind);
                                sel = back = g_count ? 1 : CANCEL;
                                if (g_count - top < 10 && top > 0)
                                    top--;
                                redraw = 1;
                            }
                        } else if (kind == LIST_LOAD) {
                            if (fe_yes_no_dialog(g_german ? "Spiel laden ?" : "Load game ?")) {
                                for (i = 0; i < 200; i++)
                                    memcpy(saved + i * 640, g_fb + (0xd2 + i) * g_fb_w, 640);
                                if (load_game(name)) {
                                    fe_timed_message(g_german ? "Spielstand erfolgreich geladen"
                                                              : "Game loaded successfully", 100);
                                    if (config_word(CFG_TRACK) < 3 && config_word(CFG_SEGMENT) > 0)
                                        result = 1;
                                }
                                for (i = 0; i < 200; i++)
                                    memcpy(g_fb + (0xd2 + i) * g_fb_w, saved + i * 640, 640);
                                video_retrace();
                                done = 1;
                            }
                        } else if (fe_yes_no_dialog(g_highlights
                                                        ? (g_german ? "Highlight laden ?" : "Load highlight ?")
                                                        : (g_german ? "Wiederholung laden ?" : "Load replay ?"))) {
                            memset(g_config + 0x3b, 0, 0x20);
                            snprintf((char *)g_config + 0x3b, 0x20, "%.31s", name);
                            result = done = 1;
                        }
                    }
                    mouse_limits(0x165, 0x6f, 0x22a, 0x16e);
                    mouse_place(px[sel], py[sel]);
                }
            }
            ready = released = 0;
            held = 0;
            ticksince();
        } else if (k && ready) {
            if (k == 0x1b)
                done = 1;
            else if (k == 0x4b00 || k == 0x4d00 || k == 0x4800 || k == 0x5000) {
                fe_mouse_hide();
                if (k == 0x4b00) {                 /* back from Cancel */
                    if (sel == CANCEL && back != CANCEL)
                        sel = back;
                } else if (k == 0x4d00) {          /* to Cancel */
                    if (sel != CANCEL)
                        back = sel, sel = CANCEL;
                } else if (k == 0x4800) {
                    /* up: the name before, past the empty rows, to the arrow; at the
                       arrow the list scrolls. (The original's code does not stop at
                       the arrow: past the first name it looks at the memory before the
                       list and loses the choice. NOTES §4.2) */
                    if (sel != CANCEL) {
                        if (sel > 0) {
                            sel--;
                            while (sel > 0 && !g_names[sel - 1][0])
                                sel--;
                        } else if (top > 0)
                            top--;
                    }
                } else if (sel != CANCEL) {        /* down: to the arrow past the last */
                    if (sel < DOWN) {
                        sel++;
                        if (!g_names[sel - 1][0])
                            sel = DOWN;
                    } else if (top < g_count - 10)
                        top++;
                }
                mouse_place(px[sel], py[sel]);
                redraw = 1;
            }
            ready = 0;
            held = 0;
            ticksince();
        } else if (!k)
            ready = 1;
        video_retrace();
    }

    /* closing: the frames the other way, a tenth of a second each */
    fe_play_sound(0x49);
    for (i = 7; i > 0 && !g_quit; i--) {
        char f[16];
        snprintf(f, sizeof f, "opn%d", i);
        settimeout(10);
        art(&anim, f, 1);
        video_retrace();
        while (!timedout() && !g_quit)
            video_retrace();
    }
    fsh_free(&anim);
    mouse_limits(0, 0, 640, 480);
    return result;
}

/* ------------------------------------------------------------------ the screen */

/* which rows may be chosen. From the options screen: save while a game is on, load
   only while none is (0x3c0c8(0)); from the checkpoint menu neither (0x3c0c8(1)); from
   the finish menu save in a tournament not yet won (0x3d4c4: every track raced, Lost
   Vegas too when offered), load never */
static int g_where, g_complete;

static int open_row(int r)
{
    if (r == SAVE_GAME)
        return g_where == LS_OPTIONS ? g_game_on
             : g_where == LS_FINISH ? config_word(CFG_RACE_TYPE) == 3 && !g_complete : 0;
    if (r == LOAD_GAME)
        return g_where == LS_OPTIONS && !g_game_on;
    return 1;
}

/* the whole screen: the background, the chosen row lit, the rows that cannot be chosen
   greyed */
static void draw(int sel)
{
    static const char *const lit[8] = { "savl", "loal", "dlgl", "sarl", "vwrl", "vwhl", "dlrl", "donl" };

    art(&g_art, "bgnd", 0);
    art(&g_art, lit[sel], 0);
    if (!open_row(SAVE_GAME))
        art(&g_art, "savm", 0);
    if (!open_row(LOAD_GAME))
        art(&g_art, "loam", 0);
}

/* 0x38e28 .. 0x38e58 (0x38e68 .. 0x38e98 the same): the mouse's places by row */
static void place(int sel)
{
    static const short x[8] = { 155, 155, 155, 155, 155, 155, 155, 40 };
    static const short y[8] = { 120, 160, 200, 240, 280, 320, 360, 450 };
    static const short gy[8] = { 120, 160, 200, 251, 302, 340, 390, 450 };

    mouse_place(x[sel], g_german ? gy[sel] : y[sel]);
}

/* a saved replay or its highlights played (race_run(2) with CONFIG.DAT +0xaf 1 or 2):
   the menus let go and back as the race menus' replay (0x1add8), but the results of the
   race before are not kept */
static void play_replay(int highlights, unsigned char pal[768], const char *path)
{
    static const unsigned char black[768];

    music_fade_out();
    pal_fade_down(0, 256);
    music_wait_end();
    fe_sound_bank_free();
    fsh_free(&g_art);
    config_set_word(0xaf, highlights + 1);
    fe_sound_unmute();             /* (0xf748e: F5's silence ends) */
    race_run();
    config_set_word(0xaf, 0);
    video_palette(black, 0, 256);
    video_mode(640, 480);
    video_palette(black, 0, 256);
    fe_sound_bank_load();
    music_random();                /* 0x63e2c */
    if (!fsh_load(&g_art, path) || !fsh_palette(&g_art, 0, pal))
        fprintf(stderr, "renfs: %s is missing\n", path);
}

int fe_load_save(int where)
{
    /* the rows' heights for the mouse (German: the rows below Save replay lower) */
    static const short y0[8] = { 0, 0x8c, 0xb4, 0xdc, 0x104, 0x12c, 0x154, 0x19a };
    static const short y1[8] = { 0x87, 0xaf, 0xd7, 0xff, 0x127, 0x14f, 0x190, 0x1e0 };
    static const short gy0[8] = { 0, 0x8c, 0xb4, 0xdc, 0x11d, 0x145, 0x16d, 0x1ae };
    static const short gy1[8] = { 0x87, 0xaf, 0xd7, 0x118, 0x140, 0x168, 0x1a4, 0x1e0 };
    unsigned char pal[768];
    char path[256], name[0x40];
    int sel = LOAD_GAME, redraw = 1, first = 1, changed = 0, done = 0;
    int ready = 1, released = 1, i, k;
    unsigned held = 0;

    g_where = where;
    g_check = where != LS_OPTIONS;
    g_complete = 0;
    if (where == LS_FINISH) {          /* every track of the tournament raced */
        int raced = 0;
        for (i = 0; i < 8; i++)
            raced += g_session.raced[i];
        g_complete = g_vegas_ok ? raced + g_session.raced[8] == 9 : raced == 8;
        if (g_game_on)
            sel = config_word(CFG_RACE_TYPE) == 3 && !g_complete ? SAVE_GAME : DELETE_GAME;
    } else if (where == LS_CHECKPOINT)
        sel = SAVE_REPLAY;
    else if (g_game_on && config_word(CFG_RACE_TYPE) == 3)
        sel = SAVE_GAME;
    place(sel);
    snprintf(path, sizeof path, "%s/%s", art_dir(),
             g_check ? "CHECK/LOADSAVE.QFS" : "OPTION/LOADSAVE.QFS");
    if (!fsh_load(&g_art, path) || !fsh_palette(&g_art, 0, pal)) {
        fprintf(stderr, "renfs: %s is missing\n", path);
        return 0;
    }
    if (g_mouse_active)
        fe_mouse_read();
    fe_mouse_save();

    while (!done && !g_quit) {
        int mx, my, over = -1;

        mouse_read(&mx, &my);
        for (i = 0; i < 8 && over < 0; i++)
            if (my >= (g_german ? gy0 : y0)[i] && my <= (g_german ? gy1 : y1)[i] && open_row(i))
                over = i;
        if (over >= 0 && over != sel) {
            sel = over;
            changed = 1;
        }

        if (redraw) {
            draw(sel);
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
            redraw = 0;
        }
        /* a new row: the whole screen drawn again. (0x3ca8d also draws the row left dark
           and the new one lit into the menu's window, but sets the flag for the whole
           picture as it does so ([esp+0x730] after a push is 0x72c), and the window is
           shown only after that: the rows left show the background's own letters) */
        if (changed) {
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
                case SAVE_GAME:
                    snprintf(name, sizeof name, "%.9s", (const char *)g_config + 9);
                    save_game(name);
                    break;
                case LOAD_GAME:
                    file_list(LIST_LOAD);
                    if (where == LS_OPTIONS) {     /* (a game is loaded at its track's start) */
                        config_set_word(CFG_SEGMENT, 0);
                        sel = DONE;
                    } else if (!g_mouse_active && sel == LOAD_GAME)
                        sel = DELETE_GAME;
                    break;
                case DELETE_GAME:
                    file_list(LIST_DELETE_GAME);
                    break;
                case SAVE_REPLAY:
                    snprintf(name, sizeof name, "%.32s", (const char *)g_config + 0x3b);
                    save_replay(name);
                    break;
                case VIEW_REPLAY:
                case VIEW_HIGHLIGHTS:
                    g_highlights = sel != VIEW_REPLAY;
                    if (file_list(LIST_VIEW)) {
                        play_replay(g_highlights, pal, path);
                        first = 1;
                    }
                    break;
                case DELETE_REPLAY:
                    file_list(LIST_DELETE_REPLAY);
                    break;
                case DONE:
                    done = 1;
                    break;
                }
                if (!done) {
                    /* (the finish menu's puts the mouse back only when it is not used) */
                    if (where != LS_FINISH || !g_mouse_active)
                        place(sel);
                    settimeout(12000);
                    redraw = 1;
                }
            }
            released = ready = 0;
            held = 0;
            ticksince();
        } else if (k && ready) {
            if (k == 0x1b) {
                fe_play_sound(0x48);
                done = 1;
            } else if (k == 0x4800 || k == 0x5000) {
                /* up and down past the rows that cannot be chosen, round */
                fe_mouse_hide();
                do
                    sel = k == 0x4800 ? (sel + 7) % 8 : (sel + 1) % 8;
                while (!open_row(sel));
                place(sel);
                changed = 1;
            }
            ready = 0;
            held = 0;
            ticksince();
        } else if (!k)
            ready = 1;

        video_retrace();
        if (!done && !first && (timedout() || (g_mouse_buttons == 2 && released))) {
            fe_play_sound(0x48);           /* 2 minutes, or the right button: as Done */
            done = 1;
        }
    }
    pal_fade_down(0, 256);
    fsh_free(&g_art);
    settimeout(12000);
    fe_mouse_save();
    return 0;
}
