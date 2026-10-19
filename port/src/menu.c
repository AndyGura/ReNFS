/* What every menu screen shares. */
#include "menu.h"
#include "files.h"
#include "fsh.h"
#include "sound.h"
#include "video.h"
#include <stdio.h>
#include <string.h>

unsigned ticksince(void)
{
    static unsigned last;
    unsigned now = clock_ticks(), d = now - last;
    last = now;
    return d;
}

int g_fkeys;                   /* 0x12bd3c: F1-F4 read too (set by the race's options) */

int fe_read_keys(void)
{
    static const struct { int code, scan; } keys[] = {
        { 0x1b, 0x01 }, { 0x20, 0x39 }, { 0x0d, 0x1c }, { 0x4800, 0x48 }, { 0x5000, 0x50 },
        { 0x4b00, 0x4b }, { 0x4d00, 0x4d }, { 0x5200, 0x52 }, { 0x5300, 0x53 },
    };
    static const int fkeys[4] = { 0x3b, 0x3c, 0x3d, 0x3e };
    static const int raw[] = { 0x2a, 0x36, 0x1d, 0x38, 0x15, 0x31, 0x24 };
    if (key_down(0x44) && key_down(0x1d))  /* Ctrl+F10: the modem game's break */
        return 0;
    if (key_down(0x3f))                    /* F5: 0x72638, a screenshot (not here yet) */
        return 0;
    for (size_t i = 0; i < sizeof keys / sizeof keys[0]; i++)
        if (key_down(keys[i].scan))
            return keys[i].code;
    for (int i = 0; g_fkeys && i < 4; i++)
        if (key_down(fkeys[i]))
            return fkeys[i] << 8;
    for (size_t i = 0; i < sizeof raw / sizeof raw[0]; i++)
        if (key_down(raw[i]))
            return raw[i];
    /* Tab: nothing; else the joystick's buttons (0x4dfe0): to come */
    return 0;
}

void fe_wait_keys_up(void)
{
    while (fe_read_keys() && !g_quit)
        video_retrace();
}

/* the mouse */
int g_mouse_active, g_mouse_buttons;
static int g_saved_x, g_saved_y;
static fsh_file g_mouse_fsh;
static shape g_ptr[2];

void fe_mouse_load(void)
{
    char path[256];
    if (g_mouse_fsh.d)
        return;
    snprintf(path, sizeof path, "%s/MOUSE.FSH", art_dir());
    if (!fsh_load(&g_mouse_fsh, path) || !fsh_find(&g_mouse_fsh, "ptr0", &g_ptr[0]) ||
        !fsh_find(&g_mouse_fsh, "ptr1", &g_ptr[1])) {
        fprintf(stderr, "renfs: %s is missing\n", path);
        return;
    }
    mouse_pointer(g_ptr[0].pixels, g_ptr[0].w, g_ptr[0].h);
}

void fe_mouse_read(void)
{
    g_mouse_buttons = mouse_read(0, 0);
}

void fe_mouse_save(void)
{
    mouse_read(&g_saved_x, &g_saved_y);
}

void fe_mouse_moved(void)
{
    int x, y;
    mouse_read(&x, &y);
    if (!g_mouse_active && (g_mouse_buttons || x != g_saved_x || y != g_saved_y))
        g_mouse_active = 1;
}

void fe_mouse_press(void)
{
    if (g_mouse_fsh.d)
        mouse_pointer(g_ptr[1].pixels, g_ptr[1].w, g_ptr[1].h);
    settimeout(20);
    while (!timedout() && !g_quit)
        video_retrace();
    if (g_mouse_fsh.d)
        mouse_pointer(g_ptr[0].pixels, g_ptr[0].w, g_ptr[0].h);
    settimeout(12000);
}

/* the text */
font g_fe_font;

