
#include <stdio.h>
#include <assert.h>

extern "C" {
#include "c11.h"
#include "c11.l.h"
#include "c11.y.h"
}

#include "c11.hpp"

////////////////////////////////////////////////////////////////////

static void c11yy_iconst_readchar(const enum c11yystringtype st,struct c11yy_struct_integer &val,const char* &s) {
	val.v.u = 0;
	if (*s == '\\') {
		unsigned int c;
		char esc;
		int v;

		s++;
		switch (esc=*s++) {
			case '\'': case '\"': case '?': case '\\':
				val.v.u = (unsigned int)esc;
				break;
			case 'a': val.v.u = 7; break;
			case 'b': val.v.u = 8; break;
			case 'f': val.v.u = 12; break;
			case 'n': val.v.u = 10; break;
			case 'r': val.v.u = 13; break;
			case 't': val.v.u = 9; break;
			case 'v': val.v.u = 11; break;
			case '0': case '1': case '2': case '3':
			case '4': case '5': case '6': case '7':
				val.v.u = (unsigned int)esc - '0';
				for (c=0;c < 2;c++) {
					if ((v=c11yy_iconst_readc(/*base*/8,s)) >= 0)
						val.v.u = (val.v.u * (uint64_t)8u) + ((uint64_t)(unsigned int)v);
					else
						break;
				}
				break;
			case 'x':
				c11yy_iconst_read(/*base*/16,/*&*/val,/*&*/s);
				break;
			/* TODO: lexer does not yet support /u and /U */
		}
	}
	else if (*s) {
		if (st != C11YY_STRT_LOCAL)
			c11yy_read_utf8(val,s);
		else
			val.v.u = *s++;
	}
}

///////////////////////////////////////////////////////

static constexpr uint32_t c11yy_string_hash_init = (uint32_t)0xA1272155ul;

static inline uint32_t c11yy_string_hash_begin() {
	return c11yy_string_hash_init;
}

static inline uint32_t c11yy_string_hash_step(const uint32_t h,const uint8_t c) {
	return ((h << (uint32_t)13ul) ^ (h >> (uint32_t)19ul) ^ (h >> (uint32_t)31u) ^ 1) + (uint32_t)c;
}

static inline uint32_t c11yy_string_hash_end(const uint32_t h) {
	return (uint32_t)(~h);
}

uint32_t c11yy_string_hash(const uint8_t *s,size_t l) {
	uint32_t h = c11yy_string_hash_begin();
	while ((l--) != 0ul) h = c11yy_string_hash_step(h,*s++);
	return c11yy_string_hash_end(h);
}

///////////////////////////////////////////////////////////////////

static void c11yy_check_stringtype_prefix(enum c11yystringtype &st,const char* &s) {
	if (*s == 'u') {
		st = C11YY_STRT_UTF16; s++;
		if (*s == '8') {
			st = C11YY_STRT_UTF8; s++;
		}
	}
	else if (*s == 'U') {
		st = C11YY_STRT_UTF32; s++;
	}
	else if (*s == 'L') {
		st = C11YY_STRT_UTF32; s++;
	}
}

////////////////////////////////////////////////////////////////////

static int c11yy_strl_write_local(uint8_t* &d,struct c11yy_struct_integer &val) {
	if (val.v.s < 0 || val.v.s > 255)
		return 1;

	*d++ = (uint8_t)val.v.u;
	return 0;
}

static int c11yy_strl_write_utf8(uint8_t* &d,struct c11yy_struct_integer &val) {
	if (val.v.s < 0 || val.v.s > 0x7FFFFFFFll)
		return 1;

	{
		/* 110x xxxx = 2 (1 more) mask 0x1F bits 5 + 6*1 = 11
		 * 1110 xxxx = 3 (2 more) mask 0x0F bits 4 + 6*2 = 16
		 * 1111 0xxx = 4 (3 more) mask 0x07 bits 3 + 6*3 = 21
		 * 1111 10xx = 5 (4 more) mask 0x03 bits 2 + 6*4 = 26
		 * 1111 110x = 6 (5 more) mask 0x01 bits 1 + 6*5 = 31 */
		uint32_t c = (uint32_t)val.v.u;
		if (c >= 0x80) {
			unsigned char more = 1;
			{
				uint32_t tmp = (uint32_t)(c) >> (uint32_t)(11u);
				while (tmp != 0) { more++; tmp >>= (uint32_t)(5u); }
				assert(more <= 5);
			}

			const uint8_t ib = 0xFC << (5 - more);
			unsigned char *wr = d; d += 1+more;
			do { wr[more] = (unsigned char)(0x80u | ((unsigned char)(c&0x3F))); c >>= 6u; } while ((--more) != 0);
			assert((uint32_t)(c) <= (uint32_t)((0x80u|(ib>>1u))^0xFFu)); /* 0xC0 0xE0 0xF0 0xF8 0xFC -> 0xE0 0xF0 0xF8 0xFC 0xFE -> 0x1F 0x0F 0x07 0x03 0x01 */
			wr[0] = (unsigned char)(ib | (unsigned char)c);
		}
		else {
			*d++ = (unsigned char)c;
		}
	}

	return 0;
}

