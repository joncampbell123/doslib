
#include <math.h>
#include <stdio.h>
#include <assert.h>

extern "C" {
#include "c11.h"
#include "c11.l.h"
#include "c11.y.h"
}

#include "c11.hpp"

int c11yy_div_fconst(struct c11yy_struct_float &d,const struct c11yy_struct_float &a,const struct c11yy_struct_float &b) {
	d = a;

	if (d.sz < a.sz) d.sz = a.sz;
	if (d.sz < b.sz) d.sz = b.sz;

	/* if either side is zero, the result is zero */
	if ((a.flags^b.flags)&C11YY_FLOATF_NEGATIVE)
		d.flags |= C11YY_FLOATF_NEGATIVE;
	else
		d.flags &= ~C11YY_FLOATF_NEGATIVE;

	if (b.mant == 0ull)
		return 1;
	if (!(b.mant & 0x8000000000000000ull))
		return 1;

	if (a.mant != 0ull) {
		if (!(a.mant & 0x8000000000000000ull))
			return 1;

		d.exponent = a.exponent - b.exponent;
		d.mant = 0;

		uint64_t bset = (uint64_t)1u << (uint64_t)63u;
		uint64_t tmp = a.mant;
		uint64_t cmv = b.mant;

		/* both tmp and cnv have bit 63 set at this point */
		while (1) {
			if (tmp >= cmv) {
				d.mant |= bset;
				bset >>= 1u;
				tmp -= cmv;
				/* two values subtracted with bit 63 set and tmp >= cnv, now bit 63 should be clear, scale cnv as well for correct math */
				if ((cmv >>= 1ull) == 0ull)
					return 1;
				break;
			}

			d.exponent--;
			if ((cmv >>= 1ull) == 0ull)
				return 1;
		}

		while (tmp != 0ull) {
			if (tmp >= cmv) {
				d.mant |= bset;
				tmp -= cmv;
			}

			tmp <<= 1ull;
			if ((bset >>= 1u) == 0ull)
				break;
		}
	}
	else {
		d.exponent = 0;
		d.mant = 0;
	}

	fprintf(stderr,"fltdiv res %.60f flags=%lx sz=%u exp=%d mant=0x%016llx\n",
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

	return 0;
}

