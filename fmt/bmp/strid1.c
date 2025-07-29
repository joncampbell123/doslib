
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>

#include "libbmp.h"

unsigned int bitmap_stride_from_bpp_and_w(unsigned int bpp,unsigned int w) {
	if (bpp == 15) bpp = 16;
	return (((w * bpp) + 31u) & (~31u)) >> 3u;
}

