
#include <hw/sndsb/sndsb.h>

signed char gallant_sc6600_map_to_dma[4] = {-1, 0, 1, 3};
signed char gallant_sc6600_map_to_irq[8] = {-1, 7, 9,10,11, 5,-1,-1};

int sndsb_read_sc400_config(struct sndsb_ctx *cx) {
	unsigned char a,b,c;

	if (!sndsb_write_dsp(cx,0x58))
		return 0;

	a = (unsigned char)sndsb_read_dsp(cx);
	if (a == 0xAA) a = (unsigned char)sndsb_read_dsp(cx);
	if (a == 0xFF) return 0;
	b = (unsigned char)sndsb_read_dsp(cx);
	c = (unsigned char)sndsb_read_dsp(cx);
	cx->is_gallant_sc6600 = 1;

	/* A: Unknown bits, and some bits that define the Windows Sound System base I/O and gameport enable.
	 * B: Unknown bits, and some bits that define the base I/O of the CD-ROM interface.
	 * C: DMA, IRQ, and MPU IRQ control bits */
	if (cx->dma8 < 0)
		cx->dma8 = gallant_sc6600_map_to_dma[c&3];
	if (cx->dma16 < 0)
		cx->dma16 = cx->dma8;
	if (cx->irq < 0)
		cx->irq = gallant_sc6600_map_to_irq[(c>>3)&7];

	/* SC400 cards also have a Windows Sound System interface on one of two base I/O ports */
	cx->wssio = (a & 0x10) ? 0xE80 : 0x530;
	/* and a game port */
	if (!(a & 2)) cx->gameio = 0x201;

	cx->do_not_probe_irq = 1;
	cx->do_not_probe_dma = 1;
	return 1;
}

