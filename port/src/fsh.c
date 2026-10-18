/* EA's shape files: "SHPI", u32 size, u32 count, 4-byte id, then count x (4-letter name,
   u32 offset). A shape is a chain of blocks: u8 type, u24 offset to the next block (0 =
   the last), u16 width, height, x centre, y centre, x, y, then its data. 0x7b = 8-bit
   pixels; 0x22 / 0x24 = a palette, 6 / 8 bits a channel. */
#include "fsh.h"
#include "files.h"
#include "unpack.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static unsigned u16(const unsigned char *p) { return p[0] | p[1] << 8; }
static unsigned long u32(const unsigned char *p) { return u16(p) | (unsigned long)u16(p + 2) << 16; }

int fsh_load(fsh_file *f, const char *rel)
{
    long len;
    unsigned char *d = file_load(rel, &len);
    f->d = 0;
    if (!d)
        return 0;
    if (is_packed(d, len)) {
        long n;
        unsigned char *u = unpack(d, len, &n);
        free(d);
        if (!u)
            return 0;
        d = u;
        len = n;
    }
    if (len < 16 || memcmp(d, "SHPI", 4) != 0) {
        free(d);
        return 0;
    }
    f->d = d;
    f->len = len;
    return 1;
}

void fsh_free(fsh_file *f)
{
    free(f->d);
    f->d = 0;
}

static int read_shape(const fsh_file *f, unsigned long off, shape *s)
{
    const unsigned char *b;
    if (off + 16 > (unsigned long)f->len)
        return 0;
    b = f->d + off;
    s->head = b;
    s->type = b[0];
    s->w = u16(b + 4);
    s->h = u16(b + 6);
    s->cx = u16(b + 8);
    s->cy = u16(b + 0xa);
    s->x = u16(b + 0xc);
    s->y = u16(b + 0xe);
    s->pixels = b + 16;
    if (s->type == 0x7b && off + 16 + (unsigned long)s->w * s->h > (unsigned long)f->len)
        return 0;
    return 1;
}

int fsh_find(const fsh_file *f, const char *name, shape *s)
{
    unsigned long count = u32(f->d + 8);
    for (unsigned long i = 0; i < count && 16 + i * 8 + 8 <= (unsigned long)f->len; i++) {
        const unsigned char *e = f->d + 16 + i * 8;
        if (strncasecmp((const char *)e, name, 4) == 0)
            return read_shape(f, u32(e + 4), s);
    }
    return 0;
}

/* a palette block into pal; 1 if it was one */
static int palette_block(const fsh_file *f, const unsigned char *b, unsigned char pal[768])
{
    int type = b[0], n = u16(b + 4);
    if (n > 256)
        n = 256;
    if ((type != 0x22 && type != 0x24) || b + 16 + 3 * n > f->d + f->len)
        return 0;
    memset(pal, 0, 768);
    for (int i = 0; i < 3 * n; i++)    /* 8 bits as they are; 6 bits x 4 */
        pal[i] = type == 0x24 ? b[16 + i] : (unsigned char)(b[16 + i] << 2);
    return 1;
}

int fsh_palette(const fsh_file *f, const shape *s, unsigned char pal[768])
{
    shape p;
    if (s) {                           /* a palette block attached to the picture */
        const unsigned char *b = s->head;
        unsigned long next;
        while ((next = b[1] | b[2] << 8 | (unsigned long)b[3] << 16) != 0) {
            b += next;
            if (b + 16 > f->d + f->len)
                break;
            if (palette_block(f, b, pal))
                return 1;
        }
    }
    return fsh_find(f, "!pal", &p) && palette_block(f, p.head, pal);
}
