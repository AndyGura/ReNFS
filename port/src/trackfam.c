/* load_track_textures 0x5b34c: the family's entry 0 is a list of texture banks (or one bank
   of them all), entry 1 the roadside pictures, entry 2 the horizon, entry 3 the models. */
#include "trackfam.h"
#include "files.h"
#include "racepal.h"
#include "roaddraw.h"
#include "track.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

track_fam g_fam;

static unsigned u32(const unsigned char *p)
{
    return (unsigned)p[0] | (unsigned)p[1] << 8 | (unsigned)p[2] << 16 | (unsigned)p[3] << 24;
}

int ww_count(const unsigned char *c)
{
    return c && memcmp(c, "wwww", 4) == 0 ? (int)u32(c + 4) : 0;
}

unsigned char *ww_entry(unsigned char *c, int i)
{
    unsigned o;
    if (i < 0 || i >= ww_count(c))
        return 0;
    o = u32(c + 8 + 4 * i);
    return o ? c + o : 0;
}

/* EA's findshape (0x80e24): the four letters as they are */
unsigned char *shpi_find(unsigned char *bank, const char *name)
{
    int n = (int)u32(bank + 8), i;
    for (i = 0; i < n; i++)
        if (memcmp(bank + 16 + 8 * i, name, 4) == 0)
            return bank + u32(bank + 20 + 8 * i);
    return 0;
}

/* 0x5b1dc: in capitals, else in small letters (the name is left as last tried) */
static unsigned char *find_any_case(unsigned char *bank, char *name)
{
    unsigned char *s;
    int i;
    for (i = 0; i < 4; i++)
        if (name[i] >= 'a' && name[i] <= 'z')
            name[i] &= ~0x20;
    if ((s = shpi_find(bank, name)) != 0)
        return s;
    for (i = 0; i < 4; i++)
        if (name[i] >= 'A' && name[i] <= 'Z')
            name[i] |= 0x20;
    return shpi_find(bank, name);
}

static int shape_pixels(const unsigned char *s)
{
    return (int)(s[4] | s[5] << 8) * (int)(s[6] | s[7] << 8);
}

/* 0x5b270: a picture with colour 255 in it is drawn see-through (type 0x45), unless its
   name starts with '!' */
static void mark_see_through(unsigned char *bank)
{
    int n = (int)u32(bank + 8), i, k, m;
    for (i = 0; i < n; i++) {
        unsigned char *s = bank + u32(bank + 20 + 8 * i);
        m = shape_pixels(s);
        for (k = 0; k < m; k++)
            if (s[16 + k] == 0xff)
                break;
        if (k < m && bank[16 + 8 * i] != '!')
            s[0] = 0x45;
    }
}

/* 0x5b2f8: eight sizes, name[3] counting up; a missing one is the one before */
static void fill_sizes(unsigned char *bank, char *name, unsigned char **slot)
{
    int i;
    if (slot[0])
        return;
    for (i = 0; i < 8; i++, name[3]++) {
        unsigned char *s = find_any_case(bank, name);
        slot[i] = !s && i > 0 ? slot[i - 1] : s;
    }
}

/* a palette of 8 bits a channel (type 0x24) to the 6 the game keeps */
static void to_6_bits(unsigned char *bank, unsigned char *pal)
{
    char name[5] = "!pal";
    unsigned char *s = find_any_case(bank, name);
    int i;
    if (s && s[0] == 0x24)
        for (i = 0; i < 768; i++)
            pal[i] >>= 2;
}

void trackfam_free(void)
{
    int i;

    for (i = 0; i < g_fam.nmodels; i++)
        model_free(&g_fam.models[i]);
    free(g_fam.file);
    memset(&g_fam, 0, sizeof g_fam);
}

