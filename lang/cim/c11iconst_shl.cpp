
#include <math.h>
#include <stdio.h>
#include <assert.h>

extern "C" {
#include "c11.h"
#include "c11.l.h"
#include "c11.y.h"
}

#include "c11.hpp"

int c11yy_shl_iconst(struct c11yy_struct_integer &d,const struct c11yy_struct_integer &a,const struct c11yy_struct_integer &b) {
	d = a;

	if (d.sz < a.sz) d.sz = a.sz;
	if (d.sz < b.sz) d.sz = b.sz;

	if ((a.flags&C11YY_INTF_SIGNED) || (b.flags&C11YY_INTF_SIGNED))
		d.flags |= C11YY_INTF_SIGNED;

	if (b.v.u <= 63ll) {
		if (b.v.u != 0ll) {
			const uint64_t chkmsk = (uint64_t)(UINT64_MAX) << (uint64_t)(64ull - b.v.u);
			if (a.v.u & chkmsk) d.flags |= C11YY_INTF_OVERFLOW;
			d.v.s = a.v.s << b.v.s;
		}
	}
	else {
		d.flags |= C11YY_INTF_OVERFLOW;
		d.v.s = 0;
	}

	{
		uint8_t sz;

		if (d.flags & C11YY_INTF_SIGNED)
			sz = c11yy_iconsts_auto_size(d.v.s);
		else
			sz = c11yy_iconstu_auto_size(d.v.u);

		if (d.sz < sz) d.sz = sz;
	}

	return 0;
}

