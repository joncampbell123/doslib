
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

void sndsb_timer_tick_adlib6(struct sndsb_ctx *cx) {
	outp(cx->oplio+1,sndsb_adlib6_lookup[cx->buffer_lin[cx->direct_dsp_io]]);
	if (cx->backwards) {
		if (cx->direct_dsp_io == 0) cx->direct_dsp_io = cx->buffer_size - 1;
		else cx->direct_dsp_io--;
	}
	else {
		if ((++cx->direct_dsp_io) >= cx->buffer_size) cx->direct_dsp_io = 0;
	}
}

const unsigned char sndsb_adlib6_lookup[256] = {
	63,55,51,47,45,43,41,39,38,37,36,35,34,33,32,32,
	31,30,30,29,28,28,27,27,26,26,26,25,25,24,24,24,
	23,23,23,22,22,22,21,21,21,21,20,20,20,20,19,19,
	19,19,18,18,18,18,17,17,17,17,17,16,16,16,16,16,
	16,15,15,15,15,15,15,14,14,14,14,14,14,14,13,13,
	13,13,13,13,13,12,12,12,12,12,12,12,12,11,11,11,
	11,11,11,11,11,10,10,10,10,10,10,10,10,10,9,9,
	9,9,9,9,9,9,9,9,9,8,8,8,8,8,8,8,
	8,8,8,8,7,7,7,7,7,7,7,7,7,7,7,7,
	6,6,6,6,6,6,6,6,6,6,6,6,6,5,5,5,
	5,5,5,5,5,5,5,5,5,5,5,5,4,4,4,4,
	4,4,4,4,4,4,4,4,4,4,4,4,3,3,3,3,
	3,3,3,3,3,3,3,3,3,3,3,3,3,2,2,2,
	2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,
};
#endif

