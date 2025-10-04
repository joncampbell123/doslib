
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <math.h>

#include "cc.hpp"

bool ast_fconst_cmp(const struct floating_value_t &a,const struct floating_value_t &b,const enum cmpop_t op) {
	bool cond,gt/*a>b*/,eq/*a==b*/;

	/* apparently -0.0 == 0.0 is true */
	eq =	/*both are zero*/
		(a.flags&b.flags&floating_value_t::FL_ZERO) ||
		/*both zero or neither zero, same sign, exponent and mantissa match*/
		(((a.flags^b.flags)&(floating_value_t::FL_ZERO|floating_value_t::FL_NEGATIVE)) == 0 &&
		a.exponent == b.exponent && a.mantissa == b.mantissa);

	if (((a.flags^b.flags)&floating_value_t::FL_NEGATIVE) == 0) { /* same sign, both positive or negative */
		/* +a +b or -a -b */
		if (eq) {
			gt = false;
		}
		else {
			const bool nx = !!(a.flags&floating_value_t::FL_NEGATIVE);
			if (a.flags&floating_value_t::FL_ZERO)
				gt = nx; /* a > b if a == 0 and b < 0. a < b if a == 0 and b > 0. */
			else if (a.exponent != b.exponent)
				gt = (a.exponent > b.exponent) ^ nx; /* larger exponent wins if positive, smaller exponent wins if negative */
			else
				gt = (a.mantissa > b.mantissa) ^ nx; /* larger mantissa wins if positive, smaller mantissa wins if negative */
		}
	}
	/* since the sign is not the same, assume eq == false */
	else if (a.flags&floating_value_t::FL_NEGATIVE) { /* a negative, b positive */
		/* -a +b */
		gt = false;
	}
	else if (b.flags&floating_value_t::FL_NEGATIVE) { /* b negative, a positive */
		/* +a -b */
		gt = !eq; /* a > b if b < 0, unless equal because of 0.0 == -0.0 case */
	}

	switch (op) {
		case CMPOP_LT: cond = !gt && !eq; break;
		case CMPOP_GT: cond =  gt && !eq; break;
		case CMPOP_LE: cond = !gt ||  eq; break;
		case CMPOP_GE: cond =  gt ||  eq; break;
		case CMPOP_EQ: cond =         eq; break;
		case CMPOP_NE: cond =        !eq; break;
		default: return 1;
	};

	return cond;
}

