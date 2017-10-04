
#include <conio.h>

#include <hw/sndsb/sndsb.h>
#include <hw/8237/8237.h>		/* 8237 DMA */

void sndsb_timer_tick_goldi_cpy(struct sndsb_ctx *cx) {
	cx->timer_tick_signal = 1;
#if TARGET_MSDOS == 32
	memcpy(cx->buffer_lin+cx->direct_dsp_io,cx->goldplay_dma->lin,cx->gold_memcpy);
#else
	_fmemcpy(cx->buffer_lin+cx->direct_dsp_io,cx->goldplay_dma,cx->gold_memcpy);
#endif
	if (cx->backwards) {
		if (cx->direct_dsp_io < cx->gold_memcpy) cx->direct_dsp_io = cx->buffer_size - cx->gold_memcpy;
		else cx->direct_dsp_io -= cx->gold_memcpy;
	}
	else {
		if ((cx->direct_dsp_io += cx->gold_memcpy) >= cx->buffer_size) cx->direct_dsp_io = 0;
	}
}

void sndsb_timer_tick_goldo_cpy(struct sndsb_ctx *cx) {
	cx->timer_tick_signal = 1;
#if TARGET_MSDOS == 32
	memcpy(cx->goldplay_dma->lin,cx->buffer_lin+cx->direct_dsp_io,cx->gold_memcpy);
#else
	_fmemcpy(cx->goldplay_dma,cx->buffer_lin+cx->direct_dsp_io,cx->gold_memcpy);
#endif
	if (cx->backwards) {
		if (cx->direct_dsp_io < cx->gold_memcpy) cx->direct_dsp_io = cx->buffer_size - cx->gold_memcpy;
		else cx->direct_dsp_io -= cx->gold_memcpy;
	}
	else {
		if ((cx->direct_dsp_io += cx->gold_memcpy) >= cx->buffer_size) cx->direct_dsp_io = 0;
	}
}

