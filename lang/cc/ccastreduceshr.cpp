
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

static bool ast_constexpr_rightshift_integer(token_t &tr,const token_t &top1,const token_t &top2) {
	const struct integer_value_t &op1 = top1.v.integer;
	const struct integer_value_t &op2 = top2.v.integer;
	struct integer_value_t &r = tr.v.integer;

	r = op1;
	r.flags |= op2.flags & integer_value_t::FL_SIGNED;

	if (op2.v.s >= 0ll && op2.v.s <= 63ll) {
		if (r.flags & integer_value_t::FL_SIGNED)
			r.v.s = op1.v.s >> op2.v.s;
		else
			r.v.u = op1.v.u >> op2.v.u;
	}
	else {
		r.flags |= integer_value_t::FL_OVERFLOW;
	}

	return true;
}

bool ast_constexpr_rightshift(token_t &r,const token_t &op1,const token_t &op2) {
	if (op1.type == op2.type) {
		switch (op1.type) {
			case token_type_t::integer:
				return ast_constexpr_rightshift_integer(r,op1,op2);
			default:
				break;
		};
	}

	return false;
}

