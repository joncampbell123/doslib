
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

void utf8_to_str(unsigned char* &w,unsigned char *f,unicode_char_t c) {
	if (c < unicode_char_t(0)) {
		/* do nothing */
	}
	else if (c <= unicode_char_t(0x7Fu)) {
		if (w < f) *w++ = (unsigned char)(c&0xFFu);
	}
	else if (c <= unicode_char_t(0x7FFFFFFFul)) {
		/* 110x xxxx = 2 (1 more) mask 0x1F bits 5 + 6*1 = 11
		 * 1110 xxxx = 3 (2 more) mask 0x0F bits 4 + 6*2 = 16
		 * 1111 0xxx = 4 (3 more) mask 0x07 bits 3 + 6*3 = 21
		 * 1111 10xx = 5 (4 more) mask 0x03 bits 2 + 6*4 = 26
		 * 1111 110x = 6 (5 more) mask 0x01 bits 1 + 6*5 = 31 */
		unsigned char more = 1;
		{
			uint32_t tmp = uint32_t(c) >> uint32_t(11u);
			while (tmp != 0) { more++; tmp >>= uint32_t(5u); }
			assert(more <= 5);
		}

		const uint8_t ib = 0xFC << (5 - more);
		if ((w+1+more) > f) return;
		unsigned char *wr = w; w += 1+more; assert(w <= f);
		do { wr[more] = (unsigned char)(0x80u | ((unsigned char)(c&0x3F))); c >>= 6u; } while ((--more) != 0);
		assert(uint32_t(c) <= uint32_t((0x80u|(ib>>1u))^0xFFu)); /* 0xC0 0xE0 0xF0 0xF8 0xFC -> 0xE0 0xF0 0xF8 0xFC 0xFE -> 0x1F 0x0F 0x07 0x03 0x01 */
		wr[0] = (unsigned char)(ib | (unsigned char)c);
	}
}

std::string utf8_to_str(const unicode_char_t c) {
	unsigned char tmp[64],*w=tmp;

	utf8_to_str(/*&*/w,/*fence*/tmp+sizeof(tmp),c);
	assert(w < (tmp+sizeof(tmp)));
	*w++ = 0;

	return std::string((char*)tmp);
}

