
#include <hw/sndsb/sndsb.h>

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
    sndsb_adpcm_pred += sndsb_adpcm_2_6bit_scalemap[(sndsb_adpcm_step*8)+sdelta];
    if (sndsb_adpcm_pred < 0) sndsb_adpcm_pred = 0;
    else if (sndsb_adpcm_pred > 0xFF) sndsb_adpcm_pred = 0xFF;
    sndsb_adpcm_step += sndsb_adpcm_2_6bit_adjustmap[(sndsb_adpcm_step*4)+(sdelta&3)];
    if ((signed char)sndsb_adpcm_step < 0) sndsb_adpcm_step = 0;
    if (sndsb_adpcm_step > 4) sndsb_adpcm_step = 4;
    return (unsigned char)sdelta;
}

