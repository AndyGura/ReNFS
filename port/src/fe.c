/* The front end. */
#include "fe.h"
#include "files.h"
#include "fsh.h"
#include "video.h"
#include <stdio.h>
#include <string.h>

static unsigned char g_shown[768];     /* the palette on the card, 8 bits a channel */

static void set_palette(const unsigned char *pal, int first, int count)
{
    memcpy(g_shown + 3 * first, pal, 3 * count);
    video_palette(pal, first, count);
}

/* 0x98abc: each channel x level / 32 */
static void scale(unsigned char *out, const unsigned char *in, int n, int level)
{
    for (int i = 0; i < n; i++)
        out[i] = (unsigned char)(in[i] * level >> 5);
}

void pal_fade_up(const unsigned char *pal, int first, int count)
{
    unsigned char p[768];
    for (int level = 0; level <= 32; level += 2) {
        scale(p, pal, 3 * count, level);
        video_retrace();
        set_palette(p, first, count);
    }
}

void pal_fade_down(int first, int count)
{
    unsigned char from[768], p[768];
    /* 0x98aec reads the card back: its 6 bits widened as v << 2 | v >> 4 */
    for (int i = 0; i < 3 * count; i++) {
        unsigned v = g_shown[3 * first + i] >> 2;
        from[i] = (unsigned char)(v << 2 | v >> 4);
    }
    for (int level = 32; level >= 0; level -= 2) {
        scale(p, from, 3 * count, level);
        video_retrace();
        set_palette(p, first, count);
    }
}

/* drawshapeatpos for an 8-bit picture: at its own x, y, colour 0 drawn too */
static void draw_picture(const shape *s)
{
    for (int y = 0; y < s->h && s->y + y < g_fb_h; y++) {
        int w = s->w;
        if (s->x + w > g_fb_w)
            w = g_fb_w - s->x;
        if (w > 0)
            memcpy(g_fb + (s->y + y) * g_fb_w + s->x, s->pixels + y * s->w, w);
    }
}

int fe_show_picture(const char *name, int wait)
{
    char path[256];
    unsigned char pal[768];
    fsh_file f;
    shape bg;

    snprintf(path, sizeof path, "%s/%s", art_dir(), name);
    if (!fsh_load(&f, path)) {
        fprintf(stderr, "renfs: %s is missing\n", path);
        return 0;
    }
    if (!fsh_find(&f, "bgnd", &bg) || bg.type != 0x7b || !fsh_palette(&f, 0, pal)) {
        fprintf(stderr, "renfs: %s has no picture\n", path);
        fsh_free(&f);
        return 0;
    }
    memset(g_shown, 0, sizeof g_shown);    /* the screen is black before it comes up */
    video_palette(g_shown, 0, 256);
    memset(g_fb, 0, (size_t)g_fb_w * g_fb_h);
    draw_picture(&bg);
    fsh_free(&f);
    pal_fade_up(pal, 0, 256);
    if (wait) {
        /* 0x74b20 with mode 0: until Space, Enter or Esc, or the time set before */
        while (!g_quit && !timedout()) {
            int k = key_get();
            if (k == 0x20 || k == 0x0d || k == 0x1b)
                break;
            video_retrace();
        }
        pal_fade_down(0, 256);
    }
    return 1;
}

void fe_title_screen(void)
{
    settimeout(500);                       /* 5 seconds */
    if (!fe_show_picture("TITLE.QFS", 0))
        return;
    while (!g_quit && !timedout() && !key_get())
        video_retrace();
    pal_fade_down(0, 256);
}
