/* The game's files, found in the CD's folder whatever the case of their names. */
#ifndef RENFS_FILES_H
#define RENFS_FILES_H

#include <stdio.h>

/* the data folder: the argument, else $RENFS_DATA, else ./NFSSE, else NFSSE beside the
   program; 0 if none holds the game */
int files_init(const char *data_arg, const char *home_arg);
const char *files_data(void);
const char *files_home(void);      /* the port's own settings (made when first needed) */

/* a path inside the data folder, '/' or '\' between the parts, any case */
FILE *file_open(const char *rel);
/* the whole file, allocated (free it); 0 if it is not there */
unsigned char *file_load(const char *rel, long *len);

/* the language's folders: "ART" or "GART" .. (German has a G in front) */
extern int g_german;
const char *art_dir(void);         /* FRONTEND/ART or FRONTEND/GART */

#endif
