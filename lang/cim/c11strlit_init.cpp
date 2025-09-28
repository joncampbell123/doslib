
#include <stdio.h>
#include <assert.h>

extern "C" {
#include "c11.h"
#include "c11.l.h"
#include "c11.y.h"
}

#include "c11.hpp"

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

	c11yy_write_utf8(d,val.v.u);
	return 0;
}

static int c11yy_strl_write_utf16(uint8_t* &dst,struct c11yy_struct_integer &val) {
	if (val.v.s < 0 || val.v.s > 0x10FFFFll)
		return 1;

	{
		uint16_t *d = (uint16_t*)dst;
		c11yy_write_utf16(d,val.v.u);
		dst = (uint8_t*)d;
	}
	return 0;
}

static int c11yy_strl_write_utf32(uint8_t* &dst,struct c11yy_struct_integer &val) {
	if (val.v.s < 0 || val.v.s > 0x7FFFFFFFll)
		return 1;

	{
		uint32_t *d = (uint32_t*)dst;
		*d++ = (uint32_t)val.v.u;
		dst = (uint8_t*)d;
	}
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
		const size_t len = (size_t)(w - buf);
		const uint32_t hash = c11yy_string_hash(buf,len);

		if (len >= 65535u) {
			free(buf);
			return;//err
		}

		if (len == 0) {
			free(buf);
			buf = NULL;
		}
		else if (len < alloc) {
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

		for (const auto &st : c11yy_string_table) {
			if (st.len == len && st.hash == hash) {
				if (len == 0 || (len != 0 && memcmp(st.str.raw,buf,len) == 0)) {
					val->id = size_t(&st - c11yy_string_table.data());
					free(buf);
					return;
				}
			}
		}

		{
			c11yy_string_table.emplace_back();
			struct c11yy_string_obj &io = *c11yy_string_table.rbegin();
			val->id = size_t(&io - c11yy_string_table.data());
			io.str.raw = buf;
			io.stype = stype;
			io.hash = hash;
			io.len = len;
		}
	}
}

