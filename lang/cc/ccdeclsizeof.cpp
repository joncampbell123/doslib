
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

data_size_t calc_sizeof(declaration_specifiers_t &spec,ddip_list_t &ddip,size_t ptr_deref) {
	data_size_t data_tsz = data_size_none;
	data_size_t data_calcsz;

	if (spec.size != data_size_none)
		data_tsz = spec.size;
	else if ((spec.type_specifier & (TS_LONG|TS_DOUBLE)) == (TS_LONG|TS_DOUBLE))
		data_tsz = data_types.dt_longdouble.t.size;
	else if (spec.type_specifier & TS_SZ8)
		data_tsz = 1;
	else if (spec.type_specifier & TS_SZ16)
		data_tsz = 2;
	else if (spec.type_specifier & TS_SZ32)
		data_tsz = 4;
	else if (spec.type_specifier & TS_SZ64)
		data_tsz = 8;
	else if (spec.type_specifier & TS_CHAR)
		data_tsz = data_types.dt_char.t.size;
	else if (spec.type_specifier & TS_SHORT)
		data_tsz = data_types.dt_short.t.size;
	else if (spec.type_specifier & TS_INT)
		data_tsz = data_types.dt_int.t.size;
	else if (spec.type_specifier & TS_LONG)
		data_tsz = data_types.dt_long.t.size;
	else if (spec.type_specifier & TS_LONGLONG)
		data_tsz = data_types.dt_longlong.t.size;
	else if (spec.type_specifier & TS_FLOAT)
		data_tsz = data_types.dt_float.t.size;
	else if (spec.type_specifier & TS_DOUBLE)
		data_tsz = data_types.dt_double.t.size;
	else if (spec.type_specifier & (TS_ENUM|TS_MATCH_TYPEDEF|TS_STRUCT|TS_UNION)) {
		if (spec.type_identifier_symbol == symbol_none)
			return data_size_none;

		symbol_t &sym = symbol(spec.type_identifier_symbol);
		if ((data_tsz=calc_sizeof(sym.spec,sym.ddip)) == symbol_none)
			return data_size_none;
	}

	data_calcsz = data_tsz;
	if (ptr_deref == ptr_deref_sizeof_addressof) {
		/* & address of? That's a pointer, unconditionally */
		data_calcsz = data_types_ptr_data.dt_ptr.t.size;
	}
	else {
		if (!ddip.empty()) {
			size_t i = ddip.size() - 1u;
			do {
				data_size_t count = 1;
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

						const auto &an = ast_node(a);
						if (an.t.type != token_type_t::integer)
							return data_size_none;
						if (an.t.v.integer.v.v < 1ll)
							return data_size_none;
						if (an.t.v.integer.v.u >= (0x8000000000000000ull / count))
							return data_size_none;

						count *= an.t.v.integer.v.u;
					}
				}

				for (pi=0;pi < ent.ptr.size();pi++) {
					if (ptr_deref != 0) {
						ptr_deref--;
						continue;
					}

					data_calcsz = data_types_ptr_data.dt_ptr.t.size;
					ptrstop = true;
					break;
				}

				if (data_calcsz >= (0x8000000000000000ull / count))
					return data_size_none;
				if (data_calcsz != data_size_none)
					data_calcsz *= count;

				if (ptrstop)
					break;

#if 0
				fprintf(stderr,"dbg: calcsz=%zu count=%zu\n",data_calcsz,count);
				debug_dump_ddip("  ",ent);
#endif

			} while ((i--) != 0u);
		}

		if (ptr_deref != 0)
			return data_size_none;
	}

