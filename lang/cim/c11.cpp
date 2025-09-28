
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

int c11yy_mul_fconst(struct c11yy_struct_float &d,const struct c11yy_struct_float &a,const struct c11yy_struct_float &b) {
	d = a;

	if (d.sz < a.sz) d.sz = a.sz;
	if (d.sz < b.sz) d.sz = b.sz;

	/* if either side is zero, the result is zero */
	if ((a.flags^b.flags)&C11YY_FLOATF_NEGATIVE)
		d.flags |= C11YY_FLOATF_NEGATIVE;
	else
		d.flags &= ~C11YY_FLOATF_NEGATIVE;

	if (a.mant != 0ull && b.mant != 0ull) {
		uint64_t rh,rl;

		/* result: 128-bit value, mantissa on bit 126 because 2^63 * 2^63 = 2^126 */
		multiply64x64to128(/*&*/rh,/*&*/rl,a.mant,b.mant);
		d.exponent = a.exponent + b.exponent + 1;
		d.mant = rh;
		if (d.mant == 0ull) {
			d.mant = rl;
			d.exponent -= 64;
			rl = 0;
		}

		if (d.mant != 0ull) {
			while (!(d.mant & 0x8000000000000000ull)) {
				d.mant = (d.mant << 1ull) | (rl >> 63ull);
				d.exponent--;
				rl <<= 1ull;
			}
		}
		/* rounding */
		if (rl & 0x8000000000000000ull) {
			if ((++d.mant) == (uint64_t)0ull) {
				d.mant = (uint64_t)0x8000000000000000ull;
				d.exponent++;
			}
		}
	}
	else {
		d.exponent = 0;
		d.mant = 0;
	}

	fprintf(stderr,"fltmul res %.60f flags=%lx sz=%u exp=%d mant=0x%016llx\n",
		(double)ldexpl((long double)d.mant,d.exponent - 63) * (d.flags&C11YY_FLOATF_NEGATIVE ? -1.0 : 1.0),
		(unsigned long)d.flags,
		(unsigned int)d.sz,
		(unsigned int)d.exponent,
		(unsigned long long)d.mant);

	fprintf(stderr,"    a %.60f flags=%lx sz=%u exp=%d mant=0x%016llx\n",
		(double)ldexpl((long double)a.mant,a.exponent - 63) * (a.flags&C11YY_FLOATF_NEGATIVE ? -1.0 : 1.0),
		(unsigned long)a.flags,
		(unsigned int)a.sz,
		(unsigned int)a.exponent,
		(unsigned long long)a.mant);

	fprintf(stderr,"    b %.60f flags=%lx sz=%u exp=%d mant=0x%016llx\n",
		(double)ldexpl((long double)b.mant,b.exponent - 63) * (b.flags&C11YY_FLOATF_NEGATIVE ? -1.0 : 1.0),
		(unsigned long)b.flags,
		(unsigned int)b.sz,
		(unsigned int)b.exponent,
		(unsigned long long)b.mant);

	return 0;
}

////////////////////////////////////////////////////////////////////

