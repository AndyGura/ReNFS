/* CONFIG.DAT. The port keeps its own copy in the home folder (config.dat, the same 0x5f52
   bytes the game keeps); until there is one, the game's built-in settings and records, as
   an installed copy of the original starts (the CD's CONFIG.DAT is never read). */
#include "config.h"
#include "joystick.h"
#include "audio.h"
#include "exe.h"
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

static void put32(int at, int v)
{
    g_config[at] = (unsigned char)v;
    g_config[at + 1] = (unsigned char)(v >> 8);
    g_config[at + 2] = (unsigned char)(v >> 16);
    g_config[at + 3] = (unsigned char)(v >> 24);
}

/* a string of the program, by the pointer at addr */
static const char *exe_str(unsigned addr)
{
    return (const char *)exe_at((unsigned)exe_i32(addr), 1);
}

/* the C library's atoi (0x89903): spaces, a sign, digits */
static int number(const char *t)
{
    int v = 0, neg;
    while (*t == ' ' || (*t >= '\t' && *t <= '\r'))
        t++;
    neg = *t == '-';
    if (*t == '+' || *t == '-')
        t++;
    while (*t >= '0' && *t <= '9')
        v = v * 10 + *t++ - '0';
    return neg ? -v : v;
}

#define RECORDS 0xb5           /* 9 tracks of 0xa6b bytes, 0x27 a record */

/* a record from the program's text (six strings: the name, the time in 1/60 s, the car,
   the speed, +0x13, the kind of race), marked filled */
static void record(int track, int i, unsigned text)
{
    int at = RECORDS + track * 0xa6b + i * 0x27;
    const char *name = exe_str(text);

    memset(g_config + at, 0, 15);
    memcpy(g_config + at, name, strlen(name) + 1);
    put32(at + 0x1b, number(exe_str(text + 4)));
    put32(at + 0xf, number(exe_str(text + 8)));
    put32(at + 0x1f, number(exe_str(text + 12)));
    put32(at + 0x13, number(exe_str(text + 16)));
    put32(at + 0x23, number(exe_str(text + 20)));
    put32(at + 0x17, 1);
}

