
#include <hw/sndsb/sndsb.h>

int sndsb_halt_dma(struct sndsb_ctx *cx) {
	if (!cx->dsp_playing) return 1;
	if (cx->goldplay_mode) return 1;
	if (cx->buffer_hispeed && cx->hispeed_blocking) return 0; // DSP does not respond to commands in high-speed mode
	if (cx->dsp_play_method < SNDSB_DSPOUTMETHOD_1xx) return 1;
	if (cx->ess_extensions && cx->dsp_play_method == SNDSB_DSPOUTMETHOD_3xx) return 0; // ESS audiodrive in extended mode does not support this

	if (cx->buffer_16bit && !cx->ess_extensions && !cx->is_gallant_sc6600)
		sndsb_write_dsp(cx,0xD5); /* Halt DMA 16-bit */
	else
		sndsb_write_dsp(cx,0xD0); /* Halt DMA 8-bit */

	return 1;
}

int sndsb_continue_dma(struct sndsb_ctx *cx) {
	if (!cx->dsp_playing) return 1;
	if (cx->goldplay_mode) return 1;
	if (cx->is_gallant_sc6600) return 0; // Reveal SC400 cards do not support Continue DMA
	if (cx->buffer_hispeed && cx->hispeed_blocking) return 0; // DSP does not respond to commands in high-speed mode
	if (cx->dsp_play_method < SNDSB_DSPOUTMETHOD_1xx) return 1;
	if (cx->ess_extensions && cx->dsp_play_method == SNDSB_DSPOUTMETHOD_3xx) return 0; // ESS audiodrive in extended mode does not support this

	if (cx->buffer_16bit && !cx->ess_extensions && !cx->is_gallant_sc6600)
		sndsb_write_dsp(cx,0xD6); /* Continue DMA 16-bit */
	else
		sndsb_write_dsp(cx,0xD4); /* Continue DMA 8-bit */

	return 1;
}

