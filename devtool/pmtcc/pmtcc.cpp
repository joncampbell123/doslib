
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <math.h>

#include <exception>
#include <string>
#include <vector>
#include <memory>

using namespace std;

#ifndef O_BINARY
#define O_BINARY (0)
#endif

//////////////////// text position, to allow "syntax error at line X column Y"
struct textposition {
	unsigned int		column;
	unsigned int		row;

	textposition();
	void next_line(void);
	void reset(void);
	int procchar(int c);
};

textposition::textposition() {
	reset();
}

int textposition::procchar(int c) {
	if (c == '\n')
		next_line();
	else if (c > 0 && c != '\r')
		column++;

	return c;
}

void textposition::next_line(void) {
	row++;
	column = 1;
}

void textposition::reset(void) {
	column = row = 1;
}

////////////////////// source stream management

struct sourcestream {
	sourcestream();
	virtual ~sourcestream();
	virtual const char *getname(void);
	virtual void rewind(void);
	virtual void close(void);
	virtual int getc(void);

	bool eof = false;
};

const char *sourcestream::getname(void) {
	return "";
}

sourcestream::sourcestream() {
}

sourcestream::~sourcestream() {
}

void sourcestream::rewind(void) {
	eof = false;
}

void sourcestream::close(void) {
}

int sourcestream::getc(void) {
	eof = true;
	return -1;
}

typedef int sourcestream_ref;

///////////////////////// source parsing stack

struct sourcestack {
	struct entry {
		unique_ptr<sourcestream>	src;
		textposition			pos;
		int				savedc = -1;

		void				getpos(textposition &t);
		void				ungetc(int c);
		void				rewind(void);
		int				peekc(void);
		int				getc(void);
		bool				eof(void);
	};

	std::vector<entry>			stk; /* front() = bottom of stack  back() aka stk[stk.size()-1] = top of stack */

	void push(unique_ptr<sourcestream> &src/*will take ownership*/);
	entry &top(void);
	void pop(void);

	void _errifempty(void);
};

void sourcestack::entry::getpos(textposition &t) {
	t = pos;
	if (savedc && t.column > 1) t.column--;
}

void sourcestack::entry::rewind(void) {
	sourcestream *s;
	if ((s=src.get()) != NULL) s->rewind();
	savedc = -1;
}

void sourcestack::entry::ungetc(int c) {
	savedc = c;
}

int sourcestack::entry::peekc(void) {
	if (savedc < 0) {
		sourcestream *s;
		if ((s=src.get()) != NULL) savedc = pos.procchar(s->getc());
	}

	return savedc;
}

int sourcestack::entry::getc(void) {
	sourcestream *s;
	if (savedc >= 0) {
		const int c = savedc;
		savedc = -1;
		return c;
	}
	if ((s=src.get()) != NULL) return pos.procchar(s->getc());
	return -1;
}

bool sourcestack::entry::eof(void) {
	sourcestream *s;
	if (savedc >= 0) return false;
	if ((s=src.get()) != NULL) return s->eof;
	return true;
}

void sourcestack::push(unique_ptr<sourcestream> &src) {
	/* do not permit stack depth to exceed 128 */
	if (stk.size() >= 128u)
		throw std::runtime_error("source stack too deep");

	entry e;
	e.src = std::move(src); /* unique_ptr does not permit copy */
	e.pos.reset();
	stk.push_back(std::move(e));
}

void sourcestack::_errifempty(void) {
	if (stk.empty())
		throw std::runtime_error("source stack empty");
}

sourcestack::entry &sourcestack::top(void) {
	_errifempty();
	return stk.back();
}

void sourcestack::pop(void) {
	_errifempty();
	stk.pop_back();
}

/////////////////////// source stream provider

unique_ptr<sourcestream> source_stream_open_null(const char *path) {
	(void)path;
	return NULL;
}

