
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

token_t::token_t() : pos(1) { common_init(); }
token_t::~token_t() { common_delete(); }
token_t::token_t(const token_t &t) { common_copy(t); }
token_t::token_t(token_t &&t) { common_move(t); }
token_t::token_t(const token_type_t t) : type(t) { common_init(); }
token_t::token_t(const token_type_t t,const position_t &n_pos,const size_t n_source_file) : type(t), pos(n_pos) { set_source_file(n_source_file); common_init(); }
token_t &token_t::operator=(const token_t &t) { common_copy(t); return *this; }
token_t &token_t::operator=(token_t &&t) { common_move(t); return *this; }

void token_t::set_source_file(const size_t i) {
	if (i == source_file) return;

	if (source_file != no_source_file) {
		release_source_file(source_file);
		source_file = no_source_file;
	}

	if (i < source_files.size()) {
		source_file = i;
		addref_source_file(source_file);
	}
}

bool token_t::operator==(const token_t &t) const {
	if (type != t.type)
		return false;

	switch (type) {
		case token_type_t::integer:
			if (v.integer.v.u != t.v.integer.v.u)
				return false;
			if (v.integer.flags != t.v.integer.flags)
				return false;
			break;
		default:
			break;
	}

	return true;
}

std::string token_t::to_str(void) const {
	std::string s = token_type_t_str(type);

	switch (type) {
		case token_type_t::integer:
			s += "("; s += v.integer.to_str(); s += ")";
			break;
		case token_type_t::floating:
			s += "("; s += v.floating.to_str(); s += ")";
			break;
		case token_type_t::charliteral:
		case token_type_t::strliteral:
		case token_type_t::anglestrliteral:
			s += "("; s += csliteral(v.csliteral).to_str(); s += ")";
			break;
		case token_type_t::r_macro_paramref: {
			char tmp[64];
			sprintf(tmp,"(%u)",(unsigned int)v.paramref);
			s += tmp;
			break; }
		case token_type_t::identifier:
		case token_type_t::ppidentifier:
		case token_type_t::r___asm_text:
			s += "("; if (v.identifier != identifier_none) s += identifier(v.identifier).to_str(); s += ")";
			break;
		case token_type_t::op_compound_statement:
			s += "("; if (v.scope != scope_none) s += std::string("scope #")+std::to_string((unsigned long)v.scope); s += ")";
			break;
		case token_type_t::op_symbol:
			s += "("; if (v.symbol != symbol_none) s += std::string("symbol #")+std::to_string((unsigned long)v.symbol); s += ")";
			break;
		default:
			break;
	}

	return s;
}

void token_t::clear_v(void) {
	common_delete();
}

void token_t::common_delete(void) {
	switch (type) {
		case token_type_t::charliteral:
		case token_type_t::strliteral:
		case token_type_t::anglestrliteral:
			csliteral.release(v.csliteral);
			break;
		case token_type_t::op_declaration:
			typ_delete(v.declaration);
			break;
		case token_type_t::identifier:
		case token_type_t::ppidentifier:
		case token_type_t::r___asm_text:
			identifier.release(v.identifier);
			break;
		default:
			break;
	}

	set_source_file(no_source_file);
}

void token_t::common_init(void) {
	switch (type) {
		case token_type_t::integer:
			v.integer.init();
			break;
		case token_type_t::floating:
			v.floating.init();
			break;
		case token_type_t::charliteral:
		case token_type_t::strliteral:
		case token_type_t::anglestrliteral:
			v.csliteral = csliteral_none;
			break;
		case token_type_t::op_declaration:
			v.general_ptr = NULL;
			static_assert( offsetof(v_t,general_ptr) == offsetof(v_t,declaration), "oops" );
			break;
		case token_type_t::identifier:
		case token_type_t::ppidentifier:
		case token_type_t::r___asm_text:
			v.identifier = identifier_none;
			break;
		case token_type_t::op_compound_statement:
			v.scope = scope_none;
			break;
		case token_type_t::op_symbol:
			v.symbol = symbol_none;
			break;
		default:
			break;
	}
}

void token_t::common_copy(const token_t &x) {
	assert(&x != this);

	common_delete();

	set_source_file(x.source_file);
	type = x.type;
	pos = x.pos;

	common_init();

	switch (type) {
		case token_type_t::charliteral:
		case token_type_t::strliteral:
		case token_type_t::anglestrliteral:
			csliteral.assign(/*to*/v.csliteral,/*from*/x.v.csliteral);
			break;
		case token_type_t::op_declaration:
			throw std::runtime_error("Copy constructor not available");
			break;
		case token_type_t::identifier:
		case token_type_t::ppidentifier:
		case token_type_t::r___asm_text:
			identifier.assign(/*to*/v.identifier,/*from*/x.v.identifier);
			break;
		default:
			v = x.v;
			break;
	}
}

void token_t::common_move(token_t &x) {
	common_delete();

	source_file = x.source_file; x.source_file = no_source_file;
	type = x.type; x.type = token_type_t::none;
	pos = x.pos; x.pos = position_t();

	common_init();

	switch (type) {
		case token_type_t::charliteral:
		case token_type_t::strliteral:
		case token_type_t::anglestrliteral:
			csliteral.assignmove(/*to*/v.csliteral,/*from*/x.v.csliteral);
			break;
		case token_type_t::identifier:
		case token_type_t::ppidentifier:
		case token_type_t::r___asm_text:
			identifier.assignmove(/*to*/v.identifier,/*from*/x.v.identifier);
			break;
		case token_type_t::op_compound_statement:
			v.scope = x.v.scope; x.v.scope = scope_none;
			break;
		case token_type_t::op_symbol:
			v.symbol = x.v.symbol; x.v.symbol = symbol_none;
			break;
		default:
			v = x.v;
			memset(&x.v,0,sizeof(x.v)); /* x.type == none so pointers no longer matter */
			break;
	}
}

