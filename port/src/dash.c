/* The dashboard (NOTES §4.7). */
#include "dash.h"
#include "racepal.h"
#include "exe.h"
#include <stdio.h>
#include <string.h>

dash_state g_dash;

int dash_load(const char *car, int hires)
{
    char path[64];
    shape s;
    unsigned char *head;
    int i;

    dash_free();
    snprintf(path, sizeof path, "SIMDATA/DASH/%s%s.FSH", car, hires ? "DH" : "DL");
    if (!fsh_load(&g_dash.file, path)) {
        fprintf(stderr, "renfs: %s is missing\n", path);
        return 0;
    }
    /* "!pal", 8-bit ones made 6-bit; else the palette block after "dash" */
    if (fsh_find(&g_dash.file, "!pal", &s)) {
        head = (unsigned char *)s.head;
        g_dash.pal = head + 0x10;
        if (head[0] == 0x24)
            for (i = 0; i < 0x300; i++)
                g_dash.pal[i] >>= 2;
    } else if (fsh_find(&g_dash.file, "dash", &s)) {
        head = (unsigned char *)s.head;
        g_dash.pal = head + (int)((unsigned)(head[1] | head[2] << 8 | head[3] << 16)) + 0x10;
    } else {
        fprintf(stderr, "renfs: %s has no palette\n", path);
        dash_free();
        return 0;
    }
    return 1;
}

void dash_free(void)
{
    fsh_free(&g_dash.file);
    memset(&g_dash, 0, sizeof g_dash);
}

void dash_queue_colours(void)
{
    pal_queue_piece(0, 0x30, g_dash.pal);
}

void cockpit_queue_colours(void)
{
    pal_queue_piece(0xf0, 13, exe_at(0xf6d18, 39));
}
