/* The cars' showcase (fe_showcase 0x17724). */
#ifndef RENFS_SHOWCASE_H
#define RENFS_SHOWCASE_H

/* the player's car's showcase (CONFIG.DAT +0x9b, 0..7): its pages, its film and its
   commentary; the next and previous cars change the car (in a tournament inside the
   track's class). Called with the showcase's music playing and the screen faded */
int fe_showcase(void);

#endif
