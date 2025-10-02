
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

bool ast_constexpr_alignof(token_t &r,token_t &op,size_t ptr_deref) {
	switch (op.type) {
		case token_type_t::op_symbol:
			{
				if (op.v.symbol != symbol_none) {
					auto &sym = symbol(op.v.symbol);
					if (sym.sym_type == symbol_t::FUNCTION || sym.sym_type == symbol_t::NONE)
						return false;

					addrmask_t sz = calc_alignof(sym.spec,sym.ddip,ptr_deref);
					if (sz != addrmask_none) {
						r = token_t(token_type_t::integer);
						r.v.integer.v.u = sz;
						r.v.integer.flags = 0;
						return true;
					}
				}
				break;
			}
		case token_type_t::op_declaration:
			{
				declaration_t *decl = op.v.declaration;
				assert(decl != NULL);

				if (decl->declor.size() == 1) {
					addrmask_t sz = calc_alignof(decl->spec,decl->declor[0].ddip);
					if (sz != addrmask_none) {
						r = token_t(token_type_t::integer);
						r.v.integer.v.u = sz;
						r.v.integer.flags = 0;
						return true;
					}
				}
				break;
			}
		default:
			break;
	};

	return false;
}