int fe_font_load(void)
{
    char path[256];
    snprintf(path, sizeof path, "%s/FONTS/GRAVER18.FFN", art_dir());
    if (g_fe_font.d)
        return 1;
    if (!font_load(&g_fe_font, path)) {
        fprintf(stderr, "renfs: %s is missing\n", path);
        return 0;
    }
    return 1;
}

/* the slots: white, beige, yellow, red, the chosen one, black (0x152f04, 0x153f44,
   0x154fd4, 0x150b84, 0x14fb44, 0x151bc4) */
static text_style g_white, g_beige, g_yellow, g_red, g_chosen, g_black;

void fe_styles_init(void)
{
    /* textcolour(nearestcolour(rgb), 0) each, copied out (0x8ecc4 is source first) */
    text_colour(nearest_colour(0xffffff));
    g_white = g_text;
    text_colour(nearest_colour(0xe8d3b7));
    g_beige = g_text;
    text_colour(nearest_colour(0xffce06));
    g_yellow = g_text;
    text_colour(nearest_colour(0xff0606));
    g_red = g_text;
    text_colour(nearest_colour(0xffce06));
    g_chosen = g_text;
    text_colour(nearest_colour(0x000000));
    g_black = g_text;
}

void fe_style(int n)
{
    g_chosen = n == 3 ? g_white : n == 2 ? g_red : n == 1 ? g_yellow : g_beige;
}

void fe_text(const char *s, int x, int y)
{
    g_text = g_black;
    text_draw(&g_fe_font, s, x + 2, y + 2);
    g_text = g_chosen;
    text_draw(&g_fe_font, s, x, y);
}

void fe_button_text(const char *s, int x, int y)
{
    /* drawn in a window of the text's width + 10 by 45 holding the screen there */
    text_clip(x, y, x + text_width(&g_fe_font, s) + 10, y + 45);
    fe_text(s, x, y);
    text_clip_off();
}

void fe_text_centre(const char *s, int x, int y)
{
    fe_text(s, x - text_width(&g_fe_font, s) / 2, y);
}

/* 0x89f3c with 0x89da0: each colour scaled (16.16, rounded) and capped at 63 (the
   library thought in 6-bit colours; the menus' palettes are 8-bit, so everything comes
   out dark), then the nearest of entries 0..253, green weighed 3, red 2, blue 1 */
static void darken_table(unsigned char out[256], const unsigned char *pal, long scale)
{
    int last = 0;
    for (int i = 0; i < 256; i++) {
        long t[3];
        long best = 9999;
        for (int c = 0; c < 3; c++) {
            long p = pal[3 * i + c] * scale;
            t[c] = (p >> 16) + (p >> 15 & 1);
            if (t[c] > 63)
                t[c] = 63;
        }
        for (int j = 0; j < 254; j++) {
            long d = 2 * (t[0] - pal[3 * j]) * (t[0] - pal[3 * j]);
            if (d >= best)
                continue;
            d += 3 * (t[1] - pal[3 * j + 1]) * (t[1] - pal[3 * j + 1]);
            if (d >= best)
                continue;
            d += (t[2] - pal[3 * j + 2]) * (t[2] - pal[3 * j + 2]);
            if (d < best) {
                best = d;
                last = j;
            }
        }
        out[i] = (unsigned char)last;      /* none near: the one before (the stack's) */
    }
}

/* 0x8a0f0: a frame of one pixel */
static void frame(int x0, int y0, int x1, int y1, int colour)
{
    for (int x = x0; x <= x1; x++)
        g_fb[y0 * g_fb_w + x] = g_fb[y1 * g_fb_w + x] = (unsigned char)colour;
    for (int y = y0 + 1; y < y1; y++)
        g_fb[y * g_fb_w + x0] = g_fb[y * g_fb_w + x1] = (unsigned char)colour;
}

