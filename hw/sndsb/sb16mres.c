

#include <hw/sndsb/sndsb.h>

void sndsb_read_sb16_irqdma_resources(struct sndsb_ctx * const cx) {
	const unsigned char irqm = sndsb_read_mixer(cx,0x80);
	const unsigned char dmam = sndsb_read_mixer(cx,0x81);

	if (cx->irq < 0 && irqm != 0xFF && irqm != 0x00) {
		if (irqm & 8)		cx->irq = 10;
		else if (irqm & 4)	cx->irq = 7;
		else if (irqm & 2)	cx->irq = 5;
		else if (irqm & 1)	cx->irq = 2;

		cx->do_not_probe_irq = 1;
	}

    if (dmam != 0xFF && dmam != 0x00) {
		if (cx->dma8 < 0) {
			if (dmam & 8)		cx->dma8 = 3;
			else if (dmam & 2)	cx->dma8 = 1;
			else if (dmam & 1)	cx->dma8 = 0;
		}

		if (cx->dma16 < 0) {
			if (dmam & 0x80)	cx->dma16 = 7;
			else if (dmam & 0x40)	cx->dma16 = 6;
			else if (dmam & 0x20)	cx->dma16 = 5;
		}

		/* NTS: From the Creative programming guide:
		 *      "DSP version 4.xx also supports the transfer of 16-bit sound data through
		 *       8-bit DMA channel. To make this possible, set all 16-bit DMA channel bits
		 *       to 0 leaving only 8-bit DMA channel set" */
		if (cx->dma16 == -1)
			cx->dma16 = cx->dma8;

		cx->do_not_probe_dma = 1;
	}
}

