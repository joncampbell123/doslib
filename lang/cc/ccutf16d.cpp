
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

unicode_char_t p_utf16_decode(const uint16_t* &p,const uint16_t* const f) {
	if (p >= f) return unicode_eof;

	if ((p+2) <= f && (p[0]&0xDC00u) == 0xD800u && (p[1]&0xDC00u) == 0xDC00u) {
		const uint32_t v = uint32_t(((p[0]&0x3FFul) << 10ul) + (p[1]&0x3FFul) + 0x10000ul); p += 2;
		return unicode_char_t(v);
	}

	return unicode_char_t(*p++);
}

