
#include <math.h>
#include <stdio.h>
#include <assert.h>

extern "C" {
#include "c11.h"
#include "c11.l.h"
#include "c11.y.h"
}

#include "c11.hpp"

#include <stdexcept>

std::vector<struct c11yy_scope_obj>		c11yy_scope_table;
std::vector<c11yy_scope_id>			c11yy_scope_stack;

void c11yy_identifier_obj_free(struct c11yy_identifier_obj &o) {
	if (o.s8) free(o.s8);
	o.s8 = NULL;
	o.len = 0;
}

////////////////////////////////////////////////////////////////////

c11yy_scope_obj::c11yy_scope_obj() {
}

c11yy_scope_obj::~c11yy_scope_obj() {
	for (auto &id : idents) c11yy_identifier_obj_free(id);
	idents.clear();
}

void c11yy_scopes_table_clear(void) {
	c11yy_scope_stack.clear();
	c11yy_scope_table.clear();
}

////////////////////////////////////////////////////////////////////

static const uint16_t c11yy_itype_to_token[C11YY_IDT__MAX] = {
	IDENTIFIER,				// C11YY_IDT_NONE = 0
	IDENTIFIER,				// C11YY_IDT_IDENTIFIER
	TYPEDEF_NAME,				// C11YY_IDT_TYPEDEF_NAME
	ENUMERATION_CONSTANT			// C11YY_IDT_ENUMERATION_CONSTANT
};

struct c11yy_identifier_obj *c11yy_ident_lookup_scope(const uint32_t hash,const char *buf,int len,struct c11yy_scope_obj &scob) {
	for (auto ii=scob.idents.begin();ii!=scob.idents.end();ii++) {
		auto &id = *ii;
		if (id.len == len && id.hash == hash) {
			if (len == 0 || (len != 0 && memcmp(id.s8,buf,len) == 0))
				return &id;
		}
	}

	return NULL;
}

extern "C" struct c11yy_identifier_obj *c11yy_ident_lookup(const uint32_t hash,const char *buf,int len,c11yy_scope_id *scope) {
	struct c11yy_identifier_obj *st;

	for (auto sci=c11yy_scope_stack.rbegin();sci!=c11yy_scope_stack.rend();sci++) {
		const c11yy_scope_id scoi = *sci;
		if ((st=c11yy_ident_lookup_scope(hash,buf,len,c11yy_scope_table[scoi])) != NULL) {
			if (scope) *scope = scoi;
			return st;
		}
	}

	return NULL;
}

struct c11yy_identifier_obj *c11yy_ident_create(const uint32_t hash,const char *buf,int len,c11yy_scope_id *scope) {
	if (c11yy_scope_stack.empty())
		return NULL;

	const c11yy_scope_id sid = *c11yy_scope_stack.rbegin();
	struct c11yy_scope_obj &sob = c11yy_scope_table[sid];
	struct c11yy_identifier_obj iob;

	iob.itype = C11YY_IDT_NONE;
	iob.hash = hash;
	iob.len = len;

	if (len != 0) {
		iob.s8 = (uint8_t*)malloc(len);
		if (!iob.s8) return NULL;
		memcpy(iob.s8,buf,len);
	}

	if (scope) *scope = sid;

	const size_t id = sob.idents.size();
	sob.idents.push_back(std::move(iob));
	return &sob.idents[id];
}

extern "C" struct c11yy_identifier_obj *c11yy_init_ident(const char *buf,int len,c11yy_scope_id *scope) {
	const uint32_t hash = c11yy_string_hash((uint8_t*)buf,len);
	struct c11yy_identifier_obj *st = c11yy_ident_lookup(hash,buf,len,scope);
	if (st) return st;
	return c11yy_ident_create(hash,buf,len,scope);
}

extern "C" c11yy_identifier_id c11yy_ident_to_id(struct c11yy_identifier_obj *io,const c11yy_scope_id scope) {
	if (scope < c11yy_scope_table.size()) {
		const auto &scob = c11yy_scope_table[scope];
		const size_t i = size_t(io - scob.idents.data());
		if (i < scob.idents.size()) return (c11yy_identifier_id)i;
	}

	return c11yy_identifier_none;
}

extern "C" int c11yy_check_type(const struct c11yy_identifier_obj *io) {
	if (io && io->itype < C11YY_IDT__MAX) return c11yy_itype_to_token[io->itype];
	return IDENTIFIER;
}

////////////////////////////////////////////////////////////////////

c11yy_astnode_array c11yy_astnodes;

struct c11yy_struct_astnode &c11yy_astnode_ref(c11yy_astnode_array &anr,const c11yy_astnode_id id) {
	if (id < anr.size()) return anr[id].astnode;
	throw std::out_of_range("AST node ID out of range");
}

const struct c11yy_struct_astnode &c11yy_astnode_ref(const c11yy_astnode_array &anr,const c11yy_astnode_id id) {
	if (id < anr.size()) return anr[id].astnode;
	throw std::out_of_range("AST node ID out of range");
}

////////////////////////////////////////////////////////////////////

static int c11yy_init(void) {
	/* assume not yet init */
	{
		struct c11yy_scope_obj so;
		const size_t i = c11yy_scope_table.size();
		c11yy_scope_table.push_back(std::move(so));
		c11yy_scope_stack.push_back(i);
	}

	return 0;
}

static void c11yy_cleanup(void) {
	c11yy_string_table_clear();
	c11yy_scopes_table_clear();
}

/////////////////////////////////////////////////////////////

int main() {
	if (c11yy_init()) {
		c11yy_cleanup();
		return 1;
	}

	if (c11yy_do_compile()) {
		c11yylex_destroy();
		c11yy_cleanup();
		return 1;
	}

	c11yylex_destroy();
	c11yy_cleanup();
	return 0;
}

