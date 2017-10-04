
#include <hw/sndsb/sndsb.h>

/* NTS: This table is correct as tested against real Creative SB
   hardware. DOSBox's version has a typo on the last row
   that will make the 2-bit playback sound WORSE in it. */
const signed char sndsb_adpcm_2bit_scalemap[24] = {
    0,  1,  0,  -1,  1,  3,  -1,  -3,
    2,  6, -2,  -6,  4, 12,  -4, -12,
    8, 24, -8, -24, 16, 48, -16, -48
};

const signed char sndsb_adpcm_2bit_adjustmap[12] = {
     0, 1, -1, 1,
    -1, 1, -1, 1,
    -1, 1, -1, 0
};

