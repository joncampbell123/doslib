
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

bool ast_constexpr_multiply(token_t &r,const token_t &op1,const token_t &op2) {
	if (op1.type == op2.type) {
		switch (op1.type) {
			case token_type_t::integer:
				return ast_constexpr_multiply_integer(r,op1,op2);
			default:
				break;
		};
	}

	return false;
}

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

bool ast_constexpr_divide(token_t &r,const token_t &op1,const token_t &op2) {
	if (op1.type == op2.type) {
		switch (op1.type) {
			case token_type_t::integer:
				return ast_constexpr_divide_integer(r,op1,op2);
			default:
				break;
		};
	}

	return false;
}

static bool ast_constexpr_modulus_integer(token_t &tr,const token_t &top1,const token_t &top2) {
	const struct integer_value_t &op1 = top1.v.integer;
	const struct integer_value_t &op2 = top2.v.integer;
	struct integer_value_t &r = tr.v.integer;

	tr = top1;

	/* divide by zero? NOPE! */
	if (op2.v.u == 0ull)
		return false;

	if ((op1.flags | op2.flags) & integer_value_t::FL_SIGNED) {
		r.flags |= integer_value_t::FL_SIGNED;
		r.v.s = op1.v.s % op2.v.s;
	}
	else {
		r.v.u = op1.v.u % op2.v.u;
	}

	return true;
}

bool ast_constexpr_modulus(token_t &r,token_t &op1,token_t &op2) {
	if (op1.type == op2.type) {
		switch (op1.type) {
			case token_type_t::integer:
				return ast_constexpr_modulus_integer(r,op1,op2);
			default:
				break;
		};
	}

	return false;
}

