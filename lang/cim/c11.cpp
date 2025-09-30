
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

////////////////////////////////////////////////////////////////////

extern "C" int c11yy_add(union c11yy_struct *d,const union c11yy_struct *a,const union c11yy_struct *b) {
	if (a->base.t == I_CONSTANT && b->base.t == I_CONSTANT)
		return c11yy_add_iconst(d->intval,a->intval,b->intval);
	if (a->base.t == F_CONSTANT && b->base.t == F_CONSTANT)
		return c11yy_addsub_fconst(d->floatval,a->floatval,b->floatval,0);

	return 1;
}

////////////////////////////////////////////////////////////////////

extern "C" int c11yy_sub(union c11yy_struct *d,const union c11yy_struct *a,const union c11yy_struct *b) {
	if (a->base.t == I_CONSTANT && b->base.t == I_CONSTANT)
		return c11yy_sub_iconst(d->intval,a->intval,b->intval);
	if (a->base.t == F_CONSTANT && b->base.t == F_CONSTANT)
		return c11yy_addsub_fconst(d->floatval,a->floatval,b->floatval,C11YY_FLOATF_NEGATIVE);

	return 1;
}

////////////////////////////////////////////////////////////////////

extern "C" int c11yy_mul(union c11yy_struct *d,const union c11yy_struct *a,const union c11yy_struct *b) {
	if (a->base.t == I_CONSTANT && b->base.t == I_CONSTANT)
		return c11yy_mul_iconst(d->intval,a->intval,b->intval);
	if (a->base.t == F_CONSTANT && b->base.t == F_CONSTANT)
		return c11yy_mul_fconst(d->floatval,a->floatval,b->floatval);

	return 1;
}

////////////////////////////////////////////////////////////////////

extern "C" int c11yy_div(union c11yy_struct *d,const union c11yy_struct *a,const union c11yy_struct *b) {
	if (a->base.t == I_CONSTANT && b->base.t == I_CONSTANT)
		return c11yy_div_iconst(d->intval,a->intval,b->intval);
	if (a->base.t == F_CONSTANT && b->base.t == F_CONSTANT)
		return c11yy_div_fconst(d->floatval,a->floatval,b->floatval);

	return 1;
}

////////////////////////////////////////////////////////////////////

extern "C" int c11yy_mod(union c11yy_struct *d,const union c11yy_struct *a,const union c11yy_struct *b) {
	if (a->base.t == I_CONSTANT && b->base.t == I_CONSTANT)
		return c11yy_mod_iconst(d->intval,a->intval,b->intval);

	return 1;
}

////////////////////////////////////////////////////////////////////

extern "C" int c11yy_shl(union c11yy_struct *d,const union c11yy_struct *a,const union c11yy_struct *b) {
	if (a->base.t == I_CONSTANT && b->base.t == I_CONSTANT)
		return c11yy_shl_iconst(d->intval,a->intval,b->intval);

	return 1;
}

////////////////////////////////////////////////////////////////////

extern "C" int c11yy_shr(union c11yy_struct *d,const union c11yy_struct *a,const union c11yy_struct *b) {
	if (a->base.t == I_CONSTANT && b->base.t == I_CONSTANT)
		return c11yy_shr_iconst(d->intval,a->intval,b->intval);

	return 1;
}

////////////////////////////////////////////////////////////////////

extern "C" int c11yy_cmp(union c11yy_struct *d,const union c11yy_struct *a,const union c11yy_struct *b,const enum c11yy_cmpop op) {
	if (a->base.t == I_CONSTANT && b->base.t == I_CONSTANT)
		return c11yy_cmp_iconst(d->intval,a->intval,b->intval,op);
	if (a->base.t == F_CONSTANT && b->base.t == F_CONSTANT)
		return c11yy_cmp_fconst(d->intval,a->floatval,b->floatval,op);

	return 1;
}

////////////////////////////////////////////////////////////////////

extern "C" int c11yy_binop(union c11yy_struct *d,const union c11yy_struct *a,const union c11yy_struct *b,const enum c11yy_binop op) {
	if (a->base.t == I_CONSTANT && b->base.t == I_CONSTANT)
		return c11yy_binop_iconst(d->intval,a->intval,b->intval,op);

	return 1;
}

////////////////////////////////////////////////////////////////////

extern "C" int c11yy_logop(union c11yy_struct *d,const union c11yy_struct *a,const union c11yy_struct *b,const enum c11yy_logop op) {
	if (a->base.t == I_CONSTANT && b->base.t == I_CONSTANT)
		return c11yy_logop_iconst(d->intval,a->intval,b->intval,op);

	return 1;
}

////////////////////////////////////////////////////////////////////

