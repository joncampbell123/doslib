
#include <stdio.h>
#include <assert.h>

extern "C" {
#include "c11.h"
}

#include "c11.hpp"

extern "C" void c11yy_init_iconst(struct c11yy_struct_integer *val,const char *yytext,const char lexmatch) {
	/* lexmatch:
	 * 'H' hexadecimal
	 * 'N' decimal (starts with 1-9)
	 * 'O' octal (starts with 0, or is just zero)
	 * 'C' char constant
	 */
	*val = c11yy_struct_integer_I_CONSTANT_INIT;

	if (lexmatch == 'H') {
		c11yyskip(/*&*/yytext,2); /* assume 0x or 0X because the lexer already did the matching, see *.l file pattern {HP} */
		c11yy_iconst_read(/*base*/16,*val,/*&*/yytext);
		c11yy_iconst_readsuffix(*val,/*&*/yytext);
	}
	else if (lexmatch == 'N') {
		c11yy_iconst_read(/*base*/10,*val,/*&*/yytext);
		c11yy_iconst_readsuffix(*val,/*&*/yytext);
	}
	else if (lexmatch == 'O') {
		c11yy_iconst_read(/*base*/8,*val,/*&*/yytext); /* "0" is handled as octal, which is still correct parsing anyway */
		c11yy_iconst_readsuffix(*val,/*&*/yytext);
	}
	else if (lexmatch == 'C') {
		enum c11yystringtype st = C11YY_STRT_LOCAL;

		/* NTS: lexer syntax prevents 'u8' prefix */
		c11yy_check_stringtype_prefix(/*&*/st,/*&*/yytext);

		if (*yytext == '\'') {
			yytext++;
			c11yy_iconst_readchar(/*&*/st,*val,/*&*/yytext);
			/* assume trailing ' */
		}
	}

	{
		const uint8_t sz = c11yy_iconstu_auto_size(val->v.u);
		if (val->sz < sz) val->sz = sz;
	}

	/* this code never parses the leading minus sign, therefore everything parsed here is a nonnegative number,
	 * therefore if signed and the leading bit is set, overflow happend. */
	if (val->flags & C11YY_INTF_SIGNED) {
		if (val->v.s < (int64_t)0)
			val->flags |= C11YY_INTF_OVERFLOW;
	}
}

