
#include <stdio.h>
#include <assert.h>

extern "C" {
#include "c11.h"
#include "c11.l.h"
#include "c11.y.h"
}

#include "c11.hpp"

////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////

static const uint16_t c11yy_itype_to_token[C11YY_IDT__MAX] = {
	IDENTIFIER,				// C11YY_IDT_NONE = 0
	IDENTIFIER,				// C11YY_IDT_IDENTIFIER
	TYPEDEF_NAME,				// C11YY_IDT_TYPEDEF_NAME
	ENUMERATION_CONSTANT			// C11YY_IDT_ENUMERATION_CONSTANT
};

extern "C" struct c11yy_identifier_obj *c11yy_init_ident(const char *yytext,int yyleng) {
	return NULL;
}

extern "C" int c11yy_check_type(const struct c11yy_identifier_obj *io) {
	if (io && io->itype < C11YY_IDT__MAX) return c11yy_itype_to_token[io->itype];
	return IDENTIFIER;
}

////////////////////////////////////////////////////////////////////

static int c11yy_unary_op_none(union c11yy_struct *d,const union c11yy_struct *s) {
	*d = *s;
	return 0;
}

static int c11yy_unary_op_neg(union c11yy_struct *d,const union c11yy_struct *s) {
	if (s->base.t == I_CONSTANT) {
		*d = *s;
		d->intval.v.s = -d->intval.v.s;
		d->intval.flags |= C11YY_INTF_SIGNED;
		d->intval.sz = c11yy_iconsts_auto_size(d->intval.v.s);
		return 0;
	}

	return 1;
}

static int c11yy_unary_op_pos(union c11yy_struct *d,const union c11yy_struct *s) {
	if (s->base.t == I_CONSTANT) {
		*d = *s;
		d->intval.flags |= C11YY_INTF_SIGNED;
		d->intval.sz = c11yy_iconsts_auto_size(d->intval.v.s);
		return 0;
	}

	return 1;
}

static int c11yy_unary_op_bnot(union c11yy_struct *d,const union c11yy_struct *s) {
	if (s->base.t == I_CONSTANT) {
		/* do not update size */
		*d = *s;
		d->intval.v.u = ~d->intval.v.u;
		d->intval.flags &= ~C11YY_INTF_SIGNED;
		d->intval.flags |= C11YY_INTF_TRUNCATEOK;
		return 0;
	}

	return 1;
}

static int c11yy_unary_op_lnot(union c11yy_struct *d,const union c11yy_struct *s) {
	if (s->base.t == I_CONSTANT) {
		/* do not update size */
		*d = *s;
		d->base.t = s->base.t;
		d->intval.v.u = (s->intval.v.u == (uint64_t)0ull) ? 1u : 0u;
		return 0;
	}

	return 1;
}

static int (* const c11yy_unary_op[C11YY_UNOP__MAX])(union c11yy_struct *,const union c11yy_struct *) = { // must match enum c11yy_unop
	c11yy_unary_op_none,		// 0
	c11yy_unary_op_neg,
	c11yy_unary_op_pos,
	c11yy_unary_op_bnot,
	c11yy_unary_op_lnot
};

extern "C" int c11yy_unary(union c11yy_struct *d,const union c11yy_struct *s,const unsigned int unop) {
	if (unop < C11YY_UNOP__MAX)
		return c11yy_unary_op[unop](d,s);
	else
		return 1;
}

///////////////////////////////////////////////////////////

static int c11yy_init(void) {
	return 0;
}

static void c11yy_cleanup(void) {
}

/////////////////////////////////////////////////////////////

int main() {
	if (c11yy_init()) {
		c11yy_cleanup();
		return 1;
	}

	if (c11yy_do_compile()) {
		c11yylex_destroy();
		c11yy_cleanup();
		return 1;
	}

	c11yylex_destroy();
	c11yy_cleanup();
	return 0;
}

