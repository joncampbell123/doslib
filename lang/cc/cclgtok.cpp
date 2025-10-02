
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

int lgtok_asm_text(lgtok_state_t &lst,rbuf &buf,source_file_object &sfo,token_t &t) {
	(void)lst;

	unsigned char c;
	unsigned char data[1024];
	unsigned char *p = data;
	unsigned char *f = data + sizeof(data);

	assert(p < f);
	assert(t.type == token_type_t::none);

	rbuf_sfd_refill(buf,sfo);

	while (1) {
		c = buf.peekb();
		if (is_newline(c) || c == 0 || c == '{' || c == '}' || c == '\'' || c == '\"') break;

		/* check for _asm or __asm */
		if (c == '_') {
			size_t i = 1;
			if (buf.peekb(i) == '_') i++;
			if (buf.peekb(i) == 'a' && buf.peekb(i+1) == 's' && buf.peekb(i+2) == 'm') {
				if (p == data) {
					t.type = token_type_t::r___asm;
					buf.discardb(i+3);
					return 1;
				}
				else {
					break;
				}
			}
		}

		if ((p+1) >= f)
			CCERR_RET(ENAMETOOLONG,t.pos,"asm text too long");

		assert((p+1) <= f);
		*p++ = (unsigned char)buf.getb();
		rbuf_sfd_refill(buf,sfo);
	}

	while (p > data && is_whitespace(*(p-1))) p--;

	const size_t length = size_t(p-data);
	assert(length != 0);

	t.type = token_type_t::r___asm_text;
	t.v.identifier = identifier.alloc();
	if (!identifier(t.v.identifier).copy_from(data,length)) return errno_return(ENOMEM);
	return 1;
}

int lgtok_identifier(lgtok_state_t &lst,rbuf &buf,source_file_object &sfo,token_t &t) {
	(void)lst;

	unsigned char data[128];
	unsigned char *p = data;
	unsigned char *f = data + sizeof(data);
	bool pp = false;

	assert(t.type == token_type_t::none);

	assert(p < f);
	assert(is_identifier_first_char(buf.peekb()) || buf.peekb() == '#');
	rbuf_sfd_refill(buf,sfo);
	if (buf.peekb() == '#') {
		buf.discardb();
		pp=true;

		while (is_whitespace(buf.peekb()))
			buf.discardb();

		if (is_newline(buf.peekb()))
			CCERR_RET(EINVAL,t.pos,"Preprocessor directive with no directive");
	}
	*p++ = buf.getb();
	while (is_identifier_char(buf.peekb())) {
		if ((p+1) >= f)
			CCERR_RET(ENAMETOOLONG,t.pos,"Identifier name too long");

		assert((p+1) <= f);
		*p++ = (unsigned char)buf.getb();
		rbuf_sfd_refill(buf,sfo);
	}

	const size_t length = size_t(p-data);
	assert(length != 0);

	/* but wait: if the identifier is followed by a quotation mark, and it's some specific sequence,
	 * then it's actually a char/string literal i.e. L"blah" would be a wide char string. No space
	 * allowed. */
	if (buf.peekb() == '\'' || buf.peekb() == '\"') {
		if (length == 2 && !memcmp(data,"u8",2)) {
			return lgtok_charstrlit(buf,sfo,t,csliteral_t::type_t::UTF8);
		}
		else if (length == 1) {
			if (*((const char*)data) == 'U') {
				return lgtok_charstrlit(buf,sfo,t,csliteral_t::type_t::UNICODE32);
			}
			else if (*((const char*)data) == 'u') {
				return lgtok_charstrlit(buf,sfo,t,csliteral_t::type_t::UNICODE16);
			}
			else if (*((const char*)data) == 'L') {
				/* FIXME: A "wide" char varies between targets i.e. Windows wide char is 16 bits,
				 *        Linux wide char is 32 bits. */
				return lgtok_charstrlit(buf,sfo,t,csliteral_t::type_t::UNICODE32);
			}
		}
	}

	/* it might be a reserved keyword, check */
	if (pp) {
		for (const ident2token_t *i2t=ppident2tok;i2t < (ppident2tok+ppident2tok_length);i2t++) {
			if (length == i2t->len) {
				if (!memcmp(data,i2t->str,i2t->len)) {
					t.type = token_type_t(i2t->token);
					return 1;
				}
			}
		}
	}
	else {
		for (const ident2token_t *i2t=ident2tok_pp;i2t < (ident2tok_pp+ident2tok_pp_length);i2t++) {
			if (length == i2t->len) {
				if (!memcmp(data,i2t->str,i2t->len)) {
					t.type = token_type_t(i2t->token);
					return 1;
				}
			}
		}
	}

	t.type = pp ? token_type_t::ppidentifier : token_type_t::identifier;
	t.v.identifier = identifier.alloc();
	if (!identifier(t.v.identifier).copy_from(data,length)) return errno_return(ENOMEM);
	return 1;
}

