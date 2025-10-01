
#include <math.h>
#include <stdio.h>
#include <assert.h>

extern "C" {
#include "c11.h"
#include "c11.l.h"
#include "c11.y.h"
}

#include "c11.hpp"

int c11yy_fconst_match_mantissa_prep(int &exp,uint64_t &ama,uint64_t &bma,const struct c11yy_struct_float &a,const struct c11yy_struct_float &b) {
	/* assume already normalized */
	if (a.exponent < b.exponent) {
		const unsigned int shf = (unsigned int)(b.exponent - a.exponent);
		exp = b.exponent;
		if (shf < 64u) ama = a.mant >> (uint64_t)shf;
		else ama = 0u;
		bma = b.mant;
	}
	else if (b.exponent < a.exponent) {
		const unsigned int shf = (unsigned int)(a.exponent - b.exponent);
		exp = a.exponent;
		if (shf < 64u) bma = b.mant >> (uint64_t)shf;
		else bma = 0u;
		ama = a.mant;
	}
	else {
		exp = a.exponent;
		ama = a.mant;
		bma = b.mant;
	}

	return 0;
}

