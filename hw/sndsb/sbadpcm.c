
#include <hw/sndsb/sndsb.h>

static int                  adpcm_pred = 128;
static signed char          adpcm_last = 0;
static unsigned char        adpcm_step = 0;
static unsigned char        adpcm_error = 0;
static unsigned char        adpcm_lim = 0;

const char* sndsb_adpcm_mode_str[4] = {
    "none",
    "ADPCM 4-bit",
    "ADPCM 2.6-bit",
    "ADPCM 2-bit"
};

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
    signed int sdelta = (signed int)((signed char)(samp - adpcm_pred));
    unsigned char sign = 0;

    sdelta = (sdelta * 2) + (adpcm_step < 3 ? adpcm_error : 0);
    adpcm_error = sdelta & ((1 << (adpcm_step + 1)) - 1);
    sdelta >>= adpcm_step+1;
    if (sdelta < 0) {
        sdelta = -sdelta;
        sign = 8;
    }
    if (sdelta > 7) sdelta = 7;
    adpcm_pred += scaleMap[(adpcm_step*16)+sign+sdelta];
    if (adpcm_pred < 0) adpcm_pred = 0;
    else if (adpcm_pred > 0xFF) adpcm_pred = 0xFF;
    adpcm_step += adjustMap[(adpcm_step*8)+sdelta];
    if ((signed char)adpcm_step < 0) adpcm_step = 0;
    if (adpcm_step > 3) adpcm_step = 3;
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
    signed int sdelta = (signed int)((signed char)(samp - adpcm_pred));
    unsigned char sign = 0;

    sdelta = (sdelta * 2) + (adpcm_step == 0 ? adpcm_error : 0);
    adpcm_error = sdelta & ((1 << adpcm_step) - 1);
    sdelta >>= adpcm_step+1;

    if (sdelta < 0) {
        sdelta = -sdelta;
        sign = 2;
    }

    /* "ring" suppression */
    if (adpcm_step == 5 && sdelta == 1 && adpcm_last == 3 && sign == 0)
        sdelta = 0;

    if (sdelta > 1) sdelta = 1;
    adpcm_last = sdelta + sign;
    adpcm_pred += scaleMap[(adpcm_step*4)+sign+sdelta];
    if (adpcm_pred < 0) adpcm_pred = 0;
    else if (adpcm_pred > 0xFF) adpcm_pred = 0xFF;
    adpcm_step += adjustMap[(adpcm_step*2)+sdelta];
    if ((signed char)adpcm_step < 0) adpcm_step = 0;
    if (adpcm_step > 5) adpcm_step = 5;
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
    signed int sdelta = (signed int)((signed char)(samp - adpcm_pred));
    unsigned char sign = 0;

    sdelta = (sdelta * 2) + (adpcm_step < 2 ? adpcm_error : 0);
    adpcm_error = sdelta & ((1 << (adpcm_step + (b2 ? 2 : 1))) - 1);
    sdelta >>= adpcm_step+1;

    if (sdelta < 0) {
        sdelta = -sdelta;
        sign = 4;
    }

    if (sdelta > 3) sdelta = 3;
    sdelta += sign;
    if (b2) sdelta &= 0x6;
    adpcm_pred += scaleMap[(adpcm_step*8)+sdelta];
    if (adpcm_pred < 0) adpcm_pred = 0;
    else if (adpcm_pred > 0xFF) adpcm_pred = 0xFF;
    adpcm_step += adjustMap[(adpcm_step*4)+(sdelta&3)];
    if ((signed char)adpcm_step < 0) adpcm_step = 0;
    if (adpcm_step > 5) adpcm_step = 5;
    return (unsigned char)sdelta;
}

void sndsb_encode_adpcm_set_reference(const unsigned char c,const unsigned char mode) {
    adpcm_pred = c;
    adpcm_step = 0;
    if (mode == ADPCM_4BIT)
        adpcm_lim = 5;
    else if (mode == ADPCM_2_6BIT)
        adpcm_lim = 3;
    else if (mode == ADPCM_2BIT)
        adpcm_lim = 1;
}

/* undocumented and not properly emulated by DOSBox either:
   when Creative said the non-reference ADPCM commands "continue
   using accumulated reference byte" they apparently meant that
   it resets the step value to max. Yes, even in auto-init
   ADPCM mode. Failure to follow this results in audible
   "fluttering" once per IRQ. */
void sndsb_encode_adpcm_reset_wo_ref(const unsigned char mode) {
    if (mode == ADPCM_4BIT)
        adpcm_step = 3;
    else if (mode == ADPCM_2_6BIT)
        adpcm_step = 4;
    else
        adpcm_step = 5; /* FIXME: Testing by ear seems to favor this one. Is this correct? */
}

