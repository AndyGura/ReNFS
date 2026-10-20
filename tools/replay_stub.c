/* What the checks that leave out replay.c, menu.c, joystick.c, racekeys.c and race.c
   still need (hud.c, crash.c, drive.c, config.c, skid.c refer to them): nothing
   recorded, no replay bar, no pointer, no joystick, the keys' detail at its first, no
   loading picture. Weak, so a check that links the real ones gets those. */
#include "joystick.h"
#include "replay.h"
#include "menu.h"
#include "race.h"
#include "racekeys.h"

__attribute__((weak)) replay_state g_replay;
__attribute__((weak)) replay_bar g_bar;
__attribute__((weak)) race_keys_state g_rkeys;
__attribute__((weak)) int g_race_slide = -1;

__attribute__((weak)) void replay_mark(int event)
{
    (void)event;
}

__attribute__((weak)) void fe_mouse_look(int pressed)
{
    (void)pressed;
}

__attribute__((weak)) joystick g_joy;

__attribute__((weak)) void joy_race_read(void)
{
}

__attribute__((weak)) int joy_menu_key(void)
{
    return 0;
}
