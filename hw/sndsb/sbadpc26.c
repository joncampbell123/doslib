
#include <hw/sndsb/sndsb.h>

static const signed char scaleMap[40] = {
    0,  1,  2,  3,  0,  -1,  -2,  -3,
    1,  3,  5,  7, -1,  -3,  -5,  -7,
    2,  6, 10, 14, -2,  -6, -10, -14,
    4, 12, 20, 28, -4, -12, -20, -28,
    5, 15, 25, 35, -5, -15, -25, -35
};
static const signed char adjustMap[20] = {
    0, 0, 0, 1,
    -1, 0, 0, 1,
    -1, 0, 0, 1,
    -1, 0, 0, 1,
    -1, 0, 0, 0
};

/* NTS: This is the best documentation I could fine regarding the Sound Blaster ADPCM format.
 *      Tables and method taken from DOSBox 0.74 SB emulation. The information on multimedia.cx's
 *      Wiki is wrong. */
unsigned char sndsb_encode_adpcm_2_6bit(const unsigned char samp,const unsigned char b2) {
    signed int sdelta = (signed int)((signed char)(samp - sndsb_adpcm_pred));
    unsigned char sign = 0;

    sdelta = (sdelta * 2) + (sndsb_adpcm_step < 2 ? sndsb_adpcm_error : 0);
    sndsb_adpcm_error = sdelta & ((1 << (sndsb_adpcm_step + (b2 ? 2 : 1))) - 1);
    sdelta >>= sndsb_adpcm_step+1;

    if (sdelta < 0) {
        sdelta = -sdelta;
        sign = 4;
    }

    if (sdelta > 3) sdelta = 3;
    sdelta += sign;
    if (b2) sdelta &= 0x6;
    sndsb_adpcm_pred += scaleMap[(sndsb_adpcm_step*8)+sdelta];
    if (sndsb_adpcm_pred < 0) sndsb_adpcm_pred = 0;
    else if (sndsb_adpcm_pred > 0xFF) sndsb_adpcm_pred = 0xFF;
    sndsb_adpcm_step += adjustMap[(sndsb_adpcm_step*4)+(sdelta&3)];
    if ((signed char)sndsb_adpcm_step < 0) sndsb_adpcm_step = 0;
    if (sndsb_adpcm_step > 5) sndsb_adpcm_step = 5;
    return (unsigned char)sdelta;
}