int c11yy_div_fconst(struct c11yy_struct_float &d,const struct c11yy_struct_float &a,const struct c11yy_struct_float &b) {
	d = a;

	if (d.sz < a.sz) d.sz = a.sz;
	if (d.sz < b.sz) d.sz = b.sz;

	/* if either side is zero, the result is zero */
	if ((a.flags^b.flags)&C11YY_FLOATF_NEGATIVE)
		d.flags |= C11YY_FLOATF_NEGATIVE;
	else
		d.flags &= ~C11YY_FLOATF_NEGATIVE;

	if (b.mant == 0ull)
		return 1;
	if (!(b.mant & 0x8000000000000000ull))
		return 1;

	if (a.mant != 0ull) {
		if (!(a.mant & 0x8000000000000000ull))
			return 1;

		d.exponent = a.exponent - b.exponent;
		d.mant = 0;

		uint64_t bset = (uint64_t)1u << (uint64_t)63u;
		uint64_t tmp = a.mant;
		uint64_t cmv = b.mant;

		/* both tmp and cnv have bit 63 set at this point */
		while (1) {
			if (tmp >= cmv) {
				d.mant |= bset;
				bset >>= 1u;
				tmp -= cmv;
				/* two values subtracted with bit 63 set and tmp >= cnv, now bit 63 should be clear, scale cnv as well for correct math */
				if ((cmv >>= 1ull) == 0ull)
					return 1;
				break;
			}

			d.exponent--;
			if ((cmv >>= 1ull) == 0ull)
				return 1;
		}

		while (tmp != 0ull) {
			if (tmp >= cmv) {
				d.mant |= bset;
				tmp -= cmv;
			}

			tmp <<= 1ull;
			if ((bset >>= 1u) == 0ull)
				break;
		}
	}
	else {
		d.exponent = 0;
		d.mant = 0;
	}

	fprintf(stderr,"fltdiv res %.60f flags=%lx sz=%u exp=%d mant=0x%016llx\n",
		(double)ldexpl((long double)d.mant,d.exponent - 63) * (d.flags&C11YY_FLOATF_NEGATIVE ? -1.0 : 1.0),
		(unsigned long)d.flags,
		(unsigned int)d.sz,
		(unsigned int)d.exponent,
		(unsigned long long)d.mant);

	fprintf(stderr,"    a %.60f flags=%lx sz=%u exp=%d mant=0x%016llx\n",
		(double)ldexpl((long double)a.mant,a.exponent - 63) * (a.flags&C11YY_FLOATF_NEGATIVE ? -1.0 : 1.0),
		(unsigned long)a.flags,
		(unsigned int)a.sz,
		(unsigned int)a.exponent,
		(unsigned long long)a.mant);

	fprintf(stderr,"    b %.60f flags=%lx sz=%u exp=%d mant=0x%016llx\n",
		(double)ldexpl((long double)b.mant,b.exponent - 63) * (b.flags&C11YY_FLOATF_NEGATIVE ? -1.0 : 1.0),
		(unsigned long)b.flags,
		(unsigned int)b.sz,
		(unsigned int)b.exponent,
		(unsigned long long)b.mant);

	return 0;
}

////////////////////////////////////////////////////////////////////

int c11yy_add_iconst(struct c11yy_struct_integer &d,const struct c11yy_struct_integer &a,const struct c11yy_struct_integer &b) {
	d = a;

	if (d.sz < a.sz) d.sz = a.sz;
	if (d.sz < b.sz) d.sz = b.sz;

	if ((a.flags&C11YY_INTF_SIGNED) || (b.flags&C11YY_INTF_SIGNED)) {
		d.v.s = a.v.s + b.v.s;
		if (d.v.s < a.v.s) d.flags |= C11YY_INTF_OVERFLOW;
		d.flags |= C11YY_INTF_SIGNED;

		const uint8_t sz = c11yy_iconsts_auto_size(d.v.s);
		if (d.sz < sz) d.sz = sz;
	}
	else {
		d.v.u = a.v.u + b.v.u;
		if (d.v.u < a.v.u) d.flags |= C11YY_INTF_OVERFLOW;

		const uint8_t sz = c11yy_iconstu_auto_size(d.v.u);
		if (d.sz < sz) d.sz = sz;
	}

	return 0;
}

////////////////////////////////////////////////////////////////////

int c11yy_sub_iconst(struct c11yy_struct_integer &d,const struct c11yy_struct_integer &a,const struct c11yy_struct_integer &b) {
	d = a;

	if (d.sz < a.sz) d.sz = a.sz;
	if (d.sz < b.sz) d.sz = b.sz;

	if ((a.flags&C11YY_INTF_SIGNED) || (b.flags&C11YY_INTF_SIGNED)) {
		d.v.s = a.v.s - b.v.s;
		if (d.v.s > a.v.s) d.flags |= C11YY_INTF_OVERFLOW;
		d.flags |= C11YY_INTF_SIGNED;

		const uint8_t sz = c11yy_iconsts_auto_size(d.v.s);
		if (d.sz < sz) d.sz = sz;
	}
	else {
		d.v.u = a.v.u - b.v.u;
		if (d.v.u > a.v.u) d.flags |= C11YY_INTF_OVERFLOW;

		const uint8_t sz = c11yy_iconstu_auto_size(d.v.u);
		if (d.sz < sz) d.sz = sz;
	}

	return 0;
}

