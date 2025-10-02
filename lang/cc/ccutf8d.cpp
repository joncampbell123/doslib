
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

unicode_char_t p_utf8_decode(const unsigned char* &p,const unsigned char* const f) {
	if (p >= f) return unicode_eof;

	uint32_t v = *p++;
	if (v <  0x80) return v; /* 0x00-0x7F ASCII char */
	if (v <  0xC0) return unicode_invalid; /* 0x80-0xBF we're in the middle of a UTF-8 char */
	if (v >= 0xFE) return unicode_invalid; /* overlong 1111 1110 or 1111 1111 */

	/* 110x xxxx = 2 (1 more) mask 0x1F bits 5 + 6*1 = 11
	 * 1110 xxxx = 3 (2 more) mask 0x0F bits 4 + 6*2 = 16
	 * 1111 0xxx = 4 (3 more) mask 0x07 bits 3 + 6*3 = 21
	 * 1111 10xx = 5 (4 more) mask 0x03 bits 2 + 6*4 = 26
	 * 1111 110x = 6 (5 more) mask 0x01 bits 1 + 6*5 = 31 */
	unsigned char more = 1;
	for (unsigned char c=(unsigned char)v;(c&0xFFu) >= 0xE0u;) { c <<= 1u; more++; } assert(more <= 5);
	v &= 0x3Fu >> more; /* 1 2 3 4 5 -> 0x1F 0x0F 0x07 0x03 0x01 */

	do {
		if (p >= f) return unicode_invalid;
		const unsigned char c = *p;
		if ((c&0xC0) != 0x80) return unicode_invalid; /* must be 10xx xxxx */
		p++; v = (v << uint32_t(6u)) + uint32_t(c & 0x3Fu);
	} while ((--more) != 0);

	assert(v <= uint32_t(0x7FFFFFFFu));
	return unicode_char_t(v);
}

