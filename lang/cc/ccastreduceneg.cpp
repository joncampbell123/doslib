
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

static bool ast_constexpr_negate_integer(token_t &tr,const token_t &top) {
	const struct integer_value_t &op = top.v.integer;
	struct integer_value_t &r = tr.v.integer;

	tr = top;
	r.v.s = -op.v.s;
	r.flags |= integer_value_t::FL_SIGNED;
	return true;
}

bool ast_constexpr_negate(token_t &r,const token_t &op) {
	switch (op.type) {
		case token_type_t::integer:
			return ast_constexpr_negate_integer(r,op);
		default:
			break;
	};

	return false;
}

