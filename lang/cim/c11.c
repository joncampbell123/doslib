
#include <stdio.h>
#include <assert.h>

#include "c11.h"
#include "c11.l.h"
#include "c11.y.h"

///////////////////////////////////////////////////////

static uint8_t c11yy_iconstu_auto_size(uint64_t v) {
	uint8_t sz = 0u;

	while (v) {
		v >>= (uint64_t)8u;
		sz += 8u;
	}

	return sz;
}

static uint8_t c11yy_iconsts_auto_size(int64_t v) {
	if (v >= (int64_t)0)
		return c11yy_iconstu_auto_size(v >> (uint64_t)7ull) + 8u;
	else
		return c11yy_iconstu_auto_size((((uint64_t)(-v)) - (uint64_t)1ull) >> (uint64_t)7ull) + 8u;
}

////////////////////////////////////////////////////////////////////

static void c11yy_iconst_readsuffix(struct c11yy_struct_integer *val,const char **y) {
	const char *s = *y;
	unsigned int f = 0;
#define UF 0x1u
#define LF 0x2u
#define LLF 0x4u
	while (1) {
		if (*s == 'u' || *s == 'U') {
			s++;
			f |= UF;
		}
		else if (*s == 'l' || *s == 'L') {
			s++;
			if (*s == 'l' || *s == 'L') {
				s++;
				f |= LLF;
			}
			else {
				f |= LF;
			}
		}
		else {
			break;
		}
	}

	if (f & UF)
		val->flags &= ~C11YY_INTF_SIGNED;

	if (f & LLF) {
		if (val->sz < 64u)
			val->sz = 64u;
	}
	else if (f & LF) {
		if (val->sz < 32u)
			val->sz = 32u;
	}

	*y = s;
#undef LLF
#undef LF
#undef UF
}

////////////////////////////////////////////////////////////////////

static void c11yyskip(const char **y,unsigned int c) {
	const char *s = *y;

	while (*s && c) {
		s++;
		c--;
	}

	*y = s;
}

////////////////////////////////////////////////////////

static int c11yy_iconst_readc(const unsigned int base,const char **y) {
	const char *s = *y;
	unsigned char v;

	if (*s >= '0' && *s <= '9')
		v = (unsigned char)(*s - '0');
	else if (*s >= 'a' && *s <= 'z')
		v = (unsigned char)(*s + 10 - 'a');
	else if (*s >= 'A' && *s <= 'Z')
		v = (unsigned char)(*s + 10 - 'A');
	else
		return -1;

	if (v < base) {
		*y = ++s;
		return (int)v;
	}

	return -1;
}

static void c11yy_iconst_read(const unsigned int base,struct c11yy_struct_integer *val,const char **y) {
	const uint64_t maxv = UINT64_MAX / (uint64_t)(base);
	const char *s = *y;
	int v;

	val->v.u = 0;
	while (*s) {
		if ((v=c11yy_iconst_readc(base,&s)) < 0)
			break;

		if (val->v.u > maxv)
			val->flags |= C11YY_INTF_OVERFLOW;

		val->v.u = (val->v.u * ((uint64_t)base)) + ((uint64_t)(unsigned int)v);
	}

	*y = s;
}

static void c11yy_iconst_readchar(const enum c11yystringtype st,struct c11yy_struct_integer *val,const char **y) {
	const char *s = *y;

	val->v.u = 0;
	if (*s == '\\') {
		unsigned int c;
		char esc;
		int v;

		s++;
		switch (esc=*s++) {
			case '\'': case '\"': case '?': case '\\':
				val->v.u = (unsigned int)esc;
				break;
			case 'a': val->v.u = 7; break;
			case 'b': val->v.u = 8; break;
			case 'f': val->v.u = 12; break;
			case 'n': val->v.u = 10; break;
			case 'r': val->v.u = 13; break;
			case 't': val->v.u = 9; break;
			case 'v': val->v.u = 11; break;
			case '0': case '1': case '2': case '3':
			case '4': case '5': case '6': case '7':
				val->v.u = (unsigned int)esc - '0';
				for (c=0;c < 2;c++) {
					if ((v=c11yy_iconst_readc(/*base*/8,&s)) >= 0)
						val->v.u = (val->v.u * (uint64_t)8u) + ((uint64_t)(unsigned int)v);
					else
						break;
				}
				break;
			case 'x':
				c11yy_iconst_read(/*base*/16,val,&s);
				break;
			/* TODO: lexer does not yet support /u and /U */
		}
	}
	else if (*s) {
		if (st != C11YY_STRT_LOCAL && (unsigned char)(*s) >= 0xC0 && (unsigned char)(*s) < 0xFE) {
			/* 0x00-0x7F ASCII char */
			/* 0x80-0xBF we're in the middle of a UTF-8 char */
			/* overlong 1111 1110 or 1111 1111 */
			uint32_t v = (unsigned char)(*s++);

			/* 110x xxxx = 2 (1 more) mask 0x1F bits 5 + 6*1 = 11
			 * 1110 xxxx = 3 (2 more) mask 0x0F bits 4 + 6*2 = 16
			 * 1111 0xxx = 4 (3 more) mask 0x07 bits 3 + 6*3 = 21
			 * 1111 10xx = 5 (4 more) mask 0x03 bits 2 + 6*4 = 26
			 * 1111 110x = 6 (5 more) mask 0x01 bits 1 + 6*5 = 31 */
			unsigned char more = 1;
			for (unsigned char c=(unsigned char)v;(c&0xFFu) >= 0xE0u;) { c <<= 1u; more++; } assert(more <= 5);
			v &= 0x3Fu >> more; /* 1 2 3 4 5 -> 0x1F 0x0F 0x07 0x03 0x01 */

			do {
				unsigned char c = *s;
				if ((c&0xC0) != 0x80) c = 0; /* must be 10xx xxxx */
				if (c) s++;

				v = (v << (uint32_t)(6u)) + (uint32_t)(c & 0x3Fu);
			} while ((--more) != 0);

			val->v.u = v;
		}
		else {
			val->v.u = *s++;
		}
	}

	*y = s;
}

