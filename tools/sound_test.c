/* For tools/opponent_emu.py: the race's sounds (racesnd.c, sndlib.c) set from the
   original's globals and voice records (0x160fa4) and read back the same way. racesnd.c
   is compiled in here, so that its banks can be loaded without starting the engine. */
#include "racesnd.c"
#include "view.h"

static race_camera cam;
static const unsigned char unknown[0x48];   /* a record not in the table (the music's) */

/* the globals, in opponent_emu.py's SOUND_VARS order */
int *sound_test_var(int k)
{
    static int *v[80];
    static int n;
    if (!n) {
        int *const a[] = {
            &g_rsnd.muted, &g_rsnd.engine, &g_rsnd.beep_on, &g_rsnd.beep_wait,
            &g_rsnd.road_pitch, &g_rsnd.revs, &g_rsnd.load, &g_rsnd.road_up,
            &g_rsnd.under_vol, &g_rsnd.under_pan, &g_rsnd.skid[0], &g_rsnd.skid[1],
            &g_rsnd.echo_now, &g_rsnd.horn_kind, &g_rsnd.horn_time, &g_rsnd.horn_car,
            &g_rsnd.siren_on, &g_rsnd.echo_wait, &g_rsnd.siren_node, &g_rsnd.siren_bend,
            &g_hud.radar, &g_hud.radar_flash, &g_snd.next,
        };
        for (size_t i = 0; i < sizeof a / sizeof a[0]; i++)
            v[n++] = a[i];
        for (int c = 0; c < SND_CHANNELS; c++)
            v[n++] = &g_rsnd.bend[c];
        for (int c = 0; c < SND_CHANNELS; c++) {
            v[n++] = &g_rsnd.voice[c];
            v[n++] = &g_rsnd.sound[c];
        }
    }
    return k >= 0 && k < n ? v[k] : 0;
}

/* the settings (0x12da53's record), the library ready, the banks loaded as 0x52774 does
   (without the engine started), no voice ever ending */
void sound_test_start(int sfx, int music, int mono, int car, int other, int track)
{
    rsnd_free();
    memset(&g_rsnd, 0, sizeof g_rsnd);
    g_rsnd.engine_chan = exe_i32(0xee352) >> 16;
    g_rsnd.present = 1;
    g_rsnd.sfx = sfx;
    g_rsnd.music = music;
    g_rsnd.mono = mono;
    snd_init(1);
    g_snd.keep = 1;
    {
        const char *hits = mono ? (track == 7 ? "COLLSBMT.BNK" : "COLLSBWT.BNK")
                                : (track == 7 ? "COLLSWMT.BNK" : "COLLSWWT.BNK");
        bank_load((const char *)exe_at((unsigned)exe_i32(0x524c0 + 4 * car), 16));
        bank_load((const char *)exe_at((unsigned)exe_i32(0x524f0 + 8 * other), 16));
        bank_load(hits);
        bank_load("NFS_FMmb.bnk");
    }
}

/* sound n's record (0x48 bytes) and its EACS header (0x1c), 0 if there is none */
int sound_test_slot(int n, unsigned char *rec, unsigned char *head)
{
    if (n < 0 || n >= SND_SOUNDS || !g_snd.slot[n])
        return 0;
    memcpy(rec, g_snd.slot[n], 0x48);
    if (snd_header(n))
        memcpy(head, snd_header(n), 0x1c);
    return 1;
}

static int slot_of(const unsigned char *rec)
{
    if (!rec)
        return -1;
    for (int n = 0; n < SND_SOUNDS; n++)
        if (g_snd.slot[n] == rec)
            return n;
    return -2;
}

/* voice v's record in the original's layout, its +0 the record's sound (-1 none, -2 one
   not in the table) */
void sound_test_voice(int v, unsigned char *raw, int set)
{
    snd_voice *s = &g_snd.v[v];
    int rec;
    if (set) {
        memcpy(&rec, raw, 4);
        s->rec = rec == -1 ? 0 : rec == -2 ? unknown : g_snd.slot[rec];
        memcpy(&s->f4, raw + 4, 4);
        s->flags = raw[0xa];
        s->kind = raw[0xb];
        memcpy(&s->cents, raw + 0xc, 4);
        s->bend = raw[0x10];
        s->reach = raw[0x11];
        s->index = raw[0x12];
        s->priority = raw[0x13];
        s->playing = raw[0x16];
        s->sound = raw[0x17];
        memcpy(&s->f18, raw + 0x18, 4);
        s->volume = (signed char)raw[0x24];
        s->rec_volume = (signed char)raw[0x25];
        s->level = (signed char)raw[0x26];
        s->pan = raw[0x27];
        return;
    }
    memset(raw, 0, 0x28);
    rec = slot_of(s->rec);
    memcpy(raw, &rec, 4);
    memcpy(raw + 4, &s->f4, 4);
    raw[0xa] = s->flags;
    raw[0xb] = s->kind;
    memcpy(raw + 0xc, &s->cents, 4);
    raw[0x10] = s->bend;
    raw[0x11] = s->reach;
    raw[0x12] = s->index;
    raw[0x13] = s->priority;
    raw[0x16] = s->playing;
    raw[0x17] = s->sound;
    memcpy(raw + 0x18, &s->f18, 4);
    raw[0x24] = (unsigned char)s->volume;
    raw[0x25] = (unsigned char)s->rec_volume;
    raw[0x26] = (unsigned char)s->level;
    raw[0x27] = s->pan;
}

/* voice v's frames a second (0xa062c), 0 if none worked out yet */
unsigned sound_test_freq(int v)
{
    return (unsigned)g_snd.v[v].freq;
}

/* the cars heard on channels 2 and 3 (0xf7790): car indices, -1 none */
int sound_test_near(int k, int car, int set)
{
    if (set) {
        g_rsnd.near[k] = car >= 0 ? g_cars[car] : 0;
        return car;
    }
    for (int i = 0; i < RACE_CARS_MAX; i++)
        if (g_rsnd.near[k] && g_cars[i] == g_rsnd.near[k])
            return i;
    return g_rsnd.near[k] ? -2 : -1;
}

/* the camera the sounds are heard from (0x129ecc) */
void sound_test_camera(int mode, int x, int y, int z, int node, int yaw, int car)
{
    cam.mode = mode;
    cam.pos.x = x;
    cam.pos.y = y;
    cam.pos.z = z;
    cam.node = node;
    cam.yaw = yaw;
    cam.car = g_cars[car];
    g_camera = &cam;
}

int sound_test_cam_yaw(void)
{
    return cam.yaw;
}
