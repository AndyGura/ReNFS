/* The screen, the clock and the keys. The picture is palette numbers, as the game's;
   it is shown through the palette at each vertical retrace, so a fade is a palette fade
   as on the VGA card. The VGA's DAC kept 6 bits a channel: so does this one.

   With --shot <file> <n> the program runs without a window and its clock is the
   retraces counted (60 a second at 640x480, 70 at 320x200): the n-th retrace's picture
   is written to the file (PPM) and the program ends. */
#include "video.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned char g_fb[FB_MAX_W * FB_MAX_H];
int g_fb_w = 640, g_fb_h = 480;
int g_quit;

static SDL_Window *g_win;
static SDL_Renderer *g_ren;
static SDL_Texture *g_tex;
static unsigned char g_dac[768];        /* 6 bits a channel, as the card's */
static unsigned g_rgb[256];

static const char *g_shot;
static int g_shot_at;
static unsigned long g_retraces;        /* counted, for the headless clock */
static unsigned long g_virtual_ms;      /* the headless clock, in ms x 70 x 60 */
static unsigned g_start_ms;

static int g_keys[16], g_nkeys;

int video_init(const char *shot, int shot_frames)
{
    g_shot = shot;
    g_shot_at = shot_frames;
    if (shot)
        SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "SDL: %s\n", SDL_GetError());
        return 0;
    }
    g_win = SDL_CreateWindow("ReNFS", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                             1280, 960, SDL_WINDOW_RESIZABLE);
    if (!g_win) {
        fprintf(stderr, "SDL: %s\n", SDL_GetError());
        return 0;
    }
    g_ren = SDL_CreateRenderer(g_win, -1, shot ? SDL_RENDERER_SOFTWARE : SDL_RENDERER_PRESENTVSYNC);
    if (!g_ren)
        g_ren = SDL_CreateRenderer(g_win, -1, 0);
    if (!g_ren) {
        fprintf(stderr, "SDL: %s\n", SDL_GetError());
        return 0;
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
    video_mode(640, 480);
    g_start_ms = SDL_GetTicks();
    return 1;
}

void video_done(void)
{
    if (g_tex)
        SDL_DestroyTexture(g_tex);
    if (g_ren)
        SDL_DestroyRenderer(g_ren);
    if (g_win)
        SDL_DestroyWindow(g_win);
    SDL_Quit();
}

void video_mode(int w, int h)
{
    g_fb_w = w;
    g_fb_h = h;
    memset(g_fb, 0, sizeof g_fb);
    if (g_tex)
        SDL_DestroyTexture(g_tex);
    g_tex = SDL_CreateTexture(g_ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, w, h);
    /* always 4:3, whatever the mode: 320x200 had tall pixels on a 4:3 screen */
    SDL_RenderSetLogicalSize(g_ren, 640, 480);
}

void video_palette(const unsigned char *pal, int first, int count)
{
    for (int i = first; i < first + count && i < 256; i++) {
        for (int c = 0; c < 3; c++)
            g_dac[3 * i + c] = pal[3 * (i - first) + c] >> 2;
        /* 6 bits to 8 as the screen saw them: the top bits repeated below */
        unsigned r = g_dac[3 * i], g = g_dac[3 * i + 1], b = g_dac[3 * i + 2];
        g_rgb[i] = 0xff000000u | (r << 2 | r >> 4) << 16 | (g << 2 | g >> 4) << 8 | (b << 2 | b >> 4);
    }
}

static void write_shot(void)
{
    FILE *f = fopen(g_shot, "wb");
    if (!f) {
        perror(g_shot);
        return;
    }
    fprintf(f, "P6\n%d %d\n255\n", g_fb_w, g_fb_h);
    for (int i = 0; i < g_fb_w * g_fb_h; i++) {
        unsigned c = g_rgb[g_fb[i]];
        unsigned char px[3] = { (unsigned char)(c >> 16), (unsigned char)(c >> 8), (unsigned char)c };
        fwrite(px, 1, 3, f);
    }
    fclose(f);
    printf("%s: frame %lu\n", g_shot, g_retraces);
}

static void poll(void)
{
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT)
            g_quit = 1;
        else if (e.type == SDL_KEYDOWN && !e.key.repeat && g_nkeys < 16) {
            SDL_Keycode k = e.key.keysym.sym;
            int c = 0;
            if (k == SDLK_ESCAPE)
                c = 0x1b;
            else if (k == SDLK_RETURN || k == SDLK_KP_ENTER)
                c = 0x0d;
            else if (k >= 0x20 && k < 0x7f)
                c = (int)k;
            if (c)
                g_keys[g_nkeys++] = c;
        }
    }
}

void video_retrace(void)
{
    void *pixels;
    int pitch;
    poll();
    if (SDL_LockTexture(g_tex, 0, &pixels, &pitch) == 0) {
        for (int y = 0; y < g_fb_h; y++) {
            unsigned *row = (unsigned *)((char *)pixels + y * pitch);
            const unsigned char *src = g_fb + y * g_fb_w;
            for (int x = 0; x < g_fb_w; x++)
                row[x] = g_rgb[src[x]];
        }
        SDL_UnlockTexture(g_tex);
    }
    SDL_RenderClear(g_ren);
    SDL_RenderCopy(g_ren, g_tex, 0, 0);
    SDL_RenderPresent(g_ren);
    g_retraces++;
    /* the VGA's rates: 60 a second at 640x480, 70 at 320x200 */
    g_virtual_ms += g_fb_h == 480 ? 70 : 60;
    if (g_shot) {
        if ((int)g_retraces >= g_shot_at) {
            write_shot();
            g_quit = 1;
            exit(0);
        }
        return;
    }
    /* keep the card's rate whatever the monitor's (a fade is so many retraces) */
    {
        static Uint64 next;
        Uint64 now = SDL_GetPerformanceCounter(), f = SDL_GetPerformanceFrequency();
        Uint64 step = f / (g_fb_h == 480 ? 60 : 70);
        if (!next || now > next + f / 10)
            next = now;
        next += step;
        while ((now = SDL_GetPerformanceCounter()) < next) {
            Uint64 left_ms = (next - now) * 1000 / f;
            if (left_ms > 1)
                SDL_Delay((Uint32)(left_ms - 1));
        }
    }
}

unsigned clock_ticks(void)
{
    if (g_shot)            /* g_virtual_ms counts 1/4200 s: 100 a second = 42 */
        return (unsigned)(g_virtual_ms / 42);
    return (SDL_GetTicks() - g_start_ms) / 10;
}

static unsigned g_timeout_at;

void settimeout(unsigned n)
{
    g_timeout_at = clock_ticks() + n;
}

int timedout(void)
{
    return (int)(clock_ticks() - g_timeout_at) >= 0;
}

int key_get(void)
{
    int c;
    poll();
    if (!g_nkeys)
        return 0;
    c = g_keys[0];
    memmove(g_keys, g_keys + 1, --g_nkeys * sizeof g_keys[0]);
    return c;
}
