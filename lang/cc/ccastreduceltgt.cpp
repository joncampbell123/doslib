
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

static bool ast_constexpr_lessthan_equals_integer(struct integer_value_t &r,const struct integer_value_t &op1,const struct integer_value_t &op2) {
	if ((op1.flags | op2.flags) & integer_value_t::FL_SIGNED)
		r.v.u = (op1.v.s <= op2.v.s) ? 1 : 0;
	else
		r.v.u = (op1.v.u <= op2.v.u) ? 1 : 0;

	return true;
}

bool ast_constexpr_lessthan_equals(token_t &r,const token_t &op1,const token_t &op2) {
	if (op1.type == op2.type) {
		switch (op1.type) {
			case token_type_t::integer:
				r = token_t(token_type_t::integer);
				r.v.integer.flags = 0;
				r.v.integer.type = integer_value_t::type_t::BOOL;
				return ast_constexpr_lessthan_equals_integer(r.v.integer,op1.v.integer,op2.v.integer);
			case token_type_t::floating:
				r = token_t(token_type_t::integer);
				r.v.integer.flags = 0;
				r.v.integer.type = integer_value_t::type_t::BOOL;
				r.v.integer.v.u = ast_fconst_cmp(op1.v.floating,op2.v.floating,CMPOP_LE);
				return true;
			default:
				break;
		};
	}

	return false;
}

static bool ast_constexpr_greaterthan_equals_integer(struct integer_value_t &r,const struct integer_value_t &op1,const struct integer_value_t &op2) {
	if ((op1.flags | op2.flags) & integer_value_t::FL_SIGNED)
		r.v.u = (op1.v.s >= op2.v.s) ? 1 : 0;
	else
		r.v.u = (op1.v.u >= op2.v.u) ? 1 : 0;

	return true;
}

bool ast_constexpr_greaterthan_equals(token_t &r,const token_t &op1,const token_t &op2) {
	if (op1.type == op2.type) {
		switch (op1.type) {
			case token_type_t::integer:
				r = token_t(token_type_t::integer);
				r.v.integer.flags = 0;
				r.v.integer.type = integer_value_t::type_t::BOOL;
				return ast_constexpr_greaterthan_equals_integer(r.v.integer,op1.v.integer,op2.v.integer);
			case token_type_t::floating:
				r = token_t(token_type_t::integer);
				r.v.integer.flags = 0;
				r.v.integer.type = integer_value_t::type_t::BOOL;
				r.v.integer.v.u = ast_fconst_cmp(op1.v.floating,op2.v.floating,CMPOP_GE);
				return true;
			default:
				break;
		};
	}

	return false;
}

static bool ast_constexpr_lessthan_integer(struct integer_value_t &r,const struct integer_value_t &op1,const struct integer_value_t &op2) {
	if ((op1.flags | op2.flags) & integer_value_t::FL_SIGNED)
		r.v.u = (op1.v.s < op2.v.s) ? 1 : 0;
	else
		r.v.u = (op1.v.u < op2.v.u) ? 1 : 0;

	return true;
}

bool ast_constexpr_lessthan(token_t &r,const token_t &op1,const token_t &op2) {
	if (op1.type == op2.type) {
		switch (op1.type) {
			case token_type_t::integer:
				r = token_t(token_type_t::integer);
				r.v.integer.flags = 0;
				r.v.integer.type = integer_value_t::type_t::BOOL;
				return ast_constexpr_lessthan_integer(r.v.integer,op1.v.integer,op2.v.integer);
			case token_type_t::floating:
				r = token_t(token_type_t::integer);
				r.v.integer.flags = 0;
				r.v.integer.type = integer_value_t::type_t::BOOL;
				r.v.integer.v.u = ast_fconst_cmp(op1.v.floating,op2.v.floating,CMPOP_LT);
				return true;
			default:
				break;
		};
	}

	return false;
}

static bool ast_constexpr_greaterthan_integer(struct integer_value_t &r,const struct integer_value_t &op1,const struct integer_value_t &op2) {
	if ((op1.flags | op2.flags) & integer_value_t::FL_SIGNED)
		r.v.u = (op1.v.s > op2.v.s) ? 1 : 0;
	else
		r.v.u = (op1.v.u > op2.v.u) ? 1 : 0;

	return true;
}

bool ast_constexpr_greaterthan(token_t &r,const token_t &op1,const token_t &op2) {
	if (op1.type == op2.type) {
		switch (op1.type) {
			case token_type_t::integer:
				r = token_t(token_type_t::integer);
				r.v.integer.flags = 0;
				r.v.integer.type = integer_value_t::type_t::BOOL;
				return ast_constexpr_greaterthan_integer(r.v.integer,op1.v.integer,op2.v.integer);
			case token_type_t::floating:
				r = token_t(token_type_t::integer);
				r.v.integer.flags = 0;
				r.v.integer.type = integer_value_t::type_t::BOOL;
				r.v.integer.v.u = ast_fconst_cmp(op1.v.floating,op2.v.floating,CMPOP_GT);
				return true;
			default:
				break;
		};
	}

	return false;
}

