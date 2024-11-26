
typedef void (*draw_scanline_func_t)(unsigned int y,unsigned char *src,unsigned int pixels);

#ifdef PVGA1A
/* ET4K */
# define BANKSWITCH(bank) outpw(0x3CE,0x09 + ((bank) << 8u))
# define DSFUNC draw_scanline_pvga1a
# include "dr_bnklt.h"
# undef BANKSWITCH
# undef DSFUNC
#endif

