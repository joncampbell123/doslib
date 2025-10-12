
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

	for (auto &ppp : ddip.parameters)
		debug_dump_parameter(prefix+"  ",ppp);

	if (ddip.dd_flags & declarator_t::FL_ELLIPSIS)
		fprintf(stderr,"%s  parameter ... (ellipsis)\n",prefix.c_str());
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