// change this if compiling from something other than normal files such as memory or perhaps container formats
unique_ptr<sourcestream> (*source_stream_open)(const char *path) = source_stream_open_null;

//////////////////////// token from source

struct token {
	enum class tokid {
		None=0,				// 0
		Integer,
		Float,
		String,
		Typedef,
		Identifier,			// 5
		Eof,
		DivideOp,
		MultiplyOp,
		PowerOp,
		Comma,				// 10
		ModuloOp,
		AddGen,
		SubGen,
		Semicolon,
		IncrOp,				// 15
		DecrOp,
		EqualsOp,
		AssignOp,
		AddAssign,
		SubAssign,			// 20
		MulAssign,
		DivAssign,
		ModAssign,
		QuestionMark,
		Colon,				// 25
		ScopeResolution,
		BitwiseNot,
		LogicalNot,
		LogicalOr,
		BitwiseOrAssign,		// 30
		BitwiseOr,
		BitwiseXorAssign,
		BitwiseXor,
		LogicalAnd,
		BitwiseAndAssign,		// 35
		BitwiseAnd,
		NotEqualsOp,

		MAX
	};
	typedef uint8_t Char_t;
	typedef uint32_t Widechar_t;
	enum class stringtype {
		None=0,
		Char,
		Widechar,

		MAX
	};
	struct IntegerValue {
		union {
			uint64_t		u;
			int64_t			s;
		} v;
		unsigned int			datatype_size:6; // 0 = unspecified
		unsigned int			signbit:1;       // 0 = not signed
		unsigned int			unsignbit:1;     // 0 = not unsigned

		void reset(void);
	};
	struct FloatValue {
		long double			v;
		unsigned int			datatype_size:8;

		void reset(void);
	};
	struct StringValue {
		stringtype			st;
		// For pointer safety reasons string is not stored here! Is is in the blob field.

		void reset(void);
	};
	union {
		IntegerValue			iv;
		FloatValue			fv;
		StringValue			sv;
		size_t				typedef_id;
		size_t				identifier_id;
	} u;
	textposition pos;
	tokid token_id = tokid::None;
	std::vector<uint8_t> blob; /* WARNING: String stored as blob, StringValue stringtype says what type. Do not assume trailing NUL */
};

void token::StringValue::reset(void) {
	st = stringtype::None;
}

void token::FloatValue::reset(void) {
	datatype_size = 0;
	v = 0;
}

void token::IntegerValue::reset(void) {
	datatype_size = 0;
	unsignbit = 0;
	signbit = 0;
	v.u = 0;
}

//////////////////////// token parser

static int tokchar2int(int c) {
	if (c >= '0' && c <= '9')
		return c - '0';
	else if (c >= 'a' && c <= 'f')
		return c + 10 - 'a';
	else if (c >= 'A' && c <= 'F')
		return c + 10 - 'A';

	return -1;
}

void source_skip_whitespace(sourcestack::entry &s) {
	/* skip whitespace */
	while (isspace(s.peekc())) s.getc();
}

static constexpr uint32_t STRTOUL_SINGLE_QUOTE_MARKERS = uint32_t(1u) << uint32_t(0u);

unsigned int source_strtoul_iv(token::IntegerValue &iv,sourcestack::entry &s,const unsigned base,const uint32_t flags=0) {
	unsigned int count = 0;
	int digit;

	/* then remaining digits until we hit a non-digit.
	 * TODO: Overflow detection */
	do {
		/* The C++ standard allows long hex constants to have single quotes in the middle
		 * to help count digits, which are ignored by the compiler */
		if ((flags & STRTOUL_SINGLE_QUOTE_MARKERS) && s.peekc() == '\'') {
			s.getc();//discard
			continue;
		}
		digit = tokchar2int(s.peekc());
		if (digit >= 0 && (unsigned int)digit < base) {
			count++;
			s.getc();//discard
			iv.v.u *= (uint64_t)base;
			iv.v.u += (uint64_t)((unsigned)digit);
		}
		else {
			break;
		}
	} while (1);

	return count;
}

