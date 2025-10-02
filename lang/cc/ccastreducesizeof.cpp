
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

bool ast_constexpr_sizeof_calc_common(token_t &r,declaration_specifiers_t &spec,ddip_list_t &ddip,size_t ptr_deref=0) {
	data_size_t sz = calc_sizeof(spec,ddip,ptr_deref);
	if (sz != data_size_none) {
		r = token_t(token_type_t::integer);
		r.v.integer.flags = 0;
		r.v.integer.v.u = sz;
		return true;
	}

	return false;
}

bool ast_constexpr_sizeof_symbol(token_t &r,const symbol_id_t symbol_id,size_t ptr_deref) {
	if (symbol_id != symbol_none) {
		auto &sym = symbol(symbol_id);
		if (sym.sym_type == symbol_t::FUNCTION || sym.sym_type == symbol_t::NONE)
			return false;

		return ast_constexpr_sizeof_calc_common(r,sym.spec,sym.ddip,ptr_deref);
	}

	return false;
}

bool ast_constexpr_sizeof_declaration(token_t &r,declaration_t *decl) {
	if (decl) {
		if (decl->declor.size() == 1) {
			return ast_constexpr_sizeof_calc_common(r,decl->spec,decl->declor[0].ddip);
		}
	}

	return false;
}

bool ast_constexpr_sizeof(token_t &r,token_t &op,size_t ptr_deref) {
	switch (op.type) {
		case token_type_t::op_symbol:
			return ast_constexpr_sizeof_symbol(r,op.v.symbol,ptr_deref);
		case token_type_t::op_declaration:
			return ast_constexpr_sizeof_declaration(r,op.v.declaration);
		default:
			break;
	};

	return false;
}

