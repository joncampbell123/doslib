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

#if TARGET_MSDOS == 32
uint32_t inpd_x(const unsigned short port);
# pragma aux inpd_x = "in EAX,DX" parm [dx] value [eax];
#endif

/* instead of modifying all code, we just #define over the intrinsics */
#ifdef DOSLIB_REDEFINE_INP
# define inp(x) inpb_x(x)
# define inpw(x) inpw_x(x)
# if TARGET_MSDOS == 32
#  define inpd(x) inpd_x(x)
# endif
#endif

