
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

		/*      79 remainder 2      integer division
		 *   _________________
		 * 7 ( 555|
		 * 7   5  | 7 goes into 5 zero times, floor(5/7) = 0
		 * 7  -0  | subtract 7 * 0 = 0
		 * 7   55 | bring down next digit, 7 goes into 55, 7 times, floor(55/7) = 7
		 * 7  -49 | subtract 7 * 7 = 49
		 * 7    65| bring down next digit, 7 goes into 65, 9 times, floor(65/7) = 9
		 * 7   -63| subtract 7 * 9 = 63
		 * 7     2| this is the remainder
		 */
		/*      79.285714...        decimal division
		 *   _________________
		 * 7 ( 555.000000
		 * 7   5  .            7 goes into 5 zero times, floor(5/7) = 0
		 * 7  -0  .            subtract 7 * 0 = 0
		 * 7   55 .            bring down next digit, 7 goes into 55, 7 times, floor(55/7) = 7
		 * 7  -49 .            subtract 7 * 7 = 49
		 * 7    65.            bring down next digit, 7 goes into 65, 9 times, floor(65/7) = 9
		 * 7   -63.            subtract 7 * 9 = 63
		 * 7     20            bring down next digit (which is zero), we cross the decimal point, 7 goes into 20, 2 times, floor(20/7) = 2
		 * 7    -14            subtract 7 * 2 = 14
		 * 7      60           bring down next digit (which is zero), 7 goes into 60, 8 times, floor(60/7) = 8
		 * 7     -56           subtract 7 * 8 = 56
		 * 7      .40          bring down next digit (which is zero), 7 goes into 40, 5 times, floor(40/7) = 5
		 * 7      .35          subtract 7 * 5 = 35
		 * 7      . 50         bring down next digit (which is zero), 7 goes into 50, 7 times, floor(50/7) = 7
		 * 7      .-49         subtract 7 * 7 = 49
		 * 7      .  10        bring down next digit (which is zero), 7 goes into 10, 1 times, floor(10/7) = 1
		 * 7      . - 7        subtract 7 * 1 = 7
		 * 7      .   30       bring down next digit (which is zero), 7 goes into 30, 4 times, floor(30/7) = 4
		 * 7      .  -28       subtract 7 * 4 = 28
		 * 7      .    20      *sigh* well this is going to repeat endlessly, but you get the point
		 *
		 * Warning: If you check this on your calculator, or on a web site, you're going to be shown a decimal that doesn't
		 *          repeat because at some point the calculator will stop and round to nearest. It may show "79.2857143"
		 *          or "79.2857142857142857" for example. Even this code will do that because there's only so many mantissa
		 *          bits in floating point. Other repeating decimal digit examples like 1 / 3 = 0.3333333333333333... will
		 *          have the same limitations. There are cases where, like 1/3, you have to do the math using fractions instead
		 *          of decimals to get a more accurate answer.
		 *
		 * What this code does is basically the same thing, but using binary digits instead of decimal. */

		r.exponent = op1.exponent - op2.exponent;

		uint64_t bset = (uint64_t)1u << (uint64_t)63u;
		uint64_t tmp = op1.mantissa;
		uint64_t cmv = op2.mantissa;

		/* both tmp and cnv have bit 63 set at this point */
		while (tmp < cmv) {
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

		/* rounding */
		if (tmp >= cmv) {
			if ((uint64_t)(++r.mantissa) == 0ull)
				r.exponent++;
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