bool source_read_string_esc_char(uint64_t &pv,unsigned int &pvlen,sourcestack::entry &s) {
	int c;

	pv = 0;
	pvlen = 1;

	c = s.peekc();
	if (c < 32) return false;
	s.getc();//discard

	if (c == '\\') {
		c = s.getc();
		switch (c) {
			case '\'': pv = '\''; break;
			case '"':  pv = '"';  break;
			case '?':  pv = '?';  break;
			case '\\': pv = '\\'; break;
			case 'a':  pv = 0x07; break;
			case 'b':  pv = 0x08; break;
			case 'f':  pv = 0x0C; break;
			case 'n':  pv = 0x0A; break;
			case 'r':  pv = 0x0D; break;
			case 't':  pv = 0x09; break;
			case 'v':  pv = 0x0B; break;
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7': /* \nnn octal, maximum 3 digits */
				pv = tokchar2int(c);
				c = s.peekc(); if (c >= '0' && c <= '7') { pv <<= (uint64_t)3u; pv += (uint64_t)tokchar2int(c); s.getc();/*discard*/ };
				c = s.peekc(); if (c >= '0' && c <= '7') { pv <<= (uint64_t)3u; pv += (uint64_t)tokchar2int(c); s.getc();/*discard*/ };
				break;
			case 'x': /* \xnn hex */
				c = s.peekc(); if (isxdigit(c)) { pv <<= (uint64_t)4u; pv += (uint64_t)tokchar2int(c); s.getc();/*discard*/ };
				c = s.peekc(); if (isxdigit(c)) { pv <<= (uint64_t)4u; pv += (uint64_t)tokchar2int(c); s.getc();/*discard*/ };
				break;
			case 'u': /* \unnnn unicode */
				pvlen = 2;
				c = s.peekc(); if (isxdigit(c)) { pv <<= (uint64_t)4u; pv += (uint64_t)tokchar2int(c); s.getc();/*discard*/ };
				c = s.peekc(); if (isxdigit(c)) { pv <<= (uint64_t)4u; pv += (uint64_t)tokchar2int(c); s.getc();/*discard*/ };
				c = s.peekc(); if (isxdigit(c)) { pv <<= (uint64_t)4u; pv += (uint64_t)tokchar2int(c); s.getc();/*discard*/ };
				c = s.peekc(); if (isxdigit(c)) { pv <<= (uint64_t)4u; pv += (uint64_t)tokchar2int(c); s.getc();/*discard*/ };
				break;
			case 'U': /* \unnnnnnnn unicode */
				pvlen = 4;
				c = s.peekc(); if (isxdigit(c)) { pv <<= (uint64_t)4u; pv += (uint64_t)tokchar2int(c); s.getc();/*discard*/ };
				c = s.peekc(); if (isxdigit(c)) { pv <<= (uint64_t)4u; pv += (uint64_t)tokchar2int(c); s.getc();/*discard*/ };
				c = s.peekc(); if (isxdigit(c)) { pv <<= (uint64_t)4u; pv += (uint64_t)tokchar2int(c); s.getc();/*discard*/ };
				c = s.peekc(); if (isxdigit(c)) { pv <<= (uint64_t)4u; pv += (uint64_t)tokchar2int(c); s.getc();/*discard*/ };
				c = s.peekc(); if (isxdigit(c)) { pv <<= (uint64_t)4u; pv += (uint64_t)tokchar2int(c); s.getc();/*discard*/ };
				c = s.peekc(); if (isxdigit(c)) { pv <<= (uint64_t)4u; pv += (uint64_t)tokchar2int(c); s.getc();/*discard*/ };
				c = s.peekc(); if (isxdigit(c)) { pv <<= (uint64_t)4u; pv += (uint64_t)tokchar2int(c); s.getc();/*discard*/ };
				c = s.peekc(); if (isxdigit(c)) { pv <<= (uint64_t)4u; pv += (uint64_t)tokchar2int(c); s.getc();/*discard*/ };
				break;
			default:
				pvlen = 0;
				break;
		};
	}
	else {
		pv = (uint64_t)c;
	}

	return true;
}

