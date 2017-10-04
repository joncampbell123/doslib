
#include <hw/sndsb/sndsb.h>

/* NTS: This is the best documentation I could fine regarding the Sound Blaster ADPCM format.
 *      Tables and method taken from DOSBox 0.74 SB emulation. The information on multimedia.cx's
 *      Wiki is wrong. */
unsigned char sndsb_encode_adpcm_4bit(const unsigned char samp) {
    signed int sdelta = (signed int)((signed char)(samp - sndsb_adpcm_pred));
    unsigned char sign = 0;

    sdelta = (sdelta * 2) + (sndsb_adpcm_step < 3 ? sndsb_adpcm_error : 0);
    sndsb_adpcm_error = sdelta & ((1 << (sndsb_adpcm_step + 1)) - 1);
    sdelta >>= sndsb_adpcm_step+1;
    if (sdelta < 0) {
        sdelta = -sdelta;
        sign = 8;
    }
    if (sdelta > 7) sdelta = 7;
    sndsb_adpcm_pred += sndsb_adpcm_4bit_scalemap[(sndsb_adpcm_step*16)+sign+sdelta];
    if (sndsb_adpcm_pred < 0) sndsb_adpcm_pred = 0;
    else if (sndsb_adpcm_pred > 0xFF) sndsb_adpcm_pred = 0xFF;
    sndsb_adpcm_step += sndsb_adpcm_4bit_adjustmap[(sndsb_adpcm_step*8)+sdelta];
    if ((signed char)sndsb_adpcm_step < 0) sndsb_adpcm_step = 0;
    if (sndsb_adpcm_step > 3) sndsb_adpcm_step = 3;
    return (unsigned char)sdelta | sign;
}

