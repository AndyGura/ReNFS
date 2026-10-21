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
/* the real path of a file or folder inside the data folder; 0 if it is not there */
int file_path(const char *rel, char *out, size_t size);
/* the whole file, allocated (free it); 0 if it is not there */
unsigned char *file_load(const char *rel, long *len);

/* renfs.cfg in the home folder, the port's own settings ("<name> <number>" a line, '#'
   comments): one read (dflt if it has none), one written in place (the other lines kept);
   files_cfg_init writes the file with every setting explained if there is none */
int files_cfg_get(const char *name, int dflt);
void files_cfg_set(const char *name, int value);
void files_cfg_init(void);

/* a reason the program cannot go on: on the terminal and in a message window (Windows
   has no terminal; nothing shown without a window, under --shot's dummy driver) */
void files_error(const char *text);

/* the language's folders: "ART" or "GART" .. (German has a G in front) */
extern int g_german;
const char *art_dir(void);         /* FRONTEND/ART or FRONTEND/GART */

#endif
