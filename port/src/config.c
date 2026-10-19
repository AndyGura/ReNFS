/* CONFIG.DAT. The port keeps its own copy in the home folder (config.dat, the same 0x5f52
   bytes the game keeps); the CD's is the start until there is one. */
#include "config.h"
#include "files.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned char g_config[CONFIG_SIZE];

static const char *home_path(char *path, size_t n)
{
    snprintf(path, n, "%s/config.dat", files_home());
    return path;
}

/* a copy of the whole state, if it is one (its size) */
static int config_read(const char *path)
{
    unsigned char d[CONFIG_SIZE + 1];
    FILE *f = fopen(path, "rb");
    size_t n = f ? fread(d, 1, sizeof d, f) : 0;
    if (f)
        fclose(f);
    if (n != CONFIG_SIZE)
        return 0;
    memcpy(g_config, d, CONFIG_SIZE);
    return 1;
}

int config_load(void)
{
    long len;
    unsigned char *d;
    char path[1024];
    if (getenv("RENFS_CONFIG") && config_read(getenv("RENFS_CONFIG")))
        return 1;                          /* a test: another copy (the original's, read
                                              from its memory) */
    if (config_read(home_path(path, sizeof path)))
        return 1;
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

void config_save(void)
{
    char path[1024], tmp[1040];
    FILE *f;
    if (getenv("RENFS_CONFIG"))            /* a test leaves the settings alone */
        return;
    home_path(path, sizeof path);
    snprintf(tmp, sizeof tmp, "%s.new", path);
    f = fopen(tmp, "wb");
    if (!f || fwrite(g_config, 1, CONFIG_SIZE, f) != CONFIG_SIZE || fclose(f) != 0) {
        if (f)
            fclose(f);
        remove(tmp);
        fprintf(stderr, "renfs: cannot write %s\n", path);
        return;
    }
    rename(tmp, path);                     /* whole or not at all */
}
