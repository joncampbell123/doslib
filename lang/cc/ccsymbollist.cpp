
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

//////////////////////////////////////////////////////////////////////////////

std::vector<symbol_t>				symbols;

//////////////////////////////////////////////////////////////////////////////

symbol_id_t match_str_symbol(const csliteral_t &csl) {
	for (const auto &sym : symbols) {
		if (sym.sym_type == symbol_t::STR && sym.expr != ast_node_none) {
			ast_node_t &sa = ast_node(sym.expr);
			if (sa.t.type == token_type_t::strliteral && sa.t.v.csliteral != csliteral_none) {
				if (csliteral(sa.t.v.csliteral) == csl)
					return (symbol_id_t)(&sym - &symbols[0]);
			}
		}
	}

	return symbol_none;
}

/* symbols are never removed from the vector, and they are always added in increasing ID order */
symbol_id_t new_symbol(const identifier_id_t name) {
	const symbol_id_t r = symbols.size();
	symbols.resize(r+1u);
	identifier.assign(/*to*/symbols[r].name,/*from*/name);
	return r;
}

symbol_t &symbol(symbol_id_t id) {
#if 1//DEBUG
	if (id < symbols.size())
		return symbols[id];

	throw std::out_of_range("symbol out of range");
#else
	return symbols[id];
#endif
}

bool sym_match_type(const symbol_t::type_t at,const symbol_t::type_t bt) {
	if (at == bt)
		return true;

	const bool atf = (at == symbol_t::FUNCTION || at == symbol_t::VARIABLE || at == symbol_t::TYPEDEF || at == symbol_t::ENUM || at == symbol_t::CONST || at == symbol_t::STRUCT || at == symbol_t::UNION);
	const bool btf = (bt == symbol_t::FUNCTION || bt == symbol_t::VARIABLE || bt == symbol_t::TYPEDEF || bt == symbol_t::ENUM || bt == symbol_t::CONST || bt == symbol_t::STRUCT || bt == symbol_t::UNION);
	if ((atf || btf) && atf == btf)
		return true;

	return false;
}

symbol_id_t lookup_symbol(scope_t &sco,const identifier_id_t name,const symbol_t::type_t st) {
	if (name == identifier_none)
		return symbol_none;

#if 0//DEBUG
	fprintf(stderr,"lookup '%s' scope#%lu:\n",identifier(name).to_str().c_str(),(unsigned long)(&sco-&scopes[0]));
#endif

	for (const auto &sid : sco.symbols) {
		symbol_t &chk_s = symbol(sid);

		if (chk_s.name != identifier_none) {
			if (sym_match_type(chk_s.sym_type,st) && identifier(name) == identifier(chk_s.name))
				return sid;
		}
	}

#if 0//DEBUG
	fprintf(stderr,"lookup '%s' scope#%lu not found\n",identifier(name).to_str().c_str(),(unsigned long)(&sco-&scopes[0]));
#endif

	return symbol_none;
}

/* this automatically uses the scope_stack and current_scope() return value */
symbol_id_t lookup_symbol(const identifier_id_t name,const symbol_t::type_t st) {
	if (name == identifier_none)
		return symbol_none;

#if 0//DEBUG
	fprintf(stderr,"lookup '%s'\n",identifier(name).to_str().c_str());
	fprintf(stderr,"scope:");
	for (auto i=scope_stack.rbegin();i!=scope_stack.rend();i++)
		fprintf(stderr," #%lu",(unsigned long)(*i));
	fprintf(stderr,"\n");
#endif

	symbol_id_t sid;
	for (auto i=scope_stack.rbegin();i!=scope_stack.rend();i++)
		if ((sid=lookup_symbol(scope(*i),name,st)) != symbol_none)
			return sid;

	return symbol_none;
}

symbol_t::type_t classify_symbol(const declaration_specifiers_t &spec,const declarator_t &declor) {
	if ((declor.flags & (declarator_t::FL_FUNCTION|declarator_t::FL_FUNCTION_POINTER)) == declarator_t::FL_FUNCTION)/* function, but not function pointer */
		return symbol_t::FUNCTION;
	else if (spec.storage_class & SC_TYPEDEF)
		return symbol_t::TYPEDEF;
	else
		return symbol_t::VARIABLE;
}

