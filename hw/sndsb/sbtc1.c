
#include <hw/sndsb/sndsb.h>

unsigned char sndsb_rate_to_time_constant(struct sndsb_ctx *cx,unsigned long rate) {
	if (rate < 4000UL) rate = 4000UL;
	return (unsigned char)((65536UL - (256000000UL / rate)) >> 8);
}