bool source_toke(token &t,sourcestack::entry &s) {
	int c;

	t.token_id = token::tokid::None;

	source_skip_whitespace(s);
	s.getpos(t.pos);
	if (s.eof()) {
		t.token_id = token::tokid::Eof;
		return true;
	}

	c = s.peekc();
	if (c == '~') { /* ~ */
		t.token_id = token::tokid::BitwiseNot;
		s.getc();
	}
	else if (c == '!') { /* ! */
		s.getc();
		c = s.peekc();

		if (c == '=') { /* != */
			t.token_id = token::tokid::NotEqualsOp;
			s.getc();
		}
		else {
			t.token_id = token::tokid::LogicalNot;
		}
	}
	else if (c == '|') {
		s.getc();
		c = s.peekc();

		if (c == '|') { /* || */
			t.token_id = token::tokid::LogicalOr;
			s.getc();
		}
		else if (c == '=') { /* |= */
			t.token_id = token::tokid::BitwiseOrAssign;
			s.getc();
		}
		else { /* | */
			t.token_id = token::tokid::BitwiseOr;
		}
	}
	else if (c == '^') {
		s.getc();
		c = s.peekc();

		if (c == '=') { /* ^= */
			t.token_id = token::tokid::BitwiseXorAssign;
			s.getc();
		}
		else { /* ^ */
			t.token_id = token::tokid::BitwiseXor;
		}
	}
	else if (c == '&') {
		s.getc();
		c = s.peekc();

		if (c == '&') { /* && */
			t.token_id = token::tokid::LogicalAnd;
			s.getc();
		}
		else if (c == '=') { /* &= */
			t.token_id = token::tokid::BitwiseAndAssign;
			s.getc();
		}
		else { /* & */
			t.token_id = token::tokid::BitwiseAnd;
		}
	}
	else if (c == '?') {
		s.getc(); // discard

		// No, I am not going to support trigraphs

		t.token_id = token::tokid::QuestionMark;
	}
	else if (c == ':') {
		s.getc();
		c = s.peekc();

		if (c == ':') { /* :: */
			t.token_id = token::tokid::ScopeResolution;
			s.getc(); // discard
		}
		else {
			t.token_id = token::tokid::Colon;
		}
	}
	else if (c == '=') {
		s.getc(); // discard
		c = s.peekc();

		if (c == '=') { /* == operator */
			t.token_id = token::tokid::EqualsOp;
			s.getc(); // discard
		}
		else {
			t.token_id = token::tokid::AssignOp;
		}
	}
	else if (c == ';') {
		s.getc(); // discard

		t.token_id = token::tokid::Semicolon;
	}
	else if (c == '+') {
		s.getc(); // discard
		c = s.peekc();

		if (c == '+') { /* ++ operator */
			t.token_id = token::tokid::IncrOp;
			s.getc(); // discard
		}
		else if (c == '=') { /* += operator */
			t.token_id = token::tokid::AddAssign;
			s.getc(); // discard
		}
		else {
			t.token_id = token::tokid::AddGen; // could be positive sign or add
		}
	}
	else if (c == '-') {
		s.getc(); // discard
		c = s.peekc();

		if (c == '-') { /* -- operator */
			t.token_id = token::tokid::DecrOp;
			s.getc(); // discard
		}
		else if (c == '=') { /* -= operator */
			t.token_id = token::tokid::SubAssign;
			s.getc(); // discard
		}
		else {
			t.token_id = token::tokid::SubGen; // could be negate or subtract
		}
	}
	else if (c == ',') {
		s.getc(); // discard

		t.token_id = token::tokid::Comma;
	}
	else if (c == '%') {
		s.getc();
		c = s.peekc();

		if (c == '=') { /* -= operator */
			t.token_id = token::tokid::ModAssign;
			s.getc(); // discard
		}
		else {
			t.token_id = token::tokid::ModuloOp;
		}
	}
	/* divide operator or beginning of comment */
	else if (c == '/') {
		s.getc(); // discard
		c = s.peekc();

		if (c == '*') { /* / and then * to start comment */
			s.getc();//discard

			// scan and discard characters until * and then /
			do {
				c = s.getc();
				if (c == '*') {
					if (s.peekc() == '/') {
						s.getc();//discard
						break;
					}
				}
				else if (c < 0) {
					return false;
				}
			} while (1);

			/* then recursively call ourself to parse past the comment */
			return source_toke(t,s);
		}
		else if (c == '/') { /* / and then / until end of line */
			s.getc();//discard

			do {
				c = s.getc();
				if (c < 0 || c == '\n') break;
			} while (1);

			/* then recursively call ourself to parse past the comment */
			return source_toke(t,s);
		}
		else if (c == '=') { /* /= operator */
			t.token_id = token::tokid::DivAssign;
			s.getc(); // discard
		}
		else {
			t.token_id = token::tokid::DivideOp;
		}
	}
	else if (c == '*') {
		s.getc();
		c = s.peekc();

		if (c == '*') { /* ** operator */
			t.token_id = token::tokid::PowerOp;
			s.getc();
		}
		else if (c == '=') { /* *= operator */
			t.token_id = token::tokid::MulAssign;
			s.getc(); // discard
		}
		else {
			t.token_id = token::tokid::MultiplyOp;
		}
	}
	/* char constant, or even multi-char constant */
	else if (c == '\'') {
		token::IntegerValue iv;
		unsigned int pvlen;
		uint64_t pv;

		s.getc();//discard
		iv.reset();

		do {
			if (s.peekc() == '\'') {
				s.getc();//discard
				break;
			}
			else {
				if (!source_read_string_esc_char(pv,pvlen,s))
					return false;

				iv.v.u <<= (uint64_t)pvlen * (uint64_t)8u;
				iv.v.u += pv;
			}
		} while(1);

		t.token_id = token::tokid::Integer;
		t.u.iv = iv;
	}
	/* integer value (decimal)? If we find a decimal point, then it becomes a float. */
	else if (isdigit(c)) {
		token::IntegerValue iv;
		unsigned int base = 10; // decimal [0-9]+

		iv.reset(); // prepare integer
		if (c == '0') {
			base = 8; // octal 0[0-7]+
			s.getc(); // discard c
			c = s.peekc();
			if (c == 'x') {
				s.getc(); // discard
				base = 16; // hexadecimal 0x[0-9a-f]+
				c = s.peekc();
			}
			else if (c == 'b') {
				s.getc(); // discard
				base = 2; // binary 0b[01]+
				c = s.peekc();
			}
		}

		/* parse digits */
		source_strtoul_iv(iv,s,base,STRTOUL_SINGLE_QUOTE_MARKERS);

		/* if the number is just zero then it's decimal, not octal */
		if (base == 8 && iv.v.u == 0ull) base = 10;

		/* if we hit a decimal '.' then we just parsed the whole part of a float, valid
		 * only if we were parsing decimal or hexadecimal. */
		if (s.peekc() == '.' && (base == 10 || base == 16)) {
			unsigned int fivdigits=0;
			token::IntegerValue fiv;
			token::IntegerValue eiv;
			token::FloatValue fv;

			eiv.reset(); // prepare
			fiv.reset(); // prepare
			s.getc(); // discard

			/* it could be something like 27271.2127274727 or
			 * 0x1.a00fff1111111111p+3 */

			/* parse digits */
			fivdigits = source_strtoul_iv(fiv,s,base,STRTOUL_SINGLE_QUOTE_MARKERS);

			if (s.peekc() == 'p' || s.peekc() == 'e') {
				bool negative = false;

				// then parse the exponent, which is signed
				s.getc(); // discard
				eiv.signbit = 1;

				if (s.peekc() == '+') {
					negative = false;
					s.getc(); // discard
				}
				else if (s.peekc() == '-') {
					negative = true;
					s.getc(); // discard
				}

				/* parse digits */
				source_strtoul_iv(eiv,s,base);

				if (negative)
					eiv.v.s = -eiv.v.s;
			}

			if (base == 16)
				fv.v = ldexpl(iv.v.u,(int)eiv.v.s) + ldexpl(fiv.v.u,(int)eiv.v.s - ((int)fivdigits * 4));
			else
				fv.v = ldexpl((long double)iv.v.u + ((long double)powl(10,-((int)fivdigits)) * (long double)fiv.v.u),(int)eiv.v.s);

			t.token_id = token::tokid::Float;
			t.u.fv = fv;
		}
		else {
			t.token_id = token::tokid::Integer;
			t.u.iv = iv;
		}
	}
	else {
		return false;
	}

	return true;
}