///////////////////////////////////////////////////////

struct c11yy_string_objarray {
	struct c11yy_string_obj*		array;
	size_t					length,alloc,next;
};

#define c11yy_string_objarray_INIT { NULL, 0, 0, 0 }

///////////////////////////////////////////////////////

#define c11yy_string_hash_init ((uint32_t)0xA1272155ul)

static inline uint32_t c11yy_string_hash_begin() {
	return c11yy_string_hash_init;
}

static inline uint32_t c11yy_string_hash_step(const uint32_t h,const uint8_t c) {
	return ((h << (uint32_t)13ul) ^ (h >> (uint32_t)19ul) ^ (h >> (uint32_t)31u) ^ 1) + (uint32_t)c;
}

static inline uint32_t c11yy_string_hash_end(const uint32_t h) {
	return (uint32_t)(~h);
}

///////////////////////////////////////////////////////////

static int c11yy_string_objarray_init(struct c11yy_string_objarray *a) {
	if (!a->array) {
		const size_t init_len = 8;

		if ((a->array=malloc(sizeof(struct c11yy_string_obj) * init_len)) != NULL) {
			a->alloc = init_len;
			a->length = 0;
			a->next = 0;
		}
		else {
			return -1;
		}
	}

	return 0;
}

static struct c11yy_string_obj *c11yy_string_objarray_id2str(struct c11yy_string_objarray *a,const c11yy_string_token_id id) {
	if (a && a->array && id < a->length)
		return a->array + id; /* pointer math equiv to &(a->array[id]) */

	return NULL;
}

static c11yy_string_token_id c11yy_string_objarray_str2id(struct c11yy_string_objarray *a,struct c11yy_string_obj *st) {
	if (a && a->array && st >= a->array) {
		const size_t i = (size_t)(st - a->array); /* equiv to ((uintptr_t)st - (uintptr_t)a->array) / (uintptr_t)sizoef(string_obj) */
		if (i < a->length) return i;
	}

	return c11yy_string_token_none;
}

static struct c11yy_string_obj *c11yy_string_objarray_scan_for_empty_slot(struct c11yy_string_objarray *a) {
	/* assume a && a->array */
	while (a->next < a->length) {
		struct c11yy_string_obj *o = a->array + a->next;

		if (o->str.raw == NULL && o->len == 0)
			return o;

		a->next++;
	}

	return NULL;
}

static struct c11yy_string_obj *c11yy_string_objarray_init_next_length_item(struct c11yy_string_objarray *a) {
	/* assume a && a->array */
	if (a->length < a->alloc) {
		struct c11yy_string_obj *o = a->array + a->length;
		memset(o,0,sizeof(*o));
		a->next = ++a->length;
		return o;
	}

	return NULL;
}

static int c11yy_string_objarray_extend(struct c11yy_string_objarray *a,size_t newlen) {
	/* assume a && a->array */
	if (a->alloc < newlen) {
		struct c11yy_string_obj *na;

		if ((na=realloc(a->array, sizeof(struct c11yy_string_obj) * newlen)) != NULL) {
			a->alloc = newlen;
			a->array = na;
			return 0;
		}
	}

	return -1;
}

