
#include <math.h>
#include <stdio.h>
#include <assert.h>

extern "C" {
#include "c11.h"
#include "c11.l.h"
#include "c11.y.h"
}

#include "c11.hpp"

int c11yy_cmp_iconst(struct c11yy_struct_integer &d,const struct c11yy_struct_integer &a,const struct c11yy_struct_integer &b,const enum c11yy_cmpop op) {
	bool cond;

	d = c11yy_struct_integer_I_CONSTANT_INIT;

	if ((a.flags&C11YY_INTF_SIGNED) || (b.flags&C11YY_INTF_SIGNED)) {
		switch (op) {
			case C11YY_CMPOP_LT: cond = a.v.s <  b.v.s; break;
			case C11YY_CMPOP_GT: cond = a.v.s >  b.v.s; break;
			case C11YY_CMPOP_LE: cond = a.v.s <= b.v.s; break;
			case C11YY_CMPOP_GE: cond = a.v.s >= b.v.s; break;
			case C11YY_CMPOP_EQ: cond = a.v.s == b.v.s; break;
			case C11YY_CMPOP_NE: cond = a.v.s != b.v.s; break;
			default: return 1;
		};
	}
	else {
		switch (op) {
			case C11YY_CMPOP_LT: cond = a.v.u <  b.v.u; break;
			case C11YY_CMPOP_GT: cond = a.v.u >  b.v.u; break;
			case C11YY_CMPOP_LE: cond = a.v.u <= b.v.u; break;
			case C11YY_CMPOP_GE: cond = a.v.u >= b.v.u; break;
			case C11YY_CMPOP_EQ: cond = a.v.u == b.v.u; break;
			case C11YY_CMPOP_NE: cond = a.v.u != b.v.u; break;
			default: return 1;
		};
	}

	d.v.u = cond ? 1 : 0;
	d.sz = 1;
	return 0;
}

