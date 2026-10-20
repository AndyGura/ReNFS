/* The audio screen (fe_audio_options 0x105dc). */
#ifndef RENFS_AUDIOOPT_H
#define RENFS_AUDIOOPT_H

/* where it is called from: the options screen, the race menus' options, the pause menu */
enum { AUDIO_OPTIONS, AUDIO_RACE_MENU, AUDIO_PAUSE };

/* the song, the three volumes, speech, the card's mode, 3D; 0 */
int fe_audio_options(int where);

#endif
