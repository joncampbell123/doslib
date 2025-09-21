
extern "C" {
#include "c11.h"
}

#include "c11.hpp"

void c11yy_iconst_readchar(const enum c11yystringtype st,struct c11yy_struct_integer &val,const char* &s) {
	val.v.u = 0;
	if (*s == '\\') {
		unsigned int c;
		char esc;
		int v;

		s++;
		switch (esc=*s++) {
			case '\'': case '\"': case '?': case '\\':
				val.v.u = (unsigned int)esc;
				break;
			case 'a': val.v.u = 7; break;
			case 'b': val.v.u = 8; break;
			case 'f': val.v.u = 12; break;
			case 'n': val.v.u = 10; break;
			case 'r': val.v.u = 13; break;
			case 't': val.v.u = 9; break;
			case 'v': val.v.u = 11; break;
			case '0': case '1': case '2': case '3':
			case '4': case '5': case '6': case '7':
				val.v.u = (unsigned int)esc - '0';
				for (c=0;c < 2;c++) {
					if ((v=c11yy_iconst_readc(/*base*/8,s)) >= 0)
						val.v.u = (val.v.u * (uint64_t)8u) + ((uint64_t)(unsigned int)v);
					else
						break;
				}
				break;
			case 'x':
				c11yy_iconst_read(/*base*/16,/*&*/val,/*&*/s);
				break;
			/* TODO: lexer does not yet support /u and /U */
		}
	}
	else if (*s) {
		if (st != C11YY_STRT_LOCAL)
			val.v.s = c11yy_read_utf8(s);
		else
			val.v.u = *s++;
	}
}