/* the built-in settings (cfg_defaults 0x38ea8) */
static void config_defaults(void)
{
    /* the open roads' records 0..5 and their tables of race times, the circuits' */
    static const unsigned roads[3] = { 0xeac58, 0xeace8, 0xead78 };
    static const unsigned road_times[3] = { 0xeae98, 0xeaf88, 0xeb078 };
    static const unsigned laps[6] = { 0xeb168, 0xeb438, 0xeb9d8, 0xeb708, 0xebca8, 0xebf78 };
    /* the open roads' six numbers after the records (+0x5a3: three times, three speeds) */
    static const int road_split[3][6] = {
        { 0x1ca4, 0x3a50, 0x680a, 0x5a66c6, 0x4b4f38, 0x4a0302 },
        { 0x235e, 0x49ec, 0x770a, 0x54b80e, 0x464366, 0x406a41 },
        { 0x267c, 0x5bd6, 0x9892, 0x529544, 0x49d4e7, 0x3a85af },
    };
    static const struct { int at, v; } words[] = {
        { CFG_RACE_TYPE, 1 }, { 0x6f, 0 }, { 0x71, 0 }, { 0x7d, 1 }, { CFG_MENU_MUSIC, 1 },
        { 0x77, 100 }, { 0x75, 100 }, { 0x7b, 1 }, { CFG_SFX_VOLUME, 0x7f }, { CFG_AUTO, 1 },
        { CFG_TRACK, 0 }, { CFG_ABS, 0 }, { 0xaf, 0 }, { 0x8b, 0 }, { CFG_TCS, 2 },
        { 0x93, 0 }, { CFG_TRANSMISSION, 0 }, { CFG_RACE_LENGTH, 1 }, { CFG_LAPS, 4 },
        { CFG_OPPONENT, 7 }, { CFG_CAR, 6 },
        /* the detail settings by the speed test (0x74ca0): the port answers below 300,
           a fast machine: races at 640x480 (+0x7f = 1 is 320x200; the port's races are
           320x200 whatever it says, for now), the most detail (the view's pixels 1:1,
           the road 28 sections ahead, the outside view at its largest) */
        { 0x7f, 0 }, { 0x83, 0 }, { 0x85, 0 }, { 0x81, 0 }, { 0x89, 0 }, { 0x87, 0 },
        /* the keys (0x15cb4a..), the modem's numbers */
        { 0x5f3c, 0x4b00 }, { 0x5f3e, 0x4d00 }, { 0x5f42, 0x5000 }, { 0x5f36, 0 },
        { 0x5f3a, 0 }, { 0x5f38, 0 }, { 0x5f44, 0x41 }, { 0x5f40, 0x4800 }, { 0x5f4a, 0x43 },
        { 0x5f48, 0x20 }, { 0x5f46, 0x5a }, { 0x5f4c, 0x48 }, { 0x5f4e, 0x4d },
    };
    int i, k, t;

    memset(g_config, 0, CONFIG_SIZE);
    strcpy((char *)g_config, g_german ? "Spieler" : "Player");
    for (i = 0; i < (int)(sizeof words / sizeof words[0]); i++)
        config_set_word(words[i].at, words[i].v);
    for (t = 0; t < 9; t++)                /* each track's top speed */
        record(t, 36, 0xeab80 + t * 0x18);
    for (i = 0; i < 10; i++)
        for (t = 0; t < 3; t++)
            record(t, 6 + i, road_times[t] + i * 0x18);
    for (i = 0; i < 6; i++)                /* the segments' times, then their speeds */
        for (t = 0; t < 3; t++)
            record(t, i, roads[t] + i * 0x18);
    for (i = 0; i < 10; i++)               /* the circuits' three tables of race times */
        for (t = 0; t < 6; t++)
            for (k = 0; k < 3; k++)
                record(3 + t, 6 + k * 10 + i, laps[t] + k * 0xf0 + i * 0x18);
    for (t = 0; t < 6; t++)                /* the circuits' best laps */
        record(3 + t, 0, 0xeae08 + t * 0x18);
    for (t = 0; t < 3; t++)
        for (i = 0; i < 6; i++)
            put32(RECORDS + t * 0xa6b + 0x5a3 + 4 * i, road_split[t][i]);
    if (g_german) {                        /* (Y and Z change places) */
        config_set_word(0x5f46, 0x59);
        config_set_word(0x5f4a, 0x4b);
    }
    strcpy((char *)g_config + 0x5e78, "Custom");
    strcpy((char *)g_config + 0x5eaa, "ATZ");
    strcpy((char *)g_config + 0x5ee6, "ATD");
    config_set_word(0x5ea6, 1);
    config_set_word(0x5ea2, 0);
    config_set_word(0x5ea4, 4);
    config_set_word(CFG_UNLOCKED, 0);
    config_set_word(0x5ea0, 9);
}

static int config_find(void)
{
    char path[1024];
    if (getenv("RENFS_CONFIG") && config_read(getenv("RENFS_CONFIG")))
        return 1;                          /* a test: another copy (the original's, read
                                              from its memory) */
    if (config_read(home_path(path, sizeof path)))
        return 1;
    config_defaults();
    return 0;
}

int config_load(void)
{
    int found = config_find();
    /* (0x39d08) without a joystick, a control left on it goes back to the keys */
    if (!g_joy.on) {
        static const int ctl[3][3] = { { 0x5f36, 0x4b00, 0x4d00 }, { 0x5f3a, 0x41, 0x5a },
                                       { 0x5f38, 0x4800, 0x5000 } };
        static const int keys[3] = { 0x5f3c, 0x5f44, 0x5f40 };
        for (int i = 0; i < 3; i++)
            if (config_word(ctl[i][0]) > 0 && config_word(ctl[i][0]) < 4) {
                config_set_word(ctl[i][0], 0);
                config_set_word(keys[i], ctl[i][1]);
                config_set_word(keys[i] + 2, ctl[i][2]);
            }
    }
    audio_mode(config_word(CFG_AUDIO_MODE));   /* the card as the audio screen left it */
    return found;
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

int config_long(int at)
{
    return (int)((unsigned)g_config[at] | (unsigned)g_config[at + 1] << 8
                 | (unsigned)g_config[at + 2] << 16 | (unsigned)g_config[at + 3] << 24);
}

void config_set_long(int at, int v)
{
    put32(at, v);
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
