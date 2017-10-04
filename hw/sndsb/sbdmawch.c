
#include <hw/sndsb/sndsb.h>

signed char sndsb_dsp_playback_will_use_dma_channel(struct sndsb_ctx * const cx,const unsigned long rate,const unsigned char stereo,const unsigned char bit16) {
    if (cx->dsp_play_method == SNDSB_DSPOUTMETHOD_DIRECT)
        return -1;
    if (cx->dsp_play_method == SNDSB_DSPOUTMETHOD_4xx) {
        if (bit16) return cx->dma16;
    }

    return cx->dma8;
}

