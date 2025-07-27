
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>

#include "libbmp.h"

void bitmap_mask2shift(uint32_t mask,uint8_t *shift,uint8_t *width) {
	if (mask != 0) {
		uint8_t c;

		c = 0;
		while ((mask&(uint32_t)1ul) == (uint32_t)0ul) {
			mask >>= (uint32_t)1ul;
			c++;
		}
		*shift = c;

		c = 0;
		while (mask&(uint32_t)1ul) {
			mask >>= (uint32_t)1ul;
			c++;
		}
		*width = c;
	}
	else {
		*shift = *width = 0;
	}
}

