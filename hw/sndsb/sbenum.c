
#include <hw/sndsb/sndsb.h>

struct sndsb_ctx *sndsb_by_irq(const int8_t x) {
	unsigned int i;

	for (i=0;i < SNDSB_MAX_CARDS;i++) {
		if (sndsb_card[i].irq == x)
			return sndsb_card+i;
	}

	return 0;
}

struct sndsb_ctx *sndsb_by_dma(const int8_t x) {
	unsigned int i;

	for (i=0;i < SNDSB_MAX_CARDS;i++) {
		if (sndsb_card[i].baseio > 0 && (sndsb_card[i].dma8 == x || sndsb_card[i].dma16 == x))
			return sndsb_card+i;
	}

	return 0;
}