int prep_symbol_lookup(symbol_lookup_t &sl,const declaration_specifiers_t &spec,const declarator_t &declor) {
	if (sl.st == symbol_t::NONE) sl.st = classify_symbol(spec,declor);
	sl.cursco = current_scope();

	if (spec.storage_class & SC_TYPEDEF)
		sl.flags |= symbol_t::FL_DECLARED | symbol_t::FL_DEFINED;
	else if (sl.st == symbol_t::CONST)
		sl.flags |= symbol_t::FL_DECLARED | symbol_t::FL_DEFINED;
	else {
		sl.flags |= symbol_t::FL_DECLARED;
		switch (sl.st) {
			case symbol_t::VARIABLE:
				if (!(spec.storage_class & SC_EXTERN))
					sl.flags |= symbol_t::FL_DEFINED;

				/* if declared as const/constexpr/constinit and defined and the expression has been reduced to an integer, mark as const */
				if (sl.flags & symbol_t::FL_DEFINED) {
					if ((spec.storage_class & (SC_CONSTEXPR|SC_CONSTINIT|SC_CONSTEVAL)) != 0 || (spec.type_qualifier & TQ_CONST)) {
						if (declor.expr != ast_node_none) {
							ast_node_t &an = ast_node(declor.expr);
							if (an.child == ast_node_none && an.next == ast_node_none) {
								if (an.t.type == token_type_t::integer) {
									sl.st = symbol_t::CONST;
								}
							}
						}
					}
				}
				break;
			case symbol_t::FUNCTION:
				sl.cursco = scope_global; /* All functions are global scope in C. We're not a C++ compiler. */
				if (declor.expr != ast_node_none)
					sl.flags |= symbol_t::FL_DEFINED;
				break;
			case symbol_t::ENUM:
				sl.flags |= symbol_t::FL_DEFINED;
				break;
			default:
				break;
		}
	}

	if (declor.flags & declarator_t::FL_FUNCTION_POINTER)
		sl.flags |= symbol_t::FL_FUNCTION_POINTER;
	if (declor.flags & declarator_t::FL_ELLIPSIS)
		sl.flags |= symbol_t::FL_ELLIPSIS;

	return 1;
}

bool do_local_symbol_lookup(symbol_lookup_t &sl,const declaration_specifiers_t &spec,const declarator_t &declor) {
	(void)spec;

	assert(sl.sid == symbol_none);
	assert(sl.st != symbol_t::NONE);

	if ((sl.sid=lookup_symbol(scope(sl.cursco),declor.name,sl.st)) != symbol_none)
		return true;

	return false;
}

int check_symbol_param_match(symbol_lookup_t &sl,const ddip_list_t &p1,const ddip_list_t &p2) {
	if (p1.size() != p2.size())
		CCERR_RET(EINVAL,sl.pos,"Parameter type does not match");

	for (size_t pi=0;pi < p1.size();pi++) {
		auto &e1 = p1[pi];
		auto &e2 = p2[pi];

		if (e1.ptr.size() != e2.ptr.size())
			CCERR_RET(EINVAL,sl.pos,"Parameter type does not match");
		if (e1.arraydef.size() != e2.arraydef.size())
			CCERR_RET(EINVAL,sl.pos,"Parameter type does not match");
		if (e1.parameters.size() != e2.parameters.size())
			CCERR_RET(EINVAL,sl.pos,"Parameter type does not match");

		for (size_t ai=0;ai < e1.arraydef.size();ai++) {
			if ((e1.arraydef[ai] == ast_node_none) != (e2.arraydef[ai] == ast_node_none))
				CCERR_RET(EINVAL,sl.pos,"Parameter type does not match");

			const ast_node_t &a1 = ast_node(e1.arraydef[ai]);
			const ast_node_t &a2 = ast_node(e2.arraydef[ai]);

			if (a1.t != a2.t)
				CCERR_RET(EINVAL,sl.pos,"Parameter type does not match");
		}
	}

	return 1;
}

int check_symbol_param_match(symbol_lookup_t &sl,const ddip_list_t &p) {
	int r;

	assert(sl.sid != symbol_none);

	symbol_t &chk_s = symbol(sl.sid);

	/* exception: if the function was without any parameters, and you specify parameters, then take the parameters */
	if (chk_s.ddip.empty())
		chk_s.ddip = p;
	else if ((r=check_symbol_param_match(sl,chk_s.ddip,p)) < 1)
		return r;

	return 1;
}