int fe_yes_no_dialog(const char *question)
{
    enum { BX = 150, BY = 190, BW = 340, BH = 100 };
    static const int mouse_x[2] = { 225, 420 }, mouse_y[2] = { 260, 260 };
    static unsigned char saved[BW * BH];
    const char *yes = g_german ? "Ja" : "Yes", *no = g_german ? "Nein" : "No";
    unsigned char remap[256];
    int sel = 1, changed = 1, ready = 0, released = 0, done = 0, result = 0;

    for (int y = 0; y < BH; y++)           /* what was under it */
        memcpy(saved + y * BW, g_fb + (BY + y) * g_fb_w + BX, BW);
    if (!g_mouse_active) {
        mouse_place(mouse_x[sel], mouse_y[sel]);
        fe_mouse_save();
    } else
        fe_mouse_read();
    /* the box: the screen under it darkened (0x8a0cf through the table), two red frames */
    darken_table(remap, video_palette8(), 0x9000);
    for (int y = 0; y < BH; y++)
        for (int x = 0; x < BW; x++) {
            unsigned char *p = &g_fb[(BY + y) * g_fb_w + BX + x];
            *p = remap[*p];
        }
    frame(BX + 2, BY + 2, BX + 337, BY + 97, nearest_colour(0xff0000));
    frame(BX + 3, BY + 3, BX + 336, BY + 96, nearest_colour(0xff0000));
    fe_styles_init();
    fe_style(2);
    fe_text_centre(question, BX + 170, BY + 20);
    fe_style(0);
    fe_text(yes, BX + 60, BY + 60);
    fe_style(1);
    fe_text(no, BX + 250, BY + 60);
    video_retrace();
    mouse_show(1);
    while (fe_read_keys() && !g_quit) {
        fe_mouse_read();
        video_retrace();
    }
    fe_wait_keys_up();
    while (!done && !g_quit) {
        int mx, my, k;
        mouse_read(&mx, &my);
        if (mx >= 0xbe && mx <= 0x104 && my >= 0xf5 && my <= 0x118) {
            if (sel) {
                sel = 0;
                changed = 1;
            }
        } else if (mx >= 0x17c && mx <= 0x1cc && my >= 0xf5 && my <= 0x118 && sel != 1) {
            sel = 1;
            changed = 1;
        }
        if (changed) {                     /* at the start too: drawn twice, brighter */
            fe_play_sound(0x47);
            fe_style(sel ? 0 : 1);
            fe_button_text(yes, BX + 60, BY + 60);
            fe_style(sel ? 1 : 0);
            fe_button_text(no, BX + 250, BY + 60);
            changed = 0;
        }
        if (g_mouse_active)
            fe_mouse_read();
        if (!g_mouse_buttons)
            released = 1;
        fe_mouse_moved();
        k = fe_read_keys();
        if (k == 0x20 || k == 0x0d || (g_mouse_buttons && released)) {
            int ok = ready;
            fe_mouse_press();
            if (ok) {
                fe_play_sound(0x48);
                result = g_mouse_buttons != 2 && !sel;    /* the right button: no */
                done = 1;
            }
        } else if (k && ready) {
            if (k == 0x1b || k == 0x31) {                  /* Esc, N */
                fe_play_sound(0x48);
                done = 1;
            } else if ((k == 0x15 && !g_german) || (k == 0x24 && g_german)) {  /* Y, J */
                fe_play_sound(0x48);
                result = done = 1;
            } else if (k == 0x4b00 || k == 0x4d00) {
                g_mouse_active = 0;
                sel ^= 1;
                mouse_place(mouse_x[sel], mouse_y[sel]);
                fe_mouse_save();
                changed = 1;
            }
            ready = 0;
        } else if (!k)
            ready = 1;
        video_retrace();
    }
    for (int y = 0; y < BH; y++)
        memcpy(g_fb + (BY + y) * g_fb_w + BX, saved + y * BW, BW);
    video_retrace();
    return result;
}

int fe_ask_exit(void)
{
    char q[64];
    if (g_german)
        snprintf(q, sizeof q, "Zur%cck zum system ?", 0x81);
    else
        snprintf(q, sizeof q, "Exit to system ?");
    return fe_yes_no_dialog(q);
}