int lgtok_number(rbuf &buf,source_file_object &sfo,token_t &t) {
	unsigned int base = 10;
	int r;

	/* detect numeric base from prefix */
	r = cc_parsedigit(buf.peekb());
	assert(r >= 0);
	if (r == 0) {
		const unsigned char c = (unsigned char)tolower((char)buf.peekb(1));
		if (c == 'x') { /* 0x<hex> */
			base = 16;
			buf.discardb(2);
		}
		else if (c == 'b') { /* 0b<binary> */
			base = 2;
			buf.discardb(2);
		}
		else {
			r = cc_parsedigit(c);
			if (r >= 0 && r <= 7) { /* 0<octal> */
				base = 8;
				buf.discardb();
			}
			else if (r >= 8) {
				/* uh, 0<digits> that isn't octal is invalid */
				CCERR_RET(EINVAL,buf.pos,"Invalid octal digit");
			}
			else {
				/* it's just zero and nothing more */
			}
		}
	}

	/* look ahead, refill buf, no constant should be 4096/2 = 2048 chars long!
	 * we need to know if this is an integer constant or floating point constant,
	 * which is only possible if decimal or hexadecimal. */
	rbuf_sfd_refill(buf,sfo);
	if (base == 10 || base == 16) {
		unsigned char *scan = buf.data;
		while (scan < buf.end && (*scan == '\'' || cc_parsedigit(*scan,base) >= 0)) scan++;
		if (scan < buf.end && (*scan == '.' || tolower((char)(*scan)) == 'e' || tolower((char)(*scan)) == 'f' || tolower((char)(*scan)) == 'd' || tolower((char)(*scan)) == 'p')) {
			/* It's a floating point constant */
			t.type = token_type_t::floating;
			t.v.floating.init();

			/* maybe after the additional digits there is the 'p' suffix */
			if (*scan == '.') {
				scan++;
				while (scan < buf.end && (*scan == '\'' || cc_parsedigit(*scan,base) >= 0)) scan++;
			}

			if (tolower((char)(*scan)) == 'p') {
				scan = buf.data;

				if (base != 16) CCERR_RET(ESRCH,buf.pos,"float 'p' constants must be hexadecimal");

				unsigned char ldigit = 0;
				unsigned char shf = 64;
				int v;

				/* the above code already skipped the "0x" */

				/* read hex digits, fill the mantissa from the top bits down 4 bits at a time.
				 * shf must be a multiple of 4 at this point of the code.
				 * while filling the whole part, track the exponent.
				 * do not track the exponent for the fractional part. */

				/* whole part */
				/* skip leading zeros */
				while (*scan == '0') scan++;
				if (*scan != '0') {
					t.v.floating.exponent--; /* whole part is nonzero, adjust so the first digit has exponent=3 */
					while ((v=cc_parsedigit(*scan,16)) >= 0) {
						scan++;
						if (shf >= 4) { /* assume shf a multiple of 4 */
							shf -= 4; t.v.floating.exponent += 4; t.v.floating.mantissa |= (uint64_t)v << (uint64_t)shf;
						}
						else {
							ldigit = (unsigned char)v;
							while ((v=cc_parsedigit(*scan,16)) >= 0) { t.v.floating.exponent += 4; scan++; }
							break;
						}
					}

					/* normalize mantissa */
					if (t.v.floating.mantissa != 0ull) {
						while (!(t.v.floating.mantissa & floating_value_t::mant_msb)) {
							t.v.floating.mantissa <<= 1ull;
							t.v.floating.exponent--;
							shf++;
						}
					}
				}
				/* fractional part */
				if (*scan == '.') {
					scan++;
					while ((v=cc_parsedigit(*scan,16)) >= 0) {
						scan++;
						if (shf >= 4) { /* assume shf a multiple of 4 */
							shf -= 4; t.v.floating.mantissa |= (uint64_t)v << (uint64_t)shf;
						}
						else {
							ldigit = (unsigned char)v;
							while ((v=cc_parsedigit(*scan,16)) >= 0) scan++;
							break;
						}
					}

					/* normalize mantissa */
					if (t.v.floating.mantissa != 0ull) {
						while (!(t.v.floating.mantissa & floating_value_t::mant_msb)) {
							t.v.floating.mantissa <<= 1ull;
							shf++;
						}
					}
				}
				/* was there a leftover digit that didn't fit, and normalization made some room for it? */
				if (shf && ldigit) {
					/* assume shf < 4 then (leftover bits to fill), ldigit is not set otherwise */
					t.v.floating.mantissa |= (uint64_t)ldigit >> (uint64_t)(4u - shf);
					shf = 0;
				}
				/* parse the 'p' power of 2 exponent */
				if (*scan == 'p') {
					bool neg = false;

					scan++;
					if (*scan == '-') {
						neg = true;
						scan++;
					}

					if (isdigit(*scan)) {
						unsigned int pex = strtoul((char*)scan,(char**)(&scan),10);
						if (neg) t.v.floating.exponent -= (int)pex;
						else t.v.floating.exponent += (int)pex;
					}
				}

				if (t.v.floating.mantissa == 0ull) {
					t.v.floating.flags |= floating_value_t::FL_ZERO;
					t.v.floating.exponent = 0;
				}
				else {
					t.v.floating.flags &= ~floating_value_t::FL_ZERO;
				}
			}
			else {
				scan = buf.data;

				if (base != 10) CCERR_RET(ESRCH,buf.pos,"float cannot be hexadecimal");

				/* use strtold directly on the buffer--this is why the rbuf code allocates in such a way that
				 * there is a NUL at the end of the buffer. someday we'll have our own code to do this. */
				assert(buf.end <= buf.fence); *buf.end = 0;

				/* assume strtold will never move scan to a point before the initial scan */
				const long double x = strtold((char*)scan,(char**)(&scan));
				assert(scan <= buf.end);

				/* convert to mantissa, exponent, assume positive number because our parsing method should never strtold() a negative number */
				int exp = 0; const long double m = frexpl(x,&exp); assert(m == 0.0 || (m >= 0.5 && m < 1.0)); /* <- as documented */
				const uint64_t mc = (uint64_t)ldexpl(m,64); /* i.e 0.5 => 0x8000'0000'0000'0000 */
				t.v.floating.setsn(mc,exp-64);
			}

			/* look for suffixes */
			if (scan < buf.end) {
				if (tolower(*scan) == 'f') {
					t.v.floating.type = floating_value_t::type_t::FLOAT;
					scan++;
				}
				else if (tolower(*scan) == 'd') {
					t.v.floating.type = floating_value_t::type_t::DOUBLE;
					scan++;
				}
				else if (tolower(*scan) == 'l') {
					t.v.floating.type = floating_value_t::type_t::LONGDOUBLE;
					scan++;
				}
			}

			buf.pos_track(buf.data,scan);
			buf.data = scan;
			return 1;
		}
	}

	t.type = token_type_t::integer;
	t.v.integer.init();

	const uint64_t sat_v = 0xFFFFFFFFFFFFFFFFull;
	const uint64_t max_v = sat_v / uint64_t(base);

	do {
		const unsigned char c = buf.peekb();
		if (c == '\'') { buf.discardb(); continue; } /* ' separators */
		r = cc_parsedigit(c,base); if (r < 0) break; buf.discardb();

		if (t.v.integer.v.u <= max_v) {
			t.v.integer.v.u *= uint64_t(base);
			t.v.integer.v.u += uint64_t(r);
		}
		else {
			t.v.integer.v.u = sat_v;
			t.v.integer.flags |= integer_value_t::FL_OVERFLOW;
			while (cc_parsedigit(buf.peekb(),base) >= 0) buf.discardb();
			break;
		}
	} while (1);

	do {
		unsigned char c = (unsigned char)tolower((char)buf.peekb());

		if (c == 'u') {
			t.v.integer.flags &= ~integer_value_t::FL_SIGNED;
			buf.discardb();
		}
		else if (c == 'l') {
			t.v.integer.type = integer_value_t::type_t::LONG;
			buf.discardb();

			c = (unsigned char)tolower((char)buf.peekb());
			if (c == 'l') {
				t.v.integer.type = integer_value_t::type_t::LONGLONG;
				buf.discardb();
			}
		}
		else {
			break;
		}
	} while(1);

	return 1;
}