static int c11yy_strl_write_utf16(uint8_t* &dst,struct c11yy_struct_integer &val) {
	uint16_t *d = (uint16_t*)dst;

	if (val.v.s < 0 || val.v.s > 0x10FFFFll)
		return 1;

	// TODO: Encode UTF-16 including surrogate pairs
	*d++ = (uint16_t)val.v.u;
	dst = (uint8_t*)d;
	return 0;
}

static int c11yy_strl_write_utf32(uint8_t* &dst,struct c11yy_struct_integer &val) {
	uint32_t *d = (uint32_t*)dst;

	if (val.v.s < 0 || val.v.s > 0xFFFFFFFFll)
		return 1;

	*d++ = (uint32_t)val.v.u;
	dst = (uint8_t*)d;
	return 0;
}

//////////////////////////////////////////////////////////////////

extern "C" void c11yy_init_strlit(struct c11yy_struct_strliteral *val,const char *yytext,int yyleng) {
	int (*strw)(uint8_t* &,struct c11yy_struct_integer &) = c11yy_strl_write_local;
	enum c11yystringtype stype = C11YY_STRT_LOCAL;
	struct c11yy_struct_integer ival;
	uint8_t *buf,*w;
	size_t alloc;

	val->id = c11yy_string_token_none;
	val->t = STRING_LITERAL;

	if (*yytext == 'u') {
		strw = c11yy_strl_write_utf16;
		stype = C11YY_STRT_UTF16;
		yytext++;
		if (*yytext == '8') {
			strw = c11yy_strl_write_utf8;
			stype = C11YY_STRT_UTF8;
			yytext++;
		}
	}
	else if (*yytext == 'U') {
		strw = c11yy_strl_write_utf32;
		stype = C11YY_STRT_UTF32;
		yytext++;
	}
	else if (*yytext == 'L') {
		strw = c11yy_strl_write_utf16;
		stype = C11YY_STRT_UTF16;
		yytext++;
	}

	{
		const size_t extra = 16u; /* enough space for encoding and writing a NUL at end of loop */

		alloc = ((unsigned long)yyleng >= 16ul/*min*/ ? ((unsigned long)yyleng < 65535ul/*max*/ ? (unsigned long)yyleng : 4096ul) : 16ul);
		w = buf = (uint8_t*)malloc(alloc);
		if (!buf) return;//err

		while (*yytext) {
			// trust the lexer has provided us properly formatted strings with escapes and possible whitespace between them
			if (*yytext == '\"') {
				yytext++;

				while (*yytext) {
					if (*yytext == '\"') {
						yytext++;
						break;//err
					}

					if ((w+extra) > (buf+alloc)) {
						const size_t nal = alloc + 8u + (alloc / 2u);
						const size_t wo = (size_t)(w - buf); /* realloc may move buffer */
						uint8_t *np = (uint8_t*)realloc((void*)buf,nal);
						if (!np) {
							free(buf);
							return;//err
						}
						buf = np;
						alloc = nal;
						w = buf + wo; /* realloc may move buffer */
					}

					assert((w+extra) <= (buf+alloc));

					c11yy_iconst_readchar(stype,ival,yytext);
					if (strw(/*&*/w,/*&*/ival)) {
						free(buf);
						return;//err
					}
				}
			}
			else {
				yytext++;
			}
		}

		assert(w >= buf);
		assert(w <= (buf+alloc));
	}

	{
		struct c11yy_string_obj *st;
		const size_t len = (size_t)(w - buf);
		const uint32_t hash = c11yy_string_hash(buf,len);

		if (len > 32768) {
			free(buf);
			return;//err
		}

		if (len < alloc) {
			uint8_t *np = (uint8_t*)realloc((void*)buf,len);
			if (np) {
				alloc = len;
				buf = np;
			}
			else {
				free(buf);
				return;//err
			}
		}

		//TODO
		free(buf);

#if 0
		st = c11yy_string_objarray_find(&c11yy_stringarray,hash,buf,len);
		if (st) {
			val->id = c11yy_string_objarray_to_id(&c11yy_stringarray,st);
			free(buf);
		}
		else {
			st = c11yy_string_objarray_new(&c11yy_stringarray);
			if (st) {
				val->id = c11yy_string_objarray_to_id(&c11yy_stringarray,st);
				st->stype = stype;
				st->str.s8 = buf;
				st->hash = hash;
				st->len = len;
			}
			else {
				free(buf);
				return;//err
			}
		}
#endif
	}
}

////////////////////////////////////////////////////////

