
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

static bool ast_constexpr_add_integer(token_t &tr,const token_t &top1,const token_t &top2) {
	const struct integer_value_t &op1 = top1.v.integer;
	const struct integer_value_t &op2 = top2.v.integer;
	struct integer_value_t &r = tr.v.integer;

	tr = top1;
	if ((op1.flags | op2.flags) & integer_value_t::FL_SIGNED) {
		r.flags |= integer_value_t::FL_SIGNED;
		r.v.s = op1.v.s + op2.v.s;
		if (r.v.s < op1.v.s) r.flags |= integer_value_t::FL_OVERFLOW;
	}
	else {
		r.v.u = op1.v.u + op2.v.u;
		if (r.v.u < op1.v.u) r.flags |= integer_value_t::FL_OVERFLOW;
	}

	return true;
}

bool ast_constexpr_add(token_t &r,const token_t &op1,const token_t &op2) {
	if (op1.type == op2.type) {
		switch (op1.type) {
			case token_type_t::integer:
				return ast_constexpr_add_integer(r,op1,op2);
			case token_type_t::floating:
				return ast_constexpr_addsub_floating(r,op1,op2,0);
			default:
				break;
		};
	}
	else if (ast_convert_promote_types(r,op1,op2,ast_constexpr_add)) {
		return true;
	}

	return false;
}

