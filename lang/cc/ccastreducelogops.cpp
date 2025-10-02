
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

static bool ast_constexpr_logical_not_integer(token_t &tr,const token_t &top) {
	const struct integer_value_t &op = top.v.integer;
	struct integer_value_t &r = tr.v.integer;

	tr = top;
	r.v.u = ast_constexpr_to_bool(op) ? 0 : 1;
	return true;
}

bool ast_constexpr_logical_not(token_t &r,const token_t &op) {
	switch (op.type) {
		case token_type_t::integer:
			r = token_t(token_type_t::integer);
			r.v.integer.flags = 0;
			r.v.integer.type = integer_value_t::type_t::BOOL;
			return ast_constexpr_logical_not_integer(r,op);
		default:
			break;
	};

	return false;
}

static bool ast_constexpr_logical_or_integer(token_t &tr,const token_t &top1,const token_t &top2) {
	const struct integer_value_t &op1 = top1.v.integer;
	const struct integer_value_t &op2 = top2.v.integer;
	struct integer_value_t &r = tr.v.integer;

	tr = top1;
	r.flags |= op2.flags & integer_value_t::FL_SIGNED;
	r.v.u = (ast_constexpr_to_bool(op1) || ast_constexpr_to_bool(op2)) ? 1 : 0;
	return true;
}

bool ast_constexpr_logical_or(token_t &r,token_t &op1,token_t &op2) {
	if (op1.type == op2.type) {
		switch (op1.type) {
			case token_type_t::integer:
				r = token_t(token_type_t::integer);
				r.v.integer.flags = 0;
				r.v.integer.type = integer_value_t::type_t::BOOL;
				return ast_constexpr_logical_or_integer(r,op1,op2);
			default:
				break;
		};
	}

	return false;
}

static bool ast_constexpr_logical_and_integer(token_t &tr,const token_t &top1,const token_t &top2) {
	const struct integer_value_t &op1 = top1.v.integer;
	const struct integer_value_t &op2 = top2.v.integer;
	struct integer_value_t &r = tr.v.integer;

	tr = top1;
	r.flags |= op2.flags & integer_value_t::FL_SIGNED;
	r.v.u = (ast_constexpr_to_bool(op1) && ast_constexpr_to_bool(op2)) ? 1 : 0;
	return true;
}

bool ast_constexpr_logical_and(token_t &r,token_t &op1,token_t &op2) {
	if (op1.type == op2.type) {
		switch (op1.type) {
			case token_type_t::integer:
				r = token_t(token_type_t::integer);
				r.v.integer.flags = 0;
				r.v.integer.type = integer_value_t::type_t::BOOL;
				return ast_constexpr_logical_and_integer(r,op1,op2);
			default:
				break;
		};
	}

	return false;
}

