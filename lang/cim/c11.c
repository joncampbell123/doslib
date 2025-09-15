
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
	}

	fprintf(stderr,"%llu sz=%u\n",(unsigned long long)val->v.u,val->sz);
}

int main() {
	return c11yy_do_compile();
}

