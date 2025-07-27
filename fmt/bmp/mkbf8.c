
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>

#include "libbmp.h"

unsigned char bitmap_mkbf8(uint32_t w,const uint8_t fs,const uint8_t fw) {
	if (fw != 0u) {
		w >>= fs;
		w &= (1u << fw) - 1u;
		if (fw > 8u) w >>= (uint32_t)(fw - 8u); /* truncate to 8 bits if larger */
		if (fw < 8u) w = (w * 255u) / ((1u << fw) - 1u);
		return (unsigned char)w;
	}

	return 0;
}

