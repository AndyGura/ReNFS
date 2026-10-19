/* EA's sound library as the race uses it (NOTES §4.18): the banks of sounds (BNK) put
   into one table of 128, and 16 voices handed out by each sound's mask and priority.
   A sound starts at a pan, a volume and a pitch bend, its record adding its own volume and
   pitch and, some of them, a spread drawn from EA's random numbers (the race's own,
   collide.c); the game then moves a voice's volume, pan and pitch as it plays. The voices
   are the original's records (0x160fa4), compared byte for byte by tools/sound_emu.py;
   the samples themselves go to the mixer (audio.c). */
#ifndef RENFS_SNDLIB_H
#define RENFS_SNDLIB_H

#define SND_VOICES 16
#define SND_SOUNDS 128
#define SND_BANKS 5

/* a voice (0x160fa4, 0x28 bytes a voice) */
typedef struct {
    const unsigned char *rec;  /* +0: the sound's record in its bank, 0 when free */
    int f4;                    /* +4: the record's +8 less 1, -1 when free */
    unsigned char flags;       /* +0xa: the record's +0x1c (1: one of a pair) */
    unsigned char kind;        /* +0xb: 1 a bank's sound, 2 a stream (the music) */
    int cents;                 /* +0xc: its pitch, 100 a semitone: the record's +0x10,
                                  give or take its +0xc at random */
    unsigned char bend;        /* +0x10: the pitch it was started at, 0x40 as it is */
    unsigned char reach;       /* +0x11: the record's +0x17: a bend of 0x40 moves it this
                                  many semitones */
    unsigned char index;       /* +0x12 */
    unsigned char priority;    /* +0x13: the record's +0x14 */
    unsigned char playing;     /* +0x16 */
    unsigned char sound;       /* +0x17: its number, 0xff none */
    int f18;                   /* +0x18 */
    signed char volume;        /* +0x24: the volume asked for, 0..127 */
    signed char rec_volume;    /* +0x25: the record's +0x19, give or take its +0x1a */
    signed char level;         /* +0x26: the master volume times both, / 127² */
    unsigned char pan;         /* +0x27: 0 left, 0x40 the middle, 0x7f right */
    /* the port's: how far it has played (frames, 32.32) and how fast, for its end */
    unsigned long long pos;
    unsigned long freq;        /* frames a second, as 0xa062c works it out */
} snd_voice;

typedef struct {
    snd_voice v[SND_VOICES];
    int next;                  /* 0xf973c: where the search for a free voice starts */
    int master;                /* 0xf99e6: 0..127 */
    int on;                    /* 0xf99d8 in 1..6: a sound card (else every call fails) */
    const unsigned char *slot[SND_SOUNDS]; /* 0x161224: each sound's record */
    const unsigned char *bank[SND_BANKS];  /* 0xf950c: the banks put in */
    long bank_len[SND_BANKS];
    unsigned last;             /* the clock's tick the voices were last moved on to */
    int keep;                  /* tests: no voice ever ends (the original in an emulator) */
} snd_lib;

extern snd_lib g_snd;

/* the library ready, no banks, every voice free (0x9eaae on each) */
void snd_init(int on);
/* 0x958f5: a bank's sounds into the table (the bank stays the caller's); its place, -1 if
   one of its sounds is there already (those before it stay), -9 no place left */
int snd_bank_add(const unsigned char *d, long len);
/* 0x959b4: bank b's sounds out of the table (-1 all) */
void snd_bank_remove(int b);
/* sound n's EACS header in its bank, 0 if none */
const unsigned char *snd_header(int n);
/* 0x95a7a: sound n started at pan (-1 the record's), volume and bend (0..0x7f); its voice,
   or for a pair the second's voice << 16 | the first's; < 0 if none */
int snd_play(int n, int pan, int volume, int bend);
/* 0x8169b, 0x8171b, 0x8179e: a playing voice's volume (0..0x7f), pan (0..0xff), pitch bend
   (0..0x7f); 0, or < 0 if the voice is not one */
int snd_volume(int v, int volume);
int snd_pan(int v, int pan);
int snd_bend(int v, int bend);
/* 0x8193c: 0 while voice v plays, 1 if it does not, -8 no voice */
int snd_idle(int v);
/* 0x81814: voice v stopped */
int snd_stop(int v);
/* the voices moved on to the clock's tick now (100 a second): those played to their end
   free again, as the mixer frees them */
void snd_time(unsigned now);
/* voice v held by a stream at this priority and volume (the race's music on voice 0,
   until the music is ported) */
void snd_hold(int v, int priority, int volume);

#endif
