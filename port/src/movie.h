/* The movies' player. */
#ifndef RENFS_MOVIE_H
#define RENFS_MOVIE_H

/* movie_play 0x34ba0: FRONTEND/MOVIE/<name> at 320x240, 15 pictures a second, with its
   sound; 1 if Space, Enter or Esc stopped it, 0 at its end, -1 if it is missing. The
   screen is left in that mode, the palette black */
int movie_play(const char *name);

#endif
