#ifndef DOSLIB_LITEIO_H
#define DOSLIB_LITEIO_H

/* these macros are needed for Open Watcom at this time
 * only because of a weird unsigned int zero extension
 * bug with inp(), considering inp() does an 8-bit
 * unsigned char read.
 *
 * Bah! Fix your intrinsics! */

#include <stdint.h>

uint8_t inpb_x(const unsigned short port);
#pragma aux inpb_x = "in AL,DX" parm [dx] value [al];

uint16_t inpw_x(const unsigned short port);
#pragma aux inpw_x = "in AX,DX" parm [dx] value [ax];

uint32_t inpd_x(const unsigned short port);
#if TARGET_MSDOS == 32
# pragma aux inpd_x = "in EAX,DX" parm [dx] value [eax];
#else
# pragma aux inpd_x = ".386" "in EAX,DX" "mov EDX,EAX" "shr EDX,16" parm [dx] value [dx ax];
#endif

void outpb_x(const unsigned short port,const uint8_t v);
#pragma aux outpb_x = "out DX,AL" parm [dx] [al];

void outpw_x(const unsigned short port,const uint16_t v);
#pragma aux outpw_x = "out DX,AX" parm [dx] [ax];

void outpd_x(const unsigned short port,const uint32_t v);
#if TARGET_MSDOS == 32
# pragma aux outpd_x = "out DX,EAX" parm [dx] [eax];
#else
# pragma aux outpd_x = ".386" "shl EDX,16" "movzx EAX,AX" "add EAX,EDX" "mov DX,BX" "out DX,EAX" parm [bx] [dx ax];
#endif

/* instead of modifying all code, we just #define over the intrinsics */
#ifdef DOSLIB_REDEFINE_INP
# define inp(x) inpb_x(x)
# define inpw(x) inpw_x(x)
# define inpd(x) inpd_x(x)
# define outp(x,y) outpb_x(x,y)
# define outpw(x,y) outpw_x(x,y)
# define outpd(x,y) outpd_x(x,y)
#endif

#endif /* DOSLIB_LITEIO_H */

