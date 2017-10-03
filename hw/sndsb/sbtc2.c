
#include <hw/sndsb/sndsb.h>

unsigned long sndsb_real_sample_rate(struct sndsb_ctx *cx) {
	unsigned long total_rate;
	unsigned char timeconst;
	unsigned long real_rate;

	total_rate = (unsigned long)cx->buffer_rate * (cx->buffer_stereo ? 2UL : 1UL);
	if (total_rate < 4000UL) total_rate = 4000UL;
	timeconst = (unsigned char)((65536UL - (256000000UL / total_rate)) >> 8UL);
	if (cx->dsp_play_method == SNDSB_DSPOUTMETHOD_4xx) return cx->buffer_rate;
	if (cx->dsp_play_method == SNDSB_DSPOUTMETHOD_DIRECT) return cx->buffer_rate;

	/* 256 - (1000000 / rate) = const
	 * -(1000000 / rate) = const - 256
	 * 1000000 / rate = -(const - 256)
	 * 1000000 / rate = -const + 256
	 * 1000000 = (-const + 256) * rate
	 * 1000000 / (-const + 256) = rate
	 * 1000000 / (256 - const) = rate */
	real_rate = 1000000UL / (unsigned long)(256 - timeconst);
	if (cx->buffer_stereo) real_rate /= 2UL;
	return real_rate;
}

