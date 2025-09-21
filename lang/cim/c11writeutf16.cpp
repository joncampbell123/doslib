
extern "C" {
#include "c11.h"
}

#include "c11.hpp"

void c11yy_write_utf16(uint16_t* &d,uint32_t c) {
	if (c >= 0x10000l) {
		/* surrogate pair */
		*d++ = (uint16_t)((((c - 0x10000ul) >> 10ul) & 0x3FFul) + 0xD800ul);
		*d++ = (uint16_t)(( (c - 0x10000ul)          & 0x3FFul) + 0xDC00ul);
	}
	else {
		*d++ = (uint16_t)c;
	}
}