static void c11yy_string_obj_freestring(struct c11yy_string_obj *st) {
	if (st) {
		if (st->str.raw) free(st->str.raw);
		st->str.raw = NULL;
		st->len = 0;
	}
}

static void c11yy_string_objarray_freestr_id(struct c11yy_string_objarray *a,c11yy_string_token_id id) {
	if (a && a->array && id < a->length) {
		struct c11yy_string_obj *st;

		if ((st=c11yy_string_objarray_id2str(a,id)) != NULL) {
			c11yy_string_obj_freestring(st);
			memset(st,0,sizeof(*st));
			a->next = id;
		}
	}
}

static struct c11yy_string_obj *c11yy_string_objarray_newstr(struct c11yy_string_objarray *a) {
	if (a && a->array) {
		struct c11yy_string_obj *r;

		if ((r=c11yy_string_objarray_scan_for_empty_slot(a)) != NULL) return r;
		if ((r=c11yy_string_objarray_init_next_length_item(a)) != NULL) return r;
		if (c11yy_string_objarray_extend(a,a->alloc + 8u + (a->alloc / 2u))) return NULL;
		return c11yy_string_objarray_init_next_length_item(a);
	}

	return NULL;
}

static struct c11yy_string_obj *c11yy_string_objarray_findstr(struct c11yy_string_objarray *a,const uint32_t hash,const uint8_t *s,size_t l) {
	if (a && a->array) {
		struct c11yy_string_obj *sc = a->array;
		size_t sl = a->length;

		while ((sl--) != 0ul) {
			if ((hash == (uint32_t)0 || hash == sc->hash) && sc->len == l && sc->str.raw) {
				if (memcmp(sc->str.s8,s,l) == 0)
					return sc;
			}

			sc++;
		}
	}

	return NULL;
}

static void c11yy_string_objarray_free(struct c11yy_string_objarray *a) {
	if (a->array) {
		size_t i;

		for (i=0;i < a->length;i++)
			c11yy_string_objarray_freestr_id(a,i);

		free(a->array);
	}

	a->length = 0;
	a->alloc = 0;
	a->next = 0;
}

///////////////////////////////////////////////////////////////////

static void c11yy_check_stringtype_prefix(enum c11yystringtype *st,const char **y) {
	const char *s = *y;

	if (*s == 'u') {
		*st = C11YY_STRT_UTF16; s++;
		if (*s == '8') {
			*st = C11YY_STRT_UTF8; s++;
		}
	}
	else if (*s == 'U') {
		*st = C11YY_STRT_UTF32; s++;
	}
	else if (*s == 'L') {
		*st = C11YY_STRT_UTF32; s++;
	}

	*y = s;
}

////////////////////////////////////////////////////////////////////

static int c11yy_strl_write_local(uint8_t **dst,struct c11yy_struct_integer *val) {
	uint8_t *d = *dst;

	if (val->v.s < 0 || val->v.s > 255)
		return 1;

	*d++ = (uint8_t)val->v.u;
	*dst = d;
	return 0;
}

