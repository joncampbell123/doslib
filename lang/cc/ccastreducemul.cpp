
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

static bool ast_constexpr_multiply_integer(token_t &tr,const token_t &top1,const token_t &top2) {
	const struct integer_value_t &op1 = top1.v.integer;
	const struct integer_value_t &op2 = top2.v.integer;
	struct integer_value_t &r = tr.v.integer;

	tr = top1;

	if ((op1.flags | op2.flags) & integer_value_t::FL_SIGNED) {
		r.flags |= integer_value_t::FL_SIGNED;
		r.v.s *= op2.v.s;

		if (op2.v.s > 1ll) {
			const int64_t mv = INT64_MAX / op2.v.s;
			if (op1.v.s > mv) r.flags |= integer_value_t::FL_OVERFLOW;
		}
		else if (op2.v.s < -1ll) {
			const int64_t mv = INT64_MIN / -op2.v.s; /* NTS: negative / negative = positive, we want lowest negative number possible */
			if (op1.v.s < mv) r.flags |= integer_value_t::FL_OVERFLOW;
		}
	}
	else {
		r.v.u *= op2.v.u;

		if (op2.v.u > 1ull) {
			const uint64_t mv = UINT64_MAX / op2.v.u;
			if (op1.v.u > mv) r.flags |= integer_value_t::FL_OVERFLOW;
		}
	}

	return true;
}

static bool ast_constexpr_multiply_floating(token_t &tr,const token_t &top1,const token_t &top2) {
	const struct floating_value_t &op1 = top1.v.floating;
	const struct floating_value_t &op2 = top2.v.floating;
	struct floating_value_t &r = tr.v.floating;

	tr = top1;

	/* multiplication sign rules:
	 * ++ = +
	 * -- = +
	 * -+ = -
	 * +- = -
	 */
	r.flags ^= op2.flags & floating_value_t::FL_NEGATIVE;

	/* if either side is zero, the result is zero */
	if (op1.mantissa != 0ull && op2.mantissa != 0ull) {
		uint64_t rh,rl;

		/* result: 128-bit value, mantissa on bit 126 because 2^63 * 2^63 = 2^126 */
		multiply64x64to128(/*&*/rh,/*&*/rl,op1.mantissa,op2.mantissa);
		r.exponent = op1.exponent + op2.exponent + 1;
		r.mantissa = rh;
		if (r.mantissa == 0ull) {
			r.mantissa = rl;
			r.exponent -= 64;
			rl = 0;
		}

		if (r.mantissa != 0ull) {
			while (!(r.mantissa & floating_value_t::mant_msb)) {
				r.mantissa = (r.mantissa << 1ull) | (rl >> 63ull);
				r.exponent--;
				rl <<= 1ull;
			}
		}
		/* rounding */
		if (rl & floating_value_t::mant_msb) {
			if ((++r.mantissa) == (uint64_t)0ull) {
				r.mantissa = floating_value_t::mant_msb;
				r.exponent++;
			}
		}
	}
	else {
		r.exponent = 0;
		r.mantissa = 0;
	}

	return true;
}

bool ast_constexpr_multiply(token_t &r,const token_t &op1,const token_t &op2) {
	if (op1.type == op2.type) {
		switch (op1.type) {
			case token_type_t::integer:
				return ast_constexpr_multiply_integer(r,op1,op2);
			case token_type_t::floating:
				return ast_constexpr_multiply_floating(r,op1,op2);
			default:
				break;
		};
	}

	return false;
}