//////////////////////// debug

void source_dbg_print_token(FILE *fp,token &t) {
	fprintf(fp,"{");
	fprintf(fp,"@%d,%d ",t.pos.row,t.pos.column);

	switch (t.token_id) {
		case token::tokid::Integer:
			if (t.u.iv.signbit)
				fprintf(fp,"sint=%lld",(signed long long)t.u.iv.v.s);
			else if (t.u.iv.unsignbit)
				fprintf(fp,"uint=%llu",(unsigned long long)t.u.iv.v.u);
			else
				fprintf(fp,"int=%llu",(unsigned long long)t.u.iv.v.u);
			break;
		case token::tokid::Float:
			fprintf(fp,"float=%.20Lf",t.u.fv.v);
			break;
		case token::tokid::Eof:
			fprintf(fp,"eof");
			break;
		case token::tokid::DivideOp:
			fprintf(fp,"div");
			break;
		case token::tokid::MultiplyOp:
			fprintf(fp,"mul");
			break;
		case token::tokid::PowerOp:
			fprintf(fp,"pow");
			break;
		case token::tokid::Comma:
			fprintf(fp,"comma");
			break;
		case token::tokid::ModuloOp:
			fprintf(fp,"mod");
			break;
		case token::tokid::AddGen:
			fprintf(fp,"addg");
			break;
		case token::tokid::SubGen:
			fprintf(fp,"subg");
			break;
		case token::tokid::Semicolon:
			fprintf(fp,"semcol");
			break;
		case token::tokid::IncrOp:
			fprintf(fp,"incr");
			break;
		case token::tokid::DecrOp:
			fprintf(fp,"decr");
			break;
		case token::tokid::EqualsOp:
			fprintf(fp,"equ");
			break;
		case token::tokid::AssignOp:
			fprintf(fp,"assign");
			break;
		case token::tokid::AddAssign:
			fprintf(fp,"addasn");
			break;
		case token::tokid::SubAssign:
			fprintf(fp,"subasn");
			break;
		case token::tokid::MulAssign:
			fprintf(fp,"mulasn");
			break;
		case token::tokid::DivAssign:
			fprintf(fp,"divasn");
			break;
		case token::tokid::ModAssign:
			fprintf(fp,"modasn");
			break;
		case token::tokid::QuestionMark:
			fprintf(fp,"quesmrk");
			break;
		case token::tokid::Colon:
			fprintf(fp,"colon");
			break;
		case token::tokid::ScopeResolution:
			fprintf(fp,"scope");
			break;
		case token::tokid::BitwiseNot:
			fprintf(fp,"bitnot");
			break;
		case token::tokid::LogicalNot:
			fprintf(fp,"lognot");
			break;
		case token::tokid::LogicalOr:
			fprintf(fp,"logor");
			break;
		case token::tokid::BitwiseOrAssign:
			fprintf(fp,"bitorasn");
			break;
		case token::tokid::BitwiseOr:
			fprintf(fp,"bitor");
			break;
		case token::tokid::BitwiseXorAssign:
			fprintf(fp,"bitxorasn");
			break;
		case token::tokid::BitwiseXor:
			fprintf(fp,"bitxor");
			break;
		case token::tokid::LogicalAnd:
			fprintf(fp,"logand");
			break;
		case token::tokid::BitwiseAndAssign:
			fprintf(fp,"bitandasn");
			break;
		case token::tokid::BitwiseAnd:
			fprintf(fp,"bitand");
			break;
		case token::tokid::NotEqualsOp:
			fprintf(fp,"notequ");
			break;
		default:
			fprintf(fp,"?");
			break;
	};

	fprintf(fp,"} ");
}

