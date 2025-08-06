
#include <sys/types.h>

#include "libbmp.h"

void libbmp_convert_scanline_32to24(struct BMPFILEREAD *bfr,unsigned char *src,unsigned int pixels) {
	unsigned char *d = (unsigned char*)src;
	uint32_t *s32 = (uint32_t*)src;

	while (pixels-- > 0u) {
		const uint32_t sw = *s32++;
		*d++ = ((sw >> (uint32_t)bfr->blue_shift) & 0xFFu);
		*d++ = ((sw >> (uint32_t)bfr->green_shift) & 0xFFu);
		*d++ = ((sw >> (uint32_t)bfr->red_shift) & 0xFFu);
	}
}

