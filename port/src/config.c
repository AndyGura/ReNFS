/* CONFIG.DAT. The port starts from the CD's copy; its own settings file comes later. */
#include "config.h"
#include "files.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned char g_config[CONFIG_SIZE];

int config_load(void)
{
    long len;
    unsigned char *d;
    if (getenv("RENFS_CONFIG")) {          /* a test: another copy (the original's, read
                                              from its memory) */
        FILE *f = fopen(getenv("RENFS_CONFIG"), "rb");
        int ok = f && fread(g_config, 1, CONFIG_SIZE, f) == CONFIG_SIZE;
        if (f)
            fclose(f);
        if (ok)
            return 1;
    }
    d = file_load("GAMEDATA/CONFIG/CONFIG.DAT", &len);
    if (!d || len != CONFIG_SIZE) {
        free(d);
        return 0;
    }
    memcpy(g_config, d, CONFIG_SIZE);
    free(d);
    return 1;
}

int config_word(int at)
{
    return (short)(g_config[at] | g_config[at + 1] << 8);
}

void config_set_word(int at, int v)
{
    g_config[at] = (unsigned char)v;
    g_config[at + 1] = (unsigned char)(v >> 8);
}
