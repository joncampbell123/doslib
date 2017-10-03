
#include <hw/sndsb/sndsb.h>

int sndsb_exit_autoinit_mode(struct sndsb_ctx *cx) {
	if (!cx->dsp_playing) return 1;
	if (!cx->chose_autoinit_dsp) return 1;
	if (cx->buffer_hispeed && cx->hispeed_blocking) return 0; // DSP does not respond to commands in high-speed mode
	if (cx->ess_extensions && cx->dsp_play_method == SNDSB_DSPOUTMETHOD_3xx) return 0; // ESS audiodrive in extended mode does not support this
	if (cx->dsp_play_method < SNDSB_DSPOUTMETHOD_1xx) return 1;

	if (cx->buffer_16bit && !cx->ess_extensions && !cx->is_gallant_sc6600)
		sndsb_write_dsp(cx,0xD9); /* Exit auto-initialize DMA 16-bit */
	else
		sndsb_write_dsp(cx,0xDA); /* Exit auto-initialize DMA 8-bit */

	return 1;
}

