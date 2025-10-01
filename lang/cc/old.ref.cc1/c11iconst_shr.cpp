
#include <math.h>
#include <stdio.h>
#include <assert.h>

extern "C" {
#include "c11.h"
#include "c11.l.h"
#include "c11.y.h"
}

#include "c11.hpp"

int c11yy_shr_iconst(struct c11yy_struct_integer &d,const struct c11yy_struct_integer &a,const struct c11yy_struct_integer &b) {
	d = a;

	if (d.sz < a.sz) d.sz = a.sz;
	if (d.sz < b.sz) d.sz = b.sz;

	if ((a.flags&C11YY_INTF_SIGNED) || (b.flags&C11YY_INTF_SIGNED)) {
		d.flags |= C11YY_INTF_SIGNED;
		if (b.v.u <= 63ull)
			d.v.s = a.v.s >> b.v.s;
		else
			d.v.s = a.v.s >> 63ll;
	}
	else {
		if (b.v.u <= 63ull)
			d.v.u = a.v.u >> b.v.u;
		else
			d.v.u = 0;
	}

	return 0;
}

