
#include <stdio.h>

#include "c11.h"
#include "c11.y.h"

void c11yyskip(const char **y,unsigned int c) {
	const char *s = *y;

	while (*s && c) {
		s++;
		c--;
	}

	*y = s;
}

int c11yy_iconst_readc(const unsigned int base,const char **y) {
	const char *s = *y;
	unsigned char v;

	if (*s >= '0' && *s <= '9')
		v = (unsigned char)(*s - '0');
	else if (*s >= 'a' && *s <= 'z')
		v = (unsigned char)(*s + 10 - 'a');
	else if (*s >= 'A' && *s <= 'Z')
		v = (unsigned char)(*s + 10 - 'A');
	else
		return -1;

	if (v < base) {
		*y = ++s;
		return (int)v;
	}

	return -1;
}

uint8_t c11yy_iconstu_auto_size(uint64_t v) {
	uint8_t sz = 8u;
	v >>= (uint64_t)8u;

	while (v) {
		v >>= (uint64_t)8u;
		sz += 8u;
	}

	return sz;
}

void c11yy_iconst_read(const unsigned int base,struct c11yy_struct_integer *val,const char **y) {
	const uint64_t maxv = UINT64_MAX / (uint64_t)(base);
	const char *s = *y;
	int v;

	val->v.u = 0;
	while (*s) {
		if ((v=c11yy_iconst_readc(base,&s)) < 0)
			break;

		if (val->v.u > maxv)
			val->flags |= C11YY_INTF_OVERFLOW;

		val->v.u = (val->v.u * ((uint64_t)base)) + ((uint64_t)(unsigned int)v);
	}

	*y = s;
}

void c11yy_iconst_readchar(struct c11yy_struct_integer *val,const char **y) {
	const char *s = *y;

	val->v.u = 0;
	if (*s == '\\') {
		unsigned int c;
		char esc;
		int v;

		s++;
		switch (esc=*s++) {
			case '\'': case '\"': case '?': case '\\':
				val->v.u = (unsigned int)esc;
				break;
			case 'a': val->v.u = 7; break;
			case 'b': val->v.u = 8; break;
			case 'f': val->v.u = 12; break;
			case 'n': val->v.u = 10; break;
			case 'r': val->v.u = 13; break;
			case 't': val->v.u = 9; break;
			case 'v': val->v.u = 11; break;
			case '0': case '1': case '2': case '3':
			case '4': case '5': case '6': case '7':
				val->v.u = (unsigned int)esc - '0';
				for (c=0;c < 2;c++) {
					if ((v=c11yy_iconst_readc(/*base*/8,&s)) >= 0)
						val->v.u = (val->v.u * (uint64_t)8u) + ((uint64_t)(unsigned int)v);
					else
						break;
				}
				break;
			case 'x':
				c11yy_iconst_read(/*base*/16,val,&s);
				break;
			/* TODO: lexer does not yet support /u and /U */
		}
	}
	else if (*s) {
		val->v.u = *s++;
	}

	*y = s;
}

void c11yy_init_iconst(struct c11yy_struct_integer *val,const char *yytext,const char lexmatch) {
	/* lexmatch:
	 * 'H' hexadecimal
	 * 'N' decimal (starts with 1-9)
	 * 'O' octal (starts with 0, or is just zero)
	 * 'C' char constant
	 */
	val->sz = 0;
	val->v.u = 0;
	val->flags = 0;
	val->t = I_CONSTANT;

	if (lexmatch == 'H') {
		c11yyskip(&yytext,2); /* assume 0x or 0X because the lexer already did the matching, see *.l file pattern {HP} */
		c11yy_iconst_read(/*base*/16,val,&yytext);
	}
	else if (lexmatch == 'N') {
		c11yy_iconst_read(/*base*/10,val,&yytext);
	}
	else if (lexmatch == 'O') {
		c11yy_iconst_read(/*base*/8,val,&yytext); /* "0" is handled as octal, which is still correct parsing anyway */
	}
	else if (lexmatch == 'C') {
		if (*yytext == '\'') {
			yytext++;
			c11yy_iconst_readchar(val,&yytext);
			/* assume trailing ' */
		}
	}

	if (val->sz == 0)
		val->sz = c11yy_iconstu_auto_size(val->v.u);

	fprintf(stderr,"%llu sz=%u\n",(unsigned long long)val->v.u,val->sz);
}

int main() {
	return c11yy_do_compile();
}

