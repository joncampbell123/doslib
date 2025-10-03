
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

#define CHECK_WORK

static int match_mantissa_prep(int &exp,uint64_t &ama,uint64_t &bma,const struct floating_value_t &a,const struct floating_value_t &b) {
	/* assume already normalized, not zero, not special (inf, NaN) */
	if (a.exponent < b.exponent) {
		const unsigned int shf = (unsigned int)(b.exponent - a.exponent);
		exp = b.exponent;
		if (shf < 64u) ama = a.mantissa >> (uint64_t)shf;
		else ama = 0u;
		bma = b.mantissa;
	}
	else if (b.exponent < a.exponent) {
		const unsigned int shf = (unsigned int)(a.exponent - b.exponent);
		exp = a.exponent;
		if (shf < 64u) bma = b.mantissa >> (uint64_t)shf;
		else bma = 0u;
		ama = a.mantissa;
	}
	else {
		exp = a.exponent;
		ama = a.mantissa;
		bma = b.mantissa;
	}

	return 0;
}

#ifdef CHECK_WORK
static void check_addsub_result(const struct floating_value_t &r,const struct floating_value_t &op1,const struct floating_value_t &op2,const unsigned int aflags) {
	const long double r_v    = ldexpl(  r.mantissa,  r.exponent - 63) * (  r.flags & floating_value_t::FL_NEGATIVE ? -1.0l : 1.0l);
	const long double r_op1  = ldexpl(op1.mantissa,op1.exponent - 63) * (op1.flags & floating_value_t::FL_NEGATIVE ? -1.0l : 1.0l);
	const long double r_op2  = ldexpl(op2.mantissa,op2.exponent - 63) * (op2.flags & floating_value_t::FL_NEGATIVE ? -1.0l : 1.0l);
	const long double should = (aflags & floating_value_t::FL_NEGATIVE) ? (r_op1 - r_op2) : (r_op1 + r_op2);
	const long double tol    = ldexpl(  1.0,         r.exponent - 63);
	const long double err    = r_v - should;

	if (err != 0.0l)
		fprintf(stderr,"addsub chk err=%.30f / %.30f\n",(double)err,(double)tol);
}
#endif

bool ast_constexpr_addsub_floating(token_t &tr,const token_t &top1,const token_t &top2,unsigned int aflags) {
	const struct floating_value_t &op1 = top1.v.floating;
	const struct floating_value_t &op2 = top2.v.floating;
	struct floating_value_t &r = tr.v.floating;

	uint64_t ama,bma;
	int exp;

	tr = top1;

	/* skip if adding zero */
	if ((op1.flags | op2.flags) & floating_value_t::FL_ZERO) {
		/* if op1 is zero and subtraction (aflags & NEGATIVE), and op2 is not zero, then return op2 negated (i.e. 0.0 - 1.0) */
		if (aflags & floating_value_t::FL_NEGATIVE) {
			if ((op1.flags & floating_value_t::FL_ZERO) && (op2.flags & floating_value_t::FL_ZERO) == 0) {
				r = op2;
				r.flags ^= floating_value_t::FL_NEGATIVE;
			}
		}

#ifdef CHECK_WORK
		check_addsub_result(r,op1,op2,aflags);
#endif

		return true;
	}

	/* TODO: Inf, NaN */

	if (match_mantissa_prep(exp,ama,bma,op1,op2))
		return false;

	/* addition:
	 * pos + pos = add mantissa
	 * neg + neg = add mantissa
	 * pos + neg or neg + pos = subtract mantissa
	 *
	 * subtraction:
	 * pos + pos = subtract mantissa
	 * neg + neg = subtract mantissa
	 * pos + neg or neg + pos = add mantissa */
	r.exponent = exp;
	if ((op1.flags ^ op2.flags ^ aflags) & floating_value_t::FL_NEGATIVE) {
		if (ama < bma) {
			r.flags ^= floating_value_t::FL_NEGATIVE;
			r.mantissa = bma - ama;
		}
		else {
			r.mantissa = ama - bma;
		}

		if (r.mantissa != 0ull)
			r.normalize();
		else
			r.flags |= floating_value_t::FL_ZERO;
	}
	else {
		r.mantissa = ama + bma;
		if (r.mantissa < ama/*overflow*/) {
			if ((uint64_t)(++r.mantissa) == 0ull) r.exponent++;
			r.mantissa = (r.mantissa >> (uint64_t)1ull) | floating_value_t::mant_msb;
			r.exponent++;
		}
	}

#ifdef CHECK_WORK
	check_addsub_result(r,op1,op2,aflags);
#endif

	return true;
}