////////////////////////////////////////////////////////////////////

int c11yy_mul_iconst(struct c11yy_struct_integer &d,const struct c11yy_struct_integer &a,const struct c11yy_struct_integer &b) {
	d = a;

	if (d.sz < a.sz) d.sz = a.sz;
	if (d.sz < b.sz) d.sz = b.sz;

	if ((a.flags&C11YY_INTF_SIGNED) || (b.flags&C11YY_INTF_SIGNED)) {
		d.v.s = a.v.s * b.v.s;
		d.flags |= C11YY_INTF_SIGNED;

		if (b.v.s > 1ll) {
			const int64_t mv = INT64_MAX / b.v.s;
			if (a.v.s > mv) d.flags |= C11YY_INTF_OVERFLOW;
		}
		else if (b.v.s < -1ll) {
			const int64_t mv = INT64_MIN / b.v.s;
			if (a.v.s < mv) d.flags |= C11YY_INTF_OVERFLOW;
		}

		const uint8_t sz = c11yy_iconsts_auto_size(d.v.s);
		if (d.sz < sz) d.sz = sz;
	}
	else {
		d.v.u = a.v.u * b.v.u;

		if (b.v.u > 1ull) {
			const uint64_t mv = UINT64_MAX / b.v.u;
			if (a.v.u > mv) d.flags |= C11YY_INTF_OVERFLOW;
		}

		const uint8_t sz = c11yy_iconstu_auto_size(d.v.u);
		if (d.sz < sz) d.sz = sz;
	}

	return 0;
}

////////////////////////////////////////////////////////////////////

int c11yy_div_iconst(struct c11yy_struct_integer &d,const struct c11yy_struct_integer &a,const struct c11yy_struct_integer &b) {
	d = a;

	if (d.sz < a.sz) d.sz = a.sz;
	if (d.sz < b.sz) d.sz = b.sz;

	if ((a.flags&C11YY_INTF_SIGNED) || (b.flags&C11YY_INTF_SIGNED)) {
		if (b.v.s == 0ll) return 1;
		d.v.s = a.v.s / b.v.s;
		d.flags |= C11YY_INTF_SIGNED;

		const uint8_t sz = c11yy_iconsts_auto_size(d.v.s);
		if (d.sz < sz) d.sz = sz;
	}
	else {
		if (b.v.u == 0ull) return 1;
		d.v.u = a.v.u / b.v.u;

		const uint8_t sz = c11yy_iconstu_auto_size(d.v.u);
		if (d.sz < sz) d.sz = sz;
	}

	return 0;
}

////////////////////////////////////////////////////////////////////

int c11yy_mod_iconst(struct c11yy_struct_integer &d,const struct c11yy_struct_integer &a,const struct c11yy_struct_integer &b) {
	d = a;

	if (d.sz < a.sz) d.sz = a.sz;
	if (d.sz < b.sz) d.sz = b.sz;

	if ((a.flags&C11YY_INTF_SIGNED) || (b.flags&C11YY_INTF_SIGNED)) {
		if (b.v.s == 0ll) return 1;
		d.v.s = a.v.s % b.v.s;
		d.flags |= C11YY_INTF_SIGNED;

		const uint8_t sz = c11yy_iconsts_auto_size(d.v.s);
		if (d.sz < sz) d.sz = sz;
	}
	else {
		if (b.v.u == 0ull) return 1;
		d.v.u = a.v.u % b.v.u;

		const uint8_t sz = c11yy_iconstu_auto_size(d.v.u);
		if (d.sz < sz) d.sz = sz;
	}

	return 0;
}

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

