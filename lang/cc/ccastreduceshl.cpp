
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

static bool ast_constexpr_leftshift_integer(token_t &tr,const token_t &top1,const token_t &top2) {
	const struct integer_value_t &op1 = top1.v.integer;
	const struct integer_value_t &op2 = top2.v.integer;
	struct integer_value_t &r = tr.v.integer;

	tr = top1;
	r.flags |= op2.flags & integer_value_t::FL_SIGNED;

	if (op2.v.v >= 0ll && op2.v.v <= 63ll) {
		if (op2.v.u != 0ull) {
			const uint64_t chkmsk = (uint64_t)(UINT64_MAX) << (uint64_t)(64ull - op2.v.u);
			if (op1.v.u & chkmsk) r.flags |= integer_value_t::FL_OVERFLOW;
			r.v.u = op1.v.u << op2.v.u;
		}
	}
	else {
		r.flags |= integer_value_t::FL_OVERFLOW;
	}

	return true;
}

bool ast_constexpr_leftshift(token_t &r,const token_t &op1,const token_t &op2) {
	if (op1.type == op2.type) {
		switch (op1.type) {
			case token_type_t::integer:
				return ast_constexpr_leftshift_integer(r,op1,op2);
			default:
				break;
		};
	}

	return false;
}

