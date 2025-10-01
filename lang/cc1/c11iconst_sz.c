
#include <stdint.h>

#include "c11.hpp"

uint8_t c11yy_iconstu_auto_size(uint64_t v) {
	uint8_t sz = 0u;

	while (v) {
		v >>= (uint64_t)8u;
		sz += 8u;
	}

	return sz;
}

uint8_t c11yy_iconsts_auto_size(int64_t v) {
	if (v >= (int64_t)0)
		return c11yy_iconstu_auto_size(v >> (uint64_t)7ull) + 8u;
	else
		return c11yy_iconstu_auto_size((((uint64_t)(-v)) - (uint64_t)1ull) >> (uint64_t)7ull) + 8u;
}

