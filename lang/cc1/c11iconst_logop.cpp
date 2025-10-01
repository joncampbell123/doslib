
#include <math.h>
#include <stdio.h>
#include <assert.h>

extern "C" {
#include "c11.h"
#include "c11.l.h"
#include "c11.y.h"
}

#include "c11.hpp"

int c11yy_logop_iconst(struct c11yy_struct_integer &d,const struct c11yy_struct_integer &a,const struct c11yy_struct_integer &b,const enum c11yy_logop op) {
	d = c11yy_struct_integer_I_CONSTANT_INIT;
	d.sz = 1;

	switch (op) {
		case C11YY_LOGOP_AND: d.v.u = ((a.v.u != 0ull) && (b.v.u != 0ull)) ? 1 : 0; break;
		case C11YY_LOGOP_OR:  d.v.u = ((a.v.u != 0ull) || (b.v.u != 0ull)) ? 1 : 0; break;
		default: return 1;
	}

	return 0;
}

