
#include <hw/sndsb/sndsb.h>

unsigned char sndsb_test_write_mixer(struct sndsb_ctx * const cx,const uint8_t i,const uint8_t d) {
	unsigned char o,c;

	o = sndsb_read_mixer(cx,i); sndsb_write_mixer(cx,i,d);
	c = sndsb_read_mixer(cx,i); sndsb_write_mixer(cx,i,o);

	return c;
}

int sndsb_reset_mixer(struct sndsb_ctx * const cx) {
	if (cx->baseio == 0)
		return 0;

	sndsb_write_mixer(cx,0x00,0x00);	/* "write any 8-bit value to reset the chip" */
	return 1;
}

/* NTS: If DOSBox's emulation is correct, 0xFF is not necessarily what is returned for
 *      unknown registers, therefore it's not an accurate way to probe the chipset.
 *      DOSBox for example seems to return 0x0A. */
int sndsb_probe_mixer(struct sndsb_ctx * const cx) {
	cx->mixer_probed = 1;
	cx->mixer_chip = 0;

	/* If we know it's an ESS AudioDrive, then we can assume it's mixer */
	if (cx->ess_chipset != 0) {
		cx->mixer_chip = SNDSB_MIXER_ESS688;
	}
	/* if there is a wider "master volume" control 0x30 then we're a CT1745 or higher */
	else if((sndsb_test_write_mixer(cx,0x30,0x08)&0xF8) == 0x08 &&
		(sndsb_test_write_mixer(cx,0x30,0x38)&0xF8) == 0x38 &&
		(sndsb_test_write_mixer(cx,0x31,0x08)&0xF8) == 0x08 &&
		(sndsb_test_write_mixer(cx,0x31,0x38)&0xF8) == 0x38) {
		cx->mixer_chip = SNDSB_MIXER_CT1745;
	}
	/* If there is a "master volume" control at 0x22 then we're at CT1345 or higher */
	else if((sndsb_test_write_mixer(cx,0x22,0x33)&0xEE) == 0x22 &&
		(sndsb_test_write_mixer(cx,0x22,0xCC)&0xEE) == 0xCC) {
		cx->mixer_chip = SNDSB_MIXER_CT1345;
	}
	/* hm, may be at CT1335 */
	else if((sndsb_test_write_mixer(cx,0x02,0x02)&0x0E) == 0x02 &&
		(sndsb_test_write_mixer(cx,0x02,0x0C)&0x0E) == 0x0C) {
		cx->mixer_chip = SNDSB_MIXER_CT1335;
	}

	cx->mixer_ok = (cx->mixer_chip != 0);
	return cx->mixer_ok;
}