extern "C" void c11yy_init_iconst(struct c11yy_struct_integer *val,const char *yytext,const char lexmatch) {
	/* lexmatch:
	 * 'H' hexadecimal
	 * 'N' decimal (starts with 1-9)
	 * 'O' octal (starts with 0, or is just zero)
	 * 'C' char constant
	 */
	*val = c11yy_struct_integer_I_CONSTANT_INIT;

	if (lexmatch == 'H') {
		c11yyskip(/*&*/yytext,2); /* assume 0x or 0X because the lexer already did the matching, see *.l file pattern {HP} */
		c11yy_iconst_read(/*base*/16,*val,/*&*/yytext);
		c11yy_iconst_readsuffix(*val,/*&*/yytext);
	}
	else if (lexmatch == 'N') {
		c11yy_iconst_read(/*base*/10,*val,/*&*/yytext);
		c11yy_iconst_readsuffix(*val,/*&*/yytext);
	}
	else if (lexmatch == 'O') {
		c11yy_iconst_read(/*base*/8,*val,/*&*/yytext); /* "0" is handled as octal, which is still correct parsing anyway */
		c11yy_iconst_readsuffix(*val,/*&*/yytext);
	}
	else if (lexmatch == 'C') {
		enum c11yystringtype st = C11YY_STRT_LOCAL;

		/* NTS: lexer syntax prevents 'u8' prefix */
		c11yy_check_stringtype_prefix(/*&*/st,/*&*/yytext);

		if (*yytext == '\'') {
			yytext++;
			c11yy_iconst_readchar(/*&*/st,*val,/*&*/yytext);
			/* assume trailing ' */
		}
	}

	{
		const uint8_t sz = c11yy_iconstu_auto_size(val->v.u);
		if (val->sz < sz) val->sz = sz;
	}

	/* this code never parses the leading minus sign, therefore everything parsed here is a nonnegative number,
	 * therefore if signed and the leading bit is set, overflow happend. */
	if (val->flags & C11YY_INTF_SIGNED) {
		if (val->v.s < (int64_t)0)
			val->flags |= C11YY_INTF_OVERFLOW;
	}
}

////////////////////////////////////////////////////////////////////

static const uint16_t c11yy_itype_to_token[C11YY_IDT__MAX] = {
	IDENTIFIER,				// C11YY_IDT_NONE = 0
	IDENTIFIER,				// C11YY_IDT_IDENTIFIER
	TYPEDEF_NAME,				// C11YY_IDT_TYPEDEF_NAME
	ENUMERATION_CONSTANT			// C11YY_IDT_ENUMERATION_CONSTANT
};

extern "C" struct c11yy_identifier_obj *c11yy_init_ident(const char *yytext,int yyleng) {
	return NULL;
}

extern "C" int c11yy_check_type(const struct c11yy_identifier_obj *io) {
	if (io && io->itype < C11YY_IDT__MAX) return c11yy_itype_to_token[io->itype];
	return IDENTIFIER;
}

////////////////////////////////////////////////////////////////////

static int c11yy_unary_op_none(union c11yy_struct *d,const union c11yy_struct *s) {
	*d = *s;
	return 0;
}

static int c11yy_unary_op_neg(union c11yy_struct *d,const union c11yy_struct *s) {
	if (s->base.t == I_CONSTANT) {
		*d = *s;
		d->intval.v.s = -d->intval.v.s;
		d->intval.flags |= C11YY_INTF_SIGNED;
		d->intval.sz = c11yy_iconsts_auto_size(d->intval.v.s);
		return 0;
	}

	return 1;
}

static int c11yy_unary_op_pos(union c11yy_struct *d,const union c11yy_struct *s) {
	if (s->base.t == I_CONSTANT) {
		*d = *s;
		d->intval.flags |= C11YY_INTF_SIGNED;
		d->intval.sz = c11yy_iconsts_auto_size(d->intval.v.s);
		return 0;
	}

	return 1;
}

static int c11yy_unary_op_bnot(union c11yy_struct *d,const union c11yy_struct *s) {
	if (s->base.t == I_CONSTANT) {
		/* do not update size */
		*d = *s;
		d->intval.v.u = ~d->intval.v.u;
		d->intval.flags &= ~C11YY_INTF_SIGNED;
		d->intval.flags |= C11YY_INTF_TRUNCATEOK;
		return 0;
	}

	return 1;
}

static int c11yy_unary_op_lnot(union c11yy_struct *d,const union c11yy_struct *s) {
	if (s->base.t == I_CONSTANT) {
		/* do not update size */
		*d = *s;
		d->base.t = s->base.t;
		d->intval.v.u = (s->intval.v.u == (uint64_t)0ull) ? 1u : 0u;
		return 0;
	}

	return 1;
}

static int (* const c11yy_unary_op[C11YY_UNOP__MAX])(union c11yy_struct *,const union c11yy_struct *) = { // must match enum c11yy_unop
	c11yy_unary_op_none,		// 0
	c11yy_unary_op_neg,
	c11yy_unary_op_pos,
	c11yy_unary_op_bnot,
	c11yy_unary_op_lnot
};

extern "C" int c11yy_unary(union c11yy_struct *d,const union c11yy_struct *s,const unsigned int unop) {
	if (unop < C11YY_UNOP__MAX)
		return c11yy_unary_op[unop](d,s);
	else
		return 1;
}

///////////////////////////////////////////////////////////

static int c11yy_init(void) {
	return 0;
}

static void c11yy_cleanup(void) {
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