/* NTS: This is the LOWEST LEVEL tokenizer.
 *      It is never expected to look ahead or buffer tokens.
 *      It is expected to always parse tokens from whatever is next in the buffer.
 *      The reason this matters is that some other layers like the preprocessor
 *      may need to call buf.peekb() to see if whitespace exists or not before
 *      parsing another token i.e. #define processing and any readahead or buffering
 *      here would prevent that.
 *
 *      Example: Apparently the difference between #define X <tokens> and #define X(paramlist) <tokens>
 *      is whether there is a space between the identifier "X" and the open parenthesis.
 *
 *      #define X (y,z)                         X (no parameters) expands to y,z
 *      #define X(x,y)                          X (parameters x, z) expands to nothing */
int lgtok(lgtok_state_t &lst,rbuf &buf,source_file_object &sfo,token_t &t) {
	int r;

try_again:
	t = token_t();
	t.set_source_file(buf.source_file);
	eat_whitespace(buf,sfo);
	t.pos = buf.pos;

	if (buf.data_avail() < 8) rbuf_sfd_refill(buf,sfo);
	if (buf.data_avail() == 0) {
		t.type = token_type_t::eof;
		if (buf.err) return buf.err;
		else return 0;
	}

	const unsigned int lst_was_flags = lst.flags & lgtok_state_t::FL_NEWLINE;
	lst.flags &= ~lst_was_flags;

	if (lst.flags & lgtok_state_t::FL_MSASM) {
		switch (buf.peekb()) {
			case '\r':
				buf.discardb();
				if (buf.peekb() != '\n') break;
				/* fall through */
			case '\n':
				buf.discardb();
				t.type = token_type_t::newline;
				lst.flags |= lgtok_state_t::FL_NEWLINE;
				if (lst.curlies == 0) {
					lst.flags &= ~lgtok_state_t::FL_MSASM;
					t.type = token_type_t::op_end_asm;
				}
				break;
			case '{':
				buf.discardb();
				lst.curlies++;
				goto try_again;
			case '}':
				buf.discardb();
				if (lst.curlies == 0)
					return errno_return(EINVAL);
				if (--lst.curlies == 0) {
					lst.flags &= ~lgtok_state_t::FL_MSASM;
					t.type = token_type_t::op_end_asm;
					return 1;
				}
				goto try_again;
			case '\'':
			case '\"':
				return lgtok_charstrlit(buf,sfo,t);
			default:
				return lgtok_asm_text(lst,buf,sfo,t);
		}
	}
	else {
		switch (buf.peekb()) {
			case '+':
				t.type = token_type_t::plus; buf.discardb();
				if (buf.peekb() == '+') { t.type = token_type_t::plusplus; buf.discardb(); } /* ++ */
				else if (buf.peekb() == '=') { t.type = token_type_t::plusequals; buf.discardb(); } /* += */
				break;
			case '-':
				t.type = token_type_t::minus; buf.discardb();
				if (buf.peekb() == '-') { t.type = token_type_t::minusminus; buf.discardb(); } /* -- */
				else if (buf.peekb() == '=') { t.type = token_type_t::minusequals; buf.discardb(); } /* -= */
				else if (buf.peekb() == '>') {
					t.type = token_type_t::minusrightanglebracket; buf.discardb(); /* -> */
					if (buf.peekb() == '*') { t.type = token_type_t::minusrightanglebracketstar; buf.discardb(); } /* ->* */
				}
				break;
			case '*':
				t.type = token_type_t::star; buf.discardb();
				if (buf.peekb() == '=') { t.type = token_type_t::starequals; buf.discardb(); } /* *= */
				break;
			case '/':
				t.type = token_type_t::forwardslash; buf.discardb();
				if (buf.peekb() == '=') { t.type = token_type_t::forwardslashequals; buf.discardb(); } /* /= */
				else if (buf.peekb() == '*') { buf.discardb(2); eat_c_comment(1,buf,sfo); goto try_again; }
				else if (buf.peekb() == '/') { buf.discardb(2); eat_cpp_comment(buf,sfo); goto try_again; }
				break;
			case '%':
				t.type = token_type_t::percent; buf.discardb();
				if (buf.peekb() == '=') { t.type = token_type_t::percentequals; buf.discardb(); } /* %= */
				break;
			case '!':
				t.type = token_type_t::exclamation; buf.discardb();
				if (buf.peekb() == '=') { t.type = token_type_t::exclamationequals; buf.discardb(); } /* != */
				break;
			case '=':
				t.type = token_type_t::equal; buf.discardb();
				if (buf.peekb() == '=') { t.type = token_type_t::equalequal; buf.discardb(); } /* == */
				break;
			case ';':
				t.type = token_type_t::semicolon; buf.discardb();
				break;
			case '~':
				t.type = token_type_t::tilde; buf.discardb();
				break;
			case '?':
				t.type = token_type_t::question; buf.discardb();
				break;
			case ':':
				t.type = token_type_t::colon; buf.discardb();
				if (buf.peekb() == ':') { t.type = token_type_t::coloncolon; buf.discardb(); } /* :: */
				break;
			case '^':
				t.type = token_type_t::caret; buf.discardb();
				if (buf.peekb() == '=') { t.type = token_type_t::caretequals; buf.discardb(); } /* ^= */
				break;
			case '&':
				t.type = token_type_t::ampersand; buf.discardb();
				if (buf.peekb() == '&') { t.type = token_type_t::ampersandampersand; buf.discardb(); } /* && */
				else if (buf.peekb() == '=') { t.type = token_type_t::ampersandequals; buf.discardb(); } /* &= */
				break;
			case '|':
				t.type = token_type_t::pipe; buf.discardb();
				if (buf.peekb() == '|') { t.type = token_type_t::pipepipe; buf.discardb(); } /* || */
				else if (buf.peekb() == '=') { t.type = token_type_t::pipeequals; buf.discardb(); } /* |= */
				break;
			case '.':
				t.type = token_type_t::period; buf.discardb();
				if (buf.peekb() == '*') { t.type = token_type_t::periodstar; buf.discardb(); } /* .* */
				else if (buf.peekb(0) == '.' && buf.peekb(1) == '.') { t.type = token_type_t::ellipsis; buf.discardb(2); } /* ... */
				break;
			case ',':
				t.type = token_type_t::comma; buf.discardb();
				break;
			case '[':
				t.type = token_type_t::opensquarebracket; buf.discardb();
				if (buf.peekb() == '[') { t.type = token_type_t::opensquarebracketopensquarebracket; buf.discardb(); } /* [[ */
				break;
			case ']':
				t.type = token_type_t::closesquarebracket; buf.discardb();
				if (buf.peekb() == ']') { t.type = token_type_t::closesquarebracketclosesquarebracket; buf.discardb(); } /* ]] */
				break;
			case '{':
				t.type = token_type_t::opencurlybracket; buf.discardb();
				break;
			case '}':
				t.type = token_type_t::closecurlybracket; buf.discardb();
				break;
			case '(':
				t.type = token_type_t::openparenthesis; buf.discardb();
				break;
			case ')':
				t.type = token_type_t::closeparenthesis; buf.discardb();
				break;
			case '<':
				if (lst.flags & lgtok_state_t::FL_ARROWSTR) {
					if (looks_like_arrowstr(buf,sfo))
						return lgtok_charstrlit(buf,sfo,t);
				}

				t.type = token_type_t::lessthan; buf.discardb();
				if (buf.peekb() == '<') {
					t.type = token_type_t::lessthanlessthan; buf.discardb(); /* << */
					if (buf.peekb() == '=') { t.type = token_type_t::lessthanlessthanequals; buf.discardb(); } /* <<= */
				}
				else if (buf.peekb() == '=') {
					t.type = token_type_t::lessthanequals; buf.discardb(); /* <= */
					if (buf.peekb() == '>') { t.type = token_type_t::lessthanequalsgreaterthan; buf.discardb(); } /* <=> */
				}
				break;
			case '>':
				t.type = token_type_t::greaterthan; buf.discardb();
				if (buf.peekb() == '>') {
					t.type = token_type_t::greaterthangreaterthan; buf.discardb(); /* >> */
					if (buf.peekb() == '=') { t.type = token_type_t::greaterthangreaterthanequals; buf.discardb(); } /* >>= */
				}
				else if (buf.peekb() == '=') {
					t.type = token_type_t::greaterthanequals; buf.discardb(); /* >= */
				}
				break;
			case '\'':
			case '\"':
				return lgtok_charstrlit(buf,sfo,t);
			case '\\':
				buf.discardb();
				t.type = token_type_t::backslash;
				if (is_newline(buf.peekb())) {
					buf.discardb();
					t.type = token_type_t::backslashnewline;
				}
				break;
			case '#':
				if (lst_was_flags & lgtok_state_t::FL_NEWLINE) {
					if ((r=lgtok_identifier(lst,buf,sfo,t)) < 1)
						return r;

					if (	t.type == token_type_t::r_ppinclude ||
							t.type == token_type_t::r_ppinclude_next ||
							t.type == token_type_t::r_ppembed) {
						lst.flags |= lgtok_state_t::FL_ARROWSTR;
					}

					return 1;
				}
				else {
					t.type = token_type_t::pound; buf.discardb();
					if (buf.peekb() == '#') { t.type = token_type_t::poundpound; buf.discardb(); } /* ## */
				}
				break;
			case '\r':
				buf.discardb();
				if (buf.peekb() != '\n') break;
				/* fall through */
			case '\n':
				buf.discardb();
				t.type = token_type_t::newline;
				lst.flags |= lgtok_state_t::FL_NEWLINE;
				if (lst.flags & lgtok_state_t::FL_ARROWSTR) {
					if (lst.curlies == 0)
						lst.flags &= ~lgtok_state_t::FL_ARROWSTR;
				}

				break;
			case '0': case '1': case '2': case '3': case '4':
			case '5': case '6': case '7': case '8': case '9':
				return lgtok_number(buf,sfo,t);
			default:
				if (is_identifier_first_char(buf.peekb())) {
					if ((r=lgtok_identifier(lst,buf,sfo,t)) < 1)
						return r;

					if (t.type == token_type_t::r___asm) {
						lst.flags |= lgtok_state_t::FL_MSASM;
						lst.curlies = 0;
					}
				}
				else {
					CCERR_RET(ESRCH,buf.pos,"Unexpected symbol at");
				}
				break;
		}
	}

	return 1;
}

