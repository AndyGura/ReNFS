/* The credits (fe_credits 0x25600, NOTES §4.2): five pages of CREDITS.FSH, each faded up
   and held until Space, Enter or a click (the next page), Esc or the right button (the
   end), or 20 seconds; then faded down. The options screen plays CREDITS.ASF meanwhile. */
#include "credits.h"
#include "fe.h"
#include "files.h"
#include "fsh.h"
#include "menu.h"
#include "sound.h"
#include "video.h"
#include <stdio.h>

void fe_credits(void)
{
    unsigned char pal[768];
    char path[256], name[8];
    fsh_file art;
    shape s;
    int page, stop = 0, released = 0;

    snprintf(path, sizeof path, "%s/CREDITS.FSH", art_dir());
    if (!fsh_load(&art, path) || !fsh_palette(&art, 0, pal)) {
        fprintf(stderr, "renfs: %s is missing\n", path);
        return;
    }
    mouse_show(0);                         /* (no pointer drawn on these pages) */
    for (page = 1; page < 6 && !stop && !g_quit; page++) {
        snprintf(name, sizeof name, "bgn%d", page);
        if (fsh_find(&art, name, &s))
            fe_draw_shape(&s, 0);
        pal_fade_up(pal, 0, 256);
        timer_counter_set(0);
        for (;;) {
            int k;

            video_retrace();               /* (fe_service_music 0x71c20) */
            if (g_mouse_active)
                fe_mouse_read();
            if (!g_mouse_buttons)
                released = 1;              /* (once, for every page after) */
            k = fe_read_keys();
            if (k == 0x1b || (g_mouse_buttons == 2 && released) || g_quit) {
                stop = 1;
                break;
            }
            if (k == 0x20 || k == 0x0d || (g_mouse_buttons == 1 && released) ||
                timer_counter() >= 2000)
                break;
        }
        pal_fade_down(0, 256);
    }
    music_fade_out();
    pal_fade_down(0, 256);
    music_wait_end();
    fsh_free(&art);
    fe_mouse_save();
}
