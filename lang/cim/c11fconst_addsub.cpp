
#include <math.h>
#include <stdio.h>
#include <assert.h>

extern "C" {
#include "c11.h"
#include "c11.l.h"
#include "c11.y.h"
}

#include "c11.hpp"

int c11yy_addsub_fconst(struct c11yy_struct_float &d,const struct c11yy_struct_float &a,const struct c11yy_struct_float &b,unsigned int aflags) {
	uint64_t ama,bma,sm;
	int exp;

	d = a;
	if (c11yy_fconst_match_mantissa_prep(exp,ama,bma,a,b))
		return 1;

	/* addition:
	 * pos + pos = add mantissa
	 * neg + neg = add mantissa
	 * pos + neg or neg + pos = subtract mantissa
	 *
	 * subtraction:
	 * pos + pos = subtract mantissa
	 * neg + neg = subtract mantissa
	 * pos + neg or neg + pos = add mantissa */
	if ((a.flags^b.flags^aflags)&C11YY_FLOATF_NEGATIVE) {
		if (ama < bma) {
			d.flags ^= C11YY_FLOATF_NEGATIVE;
			sm = bma - ama;
		}
		else {
			sm = ama - bma;
		}

		if (sm != 0ull) {
			while (!(sm & 0x8000000000000000ull)) {
				sm <<= 1ull;
				exp--;
			}
		}
	}
	else {
		/* if a+b mantissa would overflow, then shift again */
		sm = ama+bma;
		if (sm < ama) {
			const unsigned char carry = (unsigned char)ama&(unsigned char)bma&(unsigned char)1u;/*if both LSB*/
			sm = (ama >> (uint64_t)1u) + (bma >> (uint64_t)1u);
			if (carry) sm++;
			exp++;
		}
	}

	if (d.sz < a.sz) d.sz = a.sz;
	if (d.sz < b.sz) d.sz = b.sz;

	d.exponent = exp;
	d.mant = sm;
#if 0
	fprintf(stderr,"flt%s res %.60f flags=%lx sz=%u exp=%d mant=0x%016llx\n",
		(aflags & C11YY_FLOATF_NEGATIVE) ? "sub" : "add",
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

