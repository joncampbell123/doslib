
extern "C" {
#include "c11.h"
}

#include "c11.hpp"

void c11yy_check_stringtype_prefix(enum c11yystringtype &st,const char* &s) {
	if (*s == 'u') {
		st = C11YY_STRT_UTF16; s++;
		if (*s == '8') {
			st = C11YY_STRT_UTF8; s++;
		}
	}
	else if (*s == 'U') {
		st = C11YY_STRT_UTF32; s++;
	}
	else if (*s == 'L') {
		st = C11YY_STRT_UTF32; s++;
	}
}

