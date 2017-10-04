
#include <hw/sndsb/sndsb.h>

const signed char sndsb_adpcm_2_6bit_scalemap[40] = {
    0,  1,  2,  3,  0,  -1,  -2,  -3,
    1,  3,  5,  7, -1,  -3,  -5,  -7,
    2,  6, 10, 14, -2,  -6, -10, -14,
    4, 12, 20, 28, -4, -12, -20, -28,
    5, 15, 25, 35, -5, -15, -25, -35
};

const signed char sndsb_adpcm_2_6bit_adjustmap[20] = {
     0, 0, 0, 1,
    -1, 0, 0, 1,
    -1, 0, 0, 1,
    -1, 0, 0, 1,
    -1, 0, 0, 0
};

