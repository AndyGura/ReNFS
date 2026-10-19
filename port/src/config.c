/* CONFIG.DAT. The port starts from the CD's copy; its own settings file comes later. */
#include "config.h"
#include "files.h"
#include <stdlib.h>
#include <string.h>

unsigned char g_config[CONFIG_SIZE];

int config_load(void)
{
    long len;
    unsigned char *d = file_load("GAMEDATA/CONFIG/CONFIG.DAT", &len);
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
