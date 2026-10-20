/* The menus' sound: the effects of FRONT_MW.BNK and the music (FRONTEND/MUSIC). */
#ifndef RENFS_SOUND_H
#define RENFS_SOUND_H

/* 0x724c4 / 0x725ac: the effects' bank, loaded by each screen and freed as it ends */
void fe_sound_bank_load(void);
void fe_sound_bank_free(void);
/* fe_play_sound 0x72510: the bank's sound n (0x47 a move, 0x48 a choice) */
void fe_play_sound(int n);
/* 0x72578: sound 0x4a (the audio screen's test) heard to its end, or none; 0x72564 it
   stopped; 0x721c0 the effects' volume changed while it plays */
int fe_test_sound_done(void);
void fe_test_sound_stop(void);
void fe_sfx_volume(int v);

/* 0x72638, the menus' F5: every volume to 0 (the three kept) or back; then a third of a
   second's wait. Muted: fe_sound_kept(CONFIG.DAT offset) the volume kept. Before a race,
   a replay or the end the volumes are put back without being set (fe_sound_unmute) */
void fe_sound_mute(void);
int fe_sound_muted(void);
int fe_sound_kept(int at);
void fe_sound_unmute(void);

/* 0x63ed0: the menus' music by the setting CFG_MENU_MUSIC and a counter (0xed0fc) */
void music_choose(void);
/* 0x72010: FRONTEND/MUSIC/<name>, played and looped; fed at each retrace (0x71b6c) */
void music_start(const char *name);
/* 0x63d88: the race's music by the setting CFG_MENU_MUSIC: 1 any of the twelve, 2 a rock
   one, 3 a techno one (EA's random numbers, 0x52580), 4 .. 15 that one */
void race_music_start(void);
/* 0x72224 / 0x721ec: the music's volume, 0 .. 127 (the original moves to it over half a
   second, 0x8162a) */
void music_volume(int v);
/* the volume a new stream starts at: CONFIG.DAT's at (CFG_MENU_VOLUME the menus',
   CFG_MUSIC_VOLUME the race music's), -1 the stream's own (127, a race) */
void music_level_from(int at);
/* 0x7211c + 0x72150: the music stopped at once and let go */
void music_stop_now(void);
/* 0x722b4 + 0x72438: a spoken word from FRONTEND/SPEECH (GSPEECH in German), when the
   settings' +0x7b allows it, at the sounds' volume and a half */
void speech_say(const char *name);
/* (0x72454, 0x72490) the word stopped and let go */
void speech_stop(void);
/* 0x63e2c: the menus' music again after a race or one of its screens: by the setting
   CFG_MENU_MUSIC, any of the twelve, a rock or a techno one, from EA's random numbers */
void music_random(void);
/* 0x720d8: faded out over a second; 0x72150: waited for, then stopped */
void music_fade_out(void);
void music_wait_end(void);

#endif
