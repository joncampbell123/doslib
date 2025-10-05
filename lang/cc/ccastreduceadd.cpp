
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

bool ast_to_float(token_t &d,const token_t &s) {
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
			fprintf(stderr,"%lld tofloat %.20f mant=%llx exp=%d\n",(signed long long)s.v.integer.v.s,ldexp(double(d.v.floating.mantissa),(int)d.v.floating.exponent-63),(unsigned long long)d.v.floating.mantissa,d.v.floating.exponent);
			return true;
		case token_type_t::floating:
			d = s;
			return true;
		default:
			break;
	}

	return false;
}

token_type_t ast_convert_promote_pick_type(const token_t &op1,const token_t &op2) {
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

