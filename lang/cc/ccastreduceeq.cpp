
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

static bool ast_constexpr_equals_integer(struct integer_value_t &r,const struct integer_value_t &op1,const struct integer_value_t &op2) {
	r.v.u = (op1.v.u == op2.v.u) ? 1 : 0;
	return true;
}

bool ast_constexpr_equals(token_t &r,const token_t &op1,const token_t &op2) {
	if (op1.type == op2.type) {
		switch (op1.type) {
			case token_type_t::integer:
				r = token_t(token_type_t::integer);
				r.v.integer.flags = 0;
				r.v.integer.type = integer_value_t::type_t::BOOL;
				return ast_constexpr_equals_integer(r.v.integer,op1.v.integer,op2.v.integer);
			default:
				break;
		};
	}

	return false;
}

static bool ast_constexpr_not_equals_integer(struct integer_value_t &r,const struct integer_value_t &op1,const struct integer_value_t &op2) {
	r.v.u = (op1.v.u != op2.v.u) ? 1 : 0;
	return true;
}

bool ast_constexpr_not_equals(token_t &r,const token_t &op1,const token_t &op2) {
	if (op1.type == op2.type) {
		switch (op1.type) {
			case token_type_t::integer:
				r = token_t(token_type_t::integer);
				r.v.integer.flags = 0;
				r.v.integer.type = integer_value_t::type_t::BOOL;
				return ast_constexpr_not_equals_integer(r.v.integer,op1.v.integer,op2.v.integer);
			default:
				break;
		};
	}

	return false;
}

