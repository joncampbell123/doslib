
#include <hw/sndsb/sndsb.h>

int sndsb_continue_autoinit_mode(struct sndsb_ctx * const cx) {
    if (!cx->dsp_playing) return 1;
    if (!cx->chose_autoinit_dsp) return 1;
    if (cx->is_gallant_sc6600) return 0; // Reveal SC400 cards do not support Continue auto-init DMA
    if (cx->buffer_hispeed && cx->hispeed_blocking) return 0; // DSP does not respond to commands in high-speed mode
    if (cx->ess_extensions && cx->dsp_play_method == SNDSB_DSPOUTMETHOD_3xx) return 0; // ESS audiodrive in extended mode does not support this
    if (cx->dsp_play_method < SNDSB_DSPOUTMETHOD_4xx) return 1; // SB16 at least

    /* Note these DSP commands are so little used even DOSBox doesn't implement them! */
    if (cx->buffer_16bit && !cx->ess_extensions)
        sndsb_write_dsp(cx,0x47); /* Continue auto-initialize DMA 16-bit */
    else
        sndsb_write_dsp(cx,0x45); /* Continue auto-initialize DMA 8-bit */

    return 1;
}