#if 0//test => 32, 8, 8, 4
	int **xyz[4];

	static_assert( sizeof(xyz) == sizeof(int*)*4, "oops" );
	static_assert( sizeof(*xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(**xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(***xyz) == sizeof(int), "oops" );
#endif

#if 0//test => 32, 8, 8, 4
	int **(**xyz[4])[4];

	static_assert( sizeof(xyz) == sizeof(int*)*4, "oops" );
	static_assert( sizeof(*xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(**xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(***xyz) == sizeof(int*)*4, "oops" );
	static_assert( sizeof(****xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(*****xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(******xyz) == sizeof(int), "oops" );
#endif

#if 0//test => 256, 32, 8, 8, 4
	typedef int **xyz_t[4];
	xyz_t xyz[4];

	static_assert( sizeof(xyz) == sizeof(int*)*4*4, "oops" );
	static_assert( sizeof(*xyz) == sizeof(int*)*4, "oops" );
	static_assert( sizeof(**xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(***xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(****xyz) == sizeof(int), "oops" );
#endif

#if 0//test => 256, 32, 8, 8, 4
	int **xyz[4][4];

	static_assert( sizeof(xyz) == sizeof(int*)*4*4, "oops" );
	static_assert( sizeof(*xyz) == sizeof(int*)*4, "oops" );
	static_assert( sizeof(**xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(***xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(****xyz) == sizeof(int), "oops" );
#endif

#if 0//test => 32, 8, 8, 4
	typedef int (**xyz_t);
	xyz_t xyz[4];

	static_assert( sizeof(xyz) == sizeof(int*)*4, "oops" );
	static_assert( sizeof(*xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(**xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(***xyz) == sizeof(int), "oops" );
#endif

#if 0//test => 32, 8, 8, 16, 4
	typedef int (**xyz_t)[4];
	xyz_t xyz[4];

	static_assert( sizeof(xyz) == sizeof(int*)*4, "oops" );
	static_assert( sizeof(*xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(**xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(***xyz) == sizeof(int)*4, "oops" );
	static_assert( sizeof(****xyz) == sizeof(int), "oops" );
#endif

#if 0//test => 8, 8, 16, 4
	typedef int (**xyz_t)[4];
	xyz_t xyz;

	static_assert( sizeof(xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(*xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(**xyz) == sizeof(int)*4, "oops" );
	static_assert( sizeof(***xyz) == sizeof(int), "oops" );
#endif

#if 0//test => 32, 8, 8, 4
	typedef int **xyz_t;
	xyz_t xyz[4];

	static_assert( sizeof(xyz) == sizeof(int*)*4, "oops" );
	static_assert( sizeof(*xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(**xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(***xyz) == sizeof(int), "oops" );
#endif

#if 0//test => 8, 8, 16, 4
	int (**xyz)[4];

	static_assert( sizeof(xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(*xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(**xyz) == sizeof(int)*4, "oops" );
	static_assert( sizeof(***xyz) == sizeof(int), "oops" );
#endif

#if 0//test => 40, 8, 8, 48, 12, 4
	int ((**xyz[5])[4])[3];

	static_assert( sizeof(xyz) == sizeof(int*)*5, "oops" );
	static_assert( sizeof(*xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(**xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(***xyz) == sizeof(int)*4*3, "oops" );
	static_assert( sizeof(****xyz) == sizeof(int)*3, "oops" );
	static_assert( sizeof(*****xyz) == sizeof(int), "oops" );
#endif

#if 0//test => 40, 8, 8, 96, 24, 8, 8, 4
	int **((**xyz[5])[4])[3];

	static_assert( sizeof(xyz) == sizeof(int*)*5, "oops" );
	static_assert( sizeof(*xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(**xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(***xyz) == sizeof(int*)*4*3, "oops" );
	static_assert( sizeof(****xyz) == sizeof(int*)*3, "oops" );
	static_assert( sizeof(*****xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(******xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(*******xyz) == sizeof(int), "oops" );
#endif

#if 0//test => 8, 8, 96, 32, 8, 8, 4
	int **((**xyz)[4])[3];

	static_assert( sizeof(xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(*xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(**xyz) == sizeof(int*)*4*3, "oops" );
	static_assert( sizeof(***xyz) == sizeof(int*)*3, "oops" );
	static_assert( sizeof(****xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(*****xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(******xyz) == sizeof(int), "oops" );
#endif

#if 0//test => 8, 8, 32, 8, 8, 12, 4
	int (**(**xyz)[4])[3];

	static_assert( sizeof(xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(*xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(**xyz) == sizeof(int*)*4, "oops" );
	static_assert( sizeof(***xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(****xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(*****xyz) == sizeof(int)*3, "oops" );
	static_assert( sizeof(******xyz) == sizeof(int), "oops" );
#endif

#if 0//test => 32, 8, 8, 8, 8, 4
	typedef int **(**xyz_t);
	xyz_t xyz[4];

	static_assert( sizeof(xyz) == sizeof(int*)*4, "oops" );
	static_assert( sizeof(*xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(**xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(***xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(****xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(*****xyz) == sizeof(int), "oops" );
#endif

#if 0//test => 32, 8, 8, 8, 8, 4
	typedef int ****xyz_t;
	xyz_t xyz[4];

	static_assert( sizeof(xyz) == sizeof(int*)*4, "oops" );
	static_assert( sizeof(*xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(**xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(***xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(****xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(*****xyz) == sizeof(int), "oops" );
#endif

#if 0//test => 8, 8, 8, 8, 16, 4
	int (**(**xyz))[4];

	static_assert( sizeof(xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(*xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(**xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(***xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(****xyz) == sizeof(int)*4, "oops" );
	static_assert( sizeof(*****xyz) == sizeof(int), "oops" );
#endif

#if 0//test => 8, 8, 8, 8, 16, 4
	int (****xyz)[4];

	static_assert( sizeof(xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(*xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(**xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(***xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(****xyz) == sizeof(int)*4, "oops" );
	static_assert( sizeof(*****xyz) == sizeof(int), "oops" );
#endif

#if 0//test => 8, 8, 32, 8, 8, 4
	int **(**xyz)[4];

	static_assert( sizeof(xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(*xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(**xyz) == sizeof(int*)*4, "oops" );
	static_assert( sizeof(***xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(****xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(*****xyz) == sizeof(int), "oops" );
#endif

#if 0//test => 32, 8, 8, 8, 8, 4
	int **(**xyz[4]);

	static_assert( sizeof(xyz) == sizeof(int*)*4, "oops" );
	static_assert( sizeof(*xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(**xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(***xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(****xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(*****xyz) == sizeof(int), "oops" );
#endif

#if 0//test => 32, 8, 8, 8, 8, 4
	int ****xyz[4];

	static_assert( sizeof(xyz) == sizeof(int*)*4, "oops" );
	static_assert( sizeof(*xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(**xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(***xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(****xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(*****xyz) == sizeof(int), "oops" );
#endif

#if 0//test => 96, 24, 8, 8, 8, 4
	int ****xyz[4][3];

	static_assert( sizeof(xyz) == sizeof(int*)*4*3, "oops" );
	static_assert( sizeof(*xyz) == sizeof(int*)*3, "oops" );
	static_assert( sizeof(**xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(***xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(****xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(*****xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(******xyz) == sizeof(int), "oops" );
#endif

#if 0//test => 1680
	typedef int (** (** (*xyz)[5])[6])[7];

	static_assert( sizeof(xyz) == sizeof(int*)*5*6*7, "oops" );
#endif

#if 0//test => 8, 40, 8, 8, 48, 8, 8, 28, 4
	typedef int (** (** (*xyz_t)[5])[6])[7];
	xyz_t xyz;

	static_assert( sizeof(xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(*xyz) == sizeof(int*)*5, "oops" );
	static_assert( sizeof(**xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(***xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(****xyz) == sizeof(int*)*6, "oops" );
	static_assert( sizeof(*****xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(******xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(*******xyz) == sizeof(int)*7, "oops" );
	static_assert( sizeof(********xyz) == sizeof(int), "oops" );
#endif

#if 0//test => 96, 24, 8, 8, 8, 4
	typedef int (** (** (*xyz_t)[5])[6])[7];
	xyz_t *xyz;

	static_assert( sizeof(xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(*xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(**xyz) == sizeof(int*)*5, "oops" );
	static_assert( sizeof(***xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(****xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(*****xyz) == sizeof(int*)*6, "oops" );
	static_assert( sizeof(******xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(*******xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(********xyz) == sizeof(int)*7, "oops" );
	static_assert( sizeof(*********xyz) == sizeof(int), "oops" );
#endif

#if 0//test => 96, 24, 8, 8, 8, 4
	typedef int (** (** (*xyz_t)[5])[6])[7];
	xyz_t xyz[4];

	static_assert( sizeof(xyz) == sizeof(int*)*4, "oops" );
	static_assert( sizeof(*xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(**xyz) == sizeof(int*)*5, "oops" );
	static_assert( sizeof(***xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(****xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(*****xyz) == sizeof(int*)*6, "oops" );
	static_assert( sizeof(******xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(*******xyz) == sizeof(int*), "oops" );
	static_assert( sizeof(********xyz) == sizeof(int)*7, "oops" );
	static_assert( sizeof(*********xyz) == sizeof(int), "oops" );
#endif

#if 0
	fprintf(stderr,"dbg final: calcsz=%zu count=%zu\n",data_calcsz,count);
#endif

	if (data_calcsz != data_size_none)
		return data_calcsz;

	return data_size_none;
}

