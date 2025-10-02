
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <math.h>

#include "cc.hpp"

void utf16_to_str(uint16_t* &w,uint16_t *f,unicode_char_t c) {
	if (c < unicode_char_t(0)) {
		/* do nothing */
	}
	else if (c >= 0x10000l) {
		/* surrogate pair */
		if ((w+2) <= f) {
			*w++ = (uint16_t)((((c - 0x10000l) >> 10l) & 0x3FFl) + 0xD800l);
			*w++ = (uint16_t)(( (c - 0x10000l)         & 0x3FFl) + 0xDC00l);
		}
	}
	else {
		if (w < f) *w++ = (uint16_t)c;
	}
}

