
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

static bool ast_constexpr_divide_integer(token_t &tr,const token_t &top1,const token_t &top2) {
	const struct integer_value_t &op1 = top1.v.integer;
	const struct integer_value_t &op2 = top2.v.integer;
	struct integer_value_t &r = tr.v.integer;

	tr = top1;

	/* divide by zero? NOPE! */
	if (op2.v.u == 0ull)
		return false;

	if ((op1.flags | op2.flags) & integer_value_t::FL_SIGNED) {
		r.flags |= integer_value_t::FL_SIGNED;
		r.v.s = op1.v.s / op2.v.s;
	}
	else {
		r.v.u = op1.v.u / op2.v.u;
	}

	return true;
}

#ifdef CHECK_WORK
static void check_div_result(const struct floating_value_t &r,const struct floating_value_t &op1,const struct floating_value_t &op2) {
	const long double r_v    = ldexpl(  r.mantissa,  r.exponent - 63) * (  r.flags & floating_value_t::FL_NEGATIVE ? -1.0l : 1.0l);
	const long double r_op1  = ldexpl(op1.mantissa,op1.exponent - 63) * (op1.flags & floating_value_t::FL_NEGATIVE ? -1.0l : 1.0l);
	const long double r_op2  = ldexpl(op2.mantissa,op2.exponent - 63) * (op2.flags & floating_value_t::FL_NEGATIVE ? -1.0l : 1.0l);
	const long double should = r_op1 / r_op2;
	const long double tol    = ldexpl(  1.0,         r.exponent - 63);
	const long double err    = r_v - should;

	if (err != 0.0l)
		fprintf(stderr,"div chk err=%.30f / %.30f\n",(double)err,(double)tol);
}
#endif

static bool ast_constexpr_divide_floating(token_t &tr,const token_t &top1,const token_t &top2) {
	const struct floating_value_t &op1 = top1.v.floating;
	const struct floating_value_t &op2 = top2.v.floating;
	struct floating_value_t &r = tr.v.floating;

	tr = top1;

	/* divide by zero? NOPE! */
	if (op2.flags & floating_value_t::FL_ZERO)
		return false;

	/* division sign rules:
	 * ++ = +
	 * -- = +
	 * -+ = -
	 * +- = -
	 */
	r.flags ^= op2.flags & floating_value_t::FL_NEGATIVE;

	/* sanity check */
	if (op2.flags & floating_value_t::FL_ZERO)
		return false;
	if (!(op2.mantissa & floating_value_t::mant_msb))
		return false;

	r.mantissa = 0;
	if (op1.flags & floating_value_t::FL_ZERO) {
		/* zero divided by anything is still zero */
		r.exponent = 0;
	}
	else {
		/* sanity check */
		if (!(op1.mantissa & floating_value_t::mant_msb))
			return false;

		r.exponent = op1.exponent - op2.exponent;

		uint64_t bset = (uint64_t)1u << (uint64_t)63u;
		uint64_t tmp = op1.mantissa;
		uint64_t cmv = op2.mantissa;

		/* both tmp and cnv have bit 63 set at this point */
		while (1) {
			if (tmp >= cmv) {
				r.mantissa |= bset;
				bset >>= 1u;
				tmp -= cmv;
				/* two values subtracted with bit 63 set and tmp >= cnv, now bit 63 should be clear, scale cnv as well for correct math */
				if ((cmv >>= 1ull) == 0ull)
					return 1;
				break;
			}

			r.exponent--;
			if ((cmv >>= 1ull) == 0ull)
				return 1;
		}

		while (tmp != 0ull) {
			if (tmp >= cmv) {
				r.mantissa |= bset;
				tmp -= cmv;
			}

			tmp <<= 1ull;
			if ((bset >>= 1u) == 0ull)
				break;
		}
	}

#ifdef CHECK_WORK
	check_div_result(r,op1,op2);
#endif

	return true;
}

bool ast_constexpr_divide(token_t &r,const token_t &op1,const token_t &op2) {
	if (op1.type == op2.type) {
		switch (op1.type) {
			case token_type_t::integer:
				return ast_constexpr_divide_integer(r,op1,op2);
			case token_type_t::floating:
				return ast_constexpr_divide_floating(r,op1,op2);
			default:
				break;
		};
	}

	return false;
}