//////////////////////// source file stream provider

struct sourcestream_file : public sourcestream {
	sourcestream_file(int fd,const char *path);
	virtual ~sourcestream_file();
	virtual const char *getname(void);
	virtual void rewind(void);
	virtual void close(void);
	virtual int getc(void);
private:
	std::string		file_path;
	int			file_fd = -1;
};

sourcestream_file::sourcestream_file(int fd,const char *path) : sourcestream() {
	file_path = path;
	file_fd = fd;
}

sourcestream_file::~sourcestream_file() {
	close();
}

const char *sourcestream_file::getname(void) {
	return file_path.c_str();
}

void sourcestream_file::rewind(void) {
	sourcestream::rewind();
	if (file_fd >= 0) lseek(file_fd,0,SEEK_SET);
}

void sourcestream_file::close(void) {
	if (file_fd >= 0) {
		::close(file_fd);
		file_fd = -1;
	}
	sourcestream::close();
}

int sourcestream_file::getc(void) {
	if (file_fd >= 0) {
		unsigned char c;
		if (read(file_fd,&c,1) == 1) return (int)c;
	}

	eof = true;
	return -1;
}

unique_ptr<sourcestream> source_stream_open_file(const char *path) {
	int fd = open(path,O_RDONLY|O_BINARY);
	if (fd < 0) return NULL;
	return unique_ptr<sourcestream>(reinterpret_cast<sourcestream*>(new sourcestream_file(fd,path)));
}

