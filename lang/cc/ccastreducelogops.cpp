
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

bool ast_constexpr_logical_not(token_t &r,const token_t &op) {
	switch (op.type) {
		case token_type_t::integer:
		case token_type_t::floating:
			r = token_t(token_type_t::integer);
			r.v.integer.flags = 0;
			r.v.integer.type = integer_value_t::type_t::BOOL;
			r.v.integer.v.u = ast_constexpr_to_bool(op) ? 0 : 1;
			return true;
		default:
			break;
	};

	return false;
}

bool ast_constexpr_logical_or(token_t &r,token_t &op1,token_t &op2) {
	if (op1.type == op2.type) {
		switch (op1.type) {
			case token_type_t::integer:
			case token_type_t::floating:
				r = token_t(token_type_t::integer);
				r.v.integer.flags = 0;
				r.v.integer.type = integer_value_t::type_t::BOOL;
				r.v.integer.v.u = (ast_constexpr_to_bool(op1) || ast_constexpr_to_bool(op2)) ? 1 : 0;
				return true;
			default:
				break;
		};
	}

	return false;
}

bool ast_constexpr_logical_and(token_t &r,token_t &op1,token_t &op2) {
	if (op1.type == op2.type) {
		switch (op1.type) {
			case token_type_t::integer:
			case token_type_t::floating:
				r = token_t(token_type_t::integer);
				r.v.integer.flags = 0;
				r.v.integer.type = integer_value_t::type_t::BOOL;
				r.v.integer.v.u = (ast_constexpr_to_bool(op1) && ast_constexpr_to_bool(op2)) ? 1 : 0;
				return true;
			default:
				break;
		};
	}

	return false;
}

