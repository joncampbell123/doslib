
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

bool ast_constexpr_to_bool(const integer_value_t &iv) {
	return iv.to_bool();
}

bool ast_constexpr_to_bool(const floating_value_t &iv) {
	return iv.to_bool();
}

bool ast_constexpr_to_bool(const token_t &t) {
	switch (t.type) {
		case token_type_t::integer:
			return ast_constexpr_to_bool(t.v.integer);
		case token_type_t::floating:
			return ast_constexpr_to_bool(t.v.floating);
		default:
			break;
	};

	return false;
}