///////////////////////////////////////

struct options_state {
	std::vector<std::string>		source_files;
};

static options_state				options;

static void help(void) {
	fprintf(stderr,"pmtcc [options] [source file ...]\n");
	fprintf(stderr," -h --help                         help\n");
}

static int parse_argv(options_state &ost,int argc,char **argv) {
	char *a;
	int i=1;

	while (i < argc) {
		a = argv[i++];
		if (*a == '-') {
			do { a++; } while (*a == '-');

			if (!strcmp(a,"h") || !strcmp(a,"help")) {
				help();
				return 1;
			}
			else {
				fprintf(stderr,"Unknown switch %s\n",a);
				return 1;
			}
		}
		else {
			ost.source_files.push_back(a);
		}
	}

	return 0;
}

int main(int argc,char **argv) {
	if (parse_argv(/*&*/options,argc,argv))
		return 1;

	/* open from files */
	source_stream_open = source_stream_open_file;

	/* parse each file one by one */
	for (auto si=options.source_files.begin();si!=options.source_files.end();si++) {
		sourcestack srcstack;

		unique_ptr<sourcestream> sfile = source_stream_open((*si).c_str());
		if (sfile.get() == NULL) {
			fprintf(stderr,"Unable to open source file '%s'\n",(*si).c_str());
			return 1;
		}
		srcstack.push(sfile);

		while (!srcstack.top().eof()) {
			token t;

			if (!source_toke(t,srcstack.top())) {
				fprintf(stderr,"Parse fail\n");
				return 1;
			}

			source_dbg_print_token(stderr,t);
		}
		fprintf(stderr,"\n");
	}

	return 0;
}

