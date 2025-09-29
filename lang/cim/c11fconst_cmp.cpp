
#include <math.h>
#include <stdio.h>
#include <assert.h>

extern "C" {
#include "c11.h"
#include "c11.l.h"
#include "c11.y.h"
}

#include "c11.hpp"

int c11yy_cmp_fconst(struct c11yy_struct_integer &d,const struct c11yy_struct_float &a,const struct c11yy_struct_float &b,const enum c11yy_cmpop op) {
	bool cond,gt/*a>b*/,eq/*a==b*/;

	d = c11yy_struct_integer_I_CONSTANT_INIT;

	/* apparently -0.0 == 0.0 is true */
	eq = a.exponent ==
		b.exponent && a.mant == b.mant &&
		(((a.flags^b.flags)&C11YY_FLOATF_NEGATIVE) == 0/*same sign*/ || a.mant == 0ull/*zero--remember by this point a.mant==b.mant*/);

	if (((a.flags^b.flags)&C11YY_FLOATF_NEGATIVE) == 0) { /* same sign, both positive or negative */
		/* +a +b or -a -b */
		if (eq) {
			gt = false;
		}
		else {
			const bool nx = !!(a.flags&C11YY_FLOATF_NEGATIVE);
			if (a.mant == 0ull)
				gt = nx; /* a > b if a == 0 and b < 0. a < b if a == 0 and b > 0. */
			else if (a.exponent != b.exponent)
				gt = (a.exponent > b.exponent) ^ nx; /* larger exponent wins if positive, smaller exponent wins if negative */
			else
				gt = (a.mant > b.mant) ^ nx; /* larger mantissa wins if positive, smaller mantissa wins if negative */
		}
	}
	/* since the sign is not the same, assume eq == false */
	else if (a.flags&C11YY_FLOATF_NEGATIVE) { /* a negative, b positive */
		/* -a +b */
		gt = false;
	}
	else if (b.flags&C11YY_FLOATF_NEGATIVE) { /* b negative, a positive */
		/* +a -b */
		gt = !eq; /* a > b if b < 0, unless equal because of 0.0 == -0.0 case */
	}

	switch (op) {
		case C11YY_CMPOP_LT: cond = !gt && !eq; break;
		case C11YY_CMPOP_GT: cond =  gt && !eq; break;
		case C11YY_CMPOP_LE: cond = !gt ||  eq; break;
		case C11YY_CMPOP_GE: cond =  gt ||  eq; break;
		case C11YY_CMPOP_EQ: cond =         eq; break;
		case C11YY_CMPOP_NE: cond =        !eq; break;
		default: return 1;
	};

	d.v.u = cond ? 1 : 0;
	d.sz = 1;
	return 0;
}

