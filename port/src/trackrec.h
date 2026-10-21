/* The track's screen from Control Central (fe_track_screen 0x76380) and its Best Times
   (fe_track_best 0x75288). */
#ifndef RENFS_TRACKREC_H
#define RENFS_TRACKREC_H

/* the track, its segment shown, its laps and time of day chosen; Esc puts the choices
   back as they were */
int fe_track_screen(void);

#endif
