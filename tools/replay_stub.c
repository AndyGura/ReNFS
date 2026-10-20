/* What the checks that leave out replay.c and menu.c still need (hud.c, crash.c refer
   to them): nothing recorded, no replay bar, no pointer. Weak, so a check that links
   the real ones gets those. */
#include "replay.h"
#include "menu.h"

__attribute__((weak)) replay_state g_replay;
__attribute__((weak)) replay_bar g_bar;

__attribute__((weak)) void replay_mark(int event)
{
    (void)event;
}

__attribute__((weak)) void fe_mouse_look(int pressed)
{
    (void)pressed;
}
