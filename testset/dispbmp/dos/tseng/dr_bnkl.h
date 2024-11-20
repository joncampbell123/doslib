
typedef void (*draw_scanline_func_t)(unsigned int y,unsigned char *src,unsigned int pixels);

/* ET3K */
#define BANKSWITCH(bank) outp(0x3CD,bank | (bank << 3u) | 0x40) /* [7:6] we want 64kb banks [5:3] read [2:0] write banks */
#define DSFUNC draw_scanline_et3k
#include "dr_bnklt.h"
#undef BANKSWITCH
#undef DSFUNC

#define BANKSWITCH(bank) outp(0x3CD,bank | (bank << 4u)) /* [7:4] read [3:0] write banks */
#define DSFUNC draw_scanline_et4k
#include "dr_bnklt.h"
#undef BANKSWITCH
#undef BANKSWITCH