static int c11yy_strl_write_utf8(uint8_t **dst,struct c11yy_struct_integer *val) {
	uint8_t *d = *dst;

	if (val->v.s < 0 || val->v.s > 0x7FFFFFFFll)
		return 1;

	{
		/* 110x xxxx = 2 (1 more) mask 0x1F bits 5 + 6*1 = 11
		 * 1110 xxxx = 3 (2 more) mask 0x0F bits 4 + 6*2 = 16
		 * 1111 0xxx = 4 (3 more) mask 0x07 bits 3 + 6*3 = 21
		 * 1111 10xx = 5 (4 more) mask 0x03 bits 2 + 6*4 = 26
		 * 1111 110x = 6 (5 more) mask 0x01 bits 1 + 6*5 = 31 */
		uint32_t c = (uint32_t)val->v.u;
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

	*dst = d;
	return 0;
}

static int c11yy_strl_write_utf16(uint8_t **dst,struct c11yy_struct_integer *val) {
	uint16_t *d = (uint16_t*)(*dst);

	if (val->v.s < 0 || val->v.s > 0x10FFFFll)
		return 1;

	// TODO: Encode UTF-16 including surrogate pairs
	*d++ = (uint16_t)val->v.u;
	*dst = (uint8_t*)d;
	return 0;
}

static int c11yy_strl_write_utf32(uint8_t **dst,struct c11yy_struct_integer *val) {
	uint32_t *d = (uint32_t*)(*dst);

	if (val->v.s < 0 || val->v.s > 0xFFFFFFFFll)
		return 1;

	*d++ = (uint32_t)val->v.u;
	*dst = (uint8_t*)d;
	return 0;
}

//////////////////////////////////////////////////////////////////

uint32_t c11yy_string_hash(const uint8_t *s,size_t l) {
	uint32_t h = c11yy_string_hash_begin();
	while ((l--) != 0ul) h = c11yy_string_hash_step(h,*s++);
	return c11yy_string_hash_end(h);
}

//////////////////////////////////////////////////////////////////

static struct c11yy_string_objarray c11yy_stringarray = c11yy_string_objarray_INIT;

//////////////////////////////////////////////////////////////////

void c11yy_init_strlit(struct c11yy_struct_strliteral *val,const char *yytext,int yyleng) {
	int (*strw)(uint8_t **dst,struct c11yy_struct_integer *val) = c11yy_strl_write_local;
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

		alloc = (yyleng >= 16ul/*min*/ ? (yyleng < 65535ul/*max*/ ? yyleng : 4096ul) : 16ul);
		w = buf = malloc(alloc);
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
						uint8_t *np = realloc(buf,nal);
						if (!np) {
							free(buf);
							return;//err
						}
						buf = np;
						alloc = nal;
						w = buf + wo; /* realloc may move buffer */
					}

					assert((w+extra) <= (buf+alloc));

					c11yy_iconst_readchar(stype,&ival,&yytext);
					if (strw(&w,&ival)) {
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
			uint8_t *np = realloc(buf,len);
			if (np) {
				alloc = len;
				buf = np;
			}
			else {
				free(buf);
				return;//err
			}
		}

		st = c11yy_string_objarray_findstr(&c11yy_stringarray,hash,buf,len);
		if (st) {
			val->id = c11yy_string_objarray_str2id(&c11yy_stringarray,st);
			free(buf);
		}
		else {
			st = c11yy_string_objarray_newstr(&c11yy_stringarray);
			if (st) {
				val->id = c11yy_string_objarray_str2id(&c11yy_stringarray,st);
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
	}
}

////////////////////////////////////////////////////////

void c11yy_init_iconst(struct c11yy_struct_integer *val,const char *yytext,const char lexmatch) {
	/* lexmatch:
	 * 'H' hexadecimal
	 * 'N' decimal (starts with 1-9)
	 * 'O' octal (starts with 0, or is just zero)
	 * 'C' char constant
	 */
	val->sz = 0;
	val->v.u = 0;
	val->flags = C11YY_INTF_SIGNED; /* values are signed by default */
	val->t = I_CONSTANT;

	if (lexmatch == 'H') {
		c11yyskip(&yytext,2); /* assume 0x or 0X because the lexer already did the matching, see *.l file pattern {HP} */
		c11yy_iconst_read(/*base*/16,val,&yytext);
		c11yy_iconst_readsuffix(val,&yytext);
	}
	else if (lexmatch == 'N') {
		c11yy_iconst_read(/*base*/10,val,&yytext);
		c11yy_iconst_readsuffix(val,&yytext);
	}
	else if (lexmatch == 'O') {
		c11yy_iconst_read(/*base*/8,val,&yytext); /* "0" is handled as octal, which is still correct parsing anyway */
		c11yy_iconst_readsuffix(val,&yytext);
	}
	else if (lexmatch == 'C') {
		enum c11yystringtype st = C11YY_STRT_LOCAL;

		/* NTS: lexer syntax prevents 'u8' prefix */
		c11yy_check_stringtype_prefix(&st,&yytext);

		if (*yytext == '\'') {
			yytext++;
			c11yy_iconst_readchar(st,val,&yytext);
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

static int (* const c11yy_unary_op[C11YY_UNOP__MAX])(union c11yy_struct *,const union c11yy_struct *) = {
	/* if FFMPEG has long used this kind of init than we should too */
	[C11YY_UNOP_NONE] = c11yy_unary_op_none,
	[C11YY_UNOP_NEG]  = c11yy_unary_op_neg,
	[C11YY_UNOP_POS]  = c11yy_unary_op_pos,
	[C11YY_UNOP_BNOT] = c11yy_unary_op_bnot,
	[C11YY_UNOP_LNOT] = c11yy_unary_op_lnot
};

int c11yy_unary(union c11yy_struct *d,const union c11yy_struct *s,const unsigned int unop) {
	if (unop < C11YY_UNOP__MAX)
		return c11yy_unary_op[unop](d,s);
	else
		return 1;
}

///////////////////////////////////////////////////////////

static int c11yy_init(void) {
	if (!c11yy_stringarray.array) {
		if (c11yy_string_objarray_init(&c11yy_stringarray))
			return 1;
	}

	return 0;
}

static void c11yy_cleanup(void) {
	c11yy_string_objarray_free(&c11yy_stringarray);
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

