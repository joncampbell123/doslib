
#include <assert.h>

extern "C" {
#include "c11.h"
}

#include "c11.hpp"

void c11yy_write_utf8(uint8_t* &d,uint32_t c) {
	/* 110x xxxx = 2 (1 more) mask 0x1F bits 5 + 6*1 = 11
	 * 1110 xxxx = 3 (2 more) mask 0x0F bits 4 + 6*2 = 16
	 * 1111 0xxx = 4 (3 more) mask 0x07 bits 3 + 6*3 = 21
	 * 1111 10xx = 5 (4 more) mask 0x03 bits 2 + 6*4 = 26
	 * 1111 110x = 6 (5 more) mask 0x01 bits 1 + 6*5 = 31 */
	if (c >= 0x80) {
		unsigned char more = 1;
		{
			uint32_t tmp = (uint32_t)(c) >> (uint32_t)(11u);
			while (tmp != 0) { more++; tmp >>= (uint32_t)(5u); }
			assert(more <= 5);
		}

		const uint8_t ib = 0xFC << (5 - more);
		unsigned char *wr = d; d += 1+more;
		do { wr[more] = (unsigned char)(0x80u | ((unsigned char)(c&0x3F))); c >>= 6u; } while ((--more) != 0);
		assert((uint32_t)(c) <= (uint32_t)((0x80u|(ib>>1u))^0xFFu)); /* 0xC0 0xE0 0xF0 0xF8 0xFC -> 0xE0 0xF0 0xF8 0xFC 0xFE -> 0x1F 0x0F 0x07 0x03 0x01 */
		wr[0] = (unsigned char)(ib | (unsigned char)c);
	}
	else {
		*d++ = (unsigned char)c;
	}
}

