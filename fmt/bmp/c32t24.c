
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>

#include "libbmp.h"

void bitmap_memcpy32to24(unsigned char *d24,const unsigned char *s32raw,unsigned int w,const struct BMPFILEREAD *bfr) {
	const uint32_t *s32 = (const uint32_t*)s32raw;

	while (w-- > 0) {
		const uint32_t w = *s32++;
		*d24++ = bitmap_mkbf8(w,bfr->blue_shift, bfr->blue_width);
		*d24++ = bitmap_mkbf8(w,bfr->green_shift,bfr->green_width);
		*d24++ = bitmap_mkbf8(w,bfr->red_shift,  bfr->red_width);
	}
}

