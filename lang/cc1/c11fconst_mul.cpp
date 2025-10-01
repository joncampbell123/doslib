
#include <math.h>
#include <stdio.h>
#include <assert.h>

extern "C" {
#include "c11.h"
#include "c11.l.h"
#include "c11.y.h"
}

#include "c11.hpp"

int c11yy_mul_fconst(struct c11yy_struct_float &d,const struct c11yy_struct_float &a,const struct c11yy_struct_float &b) {
	d = a;

	if (d.sz < a.sz) d.sz = a.sz;
	if (d.sz < b.sz) d.sz = b.sz;

	/* if either side is zero, the result is zero */
	if ((a.flags^b.flags)&C11YY_FLOATF_NEGATIVE)
		d.flags |= C11YY_FLOATF_NEGATIVE;
	else
		d.flags &= ~C11YY_FLOATF_NEGATIVE;

	if (a.mant != 0ull && b.mant != 0ull) {
		uint64_t rh,rl;

		/* result: 128-bit value, mantissa on bit 126 because 2^63 * 2^63 = 2^126 */
		multiply64x64to128(/*&*/rh,/*&*/rl,a.mant,b.mant);
		d.exponent = a.exponent + b.exponent + 1;
		d.mant = rh;
		if (d.mant == 0ull) {
			d.mant = rl;
			d.exponent -= 64;
			rl = 0;
		}

		if (d.mant != 0ull) {
			while (!(d.mant & 0x8000000000000000ull)) {
				d.mant = (d.mant << 1ull) | (rl >> 63ull);
				d.exponent--;
				rl <<= 1ull;
			}
		}
		/* rounding */
		if (rl & 0x8000000000000000ull) {
			if ((++d.mant) == (uint64_t)0ull) {
				d.mant = (uint64_t)0x8000000000000000ull;
				d.exponent++;
			}
		}
	}
	else {
		d.exponent = 0;
		d.mant = 0;
	}
#if 0
	fprintf(stderr,"fltmul res %.60f flags=%lx sz=%u exp=%d mant=0x%016llx\n",
		(double)ldexpl((long double)d.mant,d.exponent - 63) * (d.flags&C11YY_FLOATF_NEGATIVE ? -1.0 : 1.0),
		(unsigned long)d.flags,
		(unsigned int)d.sz,
		(unsigned int)d.exponent,
		(unsigned long long)d.mant);

	fprintf(stderr,"    a %.60f flags=%lx sz=%u exp=%d mant=0x%016llx\n",
		(double)ldexpl((long double)a.mant,a.exponent - 63) * (a.flags&C11YY_FLOATF_NEGATIVE ? -1.0 : 1.0),
		(unsigned long)a.flags,
		(unsigned int)a.sz,
		(unsigned int)a.exponent,
		(unsigned long long)a.mant);

	fprintf(stderr,"    b %.60f flags=%lx sz=%u exp=%d mant=0x%016llx\n",
		(double)ldexpl((long double)b.mant,b.exponent - 63) * (b.flags&C11YY_FLOATF_NEGATIVE ? -1.0 : 1.0),
		(unsigned long)b.flags,
		(unsigned int)b.sz,
		(unsigned int)b.exponent,
		(unsigned long long)b.mant);
#endif
	return 0;
}

