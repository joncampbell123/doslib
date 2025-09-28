
#include <math.h>
#include <stdio.h>
#include <assert.h>

extern "C" {
#include "c11.h"
#include "c11.l.h"
#include "c11.y.h"
}

#include "c11.hpp"

int c11yy_sub_iconst(struct c11yy_struct_integer &d,const struct c11yy_struct_integer &a,const struct c11yy_struct_integer &b) {
	d = a;

	if (d.sz < a.sz) d.sz = a.sz;
	if (d.sz < b.sz) d.sz = b.sz;

	if ((a.flags&C11YY_INTF_SIGNED) || (b.flags&C11YY_INTF_SIGNED)) {
		d.v.s = a.v.s - b.v.s;
		if (d.v.s > a.v.s) d.flags |= C11YY_INTF_OVERFLOW;
		d.flags |= C11YY_INTF_SIGNED;

		const uint8_t sz = c11yy_iconsts_auto_size(d.v.s);
		if (d.sz < sz) d.sz = sz;
	}
	else {
		d.v.u = a.v.u - b.v.u;
		if (d.v.u > a.v.u) d.flags |= C11YY_INTF_OVERFLOW;

		const uint8_t sz = c11yy_iconstu_auto_size(d.v.u);
		if (d.sz < sz) d.sz = sz;
	}

	return 0;
}

