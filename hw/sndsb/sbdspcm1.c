
#include <hw/sndsb/sndsb.h>

int sndsb_write_dsp_timeconst(struct sndsb_ctx * const cx,const uint8_t tc) {
	if (!sndsb_write_dsp(cx,0x40))
		return 0;
	if (!sndsb_write_dsp(cx,tc))
		return 0;
	return 1;
}

int sndsb_write_dsp_blocksize(struct sndsb_ctx * const cx,const uint16_t tc) {
	if (!sndsb_write_dsp(cx,0x48))
		return 0;
	if (!sndsb_write_dsp(cx,tc-1))
		return 0;
	if (!sndsb_write_dsp(cx,(tc-1)>>8))
		return 0;
	return 1;
}

int sndsb_write_dsp_outrate(struct sndsb_ctx * const cx,const unsigned long rate) {
	if (!sndsb_write_dsp(cx,0x41))
		return 0;
	if (!sndsb_write_dsp(cx,rate>>8)) /* Ugh, Creative, be consistent! */
		return 0;
	if (!sndsb_write_dsp(cx,rate))
		return 0;
	return 1;
}

