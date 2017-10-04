
#include <hw/sndsb/sndsb.h>

int                     sndsb_adpcm_pred = 128;
signed char             sndsb_adpcm_last = 0;
unsigned char           sndsb_adpcm_step = 0;
unsigned char           sndsb_adpcm_error = 0;
unsigned char           sndsb_adpcm_lim = 0;

/* NTS: This is the best documentation I could fine regarding the Sound Blaster ADPCM format.
 *      Tables and method taken from DOSBox 0.74 SB emulation. The information on multimedia.cx's
 *      Wiki is wrong. */
unsigned char sndsb_encode_adpcm_4bit(const unsigned char samp) {
    static const signed char scaleMap[64] = {
        0,  1,  2,  3,  4,  5,  6,  7,  0,  -1,  -2,  -3,  -4,  -5,  -6,  -7,
        1,  3,  5,  7,  9, 11, 13, 15, -1,  -3,  -5,  -7,  -9, -11, -13, -15,
        2,  6, 10, 14, 18, 22, 26, 30, -2,  -6, -10, -14, -18, -22, -26, -30,
        4, 12, 20, 28, 36, 44, 52, 60, -4, -12, -20, -28, -36, -44, -52, -60
    };
    static const signed char adjustMap[32] = {
         0, 0, 0, 0, 0, 1, 1, 1,
        -1, 0, 0, 0, 0, 1, 1, 1,
        -1, 0, 0, 0, 0, 1, 1, 1,
        -1, 0, 0, 0, 0, 0, 0, 0
    };
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
    sndsb_adpcm_pred += scaleMap[(sndsb_adpcm_step*16)+sign+sdelta];
    if (sndsb_adpcm_pred < 0) sndsb_adpcm_pred = 0;
    else if (sndsb_adpcm_pred > 0xFF) sndsb_adpcm_pred = 0xFF;
    sndsb_adpcm_step += adjustMap[(sndsb_adpcm_step*8)+sdelta];
    if ((signed char)sndsb_adpcm_step < 0) sndsb_adpcm_step = 0;
    if (sndsb_adpcm_step > 3) sndsb_adpcm_step = 3;
    return (unsigned char)sdelta | sign;
}

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

/* NTS: This is the best documentation I could fine regarding the Sound Blaster ADPCM format.
 *      Tables and method taken from DOSBox 0.74 SB emulation. The information on multimedia.cx's
 *      Wiki is wrong. */
unsigned char sndsb_encode_adpcm_2_6bit(const unsigned char samp,const unsigned char b2) {
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

void sndsb_encode_adpcm_set_reference(const unsigned char c,const unsigned char mode) {
    sndsb_adpcm_pred = c;
    sndsb_adpcm_step = 0;
    if (mode == ADPCM_4BIT)
        sndsb_adpcm_lim = 5;
    else if (mode == ADPCM_2_6BIT)
        sndsb_adpcm_lim = 3;
    else if (mode == ADPCM_2BIT)
        sndsb_adpcm_lim = 1;
}

/* undocumented and not properly emulated by DOSBox either:
   when Creative said the non-reference ADPCM commands "continue
   using accumulated reference byte" they apparently meant that
   it resets the step value to max. Yes, even in auto-init
   ADPCM mode. Failure to follow this results in audible
   "fluttering" once per IRQ. */
void sndsb_encode_adpcm_reset_wo_ref(const unsigned char mode) {
    if (mode == ADPCM_4BIT)
        sndsb_adpcm_step = 3;
    else if (mode == ADPCM_2_6BIT)
        sndsb_adpcm_step = 4;
    else
        sndsb_adpcm_step = 5; /* FIXME: Testing by ear seems to favor this one. Is this correct? */
}

