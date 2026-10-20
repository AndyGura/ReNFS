/* The game's files: the data folder is the CD's files as they are (NFSSE/); DOS names
   are upper case but the program asks for them in any case, so every part of a path is
   matched against the folder's entries without regard to case. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "files.h"
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

int g_german;

static char g_data[1024], g_home[1024];

const char *files_data(void) { return g_data; }

/* dir/name, with name's real case in dir; 0 if not there */
static int find_in(const char *dir, const char *name, char *out, size_t size)
{
    int n;
    DIR *d = opendir(dir);
    struct dirent *e;
    int found = 0;
    if (!d)
        return 0;
    while ((e = readdir(d))) {
        if (strcasecmp(e->d_name, name) == 0) {
            n = snprintf(out, size, "%s/%s", dir, e->d_name);
            found = n > 0 && (size_t)n < size;
            break;
        }
    }
    closedir(d);
    return found;
}

/* the real path of rel inside root */
static int resolve(const char *root, const char *rel, char *out, size_t size)
{
    char part[256], at[1024];
    snprintf(at, sizeof at, "%s", root);
    while (*rel) {
        size_t n = strcspn(rel, "/\\");
        if (n >= sizeof part)
            return 0;
        memcpy(part, rel, n);
        part[n] = 0;
        rel += n;
        while (*rel == '/' || *rel == '\\')
            rel++;
        if (n == 0)
            continue;
        if (!find_in(at, part, out, size))
            return 0;
        snprintf(at, sizeof at, "%s", out);
    }
    snprintf(out, size, "%s", at);
    return 1;
}

static int holds_game(const char *dir)
{
    char p[1024];
    return resolve(dir, "FRONTEND/ART/TITLE.QFS", p, sizeof p);
}

int files_init(const char *data_arg, const char *home_arg)
{
    const char *tries[4];
    char beside[1024] = "";
    int n = 0;
    ssize_t len;

    /* a folder asked for is that folder or none */
    int asked = data_arg || (getenv("RENFS_DATA") && *getenv("RENFS_DATA"));
    if (data_arg)
        tries[n++] = data_arg;
    else if (getenv("RENFS_DATA") && *getenv("RENFS_DATA"))
        tries[n++] = getenv("RENFS_DATA");
    else
        tries[n++] = "NFSSE";
    len = asked ? -1 : readlink("/proc/self/exe", beside, sizeof beside - 8);
    if (len > 0) {
        beside[len] = 0;
        char *slash = strrchr(beside, '/');
        if (slash) {
            strcpy(slash + 1, "NFSSE");
            tries[n++] = beside;
        }
    }
    g_data[0] = 0;
    for (int i = 0; i < n; i++) {
        if (holds_game(tries[i])) {
            snprintf(g_data, sizeof g_data, "%s", tries[i]);
            break;
        }
    }

    if (home_arg)
        snprintf(g_home, sizeof g_home, "%s", home_arg);
    else if (getenv("RENFS_HOME"))
        snprintf(g_home, sizeof g_home, "%s", getenv("RENFS_HOME"));
    else if (getenv("HOME"))
        snprintf(g_home, sizeof g_home, "%s/.config/renfs", getenv("HOME"));
    else
        snprintf(g_home, sizeof g_home, ".");

    if (getenv("RENFS_LANG") && strncasecmp(getenv("RENFS_LANG"), "de", 2) == 0)
        g_german = 1;
    return g_data[0] != 0;
}

const char *files_home(void)
{
    mkdir(g_home, 0755);           /* ~/.config exists on any desktop; one level is enough */
    return g_home;
}

int file_path(const char *rel, char *out, size_t size)
{
    return resolve(g_data, rel, out, size);
}

FILE *file_open(const char *rel)
{
    char path[1024];
    if (!resolve(g_data, rel, path, sizeof path))
        return 0;
    return fopen(path, "rb");
}

unsigned char *file_load(const char *rel, long *len)
{
    FILE *f = file_open(rel);
    unsigned char *d;
    long n;
    if (!f)
        return 0;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    d = malloc(n + 16);            /* a little more: readers may look a few bytes past */
    if (d && fread(d, 1, n, f) != (size_t)n) {
        free(d);
        d = 0;
    }
    fclose(f);
    if (d) {
        memset(d + n, 0, 16);
        if (len)
            *len = n;
    }
    return d;
}

const char *art_dir(void)
{
    return g_german ? "FRONTEND/GART" : "FRONTEND/ART";
}
