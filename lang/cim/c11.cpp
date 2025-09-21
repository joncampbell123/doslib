
#include <stdio.h>
#include <assert.h>

extern "C" {
#include "c11.h"
#include "c11.l.h"
#include "c11.y.h"
}

#include "c11.hpp"

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

