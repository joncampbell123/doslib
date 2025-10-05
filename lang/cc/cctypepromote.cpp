
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

static bool ast_to_float(token_t &d,const token_t &s) {
	switch (s.type) {
		case token_type_t::integer:
			d = token_type_t(token_type_t::floating);
			if (s.v.integer.v.u != 0ull) {
				d.v.floating.flags = 0;
				d.v.floating.exponent = 63;
				if (s.v.integer.flags & integer_value_t::FL_SIGNED) {
					if (s.v.integer.v.s >= 0ll) {
						d.v.floating.mantissa = s.v.integer.v.s;
					}
					else {
						d.v.floating.mantissa = -s.v.integer.v.s;
						d.v.floating.flags |= floating_value_t::FL_NEGATIVE;
					}
				}
				else {
					d.v.floating.mantissa = s.v.integer.v.u;
				}
				d.v.floating.normalize();
			}
			return true;
		case token_type_t::floating:
			d = s;
			return true;
		default:
			break;
	}

	return false;
}

static token_type_t ast_convert_promote_pick_type(const token_t &op1,const token_t &op2) {
	if (	(op1.type == token_type_t::integer && op2.type == token_type_t::floating) ||
		(op1.type == token_type_t::floating && op2.type == token_type_t::integer))
		return token_type_t::floating;

	return token_type_t::none;
}

bool ast_convert_promote_types(token_t &r,const token_t &op1,const token_t &op2,bool (*func)(token_t &,const token_t &,const token_t &)) {
	const token_type_t cnvto = ast_convert_promote_pick_type(op1,op2);

	switch (cnvto) {
		case token_type_t::floating: {
			token_t c1,c2;
			if (!ast_to_float(c1,op1)) return false;
			if (!ast_to_float(c2,op2)) return false;
			return func(r,c1,c2); }
		default:
			break;
	}

	return false;
}

