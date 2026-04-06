
#include <hw/sndsb/sndsb.h>

/* this MUST follow conio.h */
#define DOSLIB_REDEFINE_INP
#include <hw/cpu/liteio.h>

#if TARGET_MSDOS == 32 || (TARGET_MSDOS == 16 && !defined(__COMPACT__) && !defined(__SMALL__))
void sndsb_timer_tick_adlib(struct sndsb_ctx *cx) {
	outp(cx->oplio+1,cx->buffer_lin[cx->direct_dsp_io]>>4u);
	if (cx->backwards) {
		if (cx->direct_dsp_io == 0) cx->direct_dsp_io = cx->buffer_size - 1;
		else cx->direct_dsp_io--;
	}
	else {
		if ((++cx->direct_dsp_io) >= cx->buffer_size) cx->direct_dsp_io = 0;
	}
}
#endif

