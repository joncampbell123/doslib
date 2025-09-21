
extern "C" {
#include "c11.h"
}

#include "c11.hpp"

int c11yy_iconst_readc(const unsigned int base,const char* &s) {
	const int v = c11yy_char2digit(*s);

	if ((unsigned int)v >= base) return -1; /* do not increment s */
	else if (v >= 0) s++; /* v < 0, do not increment s, else, increment s */

	return v;
}