extern "C" int c11yy_ternary(union c11yy_struct *d,const union c11yy_struct *a,const union c11yy_struct *b,const union c11yy_struct *c) {
	if (a->base.t == I_CONSTANT) {
		if (a->intval.v.u != 0ull) *d = *b;
		else *d = *c;
		return 0;
	}
	else if (a->base.t == F_CONSTANT) {
		if (a->floatval.mant != 0ull) *d = *b;
		else *d = *c;
		return 0;
	}

	return 1;
}

////////////////////////////////////////////////////////////////////

std::vector<struct c11yy_scope_obj>		c11yy_scope_table;
std::vector<c11yy_identifier_obj>		c11yy_ident_table;
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
}

void c11yy_scopes_table_clear(void) {
	c11yy_scope_stack.clear();
	c11yy_scope_table.clear();
}

void c11yy_ident_table_clear() {
	for (auto &id : c11yy_ident_table) c11yy_identifier_obj_free(id);
}

////////////////////////////////////////////////////////////////////

static const uint16_t c11yy_itype_to_token[C11YY_IDT__MAX] = {
	IDENTIFIER,				// C11YY_IDT_NONE = 0
	IDENTIFIER,				// C11YY_IDT_IDENTIFIER
	TYPEDEF_NAME,				// C11YY_IDT_TYPEDEF_NAME
	ENUMERATION_CONSTANT			// C11YY_IDT_ENUMERATION_CONSTANT
};

struct c11yy_identifier_obj *c11yy_ident_lookup(const uint32_t hash,const char *buf,int len) {
	for (auto ii=c11yy_ident_table.begin();ii!=c11yy_ident_table.end();ii++) {
		auto &id = *ii;
		if (id.len == len && id.hash == hash) {
			if (len == 0 || (len != 0 && memcmp(id.s8,buf,len) == 0))
				return &id;
		}
	}

	return NULL;
}

struct c11yy_identifier_obj *c11yy_ident_create(const uint32_t hash,const char *buf,int len) {
	struct c11yy_identifier_obj iob;

	iob.hash = hash;
	iob.len = len;

	if (len != 0) {
		iob.s8 = (uint8_t*)malloc(len);
		if (!iob.s8) return NULL;
		memcpy(iob.s8,buf,len);
	}
	else {
		iob.s8 = NULL;
	}

	const size_t id = c11yy_ident_table.size();
	c11yy_ident_table.push_back(std::move(iob));
	return &c11yy_ident_table[id];
}

c11yy_identifier_id c11yy_ident_to_id(const struct c11yy_identifier_obj *io) {
	if (io == NULL || io < c11yy_ident_table.data()) return c11yy_identifier_none;
	const size_t i = size_t(io - c11yy_ident_table.data());
	if (i < c11yy_ident_table.size()) return (c11yy_identifier_id)i;
	return c11yy_identifier_none;
}

extern "C" c11yy_identifier_id c11yy_init_undef_ident(const char *buf,int len) {
	const uint32_t hash = c11yy_string_hash((uint8_t*)buf,len);
	const struct c11yy_identifier_obj *st = c11yy_ident_lookup(hash,buf,len);
	if (!st) st = c11yy_ident_create(hash,buf,len);
	return c11yy_ident_to_id(st);
}

////////////////////////////////////////////////////////////////////

c11yy_astnode_array c11yy_astnodes;

template <typename R,typename A> static inline R &c11yy_astnode_ref_common(A &anr,const c11yy_astnode_id id) {
	if (id < anr.size()) return anr[id].astnode;
	throw std::out_of_range("AST node ID out of range");
}

struct c11yy_struct_astnode &c11yy_astnode_ref(c11yy_astnode_array &anr,const c11yy_astnode_id id) {
	return c11yy_astnode_ref_common<struct c11yy_struct_astnode,c11yy_astnode_array>(anr,id);
}

const struct c11yy_struct_astnode &c11yy_astnode_ref(const c11yy_astnode_array &anr,const c11yy_astnode_id id) {
	return c11yy_astnode_ref_common<const struct c11yy_struct_astnode,const c11yy_astnode_array>(anr,id);
}

////////////////////////////////////////////////////////////////////

static int c11yy_init(void) {
	/* assume not yet init */
	{
		/* "undefined" scope for identifiers by default, corresponds to c11yy_scope_none */
		struct c11yy_scope_obj so;
		const size_t i = c11yy_scope_table.size(); assert(i == 0u);
		c11yy_scope_table.push_back(std::move(so));
		c11yy_scope_stack.push_back(i);
	}
	{
		/* global scope */
		struct c11yy_scope_obj so;
		const size_t i = c11yy_scope_table.size(); assert(i == 1u);
		c11yy_scope_table.push_back(std::move(so));
		c11yy_scope_stack.push_back(i);
	}

	return 0;
}

static void c11yy_cleanup(void) {
	c11yy_string_table_clear();
	c11yy_scopes_table_clear();
	c11yy_ident_table_clear();
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

