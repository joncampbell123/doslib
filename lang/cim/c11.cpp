
#include <math.h>
#include <stdio.h>
#include <assert.h>

extern "C" {
#include "c11.h"
#include "c11.l.h"
#include "c11.y.h"
}

#include "c11.hpp"

std::vector<struct c11yy_scope_obj>		c11yy_scope_table;
std::vector<c11yy_scope_id>			c11yy_scope_stack;

void c11yy_identifier_obj_free(struct c11yy_identifier_obj &o) {
	if (o.s8) free(o.s8);
	o.s8 = NULL;
	o.len = 0;
}

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

extern "C" void c11yy_init_fconst(struct c11yy_struct_float *val,const char *yytext,const char lexmatch) {
	const char *str = yytext;
	int exponent = 0;

	val->t = F_CONSTANT;
	val->mant = 0;
	val->exponent = 0;
	val->flags = 0;
	val->sz = 8; // assume double

	/* NTS: The lex syntax file does not consider a leading negative sign part of this number,
	 *      therefore this code does not need to worry about negative numbers. */

	if (lexmatch == 'D') {
		const long double rv = strtold(yytext,(char**)(&yytext));
		const long double nv = frexpl(rv,&exponent);
		if (nv != 0.0l) {
			/* convert 0.5 <= x < 1.0 * (2^exponent) to IEEE representation */
			val->exponent = exponent - 1;
			val->mant = (uint64_t)floor((nv * 18446744073709551616.0l) + 0.5l);
		}
	}
	else if (lexmatch == 'H') {
		unsigned char ldigit = 0;
		unsigned char shf = 64;
		int v;

		/* skip the "0x" */
		c11yyskip(yytext,2);

		/* read hex digits, fill the mantissa from the top bits down 4 bits at a time.
		 * shf must be a multiple of 4 at this point of the code.
		 * while filling the whole part, track the exponent.
		 * do not track the exponent for the fractional part. */

		/* whole part */
		/* skip leading zeros */
		while (*yytext == '0') yytext++;
		if (*yytext != '0') {
			exponent--; /* whole part is nonzero, adjust so the first digit has exponent=3 */
			while ((v=c11yy_iconst_readc(16,yytext)) >= 0) {
				if (shf >= 4) { /* assume shf a multiple of 4 */
					shf -= 4; exponent += 4; val->mant |= (uint64_t)v << (uint64_t)shf;
				}
				else {
					ldigit = (unsigned char)v;
					while ((v=c11yy_iconst_readc(16,yytext)) >= 0) exponent += 4;
					break;
				}
			}

			/* normalize mantissa */
			if (val->mant != 0ull) {
				while (!(val->mant & 0x8000000000000000ull)) {
					val->mant <<= 1ull;
					exponent--;
					shf++;
				}
			}
		}
		/* fractional part */
		if (*yytext == '.') {
			yytext++;
			while ((v=c11yy_iconst_readc(16,yytext)) >= 0) {
				if (shf >= 4) { /* assume shf a multiple of 4 */
					shf -= 4; val->mant |= (uint64_t)v << (uint64_t)shf;
				}
				else {
					ldigit = (unsigned char)v;
					while ((v=c11yy_iconst_readc(16,yytext)) >= 0);
					break;
				}
			}

			/* normalize mantissa */
			if (val->mant != 0ull) {
				while (!(val->mant & 0x8000000000000000ull)) {
					val->mant <<= 1ull;
					shf++;
				}
			}
		}
		/* was there a leftover digit that didn't fit, and normalization made some room for it? */
		if (shf && ldigit) {
			/* assume shf < 4 then (leftover bits to fill), ldigit is not set otherwise */
			val->mant |= (uint64_t)ldigit >> (uint64_t)(4u - shf);
			shf = 0;
		}
		/* parse the 'p' power of 2 exponent */
		if (*yytext == 'p') {
			struct c11yy_struct_integer val = c11yy_struct_integer_I_CONSTANT_INIT;
			bool negative = false;

			yytext++;
			if (*yytext == '-') {
				negative = true;
				yytext++;
			}

			c11yy_iconst_read(/*base*/10,/*&*/val,yytext);
			if (negative) exponent -= (int)val.v.s;
			else exponent += (int)val.v.s;
		}

		val->exponent = exponent;
	}

	if (*yytext == 'f' || *yytext == 'F') {
		val->sz = 4; /* float */
	}
	else if (*yytext == 'l' || *yytext == 'L') {
		val->sz = 10; /* long double */
	}

	const long double rv = ldexpl((long double)val->mant,val->exponent - 63);
	fprintf(stderr,"flt %.6f flags=%lx sz=%u exp=%d mant=0x%016llx '%s'\n",
		(double)rv,
		(unsigned long)val->flags,
		(unsigned int)val->sz,
		(unsigned int)val->exponent,
		(unsigned long long)val->mant,
		str);
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

