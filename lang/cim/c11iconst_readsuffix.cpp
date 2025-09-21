
#include "c11.h"
#include "c11.hpp"

void c11yy_iconst_readsuffix(struct c11yy_struct_integer &val,const char* &s) {
	static constexpr uint8_t UF = 1u << 0u;
	static constexpr uint8_t LF = 1u << 1u;
	static constexpr uint8_t LLF = 1u << 2u;
	uint8_t f = 0;

	while (1) {
		if (*s == 'u' || *s == 'U') {
			s++;
			f |= UF;
		}
		else if (*s == 'l' || *s == 'L') {
			s++;
			if (*s == 'l' || *s == 'L') {
				s++;
				f |= LLF;
			}
			else {
				f |= LF;
			}
		}
		else {
			break;
		}
	}

	if (f & UF)
		val.flags &= ~C11YY_INTF_SIGNED;

	if (f & LLF) {
		if (val.sz < 64u)
			val.sz = 64u;
	}
	else if (f & LF) {
		if (val.sz < 32u)
			val.sz = 32u;
	}
}

