
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

static bool ast_constexpr_binary_not_integer(token_t &tr,const token_t &top) {
	const struct integer_value_t &op = top.v.integer;
	struct integer_value_t &r = tr.v.integer;

	tr = top;
	r.v.u = ~op.v.u;
	return true;
}

bool ast_constexpr_binary_not(token_t &r,const token_t &op) {
	switch (op.type) {
		case token_type_t::integer:
			return ast_constexpr_binary_not_integer(r,op);
		default:
			break;
	};

	return false;
}

static bool ast_constexpr_binary_or_integer(token_t &tr,const token_t &top1,const token_t &top2) {
	const struct integer_value_t &op1 = top1.v.integer;
	const struct integer_value_t &op2 = top2.v.integer;
	struct integer_value_t &r = tr.v.integer;

	tr = top1;
	r.v.u = op1.v.u | op2.v.u;
	r.flags |= op2.flags & integer_value_t::FL_SIGNED;
	return true;
}

bool ast_constexpr_binary_or(token_t &r,const token_t &op1,const token_t &op2) {
	if (op1.type == op2.type) {
		switch (op1.type) {
			case token_type_t::integer:
				return ast_constexpr_binary_or_integer(r,op1,op2);
			default:
				break;
		};
	}

	return false;
}

static bool ast_constexpr_binary_xor_integer(token_t &tr,const token_t &top1,const token_t &top2) {
	const struct integer_value_t &op1 = top1.v.integer;
	const struct integer_value_t &op2 = top2.v.integer;
	struct integer_value_t &r = tr.v.integer;

	tr = top1;
	r.v.u = op1.v.u ^ op2.v.u;
	r.flags |= op2.flags & integer_value_t::FL_SIGNED;
	return true;
}

bool ast_constexpr_binary_xor(token_t &r,token_t &op1,token_t &op2) {
	if (op1.type == op2.type) {
		switch (op1.type) {
			case token_type_t::integer:
				return ast_constexpr_binary_xor_integer(r,op1,op2);
			default:
				break;
		};
	}

	return false;
}

