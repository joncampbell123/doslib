
#include <hw/sndsb/sndsb.h>

int                     sndsb_adpcm_pred = 128;
signed char             sndsb_adpcm_last = 0;
unsigned char           sndsb_adpcm_step = 0;
unsigned char           sndsb_adpcm_error = 0;
unsigned char           sndsb_adpcm_lim = 0;

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

