
#include <hw/sndsb/sndsb.h>

/* NTS: This is the best documentation I could fine regarding the Sound Blaster ADPCM format.
 *      Tables and method taken from DOSBox 0.74 SB emulation. The information on multimedia.cx's
 *      Wiki is wrong. */
unsigned char sndsb_encode_adpcm_2bit(const unsigned char samp) {
    static const signed char scaleMap[24] = {
        0,  1,  0,  -1,  1,  3,  -1,  -3,
        2,  6, -2,  -6,  4, 12,  -4, -12,
        8, 24, -8, -24, 16, 48, -16, -48
/* NTS: This table is correct as tested against real Creative SB
        hardware. DOSBox's version has a typo on the last row
    that will make the 2-bit playback sound WORSE in it. */
    };
    static const signed char adjustMap[12] = {
         0, 1, -1, 1,
        -1, 1, -1, 1,
        -1, 1, -1, 0
    };
    signed int sdelta = (signed int)((signed char)(samp - sndsb_adpcm_pred));
    unsigned char sign = 0;

    sdelta = (sdelta * 2) + (sndsb_adpcm_step == 0 ? sndsb_adpcm_error : 0);
    sndsb_adpcm_error = sdelta & ((1 << sndsb_adpcm_step) - 1);
    sdelta >>= sndsb_adpcm_step+1;

    if (sdelta < 0) {
        sdelta = -sdelta;
        sign = 2;
    }

    /* "ring" suppression */
    if (sndsb_adpcm_step == 5 && sdelta == 1 && sndsb_adpcm_last == 3 && sign == 0)
        sdelta = 0;

    if (sdelta > 1) sdelta = 1;
    sndsb_adpcm_last = sdelta + sign;
    sndsb_adpcm_pred += scaleMap[(sndsb_adpcm_step*4)+sign+sdelta];
    if (sndsb_adpcm_pred < 0) sndsb_adpcm_pred = 0;
    else if (sndsb_adpcm_pred > 0xFF) sndsb_adpcm_pred = 0xFF;
    sndsb_adpcm_step += adjustMap[(sndsb_adpcm_step*2)+sdelta];
    if ((signed char)sndsb_adpcm_step < 0) sndsb_adpcm_step = 0;
    if (sndsb_adpcm_step > 5) sndsb_adpcm_step = 5;
    return (unsigned char)sdelta | sign;
}

