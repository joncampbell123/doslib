
#include "c11.h"
#include "c11.hpp"

int c11yy_iconst_readc(const unsigned int base,const char* &s) {
	unsigned char v;

	if (*s >= '0' && *s <= '9')
		v = (unsigned char)(*s - '0');
	else if (*s >= 'a' && *s <= 'z')
		v = (unsigned char)(*s + 10 - 'a');
	else if (*s >= 'A' && *s <= 'Z')
		v = (unsigned char)(*s + 10 - 'A');
	else
		return -1;

	if (v < base)
		return (int)v;

	return -1;
}