int trackfam_load(const char *name, int family, int kind)
{
    track_fam *f = &g_fam;
    char path[64], n[8];
    unsigned char *pal, *pics[ROADSIDE_PICTURES + 1], *banks[ROAD_TEXTURES];
    int count, i, k, q;

    trackfam_free();
    /* 0x60078: the mirrored and the rally's families are family 1's */
    if (family == 1 && kind != 2 && kind != 3)
        snprintf(path, sizeof path, "SIMDATA/%s/%s_001.fam",
                 g_german ? "GTRACKFM" : "ETRACKFM", name);
    else
        snprintf(path, sizeof path, "SIMDATA/NTRACKFM/%s_%c01.fam", name,
                 family != 1 ? 'T' : kind == 2 ? 'M' : 'R');
    f->file = file_load(path, &f->len);
    if (!f->file || ww_count(f->file) < 3) {
        fprintf(stderr, "renfs: %s is missing or not a track family\n", path);
        trackfam_free();
        return 0;
    }
    f->banks = ww_entry(f->file, 0);
    f->pictures = ww_entry(f->file, 1);
    f->horizon = ww_entry(f->file, 2);

    /* load_track_models 0x66118, first (0x6018c): entry 3's objects, each a model (its
       entry 0) and its pictures (entry 1) with their own palette "!PAL" (6 bits, or 8 as
       shape type 0x24), tinted and queued to be re-coloured (0x65224) */
    if (ww_count(f->file) > 3) {
        unsigned char *all = ww_entry(f->file, 3);

        for (i = 0; i < ww_count(all) && i < TRACK_MODELS; i++) {
            unsigned char *e = ww_entry(all, i), *orip, *bank, *sh;

            if (!e || ww_count(e) < 2)
                break;
            orip = ww_entry(e, 0);
            bank = ww_entry(e, 1);
            sh = shpi_find(bank, "!PAL");
            if (sh) {
                pal = sh + 16;
                if (sh[0] == 0x24)
                    for (k = 0; k < 0x300; k++)
                        pal[k] >>= 2;
                pal_tint(0, 0x100, pal);
                pal_queue_remap(bank, pal);
            }
            if (!model_make(&f->models[i], orip, f->file, f->file + f->len, bank))
                break;
            if (g_road_mirror)
                model_mirror(&f->models[i]);
            f->nmodels = i + 1;
        }
    }

    count = ww_count(f->banks);
    if (count == 1) {
        /* one bank: texture 3t + k is "<t><A+k>0" .. "<t><A+k>7" */
        unsigned char *bank = ww_entry(f->banks, 0);
        banks[0] = bank;
        mark_see_through(bank);
        strcpy(n, "!pal");
        pal = find_any_case(bank, n);
        if (!pal) {
            pal = shpi_find(bank, "01A0");
            pal += u32(pal) >> 8;                      /* its own palette, the next block */
        }
        f->road_pal = pal + 16;
        to_6_bits(bank, f->road_pal);
        for (i = 0; i < 0x30; i++)
            for (k = 0; k < 3; k++) {
                snprintf(n, sizeof n, "%02d%c0", i, 'A' + k);
                fill_sizes(bank, n, f->texture[3 * i + k]);
            }
        /* "ga<k>0": textures 3q + k replaced, q = the TRI head's +0x28 / 3 */
        q = g_track.head ? (short)(g_track.head[0x28] | g_track.head[0x29] << 8) / 3 : 0;
        for (k = 0; k < 3; k++) {
            snprintf(n, sizeof n, "ga%d0", k);
            if (!find_any_case(bank, n))
                continue;
            for (i = 0; i < 8; i++, n[3]++) {
                unsigned char *s = find_any_case(bank, n);
                f->texture[3 * q + k][i] = !s && i > 0 ? f->texture[3 * q + k][i - 1] : s;
            }
        }
    } else {
        /* a bank a texture triple: "A000".., "B000".., "C000".. */
        for (i = 0; i < count && i < ROAD_TEXTURES; i++) {
            banks[i] = ww_entry(f->banks, i);
            mark_see_through(banks[i]);
            for (k = 0; k < 3; k++) {
                snprintf(n, sizeof n, "%c000", 'A' + k);
                fill_sizes(banks[i], n, f->texture[3 * i + k]);
            }
        }
        strcpy(n, "!pal");
        f->road_pal = find_any_case(banks[0], n) + 16;
        to_6_bits(banks[0], f->road_pal);
    }

    /* the roadside pictures: "<i>00" of one bank, or "0000" of a bank each */
    if (ww_count(f->pictures) == 1) {
        unsigned char *bank = ww_entry(f->pictures, 0);
        pics[0] = bank;
        pics[1] = 0;
        mark_see_through(bank);
        for (i = 0; i < 0x40; i++) {
            snprintf(n, sizeof n, "%02d00", i);
            f->picture[i][0] = f->picture[i][1] = f->picture[i][2] = f->picture[i][3] =
                shpi_find(bank, n);
        }
    } else {
        for (i = 0; i < ww_count(f->pictures) && i < ROADSIDE_PICTURES; i++) {
            pics[i] = ww_entry(f->pictures, i);
            mark_see_through(pics[i]);
            f->picture[i][0] = f->picture[i][1] = f->picture[i][2] = f->picture[i][3] =
                shpi_find(pics[i], "0000");
        }
        pics[i] = 0;
    }

    pal_tint(0, 0x100, f->road_pal);
    pal_queue_piece(0x90, 0x60, f->road_pal);
    strcpy(n, "!pal");
    f->horizon_pal = find_any_case(f->horizon, n) + 16;
    to_6_bits(f->horizon, f->horizon_pal);
    pal_tint(0, 0x100, f->horizon_pal);
    pal_queue_remap(f->horizon, f->horizon_pal);
    for (i = 0; i < count; i++)
        pal_queue_remap(count == 1 ? banks[0] : banks[i], f->road_pal);
    for (i = 0; pics[i]; i++)
        pal_queue_remap(pics[i], f->road_pal);
    return 1;
}