int check_symbol_lookup_match(symbol_lookup_t &sl,const declaration_specifiers_t &spec,const declarator_t &declor) {
	assert(sl.sid != symbol_none);
	assert(sl.st != symbol_t::NONE);

	symbol_t &chk_s = symbol(sl.sid);

	(void)declor;

	/* check for re-declaration of the same symbol with the same specification.
	 * perhaps variables once declared extern that are re-declared non-extern (but not static).
	 * function declaration re-declared, or perhaps a declaration turned into a definition with a function body.
	 * that's fine. */
	if (chk_s.sym_type == sl.st) {
		if (sl.st == symbol_t::VARIABLE) {
			const unsigned int fl_chk =
				symbol_t::FL_FUNCTION_POINTER|
				symbol_t::FL_DECLARED|
				symbol_t::FL_ELLIPSIS;

			const storage_class_t sc_chk =
				~(SC_EXTERN);

			if ((chk_s.flags&fl_chk) == (sl.flags&fl_chk) &&
					(chk_s.spec.storage_class&sc_chk) == (spec.storage_class&sc_chk) &&
					chk_s.spec.type_specifier == spec.type_specifier &&
					chk_s.spec.type_qualifier == spec.type_qualifier) {
				/* you can change EXTERN to non-EXTERN, or just declare EXTERN again, that's it */
				if (chk_s.spec.storage_class & SC_EXTERN) {
					if ((spec.storage_class & SC_EXTERN) == 0)
						chk_s.spec = spec;
					if (sl.flags & symbol_t::FL_DEFINED)
						chk_s.flags |= symbol_t::FL_DEFINED;

					return 1;
				}
			}
		}
		else if (sl.st == symbol_t::FUNCTION || sl.st == symbol_t::STRUCT || sl.st == symbol_t::UNION) {
			const unsigned int fl_chk =
				symbol_t::FL_FUNCTION_POINTER|
				symbol_t::FL_DECLARED|
				symbol_t::FL_ELLIPSIS;
			unsigned int sc_chk =
				~0u;

			if (sl.st == symbol_t::STRUCT || sl.st == symbol_t::UNION)
				sc_chk &= ~SC_STATIC;

			if ((chk_s.flags&fl_chk) == (sl.flags&fl_chk) &&
					(chk_s.spec.storage_class&sc_chk) == (spec.storage_class&sc_chk) &&
					chk_s.spec.type_specifier == spec.type_specifier &&
					chk_s.spec.type_qualifier == spec.type_qualifier) {
				if (sl.flags & symbol_t::FL_DEFINED) {
					if (chk_s.flags & symbol_t::FL_DEFINED) /* cannot define it again */
						goto exists;

					chk_s.flags |= symbol_t::FL_DEFINED;
				}

				return 1;
			}
		}
	}

exists:
	CCERR_RET(EEXIST,sl.pos,"Symbol already exists");
}

/* this automatically uses the scope_stack and current_scope() return value */
int add_symbol(symbol_lookup_t &sl,const declaration_specifiers_t &spec,const declarator_t &declor) {
	assert(sl.sid == symbol_none);
	sl.sid = new_symbol(declor.name);
	symbol_t &sym = symbol(sl.sid);
	sym.spec = spec;
	sym.scope = sl.cursco;
	scope(sym.scope).symbols.push_back(sl.sid);
	sym.flags = sl.flags;
	sym.ddip = declor.ddip;
	sym.sym_type = sl.st;
	ast_node.assign(/*to*/sym.expr,/*from*/declor.expr);
	return 1;
}

segment_id_t decide_sym_segment(symbol_t &sym) {
	if (sym.sym_type == symbol_t::FUNCTION) {
		if (sym.expr != ast_node_none)
			return code_segment;
	}
	else if (sym.sym_type == symbol_t::STR) {
		if (sym.spec.type_qualifier & TQ_CONST)
			return conststr_segment;

		return data_segment;
	}
	else if (sym.sym_type == symbol_t::VARIABLE) {
		if (sym.flags & symbol_t::FL_STACK)
			return stack_segment;
		if (sym.flags & symbol_t::FL_PARAMETER)
			return segment_none; // well... that's up to the calling convention. could be CPU registers or stack

		if (sym.expr != ast_node_none) {
			if (sym.spec.type_qualifier & TQ_CONST)
				return const_segment;

			return data_segment;
		}
		else {
			if (sym.spec.type_qualifier & TQ_CONST)
				return segment_none; // um??? const with no expression? what's the point?

			return bss_segment;
		}
	}

	return segment_none;
}

