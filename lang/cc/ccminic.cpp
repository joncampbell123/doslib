
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

data_size_t calc_sizeof(declaration_specifiers_t &spec,ddip_list_t &ddip,size_t ptr_deref=0) {
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

//////////////////////////////////////////////////////////////////////////////

void debug_dump_ast(const std::string prefix,ast_node_id_t r);
void debug_dump_general(const std::string prefix,const std::string &name=std::string());
void debug_dump_declaration_specifiers(const std::string prefix,declaration_specifiers_t &ds);
void debug_dump_data_type_set(const std::string prefix,const data_type_set_t &dts,const std::string &name=std::string());
void debug_dump_data_type_set_ptr(const std::string prefix,const data_type_set_ptr_t &dtsp,const std::string &name=std::string());
void debug_dump_var_type(const std::string prefix,const data_var_type_t &dt,const std::string &name=std::string());
void debug_dump_ptr_type(const std::string prefix,const data_ptr_type_t &dt,const std::string &name=std::string());
void debug_dump_declarator(const std::string prefix,declarator_t &declr,const std::string &name=std::string());
void debug_dump_declaration(const std::string prefix,declaration_t &decl,const std::string &name=std::string());
void debug_dump_declaration_specifier_flags(const std::string prefix,const unsigned int flags,const std::string &name=std::string());
void debug_dump_pointer(const std::string prefix,std::vector<pointer_t> &ptr,const std::string &name=std::string());
void debug_dump_direct_declarator(const std::string prefix,declarator_t &ddecl,const std::string &name=std::string());
void debug_dump_arraydef(const std::string prefix,std::vector<ast_node_id_t> &arraydef,const std::string &name=std::string());
void debug_dump_parameter(const std::string prefix,parameter_t &p,const std::string &name=std::string());
void debug_dump_structfield(const std::string prefix,structfield_t &f,const std::string &name=std::string());
void debug_dump_ddip(const std::string prefix,ddip_t &ddip,const std::string &name=std::string());
void debug_dump_ddip(const std::string prefix,ddip_list_t &ddip,const std::string &name=std::string());
void debug_dump_symbol(const std::string prefix,symbol_t &sym,const std::string &name=std::string());
void debug_dump_symbol_table(const std::string prefix,const std::string &name=std::string());
void debug_dump_scope(const std::string prefix,scope_t &sco,const std::string &name=std::string());
void debug_dump_scope_table(const std::string prefix,const std::string &name=std::string());
void debug_dump_segment(const std::string prefix,segment_t &s,const std::string &name=std::string());
void debug_dump_segment_table(const std::string prefix,const std::string &name=std::string());
void debug_dump_enumerator(const std::string prefix,enumerator_t &en);

////////////////////////////////////////////////////////////////////

bool arrange_symbols(void) {
	for (auto &sg : segments) {
		if (sg.align == addrmask_none)
			sg.align = addrmask_make(1);
	}

	for (auto &sym : symbols) {
		if (sym.part_of_segment == segment_none)
			sym.part_of_segment = decide_sym_segment(sym);

		if (sym.part_of_segment != segment_none) {
			segment_t &se = segref(sym.part_of_segment);

			const data_size_t sz = calc_sizeof(sym.spec,sym.ddip);
			const addrmask_t am = calc_alignofmask(sym.spec,sym.ddip);

			if ((sym.flags & symbol_t::FL_DEFINED) &&
					(sym.flags & (symbol_t::FL_PARAMETER|symbol_t::FL_STACK)) == 0 &&
					!(sym.part_of_segment == stack_segment) && sz != data_size_none &&
					am != addrmask_none) {

				if (sym.sym_type == symbol_t::VARIABLE ||
						sym.sym_type == symbol_t::CONST ||
						sym.sym_type == symbol_t::STR) {

					/* stack must align to largest alignment of symbol */
					se.align &= am;
				}
			}
		}
	}

	return true;
}

////////////////////////////////////////////////////////////////////

bool cc_init(void) {
	assert(scopes.empty());
	assert(scope_global == scope_id_t(0));
	scopes.resize(scope_global+1); // make sure global scope exists

	assert(scope_stack.empty());
	scope_stack.push_back(scope_global);

	/* NTS: GCC x86_64 doesn't enforce a maximum packing by default, only if you use #pragma pack
	 *      Microsoft C++ uses a default maximum according to /Zp or the default of 8 */
	current_packing = default_packing;

	{
		const segment_id_t s = code_segment = new_segment(); segment_t &so = segref(s);
		const identifier_id_t i = so.name = identifier.alloc(); identifier_t &io = identifier(i);
		so.type = segment_t::type_t::CODE;
		so.flags = segment_t::FL_READABLE | segment_t::FL_EXECUTABLE;
		default_segment_setup(so);
		io.copy_from("CODE");
	}

	{
		const segment_id_t s = const_segment = new_segment(); segment_t &so = segref(s);
		const identifier_id_t i = so.name = identifier.alloc(); identifier_t &io = identifier(i);
		so.type = segment_t::type_t::CONST;
		so.flags = segment_t::FL_READABLE;
		default_segment_setup(so);
		io.copy_from("CONST");
	}

	{
		const segment_id_t s = conststr_segment = new_segment(); segment_t &so = segref(s);
		const identifier_id_t i = so.name = identifier.alloc(); identifier_t &io = identifier(i);
		so.type = segment_t::type_t::CONST;
		so.flags = segment_t::FL_READABLE;
		default_segment_setup(so);
		io.copy_from("CONST:str");
	}

	{
		const segment_id_t s = data_segment = new_segment(); segment_t &so = segref(s);
		const identifier_id_t i = so.name = identifier.alloc(); identifier_t &io = identifier(i);
		so.type = segment_t::type_t::DATA;
		so.flags = segment_t::FL_READABLE | segment_t::FL_WRITEABLE;
		default_segment_setup(so);
		io.copy_from("DATA");
	}

	{
		const segment_id_t s = stack_segment = new_segment(); segment_t &so = segref(s);
		const identifier_id_t i = so.name = identifier.alloc(); identifier_t &io = identifier(i);
		so.type = segment_t::type_t::STACK;
		so.flags = segment_t::FL_READABLE | segment_t::FL_WRITEABLE | segment_t::FL_NOTINEXE;
		default_segment_setup(so);
		io.copy_from("STACK");
	}

	{
		const segment_id_t s = bss_segment = new_segment(); segment_t &so = segref(s);
		const identifier_id_t i = so.name = identifier.alloc(); identifier_t &io = identifier(i);
		so.type = segment_t::type_t::BSS;
		so.flags = segment_t::FL_READABLE | segment_t::FL_WRITEABLE | segment_t::FL_NOTINEXE;
		default_segment_setup(so);
		io.copy_from("BSS");
	}

	{
		const segment_id_t s = fardata_segment = new_segment(); segment_t &so = segref(s);
		const identifier_id_t i = so.name = identifier.alloc(); identifier_t &io = identifier(i);
		so.type = segment_t::type_t::DATA;
		so.flags = segment_t::FL_READABLE | segment_t::FL_WRITEABLE;
		default_segment_setup(so);
		io.copy_from("FARDATA");
	}

	return true;
}

////////////////////////////////////////////////////////////////////

bool is_ast_strconstexpr(token_t &t) {
	switch (t.type) {
		case token_type_t::strliteral:
			return true;
		case token_type_t::op_symbol:
			if (t.v.symbol != symbol_none) {
				symbol_t &so = symbol(t.v.symbol);
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
			return true;
		default:
			break;
	};

	return false;
}

bool ast_constexpr_to_bool(integer_value_t &iv) {
	return iv.v.u != 0ull;
}

bool ast_constexpr_to_bool(token_t &t) {
	switch (t.type) {
		case token_type_t::integer:
			return ast_constexpr_to_bool(t.v.integer);
		default:
			break;
	};

	return false;
}

bool ast_constexpr_sizeof(token_t &r,token_t &op,size_t ptr_deref) {
	switch (op.type) {
		case token_type_t::op_symbol:
			{
				if (op.v.symbol != symbol_none) {
					auto &sym = symbol(op.v.symbol);
					if (sym.sym_type == symbol_t::FUNCTION || sym.sym_type == symbol_t::NONE)
						return false;

					data_size_t sz = calc_sizeof(sym.spec,sym.ddip,ptr_deref);
					if (sz != data_size_none) {
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
					data_size_t sz = calc_sizeof(decl->spec,decl->declor[0].ddip);
					if (sz != data_size_none) {
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

bool ast_constexpr_leftshift(token_t &r,token_t &op1,token_t &op2) {
	/* TODO: type promotion/conversion */
	if (op1.type == op2.type) {
		switch (op1.type) {
			case token_type_t::integer:
				/* negative or over-large shift? NOPE.
				 * That is undefined behavior. On Intel CPUs other than the 8086 for example,
				 * the shift instructions only use the number of LSB bits relevent to the data
				 * type being shifted. For example shifting an 8-bit value only uses the low 3 bits (0-7),
				 * 16-bit the low 4 bits (0-15), 32-bit the low 5 bits (0-31), and so on.
				 * TODO: If too large for the target interger data type, not the 64-bit integers we use here. */
				if (op2.v.integer.v.v < 0ll || op2.v.integer.v.v >= 63ll)
					return false;

				r = op1;
				if (op1.v.integer.flags & integer_value_t::FL_SIGNED)
					r.v.integer.v.u = op1.v.integer.v.v << op2.v.integer.v.v;
				else
					r.v.integer.v.u = op1.v.integer.v.u << op2.v.integer.v.u;
				return true;
			default:
				break;
		};
	}

	return false;
}

bool ast_constexpr_rightshift(token_t &r,token_t &op1,token_t &op2) {
	/* TODO: type promotion/conversion */
	if (op1.type == op2.type) {
		switch (op1.type) {
			case token_type_t::integer:
				/* negative or over-large shift? NOPE.
				 * That is undefined behavior. On Intel CPUs other than the 8086 for example,
				 * the shift instructions only use the number of LSB bits relevent to the data
				 * type being shifted. For example shifting an 8-bit value only uses the low 3 bits (0-7),
				 * 16-bit the low 4 bits (0-15), 32-bit the low 5 bits (0-31), and so on.
				 * TODO: If too large for the target interger data type, not the 64-bit integers we use here. */
				if (op2.v.integer.v.v < 0ll || op2.v.integer.v.v >= 63ll)
					return false;

				r = op1;
				if (op1.v.integer.flags & integer_value_t::FL_SIGNED)
					r.v.integer.v.u = op1.v.integer.v.v >> op2.v.integer.v.v;
				else
					r.v.integer.v.u = op1.v.integer.v.u >> op2.v.integer.v.u;
				return true;
			default:
				break;
		};
	}

	return false;
}

bool ast_constexpr_lessthan_equals(token_t &r,token_t &op1,token_t &op2) {
	/* TODO: type promotion/conversion */
	if (op1.type == op2.type) {
		switch (op1.type) {
			case token_type_t::integer:
				r = op1;
				if (op1.v.integer.flags & integer_value_t::FL_SIGNED)
					r.v.integer.v.u = (op1.v.integer.v.v <= op2.v.integer.v.v) ? 1 : 0;
				else
					r.v.integer.v.u = (op1.v.integer.v.u <= op2.v.integer.v.u) ? 1 : 0;
				return true;
			default:
				break;
		};
	}

	return false;
}

bool ast_constexpr_greaterthan_equals(token_t &r,token_t &op1,token_t &op2) {
	/* TODO: type promotion/conversion */
	if (op1.type == op2.type) {
		switch (op1.type) {
			case token_type_t::integer:
				r = op1;
				if (op1.v.integer.flags & integer_value_t::FL_SIGNED)
					r.v.integer.v.u = (op1.v.integer.v.v >= op2.v.integer.v.v) ? 1 : 0;
				else
					r.v.integer.v.u = (op1.v.integer.v.u >= op2.v.integer.v.u) ? 1 : 0;
				return true;
			default:
				break;
		};
	}

	return false;
}

bool ast_constexpr_lessthan(token_t &r,token_t &op1,token_t &op2) {
	/* TODO: type promotion/conversion */
	if (op1.type == op2.type) {
		switch (op1.type) {
			case token_type_t::integer:
				r = op1;
				if (op1.v.integer.flags & integer_value_t::FL_SIGNED)
					r.v.integer.v.u = (op1.v.integer.v.v < op2.v.integer.v.v) ? 1 : 0;
				else
					r.v.integer.v.u = (op1.v.integer.v.u < op2.v.integer.v.u) ? 1 : 0;
				return true;
			default:
				break;
		};
	}

	return false;
}

bool ast_constexpr_greaterthan(token_t &r,token_t &op1,token_t &op2) {
	/* TODO: type promotion/conversion */
	if (op1.type == op2.type) {
		switch (op1.type) {
			case token_type_t::integer:
				r = op1;
				if (op1.v.integer.flags & integer_value_t::FL_SIGNED)
					r.v.integer.v.u = (op1.v.integer.v.v > op2.v.integer.v.v) ? 1 : 0;
				else
					r.v.integer.v.u = (op1.v.integer.v.u > op2.v.integer.v.u) ? 1 : 0;
				return true;
			default:
				break;
		};
	}

	return false;
}

bool ast_constexpr_equals(token_t &r,token_t &op1,token_t &op2) {
	/* TODO: type promotion/conversion */
	if (op1.type == op2.type) {
		switch (op1.type) {
			case token_type_t::integer:
				r = op1;
				r.v.integer.v.u = (op1.v.integer.v.u == op2.v.integer.v.u) ? 1 : 0;
				return true;
			default:
				break;
		};
	}

	return false;
}

bool ast_constexpr_not_equals(token_t &r,token_t &op1,token_t &op2) {
	/* TODO: type promotion/conversion */
	if (op1.type == op2.type) {
		switch (op1.type) {
			case token_type_t::integer:
				r = op1;
				r.v.integer.v.u = (op1.v.integer.v.u != op2.v.integer.v.u) ? 1 : 0;
				return true;
			default:
				break;
		};
	}

	return false;
}

bool ast_constexpr_binary_not(token_t &r,token_t &op) {
	/* TODO: type promotion/conversion */
	switch (op.type) {
		case token_type_t::integer:
			r = op;
			r.v.integer.v.u = ~r.v.integer.v.u;
			return true;
		default:
			break;
	};

	return false;
}

bool ast_constexpr_logical_not(token_t &r,token_t &op) {
	/* TODO: type promotion/conversion */
	switch (op.type) {
		case token_type_t::integer:
			r = op;
			r.v.integer.v.u = ast_constexpr_to_bool(op.v.integer) ? 0 : 1;
			return true;
		default:
			break;
	};

	return false;
}

bool ast_constexpr_negate(token_t &r,token_t &op) {
	/* TODO: type promotion/conversion */
	switch (op.type) {
		case token_type_t::integer:
			r = op;
			r.v.integer.v.v = -r.v.integer.v.v;
			r.v.integer.flags |= integer_value_t::FL_SIGNED;
			return true;
		default:
			break;
	};

	return false;
}

bool ast_constexpr_add(token_t &r,token_t &op1,token_t &op2) {
	/* TODO: type promotion/conversion */
	if (op1.type == op2.type) {
		switch (op1.type) {
			case token_type_t::integer:
				r = op1;
				r.v.integer.v.u += op2.v.integer.v.u;
				return true;
			default:
				break;
		};
	}

	return false;
}

bool ast_constexpr_subtract(token_t &r,token_t &op1,token_t &op2) {
	/* TODO: type promotion/conversion */
	if (op1.type == op2.type) {
		switch (op1.type) {
			case token_type_t::integer:
				r = op1;
				r.v.integer.v.u -= op2.v.integer.v.u;
				return true;
			default:
				break;
		};
	}

	return false;
}

bool ast_constexpr_logical_or(token_t &r,token_t &op1,token_t &op2) {
	/* TODO: type promotion/conversion */
	if (op1.type == op2.type) {
		switch (op1.type) {
			case token_type_t::integer:
				r = op1;
				r.v.integer.v.u = (ast_constexpr_to_bool(op1.v.integer) || ast_constexpr_to_bool(op2.v.integer)) ? 1 : 0;
				return true;
			default:
				break;
		};
	}

	return false;
}

bool ast_constexpr_binary_or(token_t &r,token_t &op1,token_t &op2) {
	/* TODO: type promotion/conversion */
	if (op1.type == op2.type) {
		switch (op1.type) {
			case token_type_t::integer:
				r = op1;
				r.v.integer.v.v |= op2.v.integer.v.v;
				return true;
			default:
				break;
		};
	}

	return false;
}

bool ast_constexpr_binary_xor(token_t &r,token_t &op1,token_t &op2) {
	/* TODO: type promotion/conversion */
	if (op1.type == op2.type) {
		switch (op1.type) {
			case token_type_t::integer:
				r = op1;
				r.v.integer.v.v ^= op2.v.integer.v.v;
				return true;
			default:
				break;
		};
	}

	return false;
}

bool ast_constexpr_logical_and(token_t &r,token_t &op1,token_t &op2) {
	/* TODO: type promotion/conversion */
	if (op1.type == op2.type) {
		switch (op1.type) {
			case token_type_t::integer:
				r = op1;
				r.v.integer.v.u = (ast_constexpr_to_bool(op1.v.integer) && ast_constexpr_to_bool(op2.v.integer)) ? 1 : 0;
				return true;
			default:
				break;
		};
	}

	return false;
}

bool ast_constexpr_binary_and(token_t &r,token_t &op1,token_t &op2) {
	/* TODO: type promotion/conversion */
	if (op1.type == op2.type) {
		switch (op1.type) {
			case token_type_t::integer:
				r = op1;
				r.v.integer.v.v &= op2.v.integer.v.v;
				return true;
			default:
				break;
		};
	}

	return false;
}

bool ast_constexpr_multiply(token_t &r,token_t &op1,token_t &op2) {
	/* TODO: type promotion/conversion */
	if (op1.type == op2.type) {
		switch (op1.type) {
			case token_type_t::integer:
				r = op1;
				if (op1.v.integer.flags & integer_value_t::FL_SIGNED)
					r.v.integer.v.v *= op2.v.integer.v.v;
				else
					r.v.integer.v.u *= op2.v.integer.v.u;
				return true;
			default:
				break;
		};
	}

	return false;
}

bool ast_constexpr_divide(token_t &r,token_t &op1,token_t &op2) {
	/* TODO: type promotion/conversion */
	if (op1.type == op2.type) {
		switch (op1.type) {
			case token_type_t::integer:
				/* divide by zero? NOPE! */
				if (op2.v.integer.v.u == 0ull)
					return false;

				r = op1;
				if (op1.v.integer.flags & integer_value_t::FL_SIGNED)
					r.v.integer.v.v /= op2.v.integer.v.v;
				else
					r.v.integer.v.u /= op2.v.integer.v.u;
				return true;
			default:
				break;
		};
	}

	return false;
}

bool ast_constexpr_modulus(token_t &r,token_t &op1,token_t &op2) {
	/* TODO: type promotion/conversion */
	if (op1.type == op2.type) {
		switch (op1.type) {
			case token_type_t::integer:
				/* divide by zero? NOPE! */
				if (op2.v.integer.v.u == 0ull)
					return false;

				r = op1;
				if (op1.v.integer.flags & integer_value_t::FL_SIGNED)
					r.v.integer.v.v %= op2.v.integer.v.v;
				else
					r.v.integer.v.u %= op2.v.integer.v.u;
				return true;
			default:
				break;
		};
	}

	return false;
}

void ast_node_reduce(ast_node_id_t &eroot,const std::string &prefix=std::string()) {
#define OP_ONE_PARAM_TEVAL ast_node_id_t op1 = erootnode.child

#define OP_TWO_PARAM_TEVAL ast_node_id_t op1 = erootnode.child; \
	ast_node_id_t op2 = ast_node(op1).next

	if (eroot == ast_node_none)
		return;

#if 1//DEBUG
	if (prefix.empty()) {
		fprintf(stderr,"%senum expr (reducing node#%lu):\n",prefix.c_str(),(unsigned long)eroot);
		debug_dump_ast(prefix+"  ",eroot);
	}
#endif

again:
	for (ast_node_id_t n=eroot;n!=ast_node_none;n=ast_node(n).next)
		ast_node_reduce(ast_node(n).child,prefix+"  ");

	if (eroot != ast_node_none) {
		/* WARNING: stale references will occur if any code during this switch statement creates new AST nodes */
		ast_node_t &erootnode = ast_node(eroot);
		switch (erootnode.t.type) {
			case token_type_t::op_sizeof:
				{
					size_t ptrdref = 0;
					OP_ONE_PARAM_TEVAL;
					do {
						assert(op1 != ast_node_none);
						ast_node_t &an = ast_node(op1);

						if (an.t.type == token_type_t::op_binary_not) {
							op1 = an.child;
						}
						else if (an.t.type == token_type_t::op_pointer_deref) {
							op1 = an.child;
							ptrdref++;
						}
						else if (an.t.type == token_type_t::op_address_of) {
							op1 = an.child;
							ptrdref = ptr_deref_sizeof_addressof;

							while (op1 != ast_node_none && ast_node(op1).t.type == token_type_t::op_address_of)
								op1 = ast_node(op1).child;

							break;
						}
						else {
							break;
						}
					} while(1);
					if (ast_constexpr_sizeof(erootnode.t,ast_node(op1).t,ptrdref)) {
						erootnode.set_child(ast_node_none);
						goto again;
					}
					break;
				}

			case token_type_t::op_alignof:
				{
					size_t ptrdref = 0;
					OP_ONE_PARAM_TEVAL;
					do {
						assert(op1 != ast_node_none);
						ast_node_t &an = ast_node(op1);

						if (an.t.type == token_type_t::op_binary_not) {
							op1 = an.child;
						}
						else if (an.t.type == token_type_t::op_pointer_deref) {
							op1 = an.child;
							ptrdref++;
						}
						else if (an.t.type == token_type_t::op_address_of) {
							op1 = an.child;
							ptrdref = ptr_deref_sizeof_addressof;

							while (op1 != ast_node_none && ast_node(op1).t.type == token_type_t::op_address_of)
								op1 = ast_node(op1).child;

							break;
						}
						else {
							break;
						}
					} while(1);
					if (ast_constexpr_alignof(erootnode.t,ast_node(op1).t,ptrdref)) {
						erootnode.set_child(ast_node_none);
						goto again;
					}
					break;
				}

			case token_type_t::op_negate:
				{
					OP_ONE_PARAM_TEVAL;
					if (is_ast_constexpr(ast_node(op1).t)) {
						if (ast_constexpr_negate(erootnode.t,ast_node(op1).t)) {
							erootnode.set_child(ast_node_none);
							goto again;
						}
					}
					break;
				}

			case token_type_t::op_binary_not:
				{
					OP_ONE_PARAM_TEVAL;
					if (is_ast_constexpr(ast_node(op1).t)) {
						if (ast_constexpr_binary_not(erootnode.t,ast_node(op1).t)) {
							erootnode.set_child(ast_node_none);
							goto again;
						}
					}
					break;
				}

			case token_type_t::op_logical_not:
				{
					OP_ONE_PARAM_TEVAL;
					if (is_ast_constexpr(ast_node(op1).t)) {
						if (ast_constexpr_logical_not(erootnode.t,ast_node(op1).t)) {
							erootnode.set_child(ast_node_none);
							goto again;
						}
					}
					break;
				}

			case token_type_t::op_leftshift:
				{
					OP_TWO_PARAM_TEVAL;
					if (is_ast_constexpr(ast_node(op1).t) && is_ast_constexpr(ast_node(op2).t)) {
						if (ast_constexpr_leftshift(erootnode.t,ast_node(op1).t,ast_node(op2).t)) {
							erootnode.set_child(ast_node_none);
							goto again;
						}
					}
					break;
				}

			case token_type_t::op_rightshift:
				{
					OP_TWO_PARAM_TEVAL;
					if (is_ast_constexpr(ast_node(op1).t) && is_ast_constexpr(ast_node(op2).t)) {
						if (ast_constexpr_rightshift(erootnode.t,ast_node(op1).t,ast_node(op2).t)) {
							erootnode.set_child(ast_node_none);
							goto again;
						}
					}
					break;
				}

			case token_type_t::op_lessthan_equals:
				{
					OP_TWO_PARAM_TEVAL;
					if (is_ast_constexpr(ast_node(op1).t) && is_ast_constexpr(ast_node(op2).t)) {
						if (ast_constexpr_lessthan_equals(erootnode.t,ast_node(op1).t,ast_node(op2).t)) {
							erootnode.set_child(ast_node_none);
							goto again;
						}
					}
					break;
				}

			case token_type_t::op_greaterthan_equals:
				{
					OP_TWO_PARAM_TEVAL;
					if (is_ast_constexpr(ast_node(op1).t) && is_ast_constexpr(ast_node(op2).t)) {
						if (ast_constexpr_greaterthan_equals(erootnode.t,ast_node(op1).t,ast_node(op2).t)) {
							erootnode.set_child(ast_node_none);
							goto again;
						}
					}
					break;
				}

			case token_type_t::op_lessthan:
				{
					OP_TWO_PARAM_TEVAL;
					if (is_ast_constexpr(ast_node(op1).t) && is_ast_constexpr(ast_node(op2).t)) {
						if (ast_constexpr_lessthan(erootnode.t,ast_node(op1).t,ast_node(op2).t)) {
							erootnode.set_child(ast_node_none);
							goto again;
						}
					}
					break;
				}

			case token_type_t::op_greaterthan:
				{
					OP_TWO_PARAM_TEVAL;
					if (is_ast_constexpr(ast_node(op1).t) && is_ast_constexpr(ast_node(op2).t)) {
						if (ast_constexpr_greaterthan(erootnode.t,ast_node(op1).t,ast_node(op2).t)) {
							erootnode.set_child(ast_node_none);
							goto again;
						}
					}
					break;
				}

			case token_type_t::op_equals:
				{
					OP_TWO_PARAM_TEVAL;
					if (is_ast_constexpr(ast_node(op1).t) && is_ast_constexpr(ast_node(op2).t)) {
						if (ast_constexpr_equals(erootnode.t,ast_node(op1).t,ast_node(op2).t)) {
							erootnode.set_child(ast_node_none);
							goto again;
						}
					}
					break;
				}

			case token_type_t::op_not_equals:
				{
					OP_TWO_PARAM_TEVAL;
					if (is_ast_constexpr(ast_node(op1).t) && is_ast_constexpr(ast_node(op2).t)) {
						if (ast_constexpr_not_equals(erootnode.t,ast_node(op1).t,ast_node(op2).t)) {
							erootnode.set_child(ast_node_none);
							goto again;
						}
					}
					break;
				}

			case token_type_t::op_add:
				{
					OP_TWO_PARAM_TEVAL;
					if (is_ast_constexpr(ast_node(op1).t) && is_ast_constexpr(ast_node(op2).t)) {
						if (ast_constexpr_add(erootnode.t,ast_node(op1).t,ast_node(op2).t)) {
							erootnode.set_child(ast_node_none);
							goto again;
						}
					}
					break;
				}

			case token_type_t::op_subtract:
				{
					OP_TWO_PARAM_TEVAL;
					if (is_ast_constexpr(ast_node(op1).t) && is_ast_constexpr(ast_node(op2).t)) {
						if (ast_constexpr_subtract(erootnode.t,ast_node(op1).t,ast_node(op2).t)) {
							erootnode.set_child(ast_node_none);
							goto again;
						}
					}
					break;
				}

			case token_type_t::op_logical_or:
				{
					OP_TWO_PARAM_TEVAL;
					if (is_ast_constexpr(ast_node(op1).t) && is_ast_constexpr(ast_node(op2).t)) {
						if (ast_constexpr_logical_or(erootnode.t,ast_node(op1).t,ast_node(op2).t)) {
							erootnode.set_child(ast_node_none);
							goto again;
						}
					}
					break;
				}

			case token_type_t::op_binary_or:
				{
					OP_TWO_PARAM_TEVAL;
					if (is_ast_constexpr(ast_node(op1).t) && is_ast_constexpr(ast_node(op2).t)) {
						if (ast_constexpr_binary_or(erootnode.t,ast_node(op1).t,ast_node(op2).t)) {
							erootnode.set_child(ast_node_none);
							goto again;
						}
					}
					break;
				}

			case token_type_t::op_binary_xor:
				{
					OP_TWO_PARAM_TEVAL;
					if (is_ast_constexpr(ast_node(op1).t) && is_ast_constexpr(ast_node(op2).t)) {
						if (ast_constexpr_binary_xor(erootnode.t,ast_node(op1).t,ast_node(op2).t)) {
							erootnode.set_child(ast_node_none);
							goto again;
						}
					}
					break;
				}

			case token_type_t::op_logical_and:
				{
					OP_TWO_PARAM_TEVAL;
					if (is_ast_constexpr(ast_node(op1).t) && is_ast_constexpr(ast_node(op2).t)) {
						if (ast_constexpr_logical_and(erootnode.t,ast_node(op1).t,ast_node(op2).t)) {
							erootnode.set_child(ast_node_none);
							goto again;
						}
					}
					break;
				}

			case token_type_t::op_binary_and:
				{
					OP_TWO_PARAM_TEVAL;
					if (is_ast_constexpr(ast_node(op1).t) && is_ast_constexpr(ast_node(op2).t)) {
						if (ast_constexpr_binary_and(erootnode.t,ast_node(op1).t,ast_node(op2).t)) {
							erootnode.set_child(ast_node_none);
							goto again;
						}
					}
					break;
				}

			case token_type_t::op_multiply:
				{
					OP_TWO_PARAM_TEVAL;
					if (is_ast_constexpr(ast_node(op1).t) && is_ast_constexpr(ast_node(op2).t)) {
						if (ast_constexpr_multiply(erootnode.t,ast_node(op1).t,ast_node(op2).t)) {
							erootnode.set_child(ast_node_none);
							goto again;
						}
					}
					break;
				}

			case token_type_t::op_divide:
				{
					OP_TWO_PARAM_TEVAL;
					if (is_ast_constexpr(ast_node(op1).t) && is_ast_constexpr(ast_node(op2).t)) {
						if (ast_constexpr_divide(erootnode.t,ast_node(op1).t,ast_node(op2).t)) {
							erootnode.set_child(ast_node_none);
							goto again;
						}
					}
					break;
				}

			case token_type_t::op_modulus:
				{
					OP_TWO_PARAM_TEVAL;
					if (is_ast_constexpr(ast_node(op1).t) && is_ast_constexpr(ast_node(op2).t)) {
						if (ast_constexpr_modulus(erootnode.t,ast_node(op1).t,ast_node(op2).t)) {
							erootnode.set_child(ast_node_none);
							goto again;
						}
					}
					break;
				}

			case token_type_t::op_comma:
				{
					OP_TWO_PARAM_TEVAL;
					if (is_ast_constexpr(ast_node(op1).t) || is_ast_strconstexpr(ast_node(op1).t)) {
						ast_node_id_t nn = ast_node.returnmove(erootnode.next);
						op1 = ast_node.returnmove(erootnode.child);
						op2 = ast_node.returnmove(ast_node(op1).next);

						ast_node.assignmove(eroot,op2);

						ast_node(eroot).set_next(nn);
						ast_node.release(op2);
						ast_node.release(op1);
						ast_node.release(nn);
						goto again;
					}
					break;
				}

			case token_type_t::op_ternary:
				{
					if (is_ast_constexpr(ast_node(erootnode.child).t)) {
						ast_node_id_t nn = ast_node.returnmove(erootnode.next);
						ast_node_id_t cn = ast_node.returnmove(erootnode.child);
						ast_node_id_t tc = ast_node.returnmove(ast_node(cn).next);
						ast_node_id_t fc = ast_node.returnmove(ast_node(tc).next);

						if (ast_constexpr_to_bool(ast_node(cn).t))
							ast_node.assignmove(eroot,tc);
						else
							ast_node.assignmove(eroot,fc);

						ast_node(eroot).set_next(nn);
						ast_node.release(fc);
						ast_node.release(tc);
						ast_node.release(cn);
						ast_node.release(nn);
						goto again;
					}
					break;
				}

			case token_type_t::op_symbol:
				{
					symbol_t &sym = symbol(ast_node(eroot).t.v.symbol);
					if (sym.sym_type == symbol_t::CONST) {
						if (sym.expr != ast_node_none) {
							/* non-destructive copy the token from the symbol.
							 * this will not work if the node has children or sibling (next) */
							if (ast_node(sym.expr).child == ast_node_none && ast_node(sym.expr).next == ast_node_none) {
								ast_node(eroot).t = ast_node(sym.expr).t;
								goto again;
							}
						}
					}
					break;
				}

			default:
				{
					for (ast_node_id_t n=eroot;n!=ast_node_none;n=ast_node(n).next)
						ast_node_reduce(ast_node(n).next,"  ");
					break;
				}
		}
	}

#if 1//DEBUG
	if (prefix.empty()) {
		fprintf(stderr,"%senum expr (reduce-complete):\n",prefix.c_str());
		debug_dump_ast(prefix+"  ",eroot);
	}
#endif
#undef OP_TWO_PARAM_TEVAL
#undef OP_ONE_PARAM_TEVAL
}

void ast_node_strliteral_to_symbol(ast_node_t &erootnode) {
	symbol_id_t sid = symbol_none;

	const csliteral_t &csl = csliteral(erootnode.t.v.csliteral);

	if (sid == symbol_none)
		sid = match_str_symbol(csl);

	if (sid == symbol_none) {
		sid = new_symbol(identifier_none);//anonymous symbol

		symbol_t &so = symbol(sid);
		so.sym_type = symbol_t::STR;
		so.part_of_segment = conststr_segment;
		so.flags = symbol_t::FL_DEFINED | symbol_t::FL_DECLARED;

		const ast_node_id_t sroot = ast_node.alloc();
		ast_node_t &srootnode = ast_node(sroot);
		srootnode.t = std::move(erootnode.t);

		switch (csl.unitsize()) {
			case 1:
				so.spec.type_specifier = TS_CHAR;
				break;
			case 2:
				so.spec.type_specifier = TS_INT|TS_SZ16;
				break;
			case 4:
				so.spec.type_specifier = TS_INT|TS_SZ32;
				break;
			default:
				abort();
		}

		so.spec.type_qualifier = TQ_CONST;
		so.spec.size = csl.length + csl.unitsize();/*string+NUL*/
		so.spec.align = addrmask_make(csl.unitsize());
		so.expr = sroot;
	}
	else {
		erootnode.t.clear_v();
	}

	erootnode.t.type = token_type_t::op_symbol;
	erootnode.t.v.symbol = sid;
}

//////////////////////////////////////////////////////////////////////////////

void debug_dump_var_type(const std::string prefix,const data_var_type_t &dt,const std::string &name) {
	fprintf(stderr,"%s%s%svar type:\n",prefix.c_str(),name.c_str(),name.empty()?"":" ");
	if (dt.t.size != data_size_none)
		fprintf(stderr,"%s  size: 0x%llx (%lld)\n",prefix.c_str(),(unsigned long long)dt.t.size,(unsigned long long)dt.t.size);
	if (dt.t.align != addrmask_none)
		fprintf(stderr,"%s  alignment: 0x%llx (%llu)\n",prefix.c_str(),(unsigned long long)(~dt.t.align) + 1ull,(unsigned long long)(~dt.t.align) + 1ull);

	if (dt.ts != 0) {
		fprintf(stderr,"%s  type:",prefix.c_str());
		for (unsigned int x=0;x < TSI__MAX;x++) { if (dt.ts&(1u<<x)) fprintf(stderr," %s",type_specifier_idx_t_str[x]); }
		fprintf(stderr,"\n");
	}
}

void debug_dump_ptr_type(const std::string prefix,const data_ptr_type_t &dt,const std::string &name) {
	fprintf(stderr,"%s%s%sptr type:\n",prefix.c_str(),name.c_str(),name.empty()?"":" ");
	if (dt.t.size != data_size_none)
		fprintf(stderr,"%s  size: 0x%llx (%lld)\n",prefix.c_str(),(unsigned long long)dt.t.size,(unsigned long long)dt.t.size);
	if (dt.t.align != addrmask_none)
		fprintf(stderr,"%s  alignment: 0x%llx (%llu)\n",prefix.c_str(),(unsigned long long)(~dt.t.align) + 1ull,(unsigned long long)(~dt.t.align) + 1ull);

	if (dt.tq != 0) {
		fprintf(stderr,"%s  qualifiers:",prefix.c_str());
		for (unsigned int x=0;x < TQI__MAX;x++) { if (dt.tq&(1u<<x)) fprintf(stderr," %s",type_qualifier_idx_t_str[x]); }
		fprintf(stderr,"\n");
	}
}

void debug_dump_general(const std::string prefix,const std::string &name) {
	fprintf(stderr,"%s%s%sgeneral info:\n",prefix.c_str(),name.c_str(),name.empty()?"":" ");
	fprintf(stderr,"%s  target cpu: %s\n",prefix.c_str(),target_cpu_str_t[target_cpu]);
	fprintf(stderr,"%s  target cpu rev: %s\n",prefix.c_str(),target_cpu_rev_str_t[target_cpurev]);
	fprintf(stderr,"%s  target cpu sub: %s\n",prefix.c_str(),target_cpu_sub_str_t[target_cpusub]);

	fprintf(stderr,"%s  default packing: ",prefix.c_str());
	if (default_packing != addrmask_none)
		fprintf(stderr,"0x%llx (%llu)\n",(unsigned long long)(~default_packing) + 1ull,(unsigned long long)(~default_packing) + 1ull);
	else
		fprintf(stderr,"none\n");

	fprintf(stderr,"%s  current packing: ",prefix.c_str());
	if (current_packing != addrmask_none)
		fprintf(stderr,"0x%llx (%llu)\n",(unsigned long long)(~current_packing) + 1ull,(unsigned long long)(~current_packing) + 1ull);
	else
		fprintf(stderr,"none\n");

	debug_dump_data_type_set(prefix+"  ",data_types);
	debug_dump_data_type_set_ptr(prefix+"  ",data_types_ptr_code,"code");
	debug_dump_data_type_set_ptr(prefix+"  ",data_types_ptr_data,"data");
	debug_dump_data_type_set_ptr(prefix+"  ",data_types_ptr_stack,"stack");
}

void debug_dump_data_type_set(const std::string prefix,const data_type_set_t &dts,const std::string &name) {
	fprintf(stderr,"%s%s%sdata types:\n",prefix.c_str(),name.c_str(),name.empty()?"":" ");
	debug_dump_var_type(prefix+"  ",dts.dt_bool,"bool");
	debug_dump_var_type(prefix+"  ",dts.dt_char,"char");
	debug_dump_var_type(prefix+"  ",dts.dt_short,"short");
	debug_dump_var_type(prefix+"  ",dts.dt_int,"int");
	debug_dump_var_type(prefix+"  ",dts.dt_long,"long");
	debug_dump_var_type(prefix+"  ",dts.dt_longlong,"longlong");
	debug_dump_var_type(prefix+"  ",dts.dt_float,"float");
	debug_dump_var_type(prefix+"  ",dts.dt_double,"double");
	debug_dump_var_type(prefix+"  ",dts.dt_longdouble,"longdouble");
}

void debug_dump_data_type_set_ptr(const std::string prefix,const data_type_set_ptr_t &dtsp,const std::string &name) {
	fprintf(stderr,"%s%s%sdata types:\n",prefix.c_str(),name.c_str(),name.empty()?"":" ");
	debug_dump_ptr_type(prefix+"  ",dtsp.dt_ptr,"ptr");
	debug_dump_ptr_type(prefix+"  ",dtsp.dt_ptr_near,"near ptr");
	debug_dump_ptr_type(prefix+"  ",dtsp.dt_ptr_far,"far ptr");
	debug_dump_ptr_type(prefix+"  ",dtsp.dt_ptr_huge,"huge ptr");
	debug_dump_var_type(prefix+"  ",dtsp.dt_size_t,"size_t");
	debug_dump_var_type(prefix+"  ",dtsp.dt_intptr_t,"intptr_t");
}

void debug_dump_enumerator(const std::string prefix,enumerator_t &en) {
	fprintf(stderr,"%s",prefix.c_str());

	if (en.name != identifier_none) fprintf(stderr,"'%s'",identifier(en.name).to_str().c_str());
	else fprintf(stderr,"?");
	fprintf(stderr,"\n");

	if (en.expr != ast_node_none) {
		fprintf(stderr,"%s  expr:\n",prefix.c_str());
		debug_dump_ast(prefix+"    ",en.expr);
	}
}

void debug_dump_declaration_specifier_flags(const std::string prefix,const unsigned int flags,const std::string &name) {
	if (flags != 0) {
		fprintf(stderr,"%s%s%sdeclspec:",prefix.c_str(),name.c_str(),name.empty()?"":" ");
		if (flags & DCS_FL_DEPRECATED) fprintf(stderr," deprecated");
		if (flags & DCS_FL_DLLIMPORT) fprintf(stderr," dllimport");
		if (flags & DCS_FL_DLLEXPORT) fprintf(stderr," dllexport");
		if (flags & DCS_FL_NAKED) fprintf(stderr," naked");
		fprintf(stderr,"\n");
	}
}

void debug_dump_declaration_specifiers(const std::string prefix,declaration_specifiers_t &ds) {
	if (ds.empty()) return;

	fprintf(stderr,"%sdeclaration_specifiers:",prefix.c_str());
	for (unsigned int i=0;i < SCI__MAX;i++) { if (ds.storage_class&(1u<<i)) fprintf(stderr," %s",storage_class_idx_t_str[i]); }
	for (unsigned int i=0;i < TSI__MAX;i++) { if (ds.type_specifier&(1u<<i)) fprintf(stderr," %s",type_specifier_idx_t_str[i]); }
	for (unsigned int i=0;i < TQI__MAX;i++) { if (ds.type_qualifier&(1u<<i)) fprintf(stderr," %s",type_qualifier_idx_t_str[i]); }
	if (ds.type_identifier_symbol != symbol_none) {
		symbol_t &sym = symbol(ds.type_identifier_symbol);
		fprintf(stderr," symbol#%lu",(unsigned long)ds.type_identifier_symbol);
		if (sym.name != identifier_none)
			fprintf(stderr," '%s'",identifier(sym.name).to_str().c_str());
		else
			fprintf(stderr," <anon>");
	}
	fprintf(stderr,"\n");

	if (ds.align != addrmask_none)
		fprintf(stderr,"%s  alignment: 0x%llx (%llu)\n",prefix.c_str(),(unsigned long long)(~ds.align) + 1ull,(unsigned long long)(~ds.align) + 1ull);

	if (ds.size != data_size_none)
		fprintf(stderr,"%s  size: 0x%llx (%llu)\n",prefix.c_str(),(unsigned long long)ds.size,(unsigned long long)ds.size);

	debug_dump_declaration_specifier_flags(prefix+"  ",ds.dcs_flags);

	if (!ds.enum_list.empty()) {
		fprintf(stderr,"%s  enum_list:\n",prefix.c_str());
		for (auto &sid : ds.enum_list)
			debug_dump_symbol(prefix+"    ",symbol(sid));
	}
}

void debug_dump_pointer(const std::string prefix,std::vector<pointer_t> &ptr,const std::string &name) {
	if (ptr.empty()) return;

	fprintf(stderr,"%s%s%spointer(s):",prefix.c_str(),name.c_str(),name.empty()?"":" ");
	for (auto i=ptr.begin();i!=ptr.end();i++) {
		fprintf(stderr," *");
		for (unsigned int x=0;x < TQI__MAX;x++) { if ((*i).tq&(1u<<x)) fprintf(stderr," %s",type_qualifier_idx_t_str[x]); }
	}
	fprintf(stderr,"\n");
}

void debug_dump_arraydef(const std::string prefix,std::vector<ast_node_id_t> &arraydef,const std::string &name) {
	if (arraydef.empty()) return;

	fprintf(stderr,"%s%s%sarraydef(s):\n",prefix.c_str(),name.c_str(),name.empty()?"":" ");
	for (const auto &expr : arraydef) {
		if (expr != ast_node_none) debug_dump_ast(prefix+"  ",expr);
		else fprintf(stderr,"%s  <none>\n",prefix.c_str());
	}
}

void debug_dump_parameter(const std::string prefix,parameter_t &p,const std::string &name) {
	fprintf(stderr,"%s%s%sparameter:\n",prefix.c_str(),name.c_str(),name.empty()?"":" ");
	debug_dump_declaration_specifiers(prefix+"  ",p.spec);

	if (p.decl.name != identifier_none)
		fprintf(stderr,"%s  identifier: '%s'\n",prefix.c_str(),identifier(p.decl.name).to_str().c_str());

	if (p.decl.symbol != symbol_none)
		fprintf(stderr,"%s  symbol: #%lu\n",prefix.c_str(),(unsigned long)p.decl.symbol);

	debug_dump_ddip(prefix+"  ",p.decl.ddip);

	if (p.decl.expr != ast_node_none) {
		fprintf(stderr,"%s  expr:\n",prefix.c_str());
		debug_dump_ast(prefix+"    ",p.decl.expr);
	}
}

void debug_dump_ddip(const std::string prefix,ddip_t &ddip,const std::string &name) {
	fprintf(stderr,"%s%s%sptr/array pair:\n",prefix.c_str(),name.c_str(),name.empty()?"":" ");
	debug_dump_pointer(prefix+"  ",ddip.ptr);
	debug_dump_arraydef(prefix+"  ",ddip.arraydef);

	if (ddip.dd_flags & declarator_t::FL_FUNCTION_POINTER)
		fprintf(stderr,"%s  is function pointer\n",prefix.c_str());
	else if (ddip.dd_flags & declarator_t::FL_FUNCTION)
		fprintf(stderr,"%s  is function\n",prefix.c_str());

	if (ddip.dd_flags & declarator_t::FL_ELLIPSIS)
		fprintf(stderr,"%s  parameter ... (ellipsis)\n",prefix.c_str());

	for (auto &ppp : ddip.parameters)
		debug_dump_parameter(prefix+"  ",ppp);
}

void debug_dump_ddip(const std::string prefix,ddip_list_t &ddip,const std::string &name) {
	if (ddip.empty())
		return;

	fprintf(stderr,"%s%s%sptr/array pairs:\n",prefix.c_str(),name.c_str(),name.empty()?"":" ");

	if (!ddip.empty())
		fprintf(stderr,"%s  representation: %s\n",prefix.c_str(),ddip_list_to_str(ddip).c_str());

	for (auto &sdip : ddip)
		debug_dump_ddip(prefix+"  ",sdip);
}

void debug_dump_direct_declarator(const std::string prefix,declarator_t &ddecl,const std::string &name) {
	if (ddecl.flags & declarator_t::FL_FUNCTION_POINTER)
		fprintf(stderr,"%s%s%sfunction pointer direct declarator:\n",prefix.c_str(),name.c_str(),name.empty()?"":" ");
	else if (ddecl.flags & declarator_t::FL_FUNCTION)
		fprintf(stderr,"%s%s%sfunction direct declarator:\n",prefix.c_str(),name.c_str(),name.empty()?"":" ");
	else
		fprintf(stderr,"%s%s%sdirect declarator:\n",prefix.c_str(),name.c_str(),name.empty()?"":" ");

	if (ddecl.name != identifier_none)
		fprintf(stderr,"%s  identifier: '%s'\n",prefix.c_str(),identifier(ddecl.name).to_str().c_str());

	if (ddecl.symbol != symbol_none)
		fprintf(stderr,"%s  symbol: #%lu\n",prefix.c_str(),(unsigned long)ddecl.symbol);

	debug_dump_ddip(prefix+"  ",ddecl.ddip);

	if (ddecl.flags & declarator_t::FL_ELLIPSIS)
		fprintf(stderr,"%s  parameter ... (ellipsis)\n",prefix.c_str());
}

void debug_dump_declarator(const std::string prefix,declarator_t &declr,const std::string &name) {
	if (declr.flags & declarator_t::FL_FUNCTION_POINTER)
		fprintf(stderr,"%s%s%sfunction pointer declarator:\n",prefix.c_str(),name.c_str(),name.empty()?"":" ");
	else if (declr.flags & declarator_t::FL_FUNCTION)
		fprintf(stderr,"%s%s%sfunction declarator:\n",prefix.c_str(),name.c_str(),name.empty()?"":" ");
	else
		fprintf(stderr,"%s%s%sdeclarator:\n",prefix.c_str(),name.c_str(),name.empty()?"":" ");

	debug_dump_direct_declarator(prefix+"  ",declr);

	if (declr.expr != ast_node_none) {
		fprintf(stderr,"%s  expr:\n",prefix.c_str());
		debug_dump_ast(prefix+"    ",declr.expr);
	}
}

void debug_dump_declaration(const std::string prefix,declaration_t &decl,const std::string &name) {
	fprintf(stderr,"%s%s%sdeclaration:{\n",prefix.c_str(),name.c_str(),name.empty()?"":" ");
	debug_dump_declaration_specifiers(prefix+"  ",decl.spec);

	for (auto &declr : decl.declor)
		debug_dump_declarator(prefix+"  ",declr);

	fprintf(stderr,"%s}\n",prefix.c_str());
}

void debug_dump_ast(const std::string prefix,ast_node_id_t r) {
	unsigned int count = 0;

	while (r != ast_node_none) {
		const auto &n = ast_node(r);
		fprintf(stderr,"%s%s[%u]\n",prefix.c_str(),n.t.to_str().c_str(),count);

		if (n.t.type == token_type_t::op_declaration) {
			if (n.t.v.declaration)
				debug_dump_declaration(prefix+"  ",*n.t.v.declaration);
		}
		else if (n.t.type == token_type_t::op_symbol) {
			if (n.t.v.symbol != symbol_none) {
				symbol_t &sym = symbol(n.t.v.symbol);
				fprintf(stderr,"%s  identifier: ",prefix.c_str());
				if (sym.name != identifier_none) fprintf(stderr,"%s\n",identifier(sym.name).to_str().c_str());
				else fprintf(stderr,"<anon>\n");

				if (sym.sym_type == symbol_t::STR && sym.expr != ast_node_none) {
					ast_node_t &sa = ast_node(sym.expr);
					if (sa.t.type == token_type_t::strliteral && sa.t.v.csliteral != csliteral_none) {
						fprintf(stderr,"%s    string: %s\n",prefix.c_str(),csliteral(sa.t.v.csliteral).to_str().c_str());
					}
				}
			}
		}

		debug_dump_ast(prefix+"  ",n.child);
		r = n.next;
		count++;
	}
}

void debug_dump_scope(const std::string prefix,scope_t &sco,const std::string &name) {
	fprintf(stderr,"%s%s%sscope:\n",prefix.c_str(),name.c_str(),name.empty()?"":" ");

	for (auto sid : sco.symbols) {
		symbol_t &sym = symbol(sid);
		debug_dump_symbol(prefix+"  ",sym);
	}

	for (auto &decl : sco.localdecl) {
		fprintf(stderr,"%s  decl:\n",prefix.c_str());
		debug_dump_declaration_specifiers(prefix+"    ",decl.spec);
		debug_dump_declarator(prefix+"    ",decl.declor);
	}

	if (sco.root != ast_node_none) {
		fprintf(stderr,"%s  expr:\n",prefix.c_str());
		debug_dump_ast(prefix+"    ",sco.root);
	}
}

void debug_dump_scope_table(const std::string prefix,const std::string &name) {
	fprintf(stderr,"%s%s%sscope table:\n",prefix.c_str(),name.c_str(),name.empty()?"":" ");
	for (auto si=scopes.begin();si!=scopes.end();si++)
		debug_dump_scope(prefix+"  ",*si,std::string("#")+std::to_string((unsigned int)(si-scopes.begin())));
}

void debug_dump_segment_table(const std::string prefix,const std::string &name) {
	fprintf(stderr,"%s%s%ssegment table:\n",prefix.c_str(),name.c_str(),name.empty()?"":" ");
	for (auto si=segments.begin();si!=segments.end();si++)
		debug_dump_segment(prefix+"  ",*si,std::string("#")+std::to_string((unsigned int)(si-segments.begin())));
}

void debug_dump_structfield(const std::string prefix,structfield_t &field,const std::string &name) {
	fprintf(stderr,"%s%s%sfield:",prefix.c_str(),name.c_str(),name.empty()?"":" ");
	if (field.name != identifier_none) fprintf(stderr," '%s'",identifier(field.name).to_str().c_str());
	else fprintf(stderr," <anon>");
	fprintf(stderr,"\n");

	debug_dump_declaration_specifiers(prefix+"  ",field.spec);
	debug_dump_ddip(prefix+"  ",field.ddip);

	if (field.offset != data_offset_none)
		fprintf(stderr,"%s  offset: %lu\n",prefix.c_str(),(unsigned long)field.offset);

	if (field.bf_start != bitfield_pos_none || field.bf_length != bitfield_pos_none) {
		fprintf(stderr,"%s  bitfield:",prefix.c_str());

		if (field.bf_start != bitfield_pos_none)
			fprintf(stderr," start=%u",field.bf_start);
		if (field.bf_length != bitfield_pos_none)
			fprintf(stderr," length=%u",field.bf_length);
		if (field.bf_start != bitfield_pos_none && field.bf_length != bitfield_pos_none) {
			if (field.bf_length > 1)
				fprintf(stderr," [%u...%u]",field.bf_start,field.bf_start+field.bf_length-1);
			else
				fprintf(stderr," [%u]",field.bf_start);
		}

		fprintf(stderr,"\n");
	}
}

void debug_dump_segment(const std::string prefix,segment_t &s,const std::string &name) {
	fprintf(stderr,"%s%s%ssegment#%lu",prefix.c_str(),name.c_str(),name.empty()?"":" ",size_t(&s-&segments[0]));

	switch (s.type) {
		case segment_t::type_t::CODE: fprintf(stderr," code"); break;
		case segment_t::type_t::CONST: fprintf(stderr," const"); break;
		case segment_t::type_t::DATA: fprintf(stderr," data"); break;
		case segment_t::type_t::BSS: fprintf(stderr," bss"); break;
		case segment_t::type_t::STACK: fprintf(stderr," stack"); break;
		default: break;
	};

	switch (s.use) {
		case segment_t::use_t::X86_16: fprintf(stderr," X86:16"); break;
		case segment_t::use_t::X86_32: fprintf(stderr," X86:32"); break;
		case segment_t::use_t::X86_64: fprintf(stderr," X86:64"); break;
		default: break;
	};

	if (s.name != identifier_none) fprintf(stderr," '%s'",identifier(s.name).to_str().c_str());
	else fprintf(stderr," <anon>");
	if (s.flags & segment_t::FL_NOTINEXE) fprintf(stderr," NOTINEXE");
	if (s.flags & segment_t::FL_READABLE) fprintf(stderr," READABLE");
	if (s.flags & segment_t::FL_WRITEABLE) fprintf(stderr," WRITEABLE");
	if (s.flags & segment_t::FL_EXECUTABLE) fprintf(stderr," EXECUTABLE");
	if (s.flags & segment_t::FL_PRIVATE) fprintf(stderr," PRIVATE");
	if (s.flags & segment_t::FL_FLAT) fprintf(stderr," FLAT");

	fprintf(stderr,"\n");

	if (s.align != addrmask_none)
		fprintf(stderr,"%s  alignment: 0x%llx (%llu)\n",prefix.c_str(),(unsigned long long)(~s.align) + 1ull,(unsigned long long)(~s.align) + 1ull);
	if (s.limit != data_size_none)
		fprintf(stderr,"%s  limit: 0x%llx (%llu)\n",prefix.c_str(),(unsigned long long)s.limit,(unsigned long long)s.limit);
}

void debug_dump_symbol(const std::string prefix,symbol_t &sym,const std::string &name) {
	if (sym.sym_type == symbol_t::NONE)
		return;

	fprintf(stderr,"%s%s%ssymbol#%lu",prefix.c_str(),name.c_str(),name.empty()?"":" ",size_t(&sym-&symbols[0]));
	switch (sym.sym_type) {
		case symbol_t::VARIABLE: fprintf(stderr," variable"); break;
		case symbol_t::FUNCTION: fprintf(stderr," function"); break;
		case symbol_t::TYPEDEF: fprintf(stderr," typedef"); break;
		case symbol_t::STRUCT: fprintf(stderr," struct"); break;
		case symbol_t::UNION: fprintf(stderr," union"); break;
		case symbol_t::CONST: fprintf(stderr," const"); break;
		case symbol_t::ENUM: fprintf(stderr," enum"); break;
		case symbol_t::STR: fprintf(stderr," str"); break;
		default: break;
	};
	if (sym.name != identifier_none) fprintf(stderr," '%s'",identifier(sym.name).to_str().c_str());
	else fprintf(stderr," <anon>");
	if (sym.flags & symbol_t::FL_DEFINED) fprintf(stderr," DEFINED");
	if (sym.flags & symbol_t::FL_DECLARED) fprintf(stderr," DECLARED");
	if (sym.flags & symbol_t::FL_PARAMETER) fprintf(stderr," PARAM");
	if (sym.flags & symbol_t::FL_STACK) fprintf(stderr," STACK");
	if (sym.flags & symbol_t::FL_ELLIPSIS) fprintf(stderr," ELLIPSIS");
	if (sym.flags & symbol_t::FL_FUNCTION_POINTER) fprintf(stderr," FUNCTION-POINTER");

	if (sym.scope == scope_none) fprintf(stderr," scope:none");
	else if (sym.scope == scope_global) fprintf(stderr," scope:global");
	else fprintf(stderr," scope:%lu",(unsigned long)sym.scope);

	if (sym.parent_of_scope != scope_none) fprintf(stderr," parent-of-scope:%lu",(unsigned long)sym.parent_of_scope);

	if (sym.part_of_segment != segment_none) {
		const segment_t &so = segref(sym.part_of_segment);
		fprintf(stderr," segment=#%u:'%s'",
				(unsigned int)(&so - &segments[0]),
				(so.name != identifier_none) ? identifier(so.name).to_str().c_str() : "(none)");
	}

	{
		const data_size_t sz = calc_sizeof(sym.spec,sym.ddip);
		if (sz != data_size_none) fprintf(stderr," size=0x%llx(%llx)",(unsigned long long)sz,(unsigned long long)sz);
	}

	{
		const addrmask_t am = calc_alignofmask(sym.spec,sym.ddip);
		if (am != addrmask_none) fprintf(stderr," symalign=0x%llx(%llx)",(unsigned long long)(~am) + 1ull,(unsigned long long)(~am) + 1ull);
	}

	fprintf(stderr,"\n");

	debug_dump_declaration_specifiers(prefix+"  ",sym.spec);
	debug_dump_ddip(prefix+"  ",sym.ddip);

	for (auto &f : sym.fields)
		debug_dump_structfield(prefix+"  ",f);

	if (sym.parent_of_scope != scope_none) {
		scope_t &sco = scope(sym.parent_of_scope);
		for (auto &decl : sco.localdecl) {
			fprintf(stderr,"%s  decl (as parent of scope):\n",prefix.c_str());
			debug_dump_declaration_specifiers(prefix+"    ",decl.spec);
			debug_dump_declarator(prefix+"    ",decl.declor);
		}
	}

	if (sym.expr != ast_node_none) {
		fprintf(stderr,"%s  expr:\n",prefix.c_str());
		debug_dump_ast(prefix+"    ",sym.expr);
	}
}

void debug_dump_symbol_table(const std::string prefix,const std::string &name) {
	fprintf(stderr,"%s%s%ssymbol table:\n",prefix.c_str(),name.c_str(),name.empty()?"":" ");
	for (auto &symbol : symbols) debug_dump_symbol(prefix+"  ",symbol);
}

//////////////////////////////////////////////////////////////////////////////

struct cc_state_t {
	lgtok_state_t		lst;
	pptok_state_t		pst;
	rbuf*			buf = NULL;
	source_file_object*	sfo = NULL;

	std::vector<token_t>	tq;
	size_t			tq_tail = 0;
	int			err = 0;

	bool			ignore_whitespace = true;

	void tq_ft(void);
	void tq_refill(const size_t i=1);
	const token_t &tq_peek(const size_t i=0);
	void tq_discard(const size_t i=1);
	token_t &tq_get(void);
	int cc_chkerr(void);
	int do_pragma(void);
	int check_for_pragma(void);
	int do_pragma_comma_or_closeparens(void);
	int parse_declspec_align(addrmask_t &align);
	int typeid_or_expr_parse(ast_node_id_t &aroot);
	int ms_declspec_parse(declspec_t &dsc,const position_t &pos);
	int gnu_attribute_parse(declspec_t &dsc,const position_t &pos);
	int cpp11_attribute_parse(declspec_t &dsc,const position_t &pos);
	int declspec_alignas(addrmask_t &ds_align,const position_t &pos);
	int cpp11_attribute_namespace_parse(bool &nsv_using,std::vector<identifier_id_t> &nsv);
	cpp11attr_namespace_t cpp11_attribute_identify_namespace(std::vector<identifier_id_t> &nsv);
	int direct_declarator_inner_parse(ddip_list_t &dp,declarator_t &dd,position_t &pos,unsigned int flags=0);
	int direct_declarator_parse(declaration_specifiers_t &ds,declarator_t &dd,unsigned int flags=0);
	int declaration_inner_parse(declaration_specifiers_t &spec,declarator_t &declor);
	int declarator_parse(declaration_specifiers_t &ds,declarator_t &declor);
	int declaration_specifiers_parse(declaration_specifiers_t &ds,const unsigned int declspec = 0);
	int enumerator_list_parse(declaration_specifiers_t &ds,std::vector<symbol_id_t> &enum_list);
	int struct_declarator_parse(const symbol_id_t sid,declaration_specifiers_t &ds,declarator_t &declor);
	int asm_statement(ast_node_id_t &aroot);
	int struct_bitfield_validate(token_t &t);
	int struct_field_layout(symbol_id_t sid);
	int msasm_statement(ast_node_id_t &aroot);
	bool declaration_specifiers_check(const unsigned int token_offset=0);
	int compound_statement(ast_node_id_t &aroot,ast_node_id_t &nroot);
	int struct_declaration_parse(const symbol_id_t sid,const token_type_t &tt);
	int asm_statement_gcc_colon_section(std::vector<token_t> &tokens,int &parens);
	int multiplicative_expression(ast_node_id_t &aroot);
	int exclusive_or_expression(ast_node_id_t &aroot);
	int inclusive_or_expression(ast_node_id_t &aroot);
	int conditional_expression(ast_node_id_t &aroot);
	int logical_and_expression(ast_node_id_t &aroot);
	int logical_or_expression(ast_node_id_t &aroot);
	int assignment_expression(ast_node_id_t &aroot);
	int relational_expression(ast_node_id_t &aroot);
	int pointer_parse(std::vector<pointer_t> &ptr);
	int expression_statement(ast_node_id_t &aroot);
	int declaration_parse(declaration_t &declion);
	int additive_expression(ast_node_id_t &aroot);
	int equality_expression(ast_node_id_t &aroot);
	int postfix_expression(ast_node_id_t &aroot);
	int primary_expression(ast_node_id_t &aroot);
	int shift_expression(ast_node_id_t &aroot);
	int unary_expression(ast_node_id_t &aroot);
	int cast_expression(ast_node_id_t &aroot);
	int compound_statement_declarators(void);
	int and_expression(ast_node_id_t &aroot);
	int xor_expression(ast_node_id_t &aroot);
	int or_expression(ast_node_id_t &aroot);
	int initializer(ast_node_id_t &aroot);
	int expression(ast_node_id_t &aroot);
	int statement(ast_node_id_t &aroot);
	int external_declaration(void);
	int translation_unit(void);
};

//////////////////////////////////////////////////////////////////////////////

void cc_state_t::tq_ft(void) {
	token_t t(token_type_t::eof);
	int r;

	do {
		if (err == 0) {
			if ((r=lctok(pst,lst,*buf,*sfo,t)) < 1)
				err = r;
		}

		if (ignore_whitespace && err == 0) {
			if (t.type == token_type_t::newline)
				continue;
		}

		break;
	} while (1);

	tq.push_back(std::move(t));
}

void cc_state_t::tq_refill(const size_t i) {
	while (tq.size() < (i+tq_tail))
		tq_ft();
}

const token_t &cc_state_t::tq_peek(const size_t i) {
	tq_refill(i+1);
	assert((tq_tail+i) < tq.size());
	return tq[tq_tail+i];
}

void cc_state_t::tq_discard(const size_t i) {
	tq_refill(i);
	tq_tail += i;
	assert(tq_tail <= tq.size());

	if (tq_tail == tq.size()) {
		tq_tail = 0;
		tq.clear();
	}
}

token_t &cc_state_t::tq_get(void) {
	tq_refill();
	assert(tq_tail < tq.size());
	return tq[tq_tail++];
}

int cc_state_t::cc_chkerr(void) {
	const token_t &t = tq_peek();
	if (t.type == token_type_t::none || t.type == token_type_t::eof || err < 0)
		return err; /* 0 or negative */

	return 1;
}

int cc_state_t::enumerator_list_parse(declaration_specifiers_t &spec,std::vector<symbol_id_t> &enum_list) {
	integer_value_t iv;
	int r;

	iv.init();
	iv.v.u = 0;
	iv.flags = integer_value_t::FL_SIGNED;

	if ((spec.type_specifier & (TS_INT|TS_LONG|TS_LONGLONG|TS_CHAR)) == 0)
		CCERR_RET(EINVAL,tq_peek().pos,"Enumeration list must use integer type");

	if (spec.type_specifier & TS_UNSIGNED)
		iv.flags &= ~integer_value_t::FL_SIGNED;

	/* caller already ate the { */

	do {
		if (tq_peek().type == token_type_t::closecurlybracket) {
			tq_discard();
			break;
		}

		enumerator_t en;

		en.pos = tq_peek().pos;
		if (tq_peek().type != token_type_t::identifier || tq_peek().v.identifier == identifier_none)
			CCERR_RET(EINVAL,tq_peek().pos,"Identifier expected");

		identifier.assign(/*to*/en.name,/*from*/tq_get().v.identifier);

		if (tq_peek().type == token_type_t::equal) {
			tq_discard();

			if ((r=conditional_expression(en.expr)) < 1)
				return r;

			ast_node_reduce(en.expr);

			/* the expression must reduce to an integer, or else it's an error */
			ast_node_t &an = ast_node(en.expr);
			if (an.t.type == token_type_t::integer) {
				iv.v = an.t.v.integer.v;
			}
			else {
				CCERR_RET(EINVAL,en.pos,"Enumeration constant is not a constant integer expression");
			}
		}
		else {
			en.expr = ast_node.alloc(token_type_t::integer);
			ast_node(en.expr).t.v.integer = iv;
		}

		iv.v.u++;

		{
			declarator_t declor;
			symbol_lookup_t sl;

			sl.pos = en.pos;
			sl.st = symbol_t::CONST;
			ast_node.assignmove(/*to*/declor.expr,/*from*/en.expr);
			identifier.assignmove(/*to*/declor.name,/*from*/en.name);
			if ((r=prep_symbol_lookup(sl,spec,declor)) < 1)
				return r;
			if (do_local_symbol_lookup(sl,spec,declor)) {
				if ((r=check_symbol_lookup_match(sl,spec,declor)) < 1)
					return r;
			}
			else if ((r=add_symbol(sl,spec,declor)) < 1) {
				return r;
			}

			enum_list.push_back(sl.sid);
		}
		if (tq_peek().type == token_type_t::closecurlybracket) {
			tq_discard();
			break;
		}
		else if (tq_peek().type == token_type_t::comma) {
			tq_discard();
		}
		else {
			return errno_return(EINVAL);
		}
	} while (1);

	return 1;
}

bool cc_state_t::declaration_specifiers_check(const unsigned int token_offset) {
	const token_t &t = tq_peek(token_offset);

	switch (t.type) {
		case token_type_t::r_typedef: return true;
		case token_type_t::r_extern: return true;
		case token_type_t::r_static: return true;
		case token_type_t::r_auto: return true;
		case token_type_t::r_register: return true;
		case token_type_t::r_constexpr: return true;
		case token_type_t::r_void: return true;
		case token_type_t::r_char: return true;
		case token_type_t::r_short: return true;
		case token_type_t::r_int: return true;
		case token_type_t::r_float: return true;
		case token_type_t::r_double: return true;
		case token_type_t::r_signed: return true;
		case token_type_t::r_unsigned: return true;
		case token_type_t::r_const: return true;
		case token_type_t::r_volatile: return true;
		case token_type_t::r_near: return true;
		case token_type_t::r_far: return true;
		case token_type_t::r_huge: return true;
		case token_type_t::r_restrict: return true;
		case token_type_t::r_long: return true;
		case token_type_t::r_enum: return true;
		case token_type_t::r_struct: return true;
		case token_type_t::r_union: return true;
		case token_type_t::r_inline: return true;
		case token_type_t::r_consteval: return true;
		case token_type_t::r_constinit: return true;
		case token_type_t::r_size_t: return true;
		case token_type_t::r_ssize_t: return true;
		case token_type_t::r___int8: return true;
		case token_type_t::r___int16: return true;
		case token_type_t::r___int32: return true;
		case token_type_t::r___int64: return true;
		case token_type_t::r_int8_t: return true;
		case token_type_t::r_int16_t: return true;
		case token_type_t::r_int32_t: return true;
		case token_type_t::r_int64_t: return true;
		case token_type_t::r_uint8_t: return true;
		case token_type_t::r_uint16_t: return true;
		case token_type_t::r_uint32_t: return true;
		case token_type_t::r_uint64_t: return true;
		case token_type_t::r_uintptr_t: return true;
		case token_type_t::r_intptr_t: return true;
		case token_type_t::r_alignas: return true;
		case token_type_t::r__Alignas: return true;

		case token_type_t::identifier:
			{
				symbol_id_t sid;

				if ((sid=lookup_symbol(t.v.identifier,symbol_t::TYPEDEF)) == symbol_none)
					return false;

				if (symbol(sid).sym_type != symbol_t::TYPEDEF)
					return false;
			}
			return true;

		default: break;
	};

	return false;
}

cpp11attr_namespace_t cc_state_t::cpp11_attribute_identify_namespace(std::vector<identifier_id_t> &nsv) {
	if (nsv.empty()) {
		return CPP11ATTR_NS_NONE;
	}
	else if (nsv.size() == 1) {
		if (identifier(nsv[0]) == "gnu")
			return CPP11ATTR_NS_GNU;
		if (identifier(nsv[0]) == "gsl")
			return CPP11ATTR_NS_GSL;
		if (identifier(nsv[0]) == "msvc")
			return CPP11ATTR_NS_MSVC;
	}

	return CPP11ATTR_NS_UNKNOWN;
}

int cc_state_t::cpp11_attribute_namespace_parse(bool &nsv_using,std::vector<identifier_id_t> &nsv) {
	if (tq_peek().type == token_type_t::coloncolon) {
		if (nsv_using)
			CCERR_RET(EINVAL,tq_peek().pos,"Cannot mix using namespace: and namespace:: in [[attribute]]");

		tq_discard();
		if (tq_peek().type != token_type_t::identifier)
			CCERR_RET(EINVAL,tq_peek().pos,"Expected identifier");
	}

	while (tq_peek(0).type == token_type_t::identifier && tq_peek(1).type == token_type_t::coloncolon) {
		if (nsv_using)
			CCERR_RET(EINVAL,tq_peek().pos,"Cannot mix using namespace: and namespace:: in [[attribute]]");

		identifier_id_t idv = identifier_none;
		identifier.assign(idv,tq_peek(0).v.identifier);
		nsv.push_back(idv);
		tq_discard(2);
	}

	return 1;
}

int cc_state_t::parse_declspec_align(addrmask_t &align) {
	int r;

	/* align(#). probably requires a number and no expressions allowed in Microsoft C/C++,
	 * but our extension is to allow an expression here */
	if (tq_get().type != token_type_t::openparenthesis)
		CCERR_RET(EINVAL,tq_peek().pos,"Opening parenthesis expected");

	ast_node_id_t expr = ast_node_none;

	if ((r=conditional_expression(expr)) < 1)
		return r;

	ast_node_reduce(expr);

	ast_node_t &an = ast_node(expr);
	if (an.t.type != token_type_t::integer)
		CCERR_RET(EINVAL,tq_peek().pos,"Not a number or does not reduce to a number");

	if (an.t.v.integer.v.u & (an.t.v.integer.v.u - 1ull))
		CCERR_RET(EINVAL,tq_peek().pos,"Alignas expression not a power of 2");

	align = addrmask_make(an.t.v.integer.v.u);

	if (tq_get().type != token_type_t::closeparenthesis)
		CCERR_RET(EINVAL,tq_peek().pos,"Closing parenthesis expected");

	ast_node.release(expr);
	return 1;
}

int cc_state_t::cpp11_attribute_parse(declspec_t &dsc,const position_t &pos) {
	cpp11attr_namespace_t ns = CPP11ATTR_NS_NONE;
	std::vector<identifier_id_t> nsv;
	bool nsv_using = false;
	int r;

	(void)pos;

	// [[ has already been taken, we should be at the open parens

	if (tq_peek().type == token_type_t::r_using) {
		tq_discard();

		std::vector<identifier_id_t> nsv;
		if ((r=cpp11_attribute_namespace_parse(nsv_using,nsv)) < 1)
			return r;

		if (tq_peek(0).type == token_type_t::identifier && tq_peek(1).type == token_type_t::colon) {
			identifier_id_t idv = identifier_none;
			identifier.assign(idv,tq_peek(0).v.identifier);
			nsv.push_back(idv);
			tq_discard(2);
		}
		else {
			CCERR_RET(EINVAL,tq_peek().pos,"Expected final identifier: at the end of using");
		}

		ns = cpp11_attribute_identify_namespace(nsv);
		nsv_using = true;

		for (auto &i : nsv)
			identifier.release(i);
	}

	do {
		nsv.clear();

		if (nsv_using) {
			if ((r=cpp11_attribute_namespace_parse(nsv_using,nsv)) < 1)
				return r;
			for (auto &i : nsv)
				identifier.release(i);
		}
		else {
			if ((r=cpp11_attribute_namespace_parse(nsv_using,nsv)) < 1)
				return r;

			ns = cpp11_attribute_identify_namespace(nsv);
			for (auto &i : nsv)
				identifier.release(i);
		}

		if (tq_peek().type == token_type_t::identifier) {
			token_t w = std::move(tq_get());
			assert(w.type == token_type_t::identifier);

			if (identifier(w.v.identifier) == "aligned" && ns == CPP11ATTR_NS_GNU) {
				if ((r=parse_declspec_align(/*&*/dsc.align)) < 1)
					return r;
			}
			else if (identifier(w.v.identifier) == "deprecated" && (ns == CPP11ATTR_NS_NONE || ns == CPP11ATTR_NS_GNU)) {
				dsc.dcs_flags |= DCS_FL_DEPRECATED;
			}
			else if (identifier(w.v.identifier) == "dllimport" && ns == CPP11ATTR_NS_GNU) {
				dsc.dcs_flags |= DCS_FL_DLLIMPORT;
			}
			else if (identifier(w.v.identifier) == "dllexport" && ns == CPP11ATTR_NS_GNU) {
				dsc.dcs_flags |= DCS_FL_DLLEXPORT;
			}
			else if (identifier(w.v.identifier) == "naked" && ns == CPP11ATTR_NS_GNU) {
				dsc.dcs_flags |= DCS_FL_NAKED;
			}
			else {
				CCERR_RET(EINVAL,tq_peek().pos,"Unknown [[attribute]] %s",identifier(w.v.identifier).to_str().c_str());
			}

			if (tq_peek().type == token_type_t::closesquarebracketclosesquarebracket) {
				/* OK */
			}
			else if (tq_peek().type == token_type_t::comma) {
				tq_discard();
				/* good */
			}
			else {
				CCERR_RET(EINVAL,tq_peek().pos,"comma or closing paran expected");
			}
		}
		else if (tq_peek().type == token_type_t::closesquarebracketclosesquarebracket) {
			break;
		}
		else {
			// FIXME: Unknown attributes are supposed to be ignored
			CCERR_RET(EINVAL,tq_peek().pos,"error parsing [[attribute]]");
		}
	} while(1);

	// closing parens expected
	if (tq_get().type != token_type_t::closesquarebracketclosesquarebracket)
		CCERR_RET(EINVAL,tq_peek().pos,"Closing ]] expected");

	return 1;
}

int cc_state_t::gnu_attribute_parse(declspec_t &dsc,const position_t &pos) {
	int r;

	(void)pos;

	// __attribute__ has already been taken, we should be at the open parens
	if (tq_get().type != token_type_t::openparenthesis)
		CCERR_RET(EINVAL,tq_peek().pos,"Opening parenthesis expected");
	if (tq_get().type != token_type_t::openparenthesis)
		CCERR_RET(EINVAL,tq_peek().pos,"Opening parenthesis expected");

	do {
		if (tq_peek().type == token_type_t::identifier) {
			token_t w = std::move(tq_get());
			assert(w.type == token_type_t::identifier);

			if (identifier(w.v.identifier) == "aligned") {
				if ((r=parse_declspec_align(/*&*/dsc.align)) < 1)
					return r;
			}
			else if (identifier(w.v.identifier) == "deprecated") {
				dsc.dcs_flags |= DCS_FL_DEPRECATED;
			}
			else if (identifier(w.v.identifier) == "dllimport") {
				dsc.dcs_flags |= DCS_FL_DLLIMPORT;
			}
			else if (identifier(w.v.identifier) == "dllexport") {
				dsc.dcs_flags |= DCS_FL_DLLEXPORT;
			}
			else if (identifier(w.v.identifier) == "naked") {
				dsc.dcs_flags |= DCS_FL_NAKED;
			}
			else {
				CCERR_RET(EINVAL,tq_peek().pos,"Unknown __attribute__");
			}

			if (tq_peek().type == token_type_t::closeparenthesis) {
				/* OK */
			}
			else if (tq_peek().type == token_type_t::comma) {
				tq_discard();
				/* good */
			}
			else {
				CCERR_RET(EINVAL,tq_peek().pos,"comma or closing paran expected");
			}
		}
		else if (tq_peek().type == token_type_t::closeparenthesis) {
			break;
		}
		else {
			// FIXME: Unknown attributes are supposed to be ignored
			//        GCC issues a warning for unknown attributes
			CCERR_RET(EINVAL,tq_peek().pos,"error parsing __attribute__");
		}
	} while(1);

	// closing parens expected
	if (tq_get().type != token_type_t::closeparenthesis)
		CCERR_RET(EINVAL,tq_peek().pos,"Closing parenthesis expected");
	if (tq_get().type != token_type_t::closeparenthesis)
		CCERR_RET(EINVAL,tq_peek().pos,"Closing parenthesis expected");

	return 1;
}

int cc_state_t::ms_declspec_parse(declspec_t &dsc,const position_t &pos) {
	int r;

	(void)pos;

	// __declspec has already been taken, we should be at the open parens
	if (tq_get().type != token_type_t::openparenthesis)
		CCERR_RET(EINVAL,tq_peek().pos,"Opening parenthesis expected");

	do {
		if (tq_peek().type == token_type_t::identifier) {
			token_t w = std::move(tq_get());
			assert(w.type == token_type_t::identifier);

			if (identifier(w.v.identifier) == "align") {
				if ((r=parse_declspec_align(/*&*/dsc.align)) < 1)
					return r;
			}
			else if (identifier(w.v.identifier) == "deprecated") {
				dsc.dcs_flags |= DCS_FL_DEPRECATED;
			}
			else if (identifier(w.v.identifier) == "dllimport") {
				dsc.dcs_flags |= DCS_FL_DLLIMPORT;
			}
			else if (identifier(w.v.identifier) == "dllexport") {
				dsc.dcs_flags |= DCS_FL_DLLEXPORT;
			}
			else if (identifier(w.v.identifier) == "naked") {
				dsc.dcs_flags |= DCS_FL_NAKED;
			}
			else {
				/* should we ignore unknown ones? Does Microsoft C/C++ ignore unknown ones? */
				CCERR_RET(EINVAL,tq_peek().pos,"Unknown __declspec");
			}
		}
		else if (tq_peek().type == token_type_t::closeparenthesis) {
			break;
		}
		else {
			CCERR_RET(EINVAL,tq_peek().pos,"error parsing __declspec");
		}
	} while(1);

	// closing parens expected
	if (tq_get().type != token_type_t::closeparenthesis)
		CCERR_RET(EINVAL,tq_peek().pos,"Closing parenthesis expected");

	return 1;
}

int cc_state_t::declspec_alignas(addrmask_t &ds_align,const position_t &pos) {
	int r;

	if (ds_align != addrmask_none)
		CCERR_RET(EINVAL,pos,"Alignas already specified");

	{
		ast_node_id_t expr = ast_node_none;

		if ((r=typeid_or_expr_parse(expr)) < 1)
			return r;

		ast_node_reduce(expr);

		ast_node_t &an = ast_node(expr);
		if (an.t.type == token_type_t::integer) {
			if (an.t.v.integer.v.u != 0) {
				if (an.t.v.integer.v.u & (an.t.v.integer.v.u - 1ull))
					CCERR_RET(EINVAL,pos,"Alignas expression not a power of 2");

				ds_align = addrmask_make(an.t.v.integer.v.u);
			}
		}
		else if (an.t.type == token_type_t::op_declaration) {
			declaration_t *decl = an.t.v.declaration;

			assert(decl != NULL);
			if (decl->declor.size() != 1)
				CCERR_RET(EINVAL,pos,"Unexpected number of declor");

			addrmask_t align = calc_alignofmask(decl->spec,decl->declor[0].ddip);
			if (align == addrmask_none)
				CCERR_RET(EINVAL,pos,"Unable to determine alignof for alignas");

			ds_align = align;
		}
		else if (an.t.type == token_type_t::op_symbol) {
			assert(an.t.v.symbol != symbol_none);
			auto &sym = symbol(an.t.v.symbol);
			if (sym.sym_type == symbol_t::FUNCTION || sym.sym_type == symbol_t::NONE)
				CCERR_RET(EINVAL,pos,"alignas for function or none");

			addrmask_t align = calc_alignofmask(sym.spec,sym.ddip);
			if (align == addrmask_none)
				CCERR_RET(EINVAL,pos,"Unable to determine alignof for alignas");

			ds_align = align;
		}

		ast_node(expr).release();
	}

	return 1;
}

int cc_state_t::declaration_specifiers_parse(declaration_specifiers_t &ds,const unsigned int declspec) {
	const position_t pos = tq_peek().pos;
	addrmask_t ds_align = addrmask_none;
	type_specifier_t builtin_ts = 0;
	int r;

#define XCHK(d,m) \
	if (d&m) { \
		CCERR_RET(EINVAL,t.pos,"declarator specifier '%s' already specified",token_type_t_str(t.type)); \
	} \
	else { \
		ds.count++; \
		d|=m; \
	}
#define X(d,m) \
	{ \
		XCHK(d,m); \
		tq_discard(); \
		continue; \
	}

	do {
		const token_t &t = tq_peek();

		switch (t.type) {
			case token_type_t::r_typedef:		X(ds.storage_class,SC_TYPEDEF);
			case token_type_t::r_extern:		X(ds.storage_class,SC_EXTERN);
			case token_type_t::r_static:		X(ds.storage_class,SC_STATIC);
			case token_type_t::r_auto:		X(ds.storage_class,SC_AUTO);
			case token_type_t::r_register:		X(ds.storage_class,SC_REGISTER);
			case token_type_t::r_constexpr:		X(ds.storage_class,SC_CONSTEXPR);
			case token_type_t::r_inline:		X(ds.storage_class,SC_INLINE);
			case token_type_t::r_consteval:		X(ds.storage_class,SC_CONSTEVAL);
			case token_type_t::r_constinit:		X(ds.storage_class,SC_CONSTINIT);

			case token_type_t::r_alignas:
			case token_type_t::r__Alignas:
				tq_discard();
				ds.count++;

				if ((r=declspec_alignas(ds_align,pos)) < 1)
					return r;

				continue;

			case token_type_t::r__declspec:
			case token_type_t::r___declspec:
				{
					declspec_t dcl;

					tq_discard();
					ds.count++;

					if ((r=ms_declspec_parse(dcl,pos)) < 1)
						return r;

					if (dcl.align != addrmask_none) {
						if (ds_align != addrmask_none)
							CCERR_RET(EINVAL,pos,"align already specified");

						ds_align = dcl.align;
					}

					if (ds.dcs_flags & dcl.dcs_flags)
						CCERR_RET(EINVAL,pos,"one or more __declspec items have been already specified");

					ds.dcs_flags |= dcl.dcs_flags;
					continue;
				}

			case token_type_t::r___attribute__:
				{
					declspec_t dcl;

					tq_discard();
					ds.count++;

					if ((r=gnu_attribute_parse(dcl,pos)) < 1)
						return r;

					if (dcl.align != addrmask_none) {
						if (ds_align != addrmask_none)
							CCERR_RET(EINVAL,pos,"align already specified");

						ds_align = dcl.align;
					}

					if (ds.dcs_flags & dcl.dcs_flags)
						CCERR_RET(EINVAL,pos,"one or more __declspec items have been already specified");

					ds.dcs_flags |= dcl.dcs_flags;
					continue;
				}

			case token_type_t::opensquarebracketopensquarebracket:
				{
					declspec_t dcl;

					tq_discard();
					ds.count++;

					if ((r=cpp11_attribute_parse(dcl,pos)) < 1)
						return r;

					if (dcl.align != addrmask_none) {
						if (ds_align != addrmask_none)
							CCERR_RET(EINVAL,pos,"align already specified");

						ds_align = dcl.align;
					}

					if (ds.dcs_flags & dcl.dcs_flags)
						CCERR_RET(EINVAL,pos,"one or more __declspec items have been already specified");

					ds.dcs_flags |= dcl.dcs_flags;
					continue;
				}

			default: break;
		}

		break;
	} while (1);

	do {
		const token_t &t = tq_peek();

		switch (t.type) {
			case token_type_t::r_void:		X(ds.type_specifier,TS_VOID);
			case token_type_t::r_char:		X(ds.type_specifier,TS_CHAR);
			case token_type_t::r_short:		X(ds.type_specifier,TS_SHORT);
			case token_type_t::r_int:		X(ds.type_specifier,TS_INT);
			case token_type_t::r_float:		X(ds.type_specifier,TS_FLOAT);
			case token_type_t::r_double:		X(ds.type_specifier,TS_DOUBLE);
			case token_type_t::r_signed:		X(ds.type_specifier,TS_SIGNED);
			case token_type_t::r_unsigned:		X(ds.type_specifier,TS_UNSIGNED);
			case token_type_t::r_const:		X(ds.type_qualifier,TQ_CONST);
			case token_type_t::r_volatile:		X(ds.type_qualifier,TQ_VOLATILE);
			case token_type_t::r_near:		X(ds.type_qualifier,TQ_NEAR);
			case token_type_t::r_far:		X(ds.type_qualifier,TQ_FAR);
			case token_type_t::r_huge:		X(ds.type_qualifier,TQ_HUGE);
			case token_type_t::r_restrict:		X(ds.type_qualifier,TQ_RESTRICT);

			case token_type_t::r_long:
				ds.count++;
				if (ds.type_specifier & TS_LONG) {
					/* second "long" promote to "long long" because GCC allows it too i.e. "long int long" is the same as "long long int" */
					if (ds.type_specifier & TS_LONGLONG)
						CCERR_RET(EINVAL,pos,"declarator specifier 'long long' already specified");

					ds.type_specifier = (ds.type_specifier & (~TS_LONG)) | TS_LONGLONG;
					tq_discard();
				}
				else {
					if (ds.type_specifier & TS_LONG)
						CCERR_RET(EINVAL,pos,"declarator specifier 'long' already specified");

					ds.type_specifier |= TS_LONG;
					tq_discard();
				}
				continue;

#define ONLYSPECALLOWED if (ds.type_specifier != 0) goto common_error
			case token_type_t::r_size_t:
				builtin_ts = data_types_ptr_data.dt_size_t.ts; ONLYSPECALLOWED; ds.type_specifier |= TS_UNSIGNED; goto common_builtin;
			case token_type_t::r_ssize_t:
				builtin_ts = data_types_ptr_data.dt_size_t.ts; ONLYSPECALLOWED; ds.type_specifier |= TS_SIGNED; goto common_builtin;
			case token_type_t::r___int8:
				builtin_ts = TS_INT | TS_SZ8; goto common_builtin;
			case token_type_t::r___int16:
				builtin_ts = TS_INT | TS_SZ16; goto common_builtin;
			case token_type_t::r___int32:
				builtin_ts = TS_INT | TS_SZ32; goto common_builtin;
			case token_type_t::r___int64:
				builtin_ts = TS_INT | TS_SZ64; goto common_builtin;
			case token_type_t::r_int8_t:
				builtin_ts = TS_INT | TS_SZ8; ONLYSPECALLOWED; ds.type_specifier |= TS_SIGNED; goto common_builtin;
			case token_type_t::r_int16_t:
				builtin_ts = TS_INT | TS_SZ16; ONLYSPECALLOWED; ds.type_specifier |= TS_SIGNED; goto common_builtin;
			case token_type_t::r_int32_t:
				builtin_ts = TS_INT | TS_SZ32; ONLYSPECALLOWED; ds.type_specifier |= TS_SIGNED; goto common_builtin;
			case token_type_t::r_int64_t:
				builtin_ts = TS_INT | TS_SZ64; ONLYSPECALLOWED; ds.type_specifier |= TS_SIGNED; goto common_builtin;
			case token_type_t::r_uint8_t:
				builtin_ts = TS_INT | TS_SZ8; ONLYSPECALLOWED; ds.type_specifier |= TS_UNSIGNED; goto common_builtin;
			case token_type_t::r_uint16_t:
				builtin_ts = TS_INT | TS_SZ16; ONLYSPECALLOWED; ds.type_specifier |= TS_UNSIGNED; goto common_builtin;
			case token_type_t::r_uint32_t:
				builtin_ts = TS_INT | TS_SZ32; ONLYSPECALLOWED; ds.type_specifier |= TS_UNSIGNED; goto common_builtin;
			case token_type_t::r_uint64_t:
				builtin_ts = TS_INT | TS_SZ64; ONLYSPECALLOWED; ds.type_specifier |= TS_UNSIGNED; goto common_builtin;
			case token_type_t::r_uintptr_t:
				builtin_ts = data_types_ptr_data.dt_intptr_t.ts; ONLYSPECALLOWED; ds.type_specifier |= TS_UNSIGNED; goto common_builtin;
			case token_type_t::r_intptr_t:
				builtin_ts = data_types_ptr_data.dt_intptr_t.ts; ONLYSPECALLOWED; ds.type_specifier |= TS_SIGNED; goto common_builtin;

#undef ONLYSPECALLOWED
common_builtin:
			if (ds.type_specifier & TS_MATCH_BUILTIN)
				break;

			ds.type_specifier |= TS_MATCH_BUILTIN;
			tq_discard();
			ds.count++;
			continue;
common_error:
			CCERR_RET(EINVAL,pos,"Extra specifiers for builtin type");

			case token_type_t::identifier:
				if (ds.type_specifier != 0)
					break;

				{
					symbol_id_t sid;

					if ((sid=lookup_symbol(t.v.identifier,symbol_t::TYPEDEF)) == symbol_none)
						break;

					if (symbol(sid).sym_type != symbol_t::TYPEDEF)
						break;

					ds.type_specifier |= TS_MATCH_TYPEDEF;
					ds.type_identifier_symbol = sid;
					tq_discard();
					ds.count++;
				}
				continue;

			case token_type_t::r_enum:
				ds.count++;
				if (ds.type_specifier & TS_ENUM)
					CCERR_RET(EINVAL,pos,"declarator specifier 'enum' already specified");
				if (ds.type_specifier != 0)
					break;

				/* typedef is ignored here */
				ds.storage_class &= ~SC_TYPEDEF;

				ds.type_specifier |= TS_ENUM;
				tq_discard();

				/* enum { list }
				 * enum identifier { list }
				 * enum identifier */

				{
					declaration_specifiers_t eds;
					declarator_t declor;
					symbol_lookup_t sl;

					eds.count = 3;
					eds.type_specifier = TS_INT;
					eds.storage_class = SC_CONSTEXPR;
					eds.type_qualifier = TQ_CONST;

					if (tq_peek().type == token_type_t::identifier)
						identifier.assign(/*to*/declor.name,/*from*/tq_get().v.identifier);

					if (tq_peek().type == token_type_t::colon) {
						tq_discard();

						eds.type_specifier &= ~TS_INT;
						if ((r=declaration_specifiers_parse(eds)) < 1)
							return r;
					}

					sl.pos = pos;
					sl.st = symbol_t::ENUM;
					if (tq_peek().type == token_type_t::opencurlybracket) {
						tq_discard();

						if (!(declspec & DECLSPEC_ALLOW_DEF))
							CCERR_RET(EINVAL,pos,"not allowed to define types here");

						sl.flags = symbol_t::FL_DEFINED|symbol_t::FL_DECLARED;
						if ((r=prep_symbol_lookup(sl,eds,declor)) < 1)
							return r;
						if (do_local_symbol_lookup(sl,eds,declor)) {
							if ((r=check_symbol_lookup_match(sl,eds,declor)) < 1)
								return r;
						}
						else if ((r=add_symbol(sl,eds,declor)) < 1) {
							return r;
						}

						if ((r=enumerator_list_parse(eds,ds.enum_list)) < 1)
							return r;

						ds.type_identifier_symbol = sl.sid;
					}
					else if (declor.name != identifier_none) {
						if ((sl.sid=lookup_symbol(declor.name,sl.st)) == symbol_none)
							CCERR_RET(ENOENT,sl.pos,"No such enum");

						ds.type_identifier_symbol = sl.sid;
					}
				}
				continue;

			case token_type_t::r_struct:
				ds.count++;
				if (ds.type_specifier & TS_STRUCT)
					CCERR_RET(EINVAL,pos,"declarator specifier 'struct' already specified");
				if (ds.type_specifier != 0)
					break;

				ds.type_specifier |= TS_STRUCT;
				tq_discard();

				{
					addrmask_t struct_align = addrmask_none;
					identifier_id_t name = identifier_none;
					declarator_t declor;
					symbol_lookup_t sl;

					/* struct { list }
					 * struct identifier { list }
					 * struct identifier */
					do {
						switch (tq_peek().type) {
							case token_type_t::r_alignas:
							case token_type_t::r__Alignas:
								tq_discard();
								ds.count++;

								if ((r=declspec_alignas(struct_align,pos)) < 1)
									return r;

								continue;

							case token_type_t::r__declspec:
							case token_type_t::r___declspec:
								{
									declspec_t dcl;

									tq_discard();
									ds.count++;

									if ((r=ms_declspec_parse(dcl,pos)) < 1)
										return r;

									if (dcl.align != addrmask_none) {
										if (struct_align != addrmask_none)
											CCERR_RET(EINVAL,pos,"align already specified");

										struct_align = dcl.align;
									}
									continue;
								}

								continue;

							case token_type_t::r___attribute__:
								{
									declspec_t dcl;

									tq_discard();
									ds.count++;

									if ((r=gnu_attribute_parse(dcl,pos)) < 1)
										return r;

									if (dcl.align != addrmask_none) {
										if (struct_align != addrmask_none)
											CCERR_RET(EINVAL,pos,"align already specified");

										struct_align = dcl.align;
									}
									continue;
								}

								continue;

							case token_type_t::opensquarebracketopensquarebracket:
								{
									declspec_t dcl;

									tq_discard();
									ds.count++;

									if ((r=cpp11_attribute_parse(dcl,pos)) < 1)
										return r;

									if (dcl.align != addrmask_none) {
										if (struct_align != addrmask_none)
											CCERR_RET(EINVAL,pos,"align already specified");

										struct_align = dcl.align;
									}
									continue;
								}

							default:
								break;
						}

						break;
					} while(1);

					if (tq_peek().type == token_type_t::identifier)
						identifier.assign(/*to*/name,/*from*/tq_get().v.identifier);

					if (!(ds.storage_class & SC_TYPEDEF))
						identifier.assignmove(/*to*/declor.name,/*from*/name);

					if (ds.align == addrmask_none)
						ds.align = struct_align;
					else
						ds.align &= struct_align;

					sl.pos = pos;
					sl.st = symbol_t::STRUCT;
					if (tq_peek().type == token_type_t::opencurlybracket) {
						tq_discard();

						if (!(declspec & DECLSPEC_ALLOW_DEF))
							CCERR_RET(EINVAL,pos,"not allowed to define types here");

						sl.flags = symbol_t::FL_DEFINED|symbol_t::FL_DECLARED;
						if ((r=prep_symbol_lookup(sl,ds,declor)) < 1)
							return r;
						if (do_local_symbol_lookup(sl,ds,declor)) {
							if ((r=check_symbol_lookup_match(sl,ds,declor)) < 1)
								return r;
						}
						else if ((r=add_symbol(sl,ds,declor)) < 1) {
							return r;
						}

						do {
							if (tq_peek().type == token_type_t::closecurlybracket) {
								tq_discard();
								break;
							}

							if ((r=struct_declaration_parse(sl.sid,token_type_t::r_struct)) < 1)
								return r;
						} while(1);

						if ((r=struct_field_layout(sl.sid)) < 1)
							return r;

						ds.type_identifier_symbol = sl.sid;
					}
					else if (declor.name != identifier_none) {
						sl.flags = symbol_t::FL_DECLARED;
						if ((r=prep_symbol_lookup(sl,ds,declor)) < 1)
							return r;
						if (do_local_symbol_lookup(sl,ds,declor)) {
							if ((r=check_symbol_lookup_match(sl,ds,declor)) < 1)
								return r;
						}
						else if ((r=add_symbol(sl,ds,declor)) < 1) {
							return r;
						}

						ds.type_identifier_symbol = sl.sid;
					}

					/* remove align */
					ds.align = addrmask_none;

					if ((ds.storage_class & SC_TYPEDEF) && name != identifier_none) {
						declaration_specifiers_t s_ds;
						declarator_t s_declor;
						symbol_lookup_t s_sl;

						if (!(declspec & DECLSPEC_ALLOW_DEF))
							CCERR_RET(EINVAL,pos,"not allowed to define types here");

						identifier.assignmove(/*to*/s_declor.name,/*from*/name);

						if (ds.type_identifier_symbol != symbol_none) {
							symbol_t &sym = symbol(ds.type_identifier_symbol);
							sym.spec.storage_class &= ~SC_TYPEDEF;
						}

						{/*alignas() is ignored for typedef....right? */
							ddip_list_t dummy;
							s_ds.align = calc_alignofmask(ds,dummy);
						}

						s_sl.pos = pos;
						s_sl.st = symbol_t::TYPEDEF;
						s_sl.flags = symbol_t::FL_DEFINED|symbol_t::FL_DECLARED;
						s_ds.storage_class = SC_TYPEDEF;
						s_ds.type_identifier_symbol = ds.type_identifier_symbol;
						ds.type_identifier_symbol = symbol_none;

						if ((r=prep_symbol_lookup(s_sl,s_ds,s_declor)) < 1)
							return r;
						if (do_local_symbol_lookup(s_sl,s_ds,s_declor)) {
							if ((r=check_symbol_lookup_match(s_sl,s_ds,s_declor)) < 1)
								return r;
						}
						else if ((r=add_symbol(s_sl,s_ds,s_declor)) < 1) {
							return r;
						}

						ds.type_identifier_symbol = s_sl.sid;
					}

					/* remove the typedef flag */
					ds.storage_class &= ~SC_TYPEDEF;
				}
				continue;

			case token_type_t::r_union:
				ds.count++;

				if (ds.type_specifier & TS_UNION)
					CCERR_RET(EINVAL,pos,"declarator specifier 'union' already specified");
				if (ds.type_specifier != 0)
					break;

				ds.type_specifier |= TS_UNION;
				tq_discard();

				/* union { list }
				 * union identifier { list }
				 * union identifier */

				{
					declarator_t declor;
					symbol_lookup_t sl;

					if (tq_peek().type == token_type_t::identifier)
						identifier.assign(/*to*/declor.name,/*from*/tq_get().v.identifier);

					sl.pos = pos;
					sl.st = symbol_t::UNION;
					if (tq_peek().type == token_type_t::opencurlybracket) {
						tq_discard();

						if (!(declspec & DECLSPEC_ALLOW_DEF))
							CCERR_RET(EINVAL,pos,"not allowed to define types here");

						sl.flags = symbol_t::FL_DEFINED|symbol_t::FL_DECLARED;
						if ((r=prep_symbol_lookup(sl,ds,declor)) < 1)
							return r;
						if (do_local_symbol_lookup(sl,ds,declor)) {
							if ((r=check_symbol_lookup_match(sl,ds,declor)) < 1)
								return r;
						}
						else if ((r=add_symbol(sl,ds,declor)) < 1) {
							return r;
						}

						do {
							if (tq_peek().type == token_type_t::closecurlybracket) {
								tq_discard();
								break;
							}

							if ((r=struct_declaration_parse(sl.sid,token_type_t::r_union)) < 1)
								return r;
						} while(1);

						if ((r=struct_field_layout(sl.sid)) < 1)
							return r;

						ds.type_identifier_symbol = sl.sid;
					}
					else if (declor.name != identifier_none) {
						sl.flags = symbol_t::FL_DECLARED;
						if ((r=prep_symbol_lookup(sl,ds,declor)) < 1)
							return r;
						if (do_local_symbol_lookup(sl,ds,declor)) {
							if ((r=check_symbol_lookup_match(sl,ds,declor)) < 1)
								return r;
						}
						else if ((r=add_symbol(sl,ds,declor)) < 1) {
							return r;
						}

						ds.type_identifier_symbol = sl.sid;
					}
				}
				continue;

			default: break;
		}
#undef XCHK
#undef X

		break;
	} while (1);

	/* unless told otherwise, it is an error for this code not to parse any tokens */
	if (!(declspec & DECLSPEC_OPTIONAL) && ds.count == 0)
		CCERR_RET(EINVAL,pos,"Declaration specifiers expected");

	/* unless asked not to parse type specifiers, it is an error not to specify one.
	 * You can't say "static x" for example */
	if (!(declspec & DECLSPEC_OPTIONAL) && ds.type_specifier == 0)
		CCERR_RET(EINVAL,pos,"Type specifiers expected. Specify a type here");

	/* sanity check */
	{
		const type_qualifier_t mm_t = ds.type_qualifier & (TQ_NEAR|TQ_FAR|TQ_HUGE); /* only one of */
		if (mm_t && !only_one_bit_set(mm_t))
			CCERR_RET(EINVAL,pos,"Multiple storage classes specified");

		const storage_class_t sc_t = ds.storage_class & (SC_TYPEDEF|SC_EXTERN|SC_STATIC|SC_AUTO|SC_REGISTER); /* only one of */
		if (sc_t && !only_one_bit_set(sc_t))
			CCERR_RET(EINVAL,pos,"Multiple storage classes specified");

		const storage_class_t ce_t = ds.storage_class & (SC_CONSTEXPR|SC_CONSTEVAL|SC_CONSTINIT); /* only one of */
		if (ce_t && !only_one_bit_set(ce_t))
			CCERR_RET(EINVAL,pos,"Multiple storage classes specified");
	}

	/* "long int" -> "long"
	 * "long long int" -> "long"
	 * "signed long long int" -> "signed long long"
	 * "short int" -> "short"
	 *
	 * The "int" part is redundant */
	{
		const type_specifier_t t = ds.type_specifier & ~(TS_SIGNED|TS_UNSIGNED);
		if (t == (TS_LONG|TS_INT) || t == (TS_LONGLONG|TS_INT) || t == (TS_SHORT|TS_INT))
			ds.type_specifier &= ~TS_INT;
	}

	/* skip type specifier checks if and only "long double" because "long double" would fail the below checks */
	if (ds.type_specifier != (TS_LONG|TS_DOUBLE)) {
		const type_specifier_t sign_t = ds.type_specifier & (TS_SIGNED|TS_UNSIGNED); /* only one of */
		if (sign_t && !only_one_bit_set(sign_t))
			CCERR_RET(EINVAL,pos,"Multiple type specifiers (signed/unsigned)");

		const type_specifier_t intlen_t = ds.type_specifier & (TS_VOID|TS_CHAR|TS_SHORT|TS_INT|TS_LONG|TS_LONGLONG|TS_ENUM|TS_STRUCT|TS_UNION|TS_MATCH_TYPEDEF|TS_MATCH_BUILTIN); /* only one of */
		if (intlen_t && !only_one_bit_set(intlen_t))
			CCERR_RET(EINVAL,pos,"Multiple type specifiers (int/char/void)");

		const type_specifier_t floattype_t = ds.type_specifier & (TS_FLOAT|TS_DOUBLE); /* only one of */
		if (floattype_t && !only_one_bit_set(floattype_t))
			CCERR_RET(EINVAL,pos,"Multiple type specifiers (float)");

		if (intlen_t && floattype_t)
			CCERR_RET(EINVAL,pos,"Multiple type specifiers (float+int)");

		if (floattype_t && sign_t)
			CCERR_RET(EINVAL,pos,"Multiple type specifiers (float+signed/unsigned)");
	}

	if (ds.type_specifier & TS_MATCH_BUILTIN) {
		if ((builtin_ts & (TS_UNSIGNED|TS_SIGNED)) == 0)
			builtin_ts |= ds.type_specifier & (TS_UNSIGNED|TS_SIGNED);

		ds.type_specifier = builtin_ts;
	}

	/* first compute the natural alignment */
	{
		ddip_list_t dummy;
		ds.align = calc_alignofmask(ds,dummy);
	}

	if (ds.align != addrmask_none && ds_align != addrmask_none)
		ds.align &= ds_align;

#if 0//DEBUG
	fprintf(stderr,"DEBUG %s:%d:\n",__FUNCTION__,__LINE__);
	debug_dump_declaration_specifiers("  ",ds);
#endif

	return 1;
}

int cc_state_t::pointer_parse(std::vector<pointer_t> &ptr) {
	int r;

#if 0//DEBUG
	fprintf(stderr,"%s():parsing\n",__FUNCTION__);
#endif

	/* pointer handling
	 *
	 *   <nothing>
	 *   *
	 *   **
	 *   * const *
	 *   *** const * const
	 *
	 *   and so on */
	while (tq_peek().type == token_type_t::star) {
		declaration_specifiers_t ds;
		pointer_t p;

		tq_discard();

		if ((r=declaration_specifiers_parse(ds,DECLSPEC_OPTIONAL)) < 1)
			return r;

		assert(ds.type_specifier == 0 && ds.storage_class == 0);
		assert(ds.type_identifier_symbol == symbol_none);
		p.tq = ds.type_qualifier;

		ptr.push_back(std::move(p));
	}

	if (!ptr.empty()) {
#if 0//DEBUG
		fprintf(stderr,"DEBUG %s:%d:\n",__FUNCTION__,__LINE__);
		debug_dump_pointer("  ",ptr);
#endif
	}

	return 1;
}

int cc_state_t::direct_declarator_inner_parse(ddip_list_t &dp,declarator_t &dd,position_t &pos,unsigned int flags) {
	int r;

	/* [*] identifier [arraydefs]
	 * ( [*] identifier [arraydefs] )
	 * ( [*] identifier ) [arraydefs]
	 * ( [*] ( [*] identifier [arraydefs] ) )
	 * ( [*] ( [*] identifier ) [arraydefs] )
	 * ( [*] ( [*] identifier ) ) [arraydefs]
	 *
	 * This parses one level within parens.
	 *
	 * apparently postfix operators (arraydefs) within a layer take priority over the pointer.
	 *
	 * int *ar[4];     ar is an array of 4 pointers to int
	 * int (*ar)[4];   ar is a pointer to an array of 4 ints */
	ddip_list_t sdp;
	ddip_t tdp;

	if ((r=pointer_parse(tdp.ptr)) < 1)
		return r;

	if (tq_peek().type == token_type_t::openparenthesis) {
		tq_discard();

		/* WARNING: pushes to vector which causes reallocation which will make references to array elements stale */
		if ((r=direct_declarator_inner_parse(sdp,dd,pos,flags)) < 1)
			return r;

		if (tq_get().type != token_type_t::closeparenthesis)
			CCERR_RET(EINVAL,tq_peek().pos,"Closing parenthesis expected");
	}
	else {
		assert(dd.name == identifier_none);
		if (tq_peek().type == token_type_t::identifier && tq_peek().v.identifier != identifier_none) {
			if (flags & DIRDECL_NO_IDENTIFIER) CCERR_RET(EINVAL,tq_peek().pos,"Identifier not allowed here");
			identifier.assign(/*to*/dd.name,/*from*/tq_get().v.identifier);
		}
		else if (flags & DIRDECL_ALLOW_ABSTRACT) {
			dd.name = identifier_none;
		}
		else {
			CCERR_RET(EINVAL,tq_peek().pos,"Identifier expected");
		}
	}

	while (tq_peek().type == token_type_t::opensquarebracket) {
		tq_discard();

		/* NTS: "[]" is acceptable */
		ast_node_id_t expr = ast_node_none;
		if (tq_peek().type != token_type_t::closesquarebracket) {
			if ((r=conditional_expression(expr)) < 1)
				return r;

			ast_node_reduce(expr);
		}

		tdp.arraydef.push_back(std::move(expr));
		if (tq_get().type != token_type_t::closesquarebracket)
			CCERR_RET(EINVAL,tq_peek().pos,"Closing square bracket expected");
	}

	if (tq_peek().type == token_type_t::openparenthesis) {
		tq_discard();

		/* NTS: "()" is acceptable */
		tdp.dd_flags |= declarator_t::FL_FUNCTION;

		/* if we're at the () in (*function)() then this is a function pointer */
		if (sdp.size() == 1)
			if (!sdp[0].ptr.empty())
				tdp.dd_flags |= declarator_t::FL_FUNCTION_POINTER;

		if (tq_peek().type != token_type_t::closeparenthesis) {
			do {
				if (tq_peek().type == token_type_t::ellipsis) {
					tq_discard();

					tdp.dd_flags |= declarator_t::FL_ELLIPSIS;

					/* At least one paremter is required for ellipsis! */
					if (tdp.parameters.empty()) {
						CCerr(pos,"Variadic functions must have at least one named parameter");
						return errno_return(EINVAL);
					}

					break;
				}

				parameter_t p;

				if ((r=declaration_specifiers_parse(p.spec,DECLSPEC_OPTIONAL)) < 1)
					return r;

				if ((r=direct_declarator_parse(p.spec,p.decl,DIRDECL_ALLOW_ABSTRACT)) < 1)
					return r;

				/* do not allow using the same name again */
				if (p.decl.name != identifier_none) {
					for (const auto &chk_p : tdp.parameters) {
						if (chk_p.decl.name != identifier_none) {
							if (identifier(chk_p.decl.name) == identifier(p.decl.name)) {
								CCerr(pos,"Parameter '%s' already defined",identifier(p.decl.name).to_str().c_str());
								return errno_return(EEXIST);
							}
						}
					}

					if (tq_peek().type == token_type_t::equal) {
						/* if no declaration specifiers were given (just a bare identifier
						 * aka the old 1980s syntax), then you shouldn't be allowed to define
						 * a default value. */
						if (p.spec.empty())
							return errno_return(EINVAL);

						tq_discard();
						if ((r=initializer(p.decl.expr)) < 1)
							return r;
					}
				}

				tdp.parameters.push_back(std::move(p));
				if (tq_peek().type == token_type_t::comma) {
					tq_discard();
					continue;
				}

				break;
			} while (1);
		}

		if (tq_get().type != token_type_t::closeparenthesis)
			CCERR_RET(EINVAL,tq_peek().pos,"Expected closing parenthesis");
	}

	dp.addcombine(std::move(tdp));
	for (auto &s : sdp)
		dp.addcombine(std::move(s));

	return 1;
}

int cc_state_t::direct_declarator_parse(declaration_specifiers_t &ds,declarator_t &dd,unsigned int flags) {
	position_t pos = tq_peek().pos;
	int r;

	/* direct declarator
	 *
	 *   IDENTIFIER
	 *   ( declarator )
	 *
	 *   followed by
	 *
	 *   <nothing>
	 *   [ constant_expression ]
	 *   [ ]
	 *   ( parameter_type_list )
	 *   ( identifier_list )                      <- the old C function syntax you'd see from 1980s code
	 *   ( ) */

	/* if the declaration specifier is an enum, struct, or union, then you're allowed not to specify an identifier */
	if (ds.type_specifier & (TS_ENUM|TS_STRUCT|TS_UNION))
		flags |= DIRDECL_ALLOW_ABSTRACT;

	if ((r=direct_declarator_inner_parse(dd.ddip,dd,pos,flags)) < 1)
		return r;

	/* typedef subst */
	if (ds.type_specifier & TS_MATCH_TYPEDEF) {
		symbol_t &sym = symbol(ds.type_identifier_symbol);
		assert(sym.sym_type == symbol_t::TYPEDEF);

		/* do not replace storage_class */

		/* copy type_specifier */
		ds.type_specifier = sym.spec.type_specifier;

		/* merge type_qualifier */
		{
			const type_qualifier_t msk = TQ_CONST|TQ_VOLATILE|TQ_RESTRICT;
			ds.type_qualifier |= sym.spec.type_qualifier & msk;
		}
		{
			const type_qualifier_t msk = TQ_NEAR|TQ_FAR|TQ_HUGE;
			if ((ds.type_qualifier&msk) == 0)
				ds.type_qualifier |= sym.spec.type_qualifier & msk;
		}

		ds.type_identifier_symbol = sym.spec.type_identifier_symbol;

		if (!sym.ddip.empty()) {
			ddip_list_t saved;
			std::swap(saved,dd.ddip);

			for (const auto &d : sym.ddip)
				dd.ddip.push_back(d);

			auto svi = saved.begin();

			if (svi != saved.end() && !dd.ddip.empty()) {
				auto &lent = dd.ddip.back();

				for (auto &p : svi->ptr)
					lent.ptr.push_back(p);

				std::vector<ast_node_id_t> savearr;
				std::swap(savearr,lent.arraydef);

				for (auto &a : svi->arraydef) {
					lent.arraydef.push_back(a);
					if (a != ast_node_none)
						ast_node(a).addref();
				}

				/* these were already in lent.arraydev and do not need addref() */
				for (auto &a : savearr)
					lent.arraydef.push_back(a);

				if (!lent.parameters.empty() && !svi->parameters.empty())
					CCERR_RET(EALREADY,pos,"Attempt to apply parameters to typedef with parameters");

				svi++;
			}

			for (;svi != saved.end();svi++)
				dd.ddip.addcombine(*svi);
		}
	}

	std::vector<parameter_t> *parameters = NULL;
	{
		ddip_t *f = dd.ddip.funcparampair();
		if (f) {
			dd.flags |= f->dd_flags;
			parameters = &f->parameters;
		}
	}

	/* parameter validation:
	 * you can have either all parameter_type parameters,
	 * or identifier_list parameters.
	 * Do not allow a mix of them. */
	if (dd.flags & declarator_t::FL_FUNCTION) {
		int type = -1; /* -1 = no spec  0 = old identifier only  1 = new parameter type */
		int cls;

		assert(parameters != NULL);
		for (const auto &p : *parameters) {
			if (p.spec.empty())
				cls = 0;
			else
				cls = 1;

			if (type < 0) {
				type = cls;
			}
			else if (type != cls) {
				CCerr(pos,"Mixed parameter style not allowed");
				return errno_return(EINVAL);
			}
		}

		/* if old-school identifier only, then after the parenthesis there will be declarations of the parameters i.e.
		 *
		 * int func(a,b,c)
		 *   int a;
		 *   float b;
		 *   const char *c;
		 * {
		 * }
		 *
		 * apparently GCC also allows:
		 *
		 * int func(a,b,c);
		 *
		 * int func(a,b,c)
		 *   int a;
		 *   int b;
		 *   int c;
		 * {
		 * }
		 *
		 * However that's pretty useless and we're not going to allow that.
		 */
		if (type == 0) {
			do {
				if (tq_peek().type == token_type_t::opencurlybracket || tq_peek().type == token_type_t::semicolon) {
					break;
				}
				else {
					declaration_specifiers_t s_spec;

					if ((r=declaration_specifiers_parse(s_spec)) < 1)
						return r;
					if ((r=cc_chkerr()) < 1)
						return r;

					do {
						declarator_t d;

						if ((r=declaration_inner_parse(s_spec,d)) < 1)
							return r;

						/* check */
						{
							/* the name must match a parameter and it must not already have been given it's type */
							size_t i=0;
							while (i < (*parameters).size()) {
								parameter_t &chk_p = (*parameters)[i];

								if (identifier(d.name) == identifier(chk_p.decl.name))
									break;

								i++;
							}

							/* no match---fail */
							if (i == (*parameters).size()) {
								CCerr(pos,"No such parameter '%s' in identifier list",csliteral(d.name).makestring().c_str());
								return errno_return(ENOENT);
							}

							parameter_t &fp = (*parameters)[i];
							if (fp.spec.empty() && fp.decl.ddip.empty()) {
								fp.spec = s_spec;
								fp.decl.ddip = std::move(d.ddip);
							}
							else {
								CCerr(pos,"Identifier already given type");
								return errno_return(EALREADY);
							}

							/* do not allow initializer for parameter type declaration */
							if (d.expr != ast_node_none) {
								CCerr(pos,"Initializer value not permitted here");
								return errno_return(EINVAL);
							}
						}

						if (tq_peek().type == token_type_t::comma) {
							tq_discard();
							continue;
						}

						break;
					} while(1);

					if (tq_peek().type == token_type_t::semicolon) {
						tq_discard();
						continue;
					}

					break;
				}
			} while (1);

			if (tq_peek().type != token_type_t::opencurlybracket && !(*parameters).empty()) {
				/* no body of the function? */
				CCerr(pos,"Identifier-only parameter list only permitted if the function has a body");
				return errno_return(EINVAL);
			}
		}
	}

#if 0//DEBUG
	fprintf(stderr,"DEBUG %s:%d:\n",__FUNCTION__,__LINE__);
	debug_dump_direct_declarator("  ",dd);
#endif

	if (dd.flags & declarator_t::FL_FUNCTION) {
		/* any parameter not yet described, is an error */
		assert(parameters != NULL);
		for (auto &p : *parameters) {
			if (p.spec.empty()) {
				CCerr(pos,"Parameter '%s' is missing type",identifier(p.decl.name).to_str().c_str());
				debug_dump_parameter("  ",p);
				return errno_return(EINVAL);
			}
		}
	}

	return 1;
}

int cc_state_t::declarator_parse(declaration_specifiers_t &ds,declarator_t &declor) {
	int r;

	if ((r=direct_declarator_parse(ds,declor)) < 1)
		return r;

	return 1;
}

int cc_state_t::struct_bitfield_validate(token_t &t) {
	if (t.type == token_type_t::integer) {
		if (t.v.integer.v.v < 0ll || t.v.integer.v.v >= 255ll)
			CCERR_RET(EINVAL,tq_peek().pos,"Bitfield value out of range");
	}
	else {
		CCERR_RET(EINVAL,tq_peek().pos,"Bitfield range is not a constant integer expression");
	}

	return 1;
}

int cc_state_t::struct_declarator_parse(const symbol_id_t sid,declaration_specifiers_t &ds,declarator_t &declor) {
	bitfield_pos_t bf_start = bitfield_pos_none,bf_length = bitfield_pos_none;
	int r;

	if ((r=direct_declarator_parse(ds,declor)) < 1)
		return r;

	if (tq_peek().type == token_type_t::colon) {
		tq_discard();

		/* This compiler extension: Take the GCC range syntax and make our own
		 * extension that allows specifying the explicit range of bits that make
		 * up the bitfield, rather than just a length and having to count bits.
		 *
		 * [expr]     1-bit field at bit number (expr)
		 * [a ... b]  bit field starting at a, extends to b inclusive.
		 *            For example [ 4 ... 7 ] means a 4-bit field, LSB at bit 4,
		 *            MSB at bit 7. */
		if (tq_peek().type == token_type_t::opensquarebracket) {
			tq_discard();

			bf_length = 1;

			ast_node_id_t expr1 = ast_node_none;
			if ((r=conditional_expression(expr1)) < 1)
				return r;

			ast_node_reduce(expr1);
			ast_node_t &ean1 = ast_node(expr1);
			if ((r=struct_bitfield_validate(ean1.t)) < 1)
				return r;

			assert(ean1.t.type == token_type_t::integer);
			bf_start = bitfield_pos_t(ean1.t.v.integer.v.u);
			ast_node(expr1).release();

			if (tq_peek().type == token_type_t::ellipsis) {
				tq_discard();

				ast_node_id_t expr2 = ast_node_none;
				if ((r=conditional_expression(expr2)) < 1)
					return r;

				ast_node_reduce(expr2);
				ast_node_t &ean2 = ast_node(expr2);
				if ((r=struct_bitfield_validate(ean2.t)) < 1)
					return r;

				assert(ean2.t.type == token_type_t::integer);
				bf_length = bitfield_pos_t(ean2.t.v.integer.v.u) + 1u;
				if (bf_length <= bf_start) CCERR_RET(EINVAL,tq_peek().pos,"Invalid bitfield range");
				bf_length -= bf_start;
				ast_node(expr2).release();
			}

			if (tq_get().type != token_type_t::closesquarebracket)
				CCERR_RET(EINVAL,tq_peek().pos,"Closing square bracket expected");
		}
		else {
			ast_node_id_t expr = ast_node_none;
			if ((r=conditional_expression(expr)) < 1)
				return r;

			ast_node_reduce(expr);
			ast_node_t &ean = ast_node(expr);
			if ((r=struct_bitfield_validate(ean.t)) < 1)
				return r;

			assert(ean.t.type == token_type_t::integer);
			bf_length = bitfield_pos_t(ean.t.v.integer.v.u);
			ast_node(expr).release();
		}
	}

	/* No, you cannot typedef wihtin a C struct (but you can a C++ struct) */
	if (ds.storage_class & SC_TYPEDEF)
		CCERR_RET(EINVAL,tq_peek().pos,"Cannot typedef in a struct or union");

	/* anon enums and unions are OK, GCC allows it */
	if (declor.name == identifier_none) {
		if (ds.type_specifier & TS_ENUM)
			return 1;
		else if (ds.type_specifier & TS_UNION)
		{ /* None. Later stages of struct/union handling will merge anon union fields in and detect conflicts later. */ }
		else
			CCERR_RET(EINVAL,tq_peek().pos,"Anonymous struct field not allowed here");
	}

	symbol_t &sym = symbol(sid);

	if (sym.identifier_exists(declor.name))
		CCERR_RET(EEXIST,tq_peek().pos,"Struct/union field already defined");

	const size_t sfi = sym.fields.size();
	sym.fields.resize(sfi + 1u);
	structfield_t &sf = sym.fields[sfi];
	sf.spec = ds;
	sf.ddip = declor.ddip;
	sf.bf_start = bf_start;
	sf.bf_length = bf_length;
	identifier.assign(/*to*/sf.name,/*from*/declor.name);
	return 1;
}

int cc_state_t::primary_expression(ast_node_id_t &aroot) {
	int r;

	if (	tq_peek().type == token_type_t::strliteral ||
			tq_peek().type == token_type_t::charliteral ||
			tq_peek().type == token_type_t::integer ||
			tq_peek().type == token_type_t::floating) {
		assert(aroot == ast_node_none);
		aroot = ast_node.alloc(tq_get());
	}
	else if (tq_peek().type == token_type_t::identifier) {
		/* if we can make it a symbol ref, do it */
		symbol_id_t sid;

		assert(aroot == ast_node_none);
		if ((sid=lookup_symbol(tq_peek().v.identifier,symbol_t::VARIABLE)) != symbol_none) {
			token_t srt = std::move(tq_get());
			srt.clear_v();
			srt.type = token_type_t::op_symbol;
			srt.v.symbol = sid;
			aroot = ast_node.alloc(srt);
		}
		else {
			// If it's not in the symbol table, it's unknown, not defined
			// aroot = ast_node.alloc(tq_get());
			if (tq_peek().v.identifier != identifier_none)
				CCERR_RET(ENOENT,tq_peek().pos,"'%s' not declared in this scope",identifier(tq_peek().v.identifier).to_str().c_str());
			else
				CCERR_RET(ENOENT,tq_peek().pos,"<anon> not declared in this scope");
		}
	}
	else if (tq_peek().type == token_type_t::openparenthesis) {
		tq_discard();

		assert(aroot == ast_node_none);
		if ((r=expression(aroot)) < 1)
			return r;

		if (tq_get().type != token_type_t::closeparenthesis)
			CCERR_RET(EINVAL,tq_peek().pos,"Subexpression is missing closing parenthesis");
	}
	else {
		CCERR_RET(EINVAL,tq_peek().pos,"Expected primary expression");
	}

	return 1;
}

int cc_state_t::postfix_expression(ast_node_id_t &aroot) {
#define nextexpr primary_expression
	int r;

	if ((r=nextexpr(aroot)) < 1)
		return r;

	do {
		if (tq_peek().type == token_type_t::openparenthesis) {
			tq_discard();

			ast_node_id_t expr = aroot; aroot = ast_node_none;

			aroot = ast_node.alloc(token_type_t::op_function_call);
			ast_node(aroot).set_child(expr); ast_node(expr).release();

			do {
				if (tq_peek().type == token_type_t::closeparenthesis)
					break;

				ast_node_id_t nexpr = ast_node_none;
				if ((r=assignment_expression(nexpr)) < 1)
					return r;

				ast_node_reduce(nexpr);

				ast_node(expr).set_next(nexpr); ast_node(nexpr).release();
				expr = nexpr;

				if (tq_peek().type == token_type_t::closeparenthesis) {
					break;
				}
				else if (tq_peek().type == token_type_t::comma) {
					tq_discard();
					continue;
				}
				else {
					return errno_return(EINVAL);
				}
			} while (1);

			if (tq_get().type != token_type_t::closeparenthesis)
				return errno_return(EINVAL);
		}
		else if (tq_peek().type == token_type_t::opensquarebracket) {
			tq_discard();

			ast_node_id_t expr1 = aroot; aroot = ast_node_none;

			aroot = ast_node.alloc(token_type_t::op_array_ref);
			ast_node(aroot).set_child(expr1); ast_node(expr1).release();

			ast_node_id_t expr2 = ast_node_none;
			if ((r=expression(expr2)) < 1)
				return r;

			ast_node(expr1).set_next(expr2); ast_node(expr2).release();

			if (tq_get().type != token_type_t::closesquarebracket)
				return errno_return(EINVAL);
		}
		else if (tq_peek().type == token_type_t::period) {
			tq_discard();

			ast_node_id_t expr1 = aroot; aroot = ast_node_none;

			aroot = ast_node.alloc(token_type_t::op_member_ref);
			ast_node(aroot).set_child(expr1); ast_node(expr1).release();

			ast_node_id_t expr2 = ast_node_none;
			if ((r=postfix_expression(expr2)) < 1)
				return r;

			ast_node(expr1).set_next(expr2); ast_node(expr2).release();
		}
		else if (tq_peek().type == token_type_t::minusrightanglebracket) {
			tq_discard();

			ast_node_id_t expr1 = aroot; aroot = ast_node_none;

			aroot = ast_node.alloc(token_type_t::op_ptr_ref);
			ast_node(aroot).set_child(expr1); ast_node(expr1).release();

			ast_node_id_t expr2 = ast_node_none;
			if ((r=postfix_expression(expr2)) < 1)
				return r;

			ast_node(expr1).set_next(expr2); ast_node(expr2).release();
		}
		else if (tq_peek().type == token_type_t::plusplus) {
			tq_discard();

			ast_node_id_t expr1 = aroot; aroot = ast_node_none;

			aroot = ast_node.alloc(token_type_t::op_post_increment);
			ast_node(aroot).set_child(expr1); ast_node(expr1).release();
		}
		else if (tq_peek().type == token_type_t::minusminus) {
			tq_discard();

			ast_node_id_t expr1 = aroot; aroot = ast_node_none;

			aroot = ast_node.alloc(token_type_t::op_post_decrement);
			ast_node(aroot).set_child(expr1); ast_node(expr1).release();
		}
		else {
			break;
		}
	} while (1);
#undef nextexpr
	return 1;
}

int cc_state_t::typeid_or_expr_parse(ast_node_id_t &aroot) {
	bool typespec = false;
	int depth = 0;
	int r;

	if (tq_peek().type == token_type_t::openparenthesis) {
		tq_discard();
		depth++;
	}

	if (depth == 1)
		typespec = declaration_specifiers_check();

	if (typespec) {
		std::unique_ptr<declaration_t> declion(new declaration_t);

		if ((r=cc_chkerr()) < 1)
			return r;
		if ((r=declaration_specifiers_parse((*declion).spec)) < 1)
			return r;

		declarator_t &declor = (*declion).new_declarator();

		if ((r=direct_declarator_parse((*declion).spec,declor,DIRDECL_ALLOW_ABSTRACT|DIRDECL_NO_IDENTIFIER)) < 1)
			return r;

		ast_node_id_t decl = ast_node.alloc(token_type_t::op_declaration);
		assert(ast_node(decl).t.type == token_type_t::op_declaration);
		ast_node(decl).t.v.declaration = declion.release();

		if (aroot == ast_node_none) {
			aroot = decl;
		}
		else {
			ast_node(aroot).set_child(decl); ast_node(decl).release();
		}
	}
	else {
		ast_node_id_t expr = ast_node_none;
		if ((r=unary_expression(expr)) < 1)
			return r;

		if (aroot == ast_node_none) {
			aroot = expr;
		}
		else {
			ast_node(aroot).set_child(expr); ast_node(expr).release();
		}
	}

	while (depth > 0 && tq_peek().type == token_type_t::closeparenthesis) {
		tq_discard();
		depth--;
	}

	return 1;
}

int cc_state_t::unary_expression(ast_node_id_t &aroot) {
#define nextexpr postfix_expression
	int r;

	if (tq_peek().type == token_type_t::plusplus) {
		tq_discard();

		assert(aroot == ast_node_none);
		aroot = ast_node.alloc(token_type_t::op_pre_increment);

		ast_node_id_t expr = ast_node_none;
		if ((r=unary_expression(expr)) < 1)
			return r;

		ast_node(aroot).set_child(expr); ast_node(expr).release();
	}
	else if (tq_peek().type == token_type_t::minusminus) {
		tq_discard();

		assert(aroot == ast_node_none);
		aroot = ast_node.alloc(token_type_t::op_pre_decrement);

		ast_node_id_t expr = ast_node_none;
		if ((r=unary_expression(expr)) < 1)
			return r;

		ast_node(aroot).set_child(expr); ast_node(expr).release();
	}
	else if (tq_peek().type == token_type_t::ampersand) {
		tq_discard();

		assert(aroot == ast_node_none);
		aroot = ast_node.alloc(token_type_t::op_address_of);

		ast_node_id_t expr = ast_node_none;
		if ((r=cast_expression(expr)) < 1)
			return r;

		ast_node(aroot).set_child(expr); ast_node(expr).release();
	}
	else if (tq_peek().type == token_type_t::ampersandampersand) {
		tq_discard();

		assert(aroot == ast_node_none);
		aroot = ast_node.alloc(token_type_t::op_address_of);

		ast_node_id_t aroot2 = ast_node.alloc(token_type_t::op_address_of);

		ast_node_id_t expr = ast_node_none;
		if ((r=cast_expression(expr)) < 1)
			return r;

		ast_node(aroot).set_child(aroot2); ast_node(aroot2).release();
		ast_node(aroot2).set_child(expr); ast_node(expr).release();
	}
	else if (tq_peek().type == token_type_t::star) {
		tq_discard();

		assert(aroot == ast_node_none);
		aroot = ast_node.alloc(token_type_t::op_pointer_deref);

		ast_node_id_t expr = ast_node_none;
		if ((r=cast_expression(expr)) < 1)
			return r;

		ast_node(aroot).set_child(expr); ast_node(expr).release();
	}
	else if (tq_peek().type == token_type_t::plus) {
		tq_discard();

		/* So basically +4, right? Which we can just treat as a no-op */
		if ((r=cast_expression(aroot)) < 1)
			return r;
	}
	else if (tq_peek().type == token_type_t::minus) {
		tq_discard();

		assert(aroot == ast_node_none);
		aroot = ast_node.alloc(token_type_t::op_negate);

		ast_node_id_t expr = ast_node_none;
		if ((r=cast_expression(expr)) < 1)
			return r;

		ast_node(aroot).set_child(expr); ast_node(expr).release();
	}
	else if (tq_peek().type == token_type_t::tilde) {
		tq_discard();

		assert(aroot == ast_node_none);
		aroot = ast_node.alloc(token_type_t::op_binary_not);

		ast_node_id_t expr = ast_node_none;
		if ((r=cast_expression(expr)) < 1)
			return r;

		ast_node(aroot).set_child(expr); ast_node(expr).release();
	}
	else if (tq_peek().type == token_type_t::exclamation) {
		tq_discard();

		assert(aroot == ast_node_none);
		aroot = ast_node.alloc(token_type_t::op_logical_not);

		ast_node_id_t expr = ast_node_none;
		if ((r=cast_expression(expr)) < 1)
			return r;

		ast_node(aroot).set_child(expr); ast_node(expr).release();
	}
	else if (tq_peek().type == token_type_t::r_sizeof) {
		tq_discard();

		assert(aroot == ast_node_none);
		aroot = ast_node.alloc(token_type_t::op_sizeof);

		if ((r=typeid_or_expr_parse(aroot)) < 1)
			return r;
	}
	else if (tq_peek().type == token_type_t::r_alignof) {

		tq_discard();

		assert(aroot == ast_node_none);
		aroot = ast_node.alloc(token_type_t::op_alignof);

		if ((r=typeid_or_expr_parse(aroot)) < 1)
			return r;
	}
	else {
		if ((r=nextexpr(aroot)) < 1)
			return r;
	}
#undef nextexpr
	return 1;
}

int cc_state_t::cast_expression(ast_node_id_t &aroot) {
#define nextexpr unary_expression
	int r;

	if (tq_peek(0).type == token_type_t::openparenthesis && declaration_specifiers_check(1)) {
		tq_discard(); /* toss the open parenthesis */

		std::unique_ptr<declaration_t> declion(new declaration_t);

		if ((r=cc_chkerr()) < 1)
			return r;
		if ((r=declaration_specifiers_parse((*declion).spec)) < 1)
			return r;

		declarator_t &declor = (*declion).new_declarator();

		if ((r=direct_declarator_parse((*declion).spec,declor,DIRDECL_ALLOW_ABSTRACT|DIRDECL_NO_IDENTIFIER)) < 1)
			return r;

		aroot = ast_node.alloc(token_type_t::op_typecast);

		ast_node_id_t decl = ast_node.alloc(token_type_t::op_declaration);
		assert(ast_node(decl).t.type == token_type_t::op_declaration);
		ast_node(decl).t.v.declaration = declion.release();

		ast_node(aroot).set_child(decl); ast_node(decl).release();

		if (tq_get().type != token_type_t::closeparenthesis)
			return errno_return(EINVAL);

		ast_node_id_t nxt = ast_node_none;
		if ((r=cast_expression(nxt)) < 1)
			return r;

		ast_node(decl).set_next(nxt); ast_node(nxt).release();
		return 1;
	}

	if ((r=nextexpr(aroot)) < 1)
		return r;

#undef nextexpr
	return 1;
}

int cc_state_t::multiplicative_expression(ast_node_id_t &aroot) {
#define nextexpr cast_expression
	int r;

	if ((r=nextexpr(aroot)) < 1)
		return r;

	do {
		if (tq_peek().type == token_type_t::star) {
			tq_discard();

			ast_node_id_t expr1 = aroot; aroot = ast_node_none;

			aroot = ast_node.alloc(token_type_t::op_multiply);
			ast_node(aroot).set_child(expr1); ast_node(expr1).release();

			ast_node_id_t expr2 = ast_node_none;
			if ((r=nextexpr(expr2)) < 1)
				return r;

			ast_node(expr1).set_next(expr2); ast_node(expr2).release();
		}
		else if (tq_peek().type == token_type_t::forwardslash) {
			tq_discard();

			ast_node_id_t expr1 = aroot; aroot = ast_node_none;

			aroot = ast_node.alloc(token_type_t::op_divide);
			ast_node(aroot).set_child(expr1); ast_node(expr1).release();

			ast_node_id_t expr2 = ast_node_none;
			if ((r=nextexpr(expr2)) < 1)
				return r;

			ast_node(expr1).set_next(expr2); ast_node(expr2).release();
		}
		else if (tq_peek().type == token_type_t::percent) {
			tq_discard();

			ast_node_id_t expr1 = aroot; aroot = ast_node_none;

			aroot = ast_node.alloc(token_type_t::op_modulus);
			ast_node(aroot).set_child(expr1); ast_node(expr1).release();

			ast_node_id_t expr2 = ast_node_none;
			if ((r=nextexpr(expr2)) < 1)
				return r;

			ast_node(expr1).set_next(expr2); ast_node(expr2).release();
		}
		else {
			break;
		}
	} while (1);
#undef nextexpr
	return 1;
}

int cc_state_t::additive_expression(ast_node_id_t &aroot) {
#define nextexpr multiplicative_expression
	int r;

	if ((r=nextexpr(aroot)) < 1)
		return r;

	do {
		if (tq_peek().type == token_type_t::plus) {
			tq_discard();

			ast_node_id_t expr1 = aroot; aroot = ast_node_none;

			aroot = ast_node.alloc(token_type_t::op_add);
			ast_node(aroot).set_child(expr1); ast_node(expr1).release();

			ast_node_id_t expr2 = ast_node_none;
			if ((r=nextexpr(expr2)) < 1)
				return r;

			ast_node(expr1).set_next(expr2); ast_node(expr2).release();
		}
		else if (tq_peek().type == token_type_t::minus) {
			tq_discard();

			ast_node_id_t expr1 = aroot; aroot = ast_node_none;

			aroot = ast_node.alloc(token_type_t::op_subtract);
			ast_node(aroot).set_child(expr1); ast_node(expr1).release();

			ast_node_id_t expr2 = ast_node_none;
			if ((r=nextexpr(expr2)) < 1)
				return r;

			ast_node(expr1).set_next(expr2); ast_node(expr2).release();
		}
		else {
			break;
		}
	} while (1);
#undef nextexpr
	return 1;
}

int cc_state_t::shift_expression(ast_node_id_t &aroot) {
#define nextexpr additive_expression
	int r;

	if ((r=nextexpr(aroot)) < 1)
		return r;

	do {
		if (tq_peek().type == token_type_t::lessthanlessthan) {
			tq_discard();

			ast_node_id_t expr1 = aroot; aroot = ast_node_none;

			aroot = ast_node.alloc(token_type_t::op_leftshift);
			ast_node(aroot).set_child(expr1); ast_node(expr1).release();

			ast_node_id_t expr2 = ast_node_none;
			if ((r=nextexpr(expr2)) < 1)
				return r;

			ast_node(expr1).set_next(expr2); ast_node(expr2).release();
		}
		else if (tq_peek().type == token_type_t::greaterthangreaterthan) {
			tq_discard();

			ast_node_id_t expr1 = aroot; aroot = ast_node_none;

			aroot = ast_node.alloc(token_type_t::op_rightshift);
			ast_node(aroot).set_child(expr1); ast_node(expr1).release();

			ast_node_id_t expr2 = ast_node_none;
			if ((r=nextexpr(expr2)) < 1)
				return r;

			ast_node(expr1).set_next(expr2); ast_node(expr2).release();
		}
		else {
			break;
		}
	} while (1);
#undef nextexpr
	return 1;
}

int cc_state_t::relational_expression(ast_node_id_t &aroot) {
#define nextexpr shift_expression
	int r;

	if ((r=nextexpr(aroot)) < 1)
		return r;

	do {
		if (tq_peek().type == token_type_t::lessthan) {
			tq_discard();

			ast_node_id_t expr1 = aroot; aroot = ast_node_none;

			aroot = ast_node.alloc(token_type_t::op_lessthan);
			ast_node(aroot).set_child(expr1); ast_node(expr1).release();

			ast_node_id_t expr2 = ast_node_none;
			if ((r=nextexpr(expr2)) < 1)
				return r;

			ast_node(expr1).set_next(expr2); ast_node(expr2).release();
		}
		else if (tq_peek().type == token_type_t::greaterthan) {
			tq_discard();

			ast_node_id_t expr1 = aroot; aroot = ast_node_none;

			aroot = ast_node.alloc(token_type_t::op_greaterthan);
			ast_node(aroot).set_child(expr1); ast_node(expr1).release();

			ast_node_id_t expr2 = ast_node_none;
			if ((r=nextexpr(expr2)) < 1)
				return r;

			ast_node(expr1).set_next(expr2); ast_node(expr2).release();
		}
		else if (tq_peek().type == token_type_t::lessthanequals) {
			tq_discard();

			ast_node_id_t expr1 = aroot; aroot = ast_node_none;

			aroot = ast_node.alloc(token_type_t::op_lessthan_equals);
			ast_node(aroot).set_child(expr1); ast_node(expr1).release();

			ast_node_id_t expr2 = ast_node_none;
			if ((r=nextexpr(expr2)) < 1)
				return r;

			ast_node(expr1).set_next(expr2); ast_node(expr2).release();
		}
		else if (tq_peek().type == token_type_t::greaterthanequals) {
			tq_discard();

			ast_node_id_t expr1 = aroot; aroot = ast_node_none;

			aroot = ast_node.alloc(token_type_t::op_greaterthan_equals);
			ast_node(aroot).set_child(expr1); ast_node(expr1).release();

			ast_node_id_t expr2 = ast_node_none;
			if ((r=nextexpr(expr2)) < 1)
				return r;

			ast_node(expr1).set_next(expr2); ast_node(expr2).release();
		}
		else {
			break;
		}
	} while (1);
#undef nextexpr
	return 1;
}

int cc_state_t::equality_expression(ast_node_id_t &aroot) {
#define nextexpr relational_expression
	int r;

	if ((r=nextexpr(aroot)) < 1)
		return r;

	do {
		if (tq_peek().type == token_type_t::equalequal) {
			tq_discard();

			ast_node_id_t expr1 = aroot; aroot = ast_node_none;

			aroot = ast_node.alloc(token_type_t::op_equals);
			ast_node(aroot).set_child(expr1); ast_node(expr1).release();

			ast_node_id_t expr2 = ast_node_none;
			if ((r=nextexpr(expr2)) < 1)
				return r;

			ast_node(expr1).set_next(expr2); ast_node(expr2).release();
		}
		else if (tq_peek().type == token_type_t::exclamationequals) {
			tq_discard();

			ast_node_id_t expr1 = aroot; aroot = ast_node_none;

			aroot = ast_node.alloc(token_type_t::op_not_equals);
			ast_node(aroot).set_child(expr1); ast_node(expr1).release();

			ast_node_id_t expr2 = ast_node_none;
			if ((r=nextexpr(expr2)) < 1)
				return r;

			ast_node(expr1).set_next(expr2); ast_node(expr2).release();
		}
		else {
			break;
		}
	} while (1);
#undef nextexpr
	return 1;
}

int cc_state_t::and_expression(ast_node_id_t &aroot) {
#define nextexpr equality_expression
	int r;

	if ((r=nextexpr(aroot)) < 1)
		return r;

	while (tq_peek().type == token_type_t::ampersand) {
		tq_discard();

		ast_node_id_t expr1 = aroot; aroot = ast_node_none;

		aroot = ast_node.alloc(token_type_t::op_binary_and);
		ast_node(aroot).set_child(expr1); ast_node(expr1).release();

		ast_node_id_t expr2 = ast_node_none;
		if ((r=nextexpr(expr2)) < 1)
			return r;

		ast_node(expr1).set_next(expr2); ast_node(expr2).release();
	}
#undef nextexpr
	return 1;
}

int cc_state_t::exclusive_or_expression(ast_node_id_t &aroot) {
#define nextexpr and_expression
	int r;

	if ((r=nextexpr(aroot)) < 1)
		return r;

	while (tq_peek().type == token_type_t::caret) {
		tq_discard();

		ast_node_id_t expr1 = aroot; aroot = ast_node_none;

		aroot = ast_node.alloc(token_type_t::op_binary_xor);
		ast_node(aroot).set_child(expr1); ast_node(expr1).release();

		ast_node_id_t expr2 = ast_node_none;
		if ((r=nextexpr(expr2)) < 1)
			return r;

		ast_node(expr1).set_next(expr2); ast_node(expr2).release();
	}
#undef nextexpr
	return 1;
}

int cc_state_t::inclusive_or_expression(ast_node_id_t &aroot) {
#define nextexpr exclusive_or_expression
	int r;

	if ((r=nextexpr(aroot)) < 1)
		return r;

	while (tq_peek().type == token_type_t::pipe) {
		tq_discard();

		ast_node_id_t expr1 = aroot; aroot = ast_node_none;

		aroot = ast_node.alloc(token_type_t::op_binary_or);
		ast_node(aroot).set_child(expr1); ast_node(expr1).release();

		ast_node_id_t expr2 = ast_node_none;
		if ((r=nextexpr(expr2)) < 1)
			return r;

		ast_node(expr1).set_next(expr2); ast_node(expr2).release();
	}
#undef nextexpr
	return 1;
}

int cc_state_t::logical_and_expression(ast_node_id_t &aroot) {
#define nextexpr inclusive_or_expression
	int r;

	if ((r=nextexpr(aroot)) < 1)
		return r;

	while (tq_peek().type == token_type_t::ampersandampersand) {
		tq_discard();

		ast_node_id_t expr1 = aroot; aroot = ast_node_none;

		aroot = ast_node.alloc(token_type_t::op_logical_and);
		ast_node(aroot).set_child(expr1); ast_node(expr1).release();

		ast_node_id_t expr2 = ast_node_none;
		if ((r=nextexpr(expr2)) < 1)
			return r;

		ast_node(expr1).set_next(expr2); ast_node(expr2).release();
	}
#undef nextexpr
	return 1;
}

int cc_state_t::logical_or_expression(ast_node_id_t &aroot) {
#define nextexpr logical_and_expression
	int r;

	if ((r=nextexpr(aroot)) < 1)
		return r;

	while (tq_peek().type == token_type_t::pipepipe) {
		tq_discard();

		ast_node_id_t expr1 = aroot; aroot = ast_node_none;

		aroot = ast_node.alloc(token_type_t::op_logical_or);
		ast_node(aroot).set_child(expr1); ast_node(expr1).release();

		ast_node_id_t expr2 = ast_node_none;
		if ((r=nextexpr(expr2)) < 1)
			return r;

		ast_node(expr1).set_next(expr2); ast_node(expr2).release();
	}
#undef nextexpr
	return 1;
}

int cc_state_t::conditional_expression(ast_node_id_t &aroot) {
#define nextexpr logical_or_expression
	int r;

	if ((r=nextexpr(aroot)) < 1)
		return r;

	if (tq_peek().type == token_type_t::question) {
		ast_node_id_t cond_expr = aroot; aroot = ast_node_none;
		tq_discard();

		ast_node_id_t true_expr = ast_node_none;
		if ((r=expression(true_expr)) < 1)
			return r;

		if (tq_get().type != token_type_t::colon)
			return errno_return(EINVAL);

		ast_node_id_t false_expr = ast_node_none;
		if ((r=conditional_expression(false_expr)) < 1)
			return r;

		aroot = ast_node.alloc(token_type_t::op_ternary);
		ast_node(aroot).set_child(cond_expr); ast_node(cond_expr).release();
		ast_node(cond_expr).set_next(true_expr); ast_node(true_expr).release();
		ast_node(true_expr).set_next(false_expr); ast_node(false_expr).release();
	}
#undef nextexpr
	return 1;
}

int cc_state_t::assignment_expression(ast_node_id_t &aroot) {
#define nextexpr conditional_expression
	ast_node_id_t to = ast_node_none,from = ast_node_none;
	int r;

	if ((r=nextexpr(aroot)) < 1)
		return r;

	switch (tq_peek().type) {
		case token_type_t::equal:
			tq_discard();
			if ((r=assignment_expression(from)) < 1)
				return r;
			to = aroot; aroot = ast_node.alloc(token_type_t::op_assign);
			ast_node(aroot).set_child(to); ast_node(to).release();
			ast_node(to).set_next(from); ast_node(from).release();
			break;
		case token_type_t::starequals:
			tq_discard();
			if ((r=assignment_expression(from)) < 1)
				return r;
			to = aroot; aroot = ast_node.alloc(token_type_t::op_multiply_assign);
			ast_node(aroot).set_child(to); ast_node(to).release();
			ast_node(to).set_next(from); ast_node(from).release();
			break;
		case token_type_t::forwardslashequals:
			tq_discard();
			if ((r=assignment_expression(from)) < 1)
				return r;
			to = aroot; aroot = ast_node.alloc(token_type_t::op_divide_assign);
			ast_node(aroot).set_child(to); ast_node(to).release();
			ast_node(to).set_next(from); ast_node(from).release();
			break;
		case token_type_t::percentequals:
			tq_discard();
			if ((r=assignment_expression(from)) < 1)
				return r;
			to = aroot; aroot = ast_node.alloc(token_type_t::op_modulus_assign);
			ast_node(aroot).set_child(to); ast_node(to).release();
			ast_node(to).set_next(from); ast_node(from).release();
			break;
		case token_type_t::plusequals:
			tq_discard();
			if ((r=assignment_expression(from)) < 1)
				return r;
			to = aroot; aroot = ast_node.alloc(token_type_t::op_add_assign);
			ast_node(aroot).set_child(to); ast_node(to).release();
			ast_node(to).set_next(from); ast_node(from).release();
			break;
		case token_type_t::minusequals:
			tq_discard();
			if ((r=assignment_expression(from)) < 1)
				return r;
			to = aroot; aroot = ast_node.alloc(token_type_t::op_subtract_assign);
			ast_node(aroot).set_child(to); ast_node(to).release();
			ast_node(to).set_next(from); ast_node(from).release();
			break;
		case token_type_t::lessthanlessthanequals:
			tq_discard();
			if ((r=assignment_expression(from)) < 1)
				return r;
			to = aroot; aroot = ast_node.alloc(token_type_t::op_leftshift_assign);
			ast_node(aroot).set_child(to); ast_node(to).release();
			ast_node(to).set_next(from); ast_node(from).release();
			break;
		case token_type_t::greaterthangreaterthanequals:
			tq_discard();
			if ((r=assignment_expression(from)) < 1)
				return r;
			to = aroot; aroot = ast_node.alloc(token_type_t::op_rightshift_assign);
			ast_node(aroot).set_child(to); ast_node(to).release();
			ast_node(to).set_next(from); ast_node(from).release();
			break;
		case token_type_t::ampersandequals:
			tq_discard();
			if ((r=assignment_expression(from)) < 1)
				return r;
			to = aroot; aroot = ast_node.alloc(token_type_t::op_and_assign);
			ast_node(aroot).set_child(to); ast_node(to).release();
			ast_node(to).set_next(from); ast_node(from).release();
			break;
		case token_type_t::caretequals:
			tq_discard();
			if ((r=assignment_expression(from)) < 1)
				return r;
			to = aroot; aroot = ast_node.alloc(token_type_t::op_xor_assign);
			ast_node(aroot).set_child(to); ast_node(to).release();
			ast_node(to).set_next(from); ast_node(from).release();
			break;
		case token_type_t::pipeequals:
			tq_discard();
			if ((r=assignment_expression(from)) < 1)
				return r;
			to = aroot; aroot = ast_node.alloc(token_type_t::op_or_assign);
			ast_node(aroot).set_child(to); ast_node(to).release();
			ast_node(to).set_next(from); ast_node(from).release();
			break;
		default:
			break;
	};

#undef nextexpr
	return 1;
}

int cc_state_t::expression(ast_node_id_t &aroot) {
#define nextexpr assignment_expression
	int r;

	if ((r=nextexpr(aroot)) < 1)
		return r;

	while (tq_peek().type == token_type_t::comma) {
		tq_discard();

		ast_node_id_t expr1 = aroot; aroot = ast_node_none;

		aroot = ast_node.alloc(token_type_t::op_comma);
		ast_node(aroot).set_child(expr1); ast_node(expr1).release();

		ast_node_id_t expr2 = ast_node_none;
		if ((r=nextexpr(expr2)) < 1)
			return r;

		ast_node(expr1).set_next(expr2); ast_node(expr2).release();
	}
#undef nextexpr
	return 1;
}

int cc_state_t::initializer(ast_node_id_t &aroot) {
	int r;

	assert(aroot == ast_node_none);

	/* the equals sign has already been consumed */

	if (tq_peek().type == token_type_t::opencurlybracket) {
		tq_discard();

		ast_node_id_t nex = ast_node_none;
		aroot = ast_node.alloc(token_type_t::op_array_define);

		do {
			if (tq_peek().type == token_type_t::closecurlybracket)
				break;

			ast_node_id_t expr = ast_node_none,s_expr = ast_node_none;
			bool c99_dinit = false;

			if (tq_peek(0).type == token_type_t::identifier && tq_peek(1).type == token_type_t::colon) {
				/* field: expression
				 *
				 * means the same as .field = expression but is an old GCC syntax */
				ast_node_id_t asq = ast_node.alloc(tq_get()); /* and then the ident */
				ast_node_id_t op = ast_node.alloc(token_type_t::op_dinit_field);
				ast_node(op).set_child(asq); ast_node(asq).release();
				tq_discard();
				s_expr = asq;
				expr = op;

				ast_node_id_t i_expr = ast_node_none;
				if ((r=initializer(i_expr)) < 1)
					return r;

				assert(expr != ast_node_none);
				assert(s_expr != ast_node_none);
				ast_node(s_expr).set_next(i_expr); ast_node(i_expr).release();
			}
			else {
				do {
					if (tq_peek().type == token_type_t::opensquarebracket) {
						tq_discard();

						ast_node_id_t asq = ast_node_none;
						if ((r=conditional_expression(asq)) < 1)
							return r;

						ast_node_reduce(asq);

						if (tq_peek().type == token_type_t::ellipsis) {
							/* GNU GCC extension: first ... last ranges */
							/* valid in case statements:
							 *   case 1 ... 3:
							 * valid in designated initializers:
							 *   int[] = { [1 ... 3] = 5 } */
							tq_discard();

							ast_node_id_t op1 = asq;
							ast_node_id_t op2 = ast_node_none;
							asq = ast_node.alloc(token_type_t::op_gcc_range);

							if ((r=conditional_expression(op2)) < 1)
								return r;

							ast_node_reduce(op2);

							ast_node(asq).set_child(op1); ast_node(op1).release();
							ast_node(op1).set_next(op2); ast_node(op2).release();
						}

						if (tq_get().type != token_type_t::closesquarebracket)
							return errno_return(EINVAL);

						ast_node_id_t op = ast_node.alloc(token_type_t::op_dinit_array);
						ast_node(op).set_child(asq); ast_node(asq).release();

						if (expr == ast_node_none) {
							expr = op;
						}
						else {
							ast_node(s_expr).set_next(op); ast_node(op).release();
						}
						s_expr = asq;

						c99_dinit = true;
					}
					else if (tq_peek(0).type == token_type_t::period && tq_peek(1).type == token_type_t::identifier) {
						tq_discard(); /* eat the '.' */

						ast_node_id_t asq = ast_node.alloc(tq_get()); /* and then the ident */
						ast_node_id_t op = ast_node.alloc(token_type_t::op_dinit_field);
						ast_node(op).set_child(asq); ast_node(asq).release();

						if (expr == ast_node_none) {
							expr = op;
						}
						else {
							ast_node(s_expr).set_next(op); ast_node(op).release();
						}
						s_expr = asq;

						c99_dinit = true;
					}
					else {
						break;
					}
				} while (1);

				if (c99_dinit) {
					if (tq_get().type != token_type_t::equal)
						CCERR_RET(EINVAL,tq_peek().pos,"Expected equal sign");

					ast_node_id_t i_expr = ast_node_none;
					if ((r=initializer(i_expr)) < 1)
						return r;

					assert(expr != ast_node_none);
					assert(s_expr != ast_node_none);
					ast_node(s_expr).set_next(i_expr); ast_node(i_expr).release();
				}
				else {
					if ((r=initializer(expr)) < 1)
						return r;
				}
			}

			if (nex == ast_node_none)
				ast_node(aroot).set_child(expr);
			else
				ast_node(nex).set_next(expr);

			ast_node(expr).release();
			nex = expr;

			if (tq_peek().type == token_type_t::comma)
				tq_discard();
			else if (tq_peek().type == token_type_t::closecurlybracket)
				break;
			else
				return errno_return(EINVAL);
		} while (1);

		if (tq_get().type != token_type_t::closecurlybracket)
			return errno_return(EINVAL);
	}
	else {
		if ((r=assignment_expression(aroot)) < 1)
			return r;
	}

#if 0//DEBUG
	fprintf(stderr,"init AST:\n");
	debug_dump_ast("  ",aroot);
#endif

	ast_node_reduce(aroot);

	return 1;
}

int cc_state_t::compound_statement_declarators(void) {
	scope_id_t scope_id = current_scope();
	scope_t &sco = scope(scope_id);
	int r;

	/* caller already ate the { */

	/* declarator (variables) phase.
	 * Use C rules where they are only allowed at the top of the compound statement.
	 * Not C++ rules where you can just do it wherever. */
	do {
		if (tq_peek().type == token_type_t::eof || tq_peek().type == token_type_t::none)
			CCERR_RET(EINVAL,tq_peek().pos,"Missing closing curly brace } in compound statement");

		if (tq_peek().type == token_type_t::semicolon)
			break;

		if (tq_peek().type == token_type_t::closecurlybracket)
			return 1; /* do not discard, let the calling function take care of it */

		if (!declaration_specifiers_check())
			break;

		declaration_specifiers_t spec;

		if ((r=cc_chkerr()) < 1)
			return r;
		if ((r=declaration_specifiers_parse(spec,DECLSPEC_ALLOW_DEF)) < 1)
			return r;

		do {
			declarator_t declor;
			symbol_lookup_t sl;

			sl.pos = tq_peek().pos;
			if ((r=declaration_inner_parse(spec,declor)) < 1)
				return r;

			if (declor.name != identifier_none) {
				if ((r=prep_symbol_lookup(sl,spec,declor)) < 1)
					return r;
				if (do_local_symbol_lookup(sl,spec,declor)) {
					if ((r=check_symbol_lookup_match(sl,spec,declor)) < 1)
						return r;
					if ((r=check_symbol_param_match(sl,declor.ddip)) < 1)
						return r;
				}
				else {
					if ((r=add_symbol(sl,spec,declor)) < 1)
						return r;

					scope_t::decl_t &sldef = sco.new_localdecl();
					sldef.spec = spec;
					sldef.declor = std::move(declor); /* this is the last time this function will use it, std::move it */
					sldef.declor.symbol = sl.sid;
				}
			}

			if (tq_peek().type == token_type_t::comma) {
				tq_discard();
				continue;
			}

			break;
		} while(1);

		if (tq_peek().type == token_type_t::semicolon) {
			tq_discard();
			continue;
		}

		CCERR_RET(EINVAL,tq_peek().pos,"Missing semicolon");
	} while (1);

	return 1;
}

int cc_state_t::expression_statement(ast_node_id_t &aroot) {
	int r;

	if ((r=statement(aroot)) < 1)
		return r;

	if (aroot == ast_node_none)
		aroot = ast_node.alloc(token_type_t::op_none);

	return 1;
}

int cc_state_t::do_pragma_comma_or_closeparens(void) {
	if (tq_peek().type == token_type_t::comma)
		tq_discard();
	else if (tq_peek().type == token_type_t::closeparenthesis)
		return 1;
	else
		CCERR_RET(EINVAL,tq_peek().pos,"Expected comma");

	return 1;
}

int cc_state_t::do_pragma(void) {
	int r;

	if (tq_peek().type == token_type_t::identifier) {
		identifier_id_t id = identifier_none;
		identifier.assign(id,tq_peek().v.identifier);
		tq_discard();

		if (identifier(id) == "pack") {
			enum todo_t {
				DO_NONE=0,
				DO_PUSH,
				DO_POP
			};
			std::string idname;
			todo_t todo = DO_NONE;
			addrmask_t pack = addrmask_none;

			identifier.assign(id,identifier_none);

			if (tq_peek().type != token_type_t::openparenthesis)
				CCERR_RET(EINVAL,tq_peek().pos,"Open parens expected");
			tq_discard();

			if (tq_peek().type == token_type_t::identifier) {
				identifier_id_t id = identifier_none;
				identifier.assign(id,tq_peek().v.identifier);

				if (identifier(id) == "push") {
					todo = DO_PUSH;
					tq_discard();
					if ((r=do_pragma_comma_or_closeparens()) < 1)
						return r;
				}
				else if (identifier(id) == "pop") {
					todo = DO_POP;
					tq_discard();
					if ((r=do_pragma_comma_or_closeparens()) < 1)
						return r;
				}

				identifier.assign(id,identifier_none);
			}

			if (tq_peek().type == token_type_t::identifier) {
				idname = identifier(tq_peek().v.identifier).to_str();
				tq_discard();
				if ((r=do_pragma_comma_or_closeparens()) < 1)
					return r;
			}

			if (tq_peek().type == token_type_t::integer) {
				if (tq_peek().v.integer.v.v < 1)
					CCERR_RET(EINVAL,tq_peek().pos,"Invalid packing value");

				const unsigned long long n = tq_peek().v.integer.v.u;
				tq_discard();

				if (n & (n - 1u))
					CCERR_RET(EINVAL,tq_peek().pos,"Packing value not a power of 2");

				pack = addrmask_make(n);
				if ((r=do_pragma_comma_or_closeparens()) < 1)
					return r;
			}

#if 0//DEBUG
			fprintf(stderr,"PRAGMA PACK: todo=%u ident='%s' packalign=%lu\n",
					todo,idname.c_str(),
					(pack != addrmask_none) ? ((~pack) + 1ul) : 0ul);
#endif

			if (tq_peek().type != token_type_t::closeparenthesis)
				CCERR_RET(EINVAL,tq_peek().pos,"Close parens expected");
			tq_discard();

			if (todo == DO_PUSH) {
				pack_state_t p;
				p.identifier = idname;
				p.align_limit = current_packing;
				packing_stack.push_back(std::move(p));
			}
			else if (todo == DO_POP) {
				if (packing_stack.empty())
					return 1; /* ignored */

				if (!idname.empty()) {
					size_t ent = packing_stack.size() - 1u;
					do {
						if (packing_stack[ent].identifier == idname) {
							packing_stack.resize(ent+1u); /* pop everything else off above it */
							break;
						}

						if (ent == 0)
							return 1; /* ignored */

						ent--;
					} while (1);
				}

				assert(!packing_stack.empty());
				auto &r = packing_stack.back();
				current_packing = r.align_limit;
				packing_stack.pop_back();
			}
			else {
				/* Our extension: Microsoft documentation says "pack( [n] )" which suggests that pack() might be valid.
				 * We'll accept that here to mean default packing */
				if (pack == addrmask_none)
					current_packing = addrmask_none;
			}

			if (pack != addrmask_none)
				current_packing = pack;

#if 0//DEBUG
			fprintf(stderr,"Current packing: %lu\n",
					(current_packing != addrmask_none) ? ((~current_packing) + 1ul) : 0ul);
#endif
		}
		else {
			identifier.assign(id,identifier_none);
		}
	}

	return 1;
}

int cc_state_t::check_for_pragma(void) {
	int r;

	do {
		if (tq_peek().type == token_type_t::op_pragma) {
			/* whatever is not parsed, read and discard.
			 * assume parsing code has counted parenthesis properly */
			int parens = 1;

			tq_discard();
			if (tq_get().type != token_type_t::openparenthesis)
				CCERR_RET(EBADF,tq_peek().pos,"op pragma without open parens");

			if ((r=do_pragma()) < 1)
				return r;

			do {
				const auto &t = tq_peek();

				if (t.type == token_type_t::eof || t.type == token_type_t::none) {
					CCERR_RET(EBADF,tq_peek().pos,"op pragma unexpected end");
				}
				else if (t.type == token_type_t::openparenthesis) {
					tq_discard();
					parens++;
				}
				else if (t.type == token_type_t::closeparenthesis) {
					tq_discard();
					parens--;
					if (parens == 0) break;
				}
				else {
					tq_discard();
				}
			} while(1);
		}
		else {
			break;
		}
	} while (1);

	return 1;
}

int cc_state_t::asm_statement_gcc_colon_section(std::vector<token_t> &tokens,int &parens) {
	if (tq_get().type != token_type_t::colon)
		CCERR_RET(EINVAL,tq_peek().pos,"colon expected");

	do {
		const token_t &t = tq_peek();

		if (t.type == token_type_t::eof || t.type == token_type_t::none)
			CCERR_RET(EINVAL,t.pos,"Unexpected end");

		if (t.type == token_type_t::openparenthesis) {
			parens++;
			tokens.push_back(std::move(tq_get()));
		}
		else if (t.type == token_type_t::closeparenthesis) {
			assert(parens > 0);
			if (--parens == 0) {
				tq_discard();
				break;
			}

			tokens.push_back(std::move(tq_get()));
		}
		else if (t.type == token_type_t::colon && parens == 1) {
			break;
		}
		else {
			tokens.push_back(std::move(tq_get()));
		}
	} while(1);

	assert(parens <= 1);
	return 1;
}

int cc_state_t::asm_statement(ast_node_id_t &aroot) {
	int r;

	/* asm("")
	 * asm inline ("")
	 * asm __inline__ ("")
	 * __asm__ __volatile__ ("")
	 *
	 * GNU style:
	 * __asm__ __volatile__ ("" : output : input : clobber) */
	std::vector<token_t> asm_tokens; /* pass THIS to the assembler handler, which will be string constants or else (TODO: Perhaps we should just construct one big csliteral from it?) */
	std::vector<token_t> input_tokens;
	std::vector<token_t> output_tokens;
	std::vector<token_t> clobber_tokens;
	std::vector<token_t> gotolabel_tokens;

	unsigned int flags = 0;
	static constexpr unsigned int FL_INLINE = 1u << 0u;
	static constexpr unsigned int FL_VOLATILE = 1u << 1u;
	static constexpr unsigned int FL_GOTO = 1u << 2u;

	int parens;

	(void)aroot;

	tq_discard(); /* discard asm */

	do {
		const token_t &t = tq_peek();

		if (t.type == token_type_t::r_inline) {
			flags |= FL_INLINE;
			tq_discard();
		}
		else if (t.type == token_type_t::r_volatile) {
			flags |= FL_VOLATILE;
			tq_discard();
		}
		else if (t.type == token_type_t::r_goto) {
			flags |= FL_GOTO;
			tq_discard();
		}
		else {
			break;
		}
	} while(1);

	if (tq_get().type != token_type_t::openparenthesis)
		CCERR_RET(EINVAL,tq_peek().pos,"Expected open parenthesis");

	parens = 1;
	do {
		const token_t &t = tq_peek();

		if (t.type == token_type_t::eof || t.type == token_type_t::none)
			CCERR_RET(EINVAL,t.pos,"Unexpected end");

		if (t.type == token_type_t::openparenthesis) {
			parens++;
			asm_tokens.push_back(std::move(tq_get()));
		}
		else if (t.type == token_type_t::closeparenthesis) {
			assert(parens > 0);
			if (--parens == 0) {
				tq_discard();
				break;
			}

			asm_tokens.push_back(std::move(tq_get()));
		}
		else if (t.type == token_type_t::colon && parens == 1) {
			break;
		}
		else if (t.type == token_type_t::strliteral) {
			csliteral_t &cst = csliteral(t.v.csliteral);

			if (cst.length != cst.units())
				CCERR_RET(EINVAL,t.pos,"asm() may not contain wide character strings");

			asm_tokens.push_back(std::move(tq_get()));
		}
		else {
			CCERR_RET(EINVAL,t.pos,"Unexpected token");
		}
	} while(1);

	assert(parens <= 1);

	if (parens == 1) {
		if ((r=asm_statement_gcc_colon_section(output_tokens,parens)) < 1)
			return r;
	}

	if (parens == 1) {
		if ((r=asm_statement_gcc_colon_section(input_tokens,parens)) < 1)
			return r;
	}

	if (parens == 1) {
		if ((r=asm_statement_gcc_colon_section(clobber_tokens,parens)) < 1)
			return r;
	}

	if (parens == 1 && (flags & FL_GOTO)) {
		if ((r=asm_statement_gcc_colon_section(gotolabel_tokens,parens)) < 1)
			return r;
	}

	if (parens == 1) {
		if (tq_get().type != token_type_t::closeparenthesis)
			CCERR_RET(EINVAL,tq_peek().pos,"expected closing parenthesis");

		parens--;
	}

	assert(parens == 0);

#if 0//DEBUG
	fprintf(stderr,"asm inline assembly block:\n");
	if (flags & FL_INLINE)
		fprintf(stderr,"  INLINE\n");
	if (flags & FL_VOLATILE)
		fprintf(stderr,"  VOLATILE\n");
	if (flags & FL_GOTO)
		fprintf(stderr,"  GOTO\n");
	fprintf(stderr,"    asm:\n");
	for (auto &t : asm_tokens)
		fprintf(stderr,"        %s\n",t.to_str().c_str());
	fprintf(stderr,"    output:\n");
	for (auto &t : output_tokens)
		fprintf(stderr,"        %s\n",t.to_str().c_str());
	fprintf(stderr,"    input:\n");
	for (auto &t : input_tokens)
		fprintf(stderr,"        %s\n",t.to_str().c_str());
	fprintf(stderr,"    clobber:\n");
	for (auto &t : clobber_tokens)
		fprintf(stderr,"        %s\n",t.to_str().c_str());
	fprintf(stderr,"    gotolabel:\n");
	for (auto &t : gotolabel_tokens)
		fprintf(stderr,"        %s\n",t.to_str().c_str());
	fprintf(stderr,"END ASM\n");
#endif

	return 1;
}

int cc_state_t::msasm_statement(ast_node_id_t &aroot) {
	/* Any tokens from here to the next __asm or op_end_asm is asm_text() and other tokens
	 * that are to be passed to an inline assembly language library and ignored by this
	 * compiler. For 386 and 8086 targets the library would emulate Microsoft C/C++ MASM-style
	 * or Open Watcom C/C++ inline assembly processing for example. */
	std::vector<token_t> asm_tokens; /* pass THIS to the assembler handler */

	(void)aroot;

	tq_discard();
	do {
		const token_t &t = tq_peek();

		if (t.type == token_type_t::eof || t.type == token_type_t::none || t.type == token_type_t::r___asm)
			break;

		if (t.type == token_type_t::op_end_asm) {
			tq_discard();
			break;
		}

		asm_tokens.push_back(std::move(tq_get()));
	} while(1);

#if 0//DEBUG
	fprintf(stderr,"__asm inline assembly block:\n");
	for (auto &t : asm_tokens)
		fprintf(stderr,"    %s\n",t.to_str().c_str());
	fprintf(stderr,"END ASM\n");
#endif

	return 1;
}

int cc_state_t::statement(ast_node_id_t &aroot) {
	int r;

	if (tq_peek().type == token_type_t::eof || tq_peek().type == token_type_t::none)
		return errno_return(EINVAL);

	if ((r=check_for_pragma()) < 1)
		return r;

	if (tq_peek().type == token_type_t::r___asm) {
		if ((r=msasm_statement(aroot)) < 1)
			return r;
	}
	else if (tq_peek().type == token_type_t::r_asm) {
		if ((r=asm_statement(aroot)) < 1)
			return r;
	}
	else if (tq_peek().type == token_type_t::semicolon) {
		tq_discard();
	}
	else if (tq_peek().type == token_type_t::opencurlybracket) {
		tq_discard();

		push_new_scope();

		/* a compound statement within a compound statement */
		ast_node_id_t cur = ast_node_none;
		ast_node_id_t curnxt = ast_node_none;
		if ((r=compound_statement(cur,curnxt)) < 1) {
			pop_scope();
			return r;
		}

		aroot = ast_node.alloc(token_type_t::op_compound_statement);
		if (cur != ast_node_none) { ast_node(aroot).set_child(cur); ast_node(cur).release(); }
		ast_node(aroot).t.v.scope = current_scope();

		pop_scope();
	}
	else if (tq_peek(0).type == token_type_t::r_switch && tq_peek(1).type == token_type_t::openparenthesis) {
		tq_discard(2);

		ast_node_id_t expr = ast_node_none;
		if ((r=expression(expr)) < 1)
			return r;

		if (tq_get().type != token_type_t::closeparenthesis)
			return errno_return(EINVAL);

		ast_node_id_t stmt = ast_node_none;
		if ((r=statement(stmt)) < 1)
			return r;

		aroot = ast_node.alloc(token_type_t::op_switch_statement);
		ast_node(aroot).set_child(expr); ast_node(expr).release();
		ast_node(expr).set_next(stmt); ast_node(stmt).release();
	}
	else if (tq_peek(0).type == token_type_t::r_if && tq_peek(1).type == token_type_t::openparenthesis) {
		tq_discard(2);

		ast_node_id_t expr = ast_node_none;
		if ((r=expression(expr)) < 1)
			return r;

		if (tq_get().type != token_type_t::closeparenthesis)
			return errno_return(EINVAL);

		ast_node_id_t stmt = ast_node_none;
		if ((r=statement(stmt)) < 1)
			return r;

		aroot = ast_node.alloc(token_type_t::op_if_statement);
		ast_node(aroot).set_child(expr); ast_node(expr).release();
		ast_node(expr).set_next(stmt); ast_node(stmt).release();

		if (tq_peek().type == token_type_t::r_else) {
			tq_discard();

			ast_node_id_t elst = ast_node_none;
			if ((r=statement(elst)) < 1)
				return r;

			ast_node(stmt).set_next(elst); ast_node(elst).release();
		}
	}
	else if (tq_peek(0).type == token_type_t::r_while && tq_peek(1).type == token_type_t::openparenthesis) {
		tq_discard(2);

		ast_node_id_t expr = ast_node_none;
		if ((r=expression(expr)) < 1)
			return r;

		if (tq_get().type != token_type_t::closeparenthesis)
			return errno_return(EINVAL);

		ast_node_id_t stmt = ast_node_none;
		if ((r=statement(stmt)) < 1)
			return r;

		aroot = ast_node.alloc(token_type_t::op_while_statement);
		ast_node(aroot).set_child(expr); ast_node(expr).release();
		ast_node(expr).set_next(stmt); ast_node(stmt).release();
	}
	else if (tq_peek(0).type == token_type_t::r_for && tq_peek(1).type == token_type_t::openparenthesis) {
		tq_discard(2);

		ast_node_id_t initexpr = ast_node_none;
		ast_node_id_t loopexpr = ast_node_none;
		ast_node_id_t iterexpr = ast_node_none;
		ast_node_id_t stmt = ast_node_none;

		if ((r=expression_statement(initexpr)) < 1)
			return r;

		if ((r=expression_statement(loopexpr)) < 1)
			return r;

		if (tq_peek().type != token_type_t::closeparenthesis) {
			if ((r=expression(iterexpr)) < 1)
				return r;
		}
		if (iterexpr == ast_node_none)
			iterexpr = ast_node.alloc(token_type_t::op_none);

		if (tq_get().type != token_type_t::closeparenthesis)
			return errno_return(EINVAL);

		if ((r=statement(stmt)) < 1)
			return r;

		aroot = ast_node.alloc(token_type_t::op_for_statement);
		ast_node(aroot).set_child(initexpr); ast_node(initexpr).release();
		ast_node(initexpr).set_next(loopexpr); ast_node(loopexpr).release();
		ast_node(loopexpr).set_next(iterexpr); ast_node(iterexpr).release();
		if (stmt != ast_node_none) { ast_node(iterexpr).set_next(stmt); ast_node(stmt).release(); }
	}
	else if (tq_peek(0).type == token_type_t::identifier && tq_peek(1).type == token_type_t::colon) {
		ast_node_id_t label = ast_node.alloc(tq_get()); /* eats tq_peek(0); */
		aroot = ast_node.alloc(token_type_t::op_label);
		ast_node(aroot).set_child(label); ast_node(label).release();
		tq_discard(); /* eats the ':' */
	}
	else if (tq_peek(0).type == token_type_t::r_default && tq_peek(1).type == token_type_t::colon) {
		aroot = ast_node.alloc(token_type_t::op_default_label);
		tq_discard(2);
	}
	else if (tq_peek().type == token_type_t::r_case) { /* case constant_expression : */
		tq_discard();
		ast_node_id_t expr = ast_node_none;

		if ((r=conditional_expression(expr)) < 1)
			return r;

		if (tq_peek().type == token_type_t::ellipsis) {
			/* GNU GCC extension: first ... last ranges */
			/* valid in case statements:
			 *   case 1 ... 3:
			 * valid in designated initializers:
			 *   int[] = { [1 ... 3] = 5 } */
			tq_discard();

			ast_node_id_t op1 = expr;
			ast_node_id_t op2 = ast_node_none;
			expr = ast_node.alloc(token_type_t::op_gcc_range);

			if ((r=conditional_expression(op2)) < 1)
				return r;

			ast_node(expr).set_child(op1); ast_node(op1).release();
			ast_node(op1).set_next(op2); ast_node(op2).release();
		}

		if (tq_get().type != token_type_t::colon)
			return errno_return(EINVAL);

		aroot = ast_node.alloc(token_type_t::op_case_statement);
		ast_node(aroot).set_child(expr); ast_node(expr).release();
	}
	else {
		if (tq_peek().type == token_type_t::r_break) {
			aroot = ast_node.alloc(token_type_t::op_break);
			tq_discard();
		}
		else if (tq_peek().type == token_type_t::r_continue) {
			aroot = ast_node.alloc(token_type_t::op_continue);
			tq_discard();
		}
		else if (tq_peek(0).type == token_type_t::r_goto && tq_peek(1).type == token_type_t::identifier) {
			tq_discard();
			ast_node_id_t label = ast_node.alloc(tq_get());
			aroot = ast_node.alloc(token_type_t::op_goto);
			ast_node(aroot).set_child(label); ast_node(label).release();
		}
		else if (tq_peek().type == token_type_t::r_do) {
			tq_discard();

			ast_node_id_t stmt = ast_node_none;
			if ((r=statement(stmt)) < 1)
				return r;

			if (tq_get().type != token_type_t::r_while)
				return errno_return(EINVAL);

			if (tq_get().type != token_type_t::openparenthesis)
				return errno_return(EINVAL);

			ast_node_id_t expr = ast_node_none;
			if ((r=expression(expr)) < 1)
				return r;

			if (tq_get().type != token_type_t::closeparenthesis)
				return errno_return(EINVAL);

			aroot = ast_node.alloc(token_type_t::op_do_while_statement);
			ast_node(aroot).set_child(stmt); ast_node(stmt).release();
			ast_node(stmt).set_next(expr); ast_node(expr).release();
		}
		else if (tq_peek().type == token_type_t::r_return) {
			aroot = ast_node.alloc(token_type_t::op_return);
			tq_discard();

			if (tq_peek().type != token_type_t::semicolon) {
				ast_node_id_t expr = ast_node_none;
				if ((r=expression(expr)) < 1)
					return r;

				ast_node(aroot).set_child(expr); ast_node(expr).release();
			}
		}
		else {
			if ((r=expression(aroot)) < 1)
				return r;
		}

		if (tq_peek().type == token_type_t::semicolon)
			tq_discard();
		else
			return errno_return(EINVAL);
	}

	return 1;
}

int cc_state_t::compound_statement(ast_node_id_t &aroot,ast_node_id_t &nroot) {
	ast_node_id_t nxt;
	int r;

	assert(current_scope() != scope_global);
	assert(current_scope() != scope_none);

	assert(
			(aroot == ast_node_none && nroot == ast_node_none) ||
			(aroot != ast_node_none && nroot == ast_node_none) ||
			(aroot != ast_node_none && nroot != ast_node_none)
	      );

	/* caller already ate the { */

	if ((r=compound_statement_declarators()) < 1)
		return r;

	/* OK, now statements */
	do {
		if (tq_peek().type == token_type_t::eof || tq_peek().type == token_type_t::none)
			return errno_return(EINVAL);

		if (tq_peek().type == token_type_t::closecurlybracket) {
			tq_discard();
			break;
		}

		nxt = ast_node_none;
		if ((r=statement(nxt)) < 1)
			return r;

		/* NTS: statement() can leave the node unset if an empty statement like a lone ';' */
		if (nxt != ast_node_none) {
			if (aroot == ast_node_none)
				aroot = nxt;
			else
			{ ast_node(nroot).set_next(nxt); ast_node(nxt).release(); }

			nroot = nxt;
		}
	} while (1);

	ast_node.assign(/*to*/scope(current_scope()).root,/*from*/aroot);

#if 0//DEBUG
	fprintf(stderr,"compound declarator, scope %u:\n",(unsigned int)current_scope());
	debug_dump_ast("  ",aroot);
#endif

	return 1;
}

int cc_state_t::declaration_inner_parse(declaration_specifiers_t &spec,declarator_t &declor) {
	int r;

	if ((r=declarator_parse(spec,declor)) < 1)
		return r;
	if ((r=cc_chkerr()) < 1)
		return r;

	if (tq_peek().type == token_type_t::equal) {
		tq_discard();

		if ((r=initializer(declor.expr)) < 1)
			return r;
	}

#if 0//DEBUG
	fprintf(stderr,"DEBUG %s:%d:\n",__FUNCTION__,__LINE__);
	debug_dump_declaration_specifiers("  ",spec); // this is the only other param
	debug_dump_declarator("  ",declor);
	for (auto &p : parameters)
		debug_dump_parameter("  ",p);
#endif

	return 1;
}

int cc_state_t::declaration_parse(declaration_t &declion) {
	int r,count = 0;

#if 0//DEBUG
	fprintf(stderr,"%s(line %d) begin parsing\n",__FUNCTION__,__LINE__);
#endif
	if ((r=check_for_pragma()) < 1)
		return r;

	if ((r=declaration_specifiers_parse(declion.spec,DECLSPEC_ALLOW_DEF)) < 1)
		return r;
	if ((r=cc_chkerr()) < 1)
		return r;

	do {
		symbol_lookup_t sl;
		declarator_t &declor = declion.new_declarator();

		sl.pos = tq_peek().pos;
		if ((r=declaration_inner_parse(declion.spec,declor)) < 1)
			return r;

		if (tq_peek().type == token_type_t::opencurlybracket && (declor.flags & declarator_t::FL_FUNCTION)) {
			tq_discard();

			if (declor.flags & declarator_t::FL_FUNCTION_POINTER)
				CCERR_RET(EINVAL,tq_peek().pos,"Function body not allowed for function pointers");
			if (declor.expr != ast_node_none)
				CCERR_RET(EINVAL,tq_peek().pos,"Function body cannot coexist with initializer expression");
			if (count != 0)
				CCERR_RET(EINVAL,tq_peek().pos,"Function body not allowed here");

			sl.flags |= symbol_t::FL_DEFINED;
			if ((r=prep_symbol_lookup(sl,declion.spec,declor)) < 1)
				return r;
			if (do_local_symbol_lookup(sl,declion.spec,declor)) {
				if ((r=check_symbol_lookup_match(sl,declion.spec,declor)) < 1)
					return r;
				if ((r=check_symbol_param_match(sl,declor.ddip)) < 1)
					return r;
			}
			else if ((r=add_symbol(sl,declion.spec,declor)) < 1) {
				return r;
			}

			/* start the new scope, tell compound_statement() we already did it,
			 * so that we can register the function parameters as part of the new scope */
			push_new_scope();

			/* add it to the symbol table */
			{
				ddip_t *fp = declor.ddip.funcparampair();

				if (fp) {
					for (auto &p : fp->parameters) {
						/* if a parameter was given without a name, don't register a symbol */
						if (p.decl.name == identifier_none)
							continue;

						symbol_lookup_t sl;

						sl.flags |= symbol_t::FL_PARAMETER | symbol_t::FL_DEFINED;
						if ((r=prep_symbol_lookup(sl,p.spec,p.decl)) < 1)
							return r;
						if (do_local_symbol_lookup(sl,p.spec,p.decl)) {
							if ((r=check_symbol_lookup_match(sl,p.spec,p.decl)) < 1)
								return r;
							if ((r=check_symbol_param_match(sl,p.decl.ddip)) < 1)
								return r;
						}
						else if ((r=add_symbol(sl,p.spec,p.decl)) < 1) {
							return r;
						}

						p.decl.symbol = sl.sid;
					}
				}
			}

			{
				/* look it up again, compound_statement() could very well have added symbols
				 * causing reallocation and the reference above would become invalid */
				symbol_t &sym = symbol(sl.sid);
				sym.parent_of_scope = current_scope();
				sym.ddip = declor.ddip;
			}

			ast_node_id_t fbroot = ast_node_none,fbrootnext = ast_node_none;
			if ((r=compound_statement(fbroot,fbrootnext)) < 1) {
				pop_scope();
				return r;
			}

			{
				/* once the compound statment ends, no more declarators.
				 * you can't do "int f() { },g() { }" */
				symbol_t &sym = symbol(sl.sid);
				ast_node.assign(/*to*/declor.expr,/*from*/fbroot);
				ast_node.assignmove(/*to*/sym.expr,/*from*/fbroot);
			}

			pop_scope();
			return 1;
		}

		/* add it to the symbol table */
		if (declor.name != identifier_none) {
			if ((r=prep_symbol_lookup(sl,declion.spec,declor)) < 1)
				return r;
			if (do_local_symbol_lookup(sl,declion.spec,declor)) {
				if ((r=check_symbol_lookup_match(sl,declion.spec,declor)) < 1)
					return r;
				if ((r=check_symbol_param_match(sl,declor.ddip)) < 1)
					return r;
			}
			else {
				if ((r=add_symbol(sl,declion.spec,declor)) < 1)
					return r;
			}
		}

		count++;
		if (tq_peek().type == token_type_t::comma) {
			tq_discard();
			continue;
		}

		break;
	} while(1);

#if 0//DEBUG
	fprintf(stderr,"DEBUG %s:%d:\n",__FUNCTION__,__LINE__);
	debug_dump_declaration("  ",declion);
#endif

	if (tq_peek().type == token_type_t::semicolon) {
		tq_discard();
		return 1;
	}

	CCERR_RET(EINVAL,tq_peek().pos,"Missing semicolon");
}

int cc_state_t::struct_field_layout(symbol_id_t sid) {
	addrmask_t align = addrmask_make(1);
	data_offset_t pos = 0,unsz = 0;

	symbol_t &sym = symbol(sid);

	auto fi = sym.fields.begin();
	while (fi != sym.fields.end()) {
		std::string name;

		if ((*fi).name != identifier_none)
			name = identifier((*fi).name).to_str();
		else
			name = "<anon>";

		addrmask_t al = calc_alignofmask((*fi).spec,(*fi).ddip);
		if (al == addrmask_none)
			CCERR_RET(EINVAL,tq_peek().pos,(std::string("cannot determine alignment of field ")+name).c_str());

		data_size_t sz = calc_sizeof((*fi).spec,(*fi).ddip);
		if (sz == data_size_none || sz == 0)
			CCERR_RET(EINVAL,tq_peek().pos,(std::string("cannot determine size of field ")+name).c_str());

		/* store the spec, this code may loop over bitfield entries of the same type.
		 * this ref will not change even when fi is later incremented, by design. */
		auto &m_fi = (*fi);

		/* #pragma pack acts like a limiter to alignment */
		/* NTS: Testing with GCC shows that #pragma pack has no effect on a struct unless
		 *      it is in effect during the END of the struct.
		 *
		 *      Affects packing:
		 *
		 *      struct abc {
		 *           char a;
		 *           short b,
		 *           long c;
		 *      #pragma pack(push,1)
		 *      };
		 *      #pragma pack(pop)
		 *
		 *      Does not affect packing:
		 *
		 *      struct abc {
		 *      #pragma pack(push,1)
		 *           char a;
		 *           short b,
		 *           long c;
		 *      #pragma pack(pop)
		 *      };
		 *
		 *      See the difference?
		 *      The best way to ensure it affects the entire struct no matter the compiler is to just wrap the struct in #pragma pack */
		if (current_packing != addrmask_none)
			al |= current_packing;

		/* in a union, fields overlap each other */
		/* in a struct, fields follow one another. */
		if (sym.sym_type != symbol_t::UNION)
			pos = (pos + (~al)) & al;

		/* alignment of struct member affects alignment of struct */
		align &= al;

		/* bit field counter */
		unsigned int bit_start = 0;
		unsigned int bits_allowed = (unsigned int)sz * 8u;

		do {
			assert(fi != sym.fields.end());
			(*fi).offset = pos;

			if ((*fi).bf_length != bitfield_pos_none) {
				if ((*fi).bf_start != bitfield_pos_none)
					bit_start = (*fi).bf_start;
				else
					(*fi).bf_start = bit_start;

				bit_start += (*fi).bf_length;

				if ((*fi).bf_start >= bits_allowed || ((*fi).bf_start+(*fi).bf_length) > bits_allowed)
					CCERR_RET(ERANGE,tq_peek().pos,
							"bitfield range [%u...%u] does not fit in data type [0...%u]",
							(*fi).bf_start,(*fi).bf_start+(*fi).bf_length-1,bits_allowed-1);
			}

#if 0//DEBUG
			fprintf(stderr,"struct field pos=%lu align=%lx\n",(unsigned long)pos,(unsigned long)al);
#endif

			/* update the field, looping over similar bitfields if any */
			if ((*fi).spec.align == addrmask_none)
				(*fi).spec.align = al;
			else if (current_packing != addrmask_none)
				(*fi).spec.align = ((*fi).spec.align & al) | current_packing;
			else
				(*fi).spec.align &= al;

			if ((*fi).spec.size == data_size_none)
				(*fi).spec.size = sz;

			fi++;
			if (fi == sym.fields.end())
				break;
			if (((*fi).spec.type_specifier&m_fi.spec.type_specifier&(TS_CHAR|TS_SHORT|TS_INT|TS_LONG|TS_LONGLONG)) == 0)
				break;
			if (m_fi.bf_length == bitfield_pos_none || (*fi).bf_length == bitfield_pos_none)
				break;
			if (!(*fi).ddip.empty() || !m_fi.ddip.empty())
				break;
			if ((*fi).spec.type_specifier != m_fi.spec.type_specifier)
				break;
		} while(1);

		/* advance */
		if (sym.sym_type == symbol_t::UNION) {
			if (unsz < sz) unsz = sz;
		}
		else {
			pos += sz;
		}
	}

#if 0//DEBUG
	fprintf(stderr,"struct field final pos=%lu align=%lx\n",(unsigned long)pos,(unsigned long)align);
#endif

	/* update struct */
	if (sym.spec.align == addrmask_none)
		sym.spec.align = align;
	else
		sym.spec.align &= align;

	if (sym.sym_type == symbol_t::UNION) {
		unsz = (unsz + (~align)) & align;
		sym.spec.size = unsz;
	}
	else {
		pos = (pos + (~align)) & align;
		sym.spec.size = pos;
	}

	return 1;
}

int cc_state_t::struct_declaration_parse(const symbol_id_t sid,const token_type_t &tt) {
	declaration_specifiers_t spec;
	int r,count = 0;

	(void)tt;

#if 0//DEBUG
	fprintf(stderr,"%s(line %d) begin parsing\n",__FUNCTION__,__LINE__);
#endif

	if ((r=declaration_specifiers_parse(spec,DECLSPEC_ALLOW_DEF)) < 1)
		return r;
	if ((r=cc_chkerr()) < 1)
		return r;

#if 0//DEBUG
	fprintf(stderr,"DEBUG %s:%d %s:\n",__FUNCTION__,__LINE__,token_type_t_str(tt));
	debug_dump_declaration_specifiers("  ",spec);
#endif

	do {
		declarator_t declor;

		if ((r=struct_declarator_parse(sid,spec,declor)) < 1)
			return r;
		if ((r=cc_chkerr()) < 1)
			return r;

#if 0//DEBUG
		fprintf(stderr,"DEBUG %s:%d field type %s:\n",__FUNCTION__,__LINE__,token_type_t_str(tt));
		debug_dump_declarator("  ",declor);
#endif

		count++;
		if (tq_peek().type == token_type_t::comma) {
			tq_discard();
			continue;
		}

		break;
	} while(1);

	if (tq_peek().type == token_type_t::semicolon) {
		tq_discard();
		return 1;
	}

	return errno_return(EINVAL);
}

int cc_state_t::external_declaration(void) {
	declaration_t declion;
	int r;

#if 0//DEBUG
	fprintf(stderr,"%s(line %d) begin parsing\n",__FUNCTION__,__LINE__);
#endif

	if ((r=check_for_pragma()) < 1)
		return r;

	if (tq_peek().type == token_type_t::semicolon) {
		tq_discard();
		return 1;
	}
	if ((r=cc_chkerr()) < 1)
		return r;
	if ((r=declaration_parse(declion)) < 1)
		return r;

	return 1;
}

int cc_state_t::translation_unit(void) {
	int r;

	if (tq_peek().type == token_type_t::none || tq_peek().type == token_type_t::eof)
		return err; /* 0 or negative */

	if (tq_peek().type == token_type_t::r___asm) {
		ast_node_id_t aroot = ast_node_none;

		if ((r=msasm_statement(aroot)) < 1)
			return r;
	}
	else if (tq_peek().type == token_type_t::r_asm) {
		ast_node_id_t aroot = ast_node_none;

		if ((r=asm_statement(aroot)) < 1)
			return r;
	}

	if ((r=external_declaration()) < 1)
		return r;

	return 1;
}

///////////////////////////////////////////////////////////////////////

int cc_step(cc_state_t &cc,rbuf &_buf,source_file_object &_sfo) {
	int r;

	cc.buf = &_buf;
	cc.sfo = &_sfo;

	if (cc.err == 0) {
		if ((r=cc.translation_unit()) < 1)
			return r;
	}

	if (cc.err < 0)
		return r;

	return 1;
}

///////////////////////////////////////////////////////////////////////

void cleanup() {
	segments.clear();
	symbols.clear();
	scopes.clear();
	scope_stack.clear();
}

enum test_mode_t {
	TEST_NONE=0,
	TEST_SFO,      /* source file object */
	TEST_RBF,      /* rbuf read buffer, manual fill */
	TEST_RBFGC,    /* rbuf read buffer, getc() */
	TEST_RBFGCNU,  /* rbuf read buffer, getcnu() (unicode getc) */
	TEST_LGTOK,    /* lgtok lowest level general tokenizer */
	TEST_PPTOK,    /* pptok preprocessor token processing and macro expansion */
	TEST_LCTOK     /* lctok low level C token processing below compiler */
};

static std::vector<std::string>		main_input_files;
static enum test_mode_t			test_mode = TEST_NONE;

static void help(void) {
	fprintf(stderr,"cimcc [options] [input file [...]]\n");
	fprintf(stderr,"  --test <none|sfo|rbf|rbfgc|rbfgcnu|lgtok|pptok|lctok>         Test mode\n");
}

static int parse_argv(int argc,char **argv) {
	int i = 1; /* argv[0] is command */
	char *a;

	while (i < argc) {
		a = argv[i++];

		if (*a == '-') {
			do { a++; } while (*a == '-');

			if (!strcmp(a,"h") || !strcmp(a,"help")) {
				help();
				return -1;
			}
			else if (!strcmp(a,"test")) {
				a = argv[i++];
				if (a == NULL) return -1;

				if (!strcmp(a,"sfo"))
					test_mode = TEST_SFO;
				else if (!strcmp(a,"rbf"))
					test_mode = TEST_RBF;
				else if (!strcmp(a,"rbfgc"))
					test_mode = TEST_RBFGC;
				else if (!strcmp(a,"rbfgcnu"))
					test_mode = TEST_RBFGCNU;
				else if (!strcmp(a,"lgtok"))
					test_mode = TEST_LGTOK;
				else if (!strcmp(a,"pptok"))
					test_mode = TEST_PPTOK;
				else if (!strcmp(a,"lctok"))
					test_mode = TEST_LCTOK;
				else if (!strcmp(a,"none"))
					test_mode = TEST_NONE;
				else
					return -1;
			}
			else {
				fprintf(stderr,"Unknown option '%s'\n",a);
				return -1;
			}
		}
		else {
			if (main_input_files.size() > 1024) return -1;
			main_input_files.push_back(a);
		}
	}

	return 0;
}

int main(int argc,char **argv) {
	if (parse_argv(argc,argv) < 0)
		return 1;

	if (main_input_files.empty()) {
		fprintf(stderr,"No input files\n");
		return 1;
	}

	for (auto mifi=main_input_files.begin();mifi!=main_input_files.end();mifi++) {
		std::unique_ptr<source_file_object> sfo;

		if (*mifi == "-") {
			struct stat st;

			if (fstat(0/*STDIN*/,&st)) {
				fprintf(stderr,"Cannot stat STDIN\n");
				return 1;
			}
			if (!(S_ISREG(st.st_mode) || S_ISSOCK(st.st_mode) || S_ISFIFO(st.st_mode))) {
				fprintf(stderr,"Cannot use STDIN. Must be a file, socket, or pipe\n");
				return 1;
			}
			sfo.reset(new source_fd(0/*STDIN, takes ownership*/,"<stdin>"));
			assert(sfo->iface == source_file_object::IF_FD);
		}
		else {
			int fd = open((*mifi).c_str(),O_RDONLY|O_BINARY);
			if (fd < 0) {
				fprintf(stderr,"Cannot open '%s', '%s'\n",(*mifi).c_str(),strerror(errno));
				return 1;
			}
			sfo.reset(new source_fd(fd/*takes ownership*/,*mifi));
			assert(sfo->iface == source_file_object::IF_FD);
		}

		if (test_mode == TEST_SFO) {
			char tmp[512];
			ssize_t sz;

			while ((sz=sfo->read(tmp,sizeof(tmp))) > 0) {
				if (write(1/*STDOUT*/,tmp,size_t(sz)) != ssize_t(sz))
					return -1;
			}

			if (sz < 0) {
				fprintf(stderr,"Read error from %s, error %d\n",sfo->getname(),(int)sz);
				return -1;
			}
		}
		else if (test_mode == TEST_RBF) {
			rbuf rb;
			assert(rb.allocate(128));

			do {
				int r = rbuf_sfd_refill(rb,*sfo);
				if (r < 0) {
					fprintf(stderr,"RBUF read err from %s, %d\n",sfo->getname(),(int)r);
					return -1;
				}
				else if (r == 0/*EOF*/) {
					break;
				}

				const size_t av = rb.data_avail();
				if (av == 0) {
					fprintf(stderr,"Unexpected data end?\n");
					break;
				}

				if (write(1/*STDOUT*/,rb.data,av) != ssize_t(av)) return -1;
				rb.data += av;
				assert(rb.sanity_check());
			} while (1);
		}
		else if (test_mode == TEST_RBFGC) {
			rbuf rb;
			assert(rb.allocate());

			do {
				unicode_char_t c = getc(rb,*sfo);
				if (c == unicode_eof) {
					assert(rb.data_avail() == 0);
					break;
				}
				else if (c == unicode_invalid) {
					if (write(1/*STDOUT*/,"<INVALID>",9) != 9) return -1;
				}
				else {
					const std::string s = utf8_to_str(c);
					if (write(1/*STDOUT*/,s.data(),s.size()) != ssize_t(s.size())) return -1;
				}
			} while (1);
		}
		else if (test_mode == TEST_RBFGCNU) {
			rbuf rb;
			assert(rb.allocate());

			do {
				unicode_char_t c = getcnu(rb,*sfo);
				if (c == unicode_eof) {
					assert(rb.data_avail() == 0);
					break;
				}
				else if (c == unicode_invalid) {
					abort(); /* should not happen! */
				}
				else {
					assert(c >= unicode_char_t(0l) && c <= unicode_char_t(0xFFul));
					unsigned char cc = (unsigned char)c;
					if (write(1/*STDOUT*/,&cc,1) != ssize_t(1)) return -1;
				}
			} while (1);
		}
		else if (test_mode == TEST_LGTOK) {
			lgtok_state_t lst;
			token_t tok;
			rbuf rb;
			int r;

			assert(rb.allocate());
			rb.set_source_file(alloc_source_file(sfo->getname()));
			while ((r=lgtok(lst,rb,*sfo,tok)) > 0) {
				printf("Token:");
				if (tok.pos.row > 0) printf(" pos:row=%u,col=%u,ofs=%u",tok.pos.row,tok.pos.col,tok.pos.ofs);
				if (tok.source_file < source_files.size()) printf(" src='%s'",source_files[tok.source_file].path.c_str());
				printf(" %s\n",tok.to_str().c_str());
			}

			if (r < 0) {
				fprintf(stderr,"Read error from %s, error %d\n",sfo->getname(),(int)r);
				return -1;
			}
		}
		else if (test_mode == TEST_PPTOK) {
			lgtok_state_t lst;
			pptok_state_t pst;
			token_t tok;
			rbuf rb;
			int r;

			assert(rb.allocate());
			rb.set_source_file(alloc_source_file(sfo->getname()));
			while ((r=pptok(pst,lst,rb,*sfo,tok)) > 0) {
				printf("Token:");
				if (tok.pos.row > 0) printf(" pos:row=%u,col=%u,ofs=%u",tok.pos.row,tok.pos.col,tok.pos.ofs);
				if (tok.source_file < source_files.size()) printf(" src='%s'",source_files[tok.source_file].path.c_str());
				printf(" %s\n",tok.to_str().c_str());
			}

			if (r < 0) {
				fprintf(stderr,"Read error from %s, error %d\n",sfo->getname(),(int)r);
				return -1;
			}
		}
		else if (test_mode == TEST_LCTOK) {
			lgtok_state_t lst;
			pptok_state_t pst;
			token_t tok;
			rbuf rb;
			int r;

			assert(rb.allocate());
			rb.set_source_file(alloc_source_file(sfo->getname()));
			while ((r=lctok(pst,lst,rb,*sfo,tok)) > 0) {
				printf("Token:");
				if (tok.pos.row > 0) printf(" pos:row=%u,col=%u,ofs=%u",tok.pos.row,tok.pos.col,tok.pos.ofs);
				if (tok.source_file < source_files.size()) printf(" src='%s'",source_files[tok.source_file].path.c_str());
				printf(" %s\n",tok.to_str().c_str());
			}

			if (r < 0) {
				fprintf(stderr,"Read error from %s, error %d\n",sfo->getname(),(int)r);
				return -1;
			}
		}
		else {
			/* normal compilation */
			cc_state_t ccst;
			rbuf rb;
			int r;

			assert(rb.allocate());
			rb.set_source_file(alloc_source_file(sfo->getname()));

			if (!cc_init()) {
				fprintf(stderr,"Failed to init compiler\n");
				return -1;
			}

			while ((r=cc_step(ccst,rb,*sfo)) > 0);

			if (!arrange_symbols())
				fprintf(stderr,"Failed to arrange symbols\n");

			debug_dump_general("");
			debug_dump_segment_table("");
			debug_dump_scope_table("");
			debug_dump_symbol_table("");

			if (r < 0) {
				fprintf(stderr,"Read error from %s, error %d\n",sfo->getname(),(int)r);
				return -1;
			}
		}

		assert(sfo.get() != NULL);
	}

	cleanup();
	source_file_refcount_check();
	identifier.refcount_check();
	csliteral.refcount_check();
	ast_node.refcount_check();

	if (test_mode == TEST_SFO || test_mode == TEST_LGTOK || test_mode == TEST_RBF ||
		test_mode == TEST_RBFGC || test_mode == TEST_RBFGCNU || test_mode == TEST_LGTOK ||
		test_mode == TEST_PPTOK || test_mode == TEST_LGTOK) return 0;

	return 0;
}

