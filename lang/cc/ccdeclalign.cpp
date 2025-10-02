
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

addrmask_t calc_alignofmask(declaration_specifiers_t &spec,ddip_list_t &ddip,size_t ptr_deref) {
	addrmask_t data_talign = addrmask_none;
	addrmask_t data_calcalign;

	if (spec.align != addrmask_none)
		data_talign = spec.align;
	else if ((spec.type_specifier & (TS_LONG|TS_DOUBLE)) == (TS_LONG|TS_DOUBLE))
		data_talign = data_types.dt_longdouble.t.align;
	else if (spec.type_specifier & TS_SZ8)
		data_talign = addrmask_make(1);
	else if (spec.type_specifier & TS_SZ16)
		data_talign = addrmask_make(2);
	else if (spec.type_specifier & TS_SZ32)
		data_talign = addrmask_make(4);
	else if (spec.type_specifier & TS_SZ64)
		data_talign = addrmask_make(8);
	else if (spec.type_specifier & TS_CHAR)
		data_talign = data_types.dt_char.t.align;
	else if (spec.type_specifier & TS_SHORT)
		data_talign = data_types.dt_short.t.align;
	else if (spec.type_specifier & TS_INT)
		data_talign = data_types.dt_int.t.align;
	else if (spec.type_specifier & TS_LONG)
		data_talign = data_types.dt_long.t.align;
	else if (spec.type_specifier & TS_LONGLONG)
		data_talign = data_types.dt_longlong.t.align;
	else if (spec.type_specifier & TS_FLOAT)
		data_talign = data_types.dt_float.t.align;
	else if (spec.type_specifier & TS_DOUBLE)
		data_talign = data_types.dt_double.t.align;
	else if (spec.type_specifier & (TS_ENUM|TS_MATCH_TYPEDEF|TS_STRUCT|TS_UNION)) {
		if (spec.type_identifier_symbol == symbol_none)
			return addrmask_none;

		symbol_t &sym = symbol(spec.type_identifier_symbol);
		if ((data_talign=calc_alignofmask(sym.spec,sym.ddip)) == addrmask_none)
			return addrmask_none;
	}

	data_calcalign = data_talign;
	if (ptr_deref == ptr_deref_sizeof_addressof) {
		/* & address of? That's a pointer, unconditionally */
		data_calcalign = data_types_ptr_data.dt_ptr.t.size;
	}
	else {
		if (!ddip.empty()) {
			size_t i = ddip.size() - 1u;
			do {
				bool ptrstop = false;
				auto &ent = ddip[i];
				size_t pi = 0;

				if ((ent.dd_flags & (declarator_t::FL_FUNCTION|declarator_t::FL_FUNCTION_POINTER)) == declarator_t::FL_FUNCTION)
					return data_size_none;

				if (!ent.arraydef.empty()) {
					for (const auto &a : ent.arraydef) {
						if (a == ast_node_none)
							return data_size_none;

						if (ptr_deref != 0) {
							ptr_deref--;
							continue;
						}
					}
				}

				for (pi=0;pi < ent.ptr.size();pi++) {
					if (ptr_deref != 0) {
						ptr_deref--;
						continue;
					}

					data_calcalign = data_types_ptr_data.dt_ptr.t.align;
					ptrstop = true;
					break;
				}

				if (ptrstop)
					break;
			} while ((i--) != 0u);
		}
	}

	return data_calcalign;
}

addrmask_t calc_alignof(declaration_specifiers_t &spec,ddip_list_t &ddip,size_t ptr_deref) {
	const addrmask_t r = calc_alignofmask(spec,ddip,ptr_deref);

	if (r != addrmask_none)
		return (~r) + addrmask_t(1u);

	return addrmask_none;
}

