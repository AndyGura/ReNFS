/* The game's own program, NFS_DOS.EXE on the CD, read for the fixed tables it holds (the
   road's levels of detail, the sine table, ...): they are the game's, so the port takes
   them from the player's copy at the addresses the original uses, and holds none itself. */
#ifndef RENFS_EXE_H
#define RENFS_EXE_H

/* NFS_DOS.EXE from the data folder into its flat layout (objects at their addresses, the
   relocations applied); 0 if it is missing or another version */
int exe_load(void);
/* len bytes at the original's address addr; 0 outside what the program holds */
const unsigned char *exe_at(unsigned addr, unsigned len);
/* a 32-bit word of the program, at addr */
int exe_i32(unsigned addr);

/* the sine table (0xf8448): a turn is 1024, 65536 = 1 */
int fix_sin(int a);
int fix_cos(int a);
/* EA's sine and cosine of a in 65536 a turn (0x80b00, 0x80b70), and the angle of
   (x, y) the same way (0x927b0): 16.16 */
int ea_sin(int a);
int ea_cos(int a);
int ea_atan2(int y, int x);
/* EA's other arctangent (0x8bb7c, the first argument pushed last): a turn of 1024, from
   the y axis towards x */
int ea_atan_1024(int x, int y);

#endif
