
extern "C" {
#include "c11.h"
}

#include "c11.hpp"

////////////////////////////////////////////////////////////////////

static constexpr uint32_t c11yy_string_hash_init = (uint32_t)0xA1272155ul;

////////////////////////////////////////////////////////////////////

static inline uint32_t c11yy_string_hash_begin() {
	return c11yy_string_hash_init;
}

static inline uint32_t c11yy_string_hash_step(const uint32_t h,const uint8_t c) {
	return ((h << (uint32_t)13ul) ^ (h >> (uint32_t)19ul) ^ (h >> (uint32_t)31u) ^ 1) + (uint32_t)c;
}

static inline uint32_t c11yy_string_hash_end(const uint32_t h) {
	return (uint32_t)(~h);
}

////////////////////////////////////////////////////////////////////

extern "C" uint32_t c11yy_string_hash(const uint8_t *s,size_t l) {
	uint32_t h = c11yy_string_hash_begin();
	while ((l--) != 0ul) h = c11yy_string_hash_step(h,*s++);
	return c11yy_string_hash_end(h);
}

