
#include <sys/types.h>

#include "libbmp.h"

void libbmp_convert_scanline_16_555to24(struct BMPFILEREAD *bfr,unsigned char *src,unsigned int pixels) {
	unsigned char *d = (unsigned char*)src;
	uint16_t *s16 = (uint16_t*)src;

	/* expansion, so, work from the end */
	s16 += pixels - 1u;
	d += (pixels - 1u) * 3u;

	while (pixels-- > 0u) {
		const uint16_t sw = *s16--;
		d[0] = ((sw >> (uint32_t)bfr->blue_shift) & 0x1Fu) << 3u;
		d[1] = ((sw >> (uint32_t)bfr->green_shift) & 0x1Fu) << 3u;
		d[2] = ((sw >> (uint32_t)bfr->red_shift) & 0x1Fu) << 3u;
		d -= 3;
	}
}

