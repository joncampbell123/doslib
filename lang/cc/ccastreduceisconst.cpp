
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

bool is_ast_strconstexpr(token_t &t) {
	switch (t.type) {
		case token_type_t::strliteral:
			return true;
		case token_type_t::op_symbol:
			if (t.v.symbol != symbol_none) {
				const symbol_t &so = symbol(t.v.symbol);
				if (so.sym_type == symbol_t::STR && (so.spec.type_qualifier & TQ_CONST))
					return true;
			}
		default:
			break;
	};

	return false;
}

bool is_ast_constexpr(token_t &t) {
	switch (t.type) {
		case token_type_t::integer:
		case token_type_t::floating:
			return true;
		default:
			break;
	};

	return false;
}

