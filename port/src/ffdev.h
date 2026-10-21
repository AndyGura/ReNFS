/* The force feedback's device (ff.c, NOTES §4.26): the original's I-Force commands
   played on what is attached. A wheel or stick with force feedback (SDL's haptics):
   kicks and the steady push as constant forces, the shakes and the rumble as sine waves.
   Else a pad's or joystick's rumble (SDL_JoystickRumble, the MiSTer's too): kicks and
   the crash's rumble on the strong motor, the road's shakes on the weak one, the steady
   push left out. renfs.cfg's "forcefeedback <0..100>" (RENFS_FF) sets the strength, 0
   none; RENFS_FFLOG=f writes every command with the race's step. */
#ifndef RENFS_FFDEV_H
#define RENFS_FFDEV_H

/* the joystick the game reads (an SDL_Joystick, 0 none): video.c's joy_sdl, set by it */
extern void *(*g_ffdev_joystick)(void);

/* a device looked for (or the log asked for): 1 if the commands go somewhere */
int ffdev_open(void);
void ffdev_close(void);
/* one command: 0xa1 kick (across, along, length / 10 ms), 0xa4 / 0xa5 the two shakes
   (two sizes, a rate; 0, 0, 0 stops one), 0xa6 rumble (size), 0xa7 steady push (across,
   along), 0x82 every effect stopped, 0x83 the forces on, 0x84 held off; sizes in percent,
   signed bytes */
void ffdev_send(int cmd, const unsigned char *b, int n);
/* each frame: a pad's motors kept going, a kick's end */
void ffdev_tick(void);

/* a test's record of the commands sent (tools/ff_emu.py): cmd, n, the bytes, each */
extern unsigned char *g_ffdev_rec;
extern int g_ffdev_rec_len, g_ffdev_rec_cap;

#endif
