
#include <math.h>
#include <stdio.h>
#include <assert.h>

extern "C" {
#include "c11.h"
#include "c11.l.h"
#include "c11.y.h"
}

#include "c11.hpp"

int c11yy_binop_iconst(struct c11yy_struct_integer &d,const struct c11yy_struct_integer &a,const struct c11yy_struct_integer &b,const enum c11yy_binop op) {
	d = a;

	if (d.sz < a.sz) d.sz = a.sz;
	if (d.sz < b.sz) d.sz = b.sz;

	switch (op) {
		case C11YY_BINOP_AND: d.v.u = a.v.u & b.v.u; break;
		case C11YY_BINOP_XOR: d.v.u = a.v.u ^ b.v.u; break;
		case C11YY_BINOP_OR:  d.v.u = a.v.u | b.v.u; break;
		default: return 1;
	}

	return 0;
}

