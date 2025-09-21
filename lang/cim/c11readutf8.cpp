
#include <assert.h>

extern "C" {
#include "c11.h"
}

#include "c11.hpp"

void c11yy_read_utf8(struct c11yy_struct_integer &val,const char* &s) {
	if ((unsigned char)(*s) >= 0xC0 && (unsigned char)(*s) < 0xFE) {
		/* 0x00-0x7F ASCII char */
		/* 0x80-0xBF we're in the middle of a UTF-8 char */
		/* overlong 1111 1110 or 1111 1111 */
		uint32_t v = (unsigned char)(*s++);

		/* 110x xxxx = 2 (1 more) mask 0x1F bits 5 + 6*1 = 11
		 * 1110 xxxx = 3 (2 more) mask 0x0F bits 4 + 6*2 = 16
		 * 1111 0xxx = 4 (3 more) mask 0x07 bits 3 + 6*3 = 21
		 * 1111 10xx = 5 (4 more) mask 0x03 bits 2 + 6*4 = 26
		 * 1111 110x = 6 (5 more) mask 0x01 bits 1 + 6*5 = 31 */
		unsigned char more = 1;
		for (unsigned char c=(unsigned char)v;(c&0xFFu) >= 0xE0u;) { c <<= 1u; more++; } assert(more <= 5);
		v &= 0x3Fu >> more; /* 1 2 3 4 5 -> 0x1F 0x0F 0x07 0x03 0x01 */

		do {
			unsigned char c = *s;
			if ((c&0xC0) != 0x80) c = 0; /* must be 10xx xxxx */
			if (c) s++;

			v = (v << (uint32_t)(6u)) + (uint32_t)(c & 0x3Fu);
		} while ((--more) != 0);

		val.v.u = v;
	}
	else if (*s) {
		val.v.u = *s++;
	}
	else {
		val.v.u = 0;
	}
}

