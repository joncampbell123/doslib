

#include <hw/sndsb/sndsb.h>

struct sndsb_ctx *sndsb_by_base(uint16_t x) {
	unsigned int i;

	for (i=0;i < SNDSB_MAX_CARDS;i++) {
		if (sndsb_card[i].baseio == x)
			return sndsb_card+i;
	}

	return 0;
}

