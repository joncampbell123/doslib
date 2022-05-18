
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <math.h>

#include <exception>
#include <cassert>
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

// TODO: We return int, this should eventually read as UTF-8 or UTF-16.
//       Let the base class read raw bytes.
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

// string constant info for runtime
unsigned char string_wchar_size = 4; // 32-bit: Linux, Mac OS X, etc.  16-bit: Windows NT/9x/ME

//////////////////////// string table
struct stringtblent {
	enum class type_t {
		None=0,
		Char,		// std::string<char>
		Utf8,		// std::string<char>
		Char16,		// std::string<int16_t>
		Char32,		// std::string<int32_t>

		MAX
	};

	static constexpr size_t no_string = (size_t)(~0ull);

	type_t		type = type_t::None;
	void*		strobj = NULL;

	void clear(void);
	void set(const char *s,const type_t Type=type_t::Char);
	void set(const std::basic_string<char> &s,const type_t Type=type_t::Char); // aka std::string
	void set(const std::basic_string<int16_t> &s);
	void set(const std::basic_string<int32_t> &s);

	std::string converttoutf(void) const;

	const char *cstring(void) const;
	const int16_t *c16string(void) const;
	const int32_t *c32string(void) const;

	stringtblent();
	stringtblent(const stringtblent &x) = delete;
	stringtblent(stringtblent &&x);
	~stringtblent();
	stringtblent &operator=(const stringtblent &x) = delete;
	stringtblent &operator=(stringtblent &&x);

private:
	void _movefrom(stringtblent &x);
};

std::string stringtblent::converttoutf(void) const {
	std::string r;

	switch (type) {
		case type_t::Char:
		case type_t::Utf8:
			return cstring();
		case type_t::Char16:
			{
				const int16_t *s = c16string();
				while (*s != 0) {
					if (*s >= 0 && *s <= 0xFF)
						r += (char)(*s);

					s++;
				}
			}
			break;
		case type_t::Char32:
			{
				const int32_t *s = c32string();
				while (*s != 0) {
					if (*s >= 0 && *s <= 0xFF)
						r += (char)(*s);

					s++;
				}
			}
			break;
		default:
			break;
	}

	return r;
}

const char *stringtblent::cstring(void) const {
	if (strobj != NULL) {
		if (type == type_t::Char || type == type_t::Utf8)
			return (reinterpret_cast< std::basic_string<char>* >(strobj))->c_str();
	}

	return NULL;
}

const int16_t *stringtblent::c16string(void) const {
	if (strobj != NULL) {
		if (type == type_t::Char16)
			return (reinterpret_cast< std::basic_string<int16_t>* >(strobj))->c_str();
	}

	return NULL;
}

const int32_t *stringtblent::c32string(void) const {
	if (strobj != NULL) {
		if (type == type_t::Char32)
			return (reinterpret_cast< std::basic_string<int32_t>* >(strobj))->c_str();
	}

	return NULL;
}

void stringtblent::_movefrom(stringtblent &x) {
	clear();

	type = x.type;
	x.type = type_t::None;

	strobj = x.strobj;
	x.strobj = NULL;
}

stringtblent::stringtblent(stringtblent &&x) {
	_movefrom(x);
}

stringtblent &stringtblent::operator=(stringtblent &&x) {
	_movefrom(x);
	return *this;
}

void stringtblent::clear(void) {
	if (strobj) {
		switch (type) {
			case type_t::Char:
			case type_t::Utf8:
				delete reinterpret_cast< std::basic_string<char>* >(strobj); break;
			case type_t::Char16:
				delete reinterpret_cast< std::basic_string<int16_t>* >(strobj); break;
			case type_t::Char32:
				delete reinterpret_cast< std::basic_string<int32_t>* >(strobj); break;
			default:
				break;
		};

		strobj = NULL;
	}
}

void stringtblent::set(const char *s,const type_t Type) {
	clear();
	assert(strobj == NULL);
	if (s != NULL && (Type == type_t::Char || Type == type_t::Utf8)) {
		std::basic_string<char>* n = new std::basic_string<char>(s);
		strobj = (void*)n;
		type = Type;
	}
}

void stringtblent::set(const std::basic_string<char> &s,const type_t Type) {
	clear();
	assert(strobj == NULL);
	if (Type == type_t::Char || Type == type_t::Utf8) {
		std::basic_string<char>* n = new std::basic_string<char>(s);
		strobj = (void*)n;
		type = Type;
	}
}

void stringtblent::set(const std::basic_string<int16_t> &s) {
	clear();
	assert(strobj == NULL);
	{
		std::basic_string<int16_t>* n = new std::basic_string<int16_t>(s);
		type = type_t::Char16;
		strobj = (void*)n;
	}
}

void stringtblent::set(const std::basic_string<int32_t> &s) {
	clear();
	assert(strobj == NULL);
	{
		std::basic_string<int32_t>* n = new std::basic_string<int32_t>(s);
		type = type_t::Char32;
		strobj = (void*)n;
	}
}

stringtblent::stringtblent() {
}

stringtblent::~stringtblent() {
	clear();
}

std::vector<stringtblent>		src_stringtbl;

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
		RightShiftAssign,
		RightShift,
		LeftShiftAssign,		// 40
		LeftShift,
		LessThanOp,
		LessOrEquOp,
		GreaterThanOp,
		GreaterOrEquOp,			// 45
		StarshipOp,
		PtrToMemDotOp,
		PtrToMemArrowOp,
		MemberDotOp,
		MemberArrowOp,			// 50
		OpenParen,
		CloseParen,
		OpenSqBrk,
		CloseSqBrk,
		OpenCurlBrk,			// 55
		CloseCurlBrk,

		MAX
	};
	typedef uint8_t Char_t;
	typedef uint32_t Widechar_t;
	struct IntegerValue {
		union {
			uint64_t		u;
			int64_t			s;
		} v;
		unsigned int			datatype_size:12;// 0 = unspecified
		unsigned int			signbit:1;       // 0 = not signed
		unsigned int			unsignbit:1;     // 0 = not unsigned
		unsigned int			longbit:1;       // 1 = long value
		unsigned int			longlongbit:1;   // 1 = long long value

		void reset(void);
	};
	struct FloatValue {
		long double			v;
		unsigned int			datatype_size:14;
		unsigned int			floatbit:1;      // 1 = float instead of double
		unsigned int			longbit:1;       // 1 = long double

		void reset(void);
	};
	struct StringValue {
		size_t				stblidx; // index into string table or stringtblent::no_string

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
	std::string identifier;
};

void token::StringValue::reset(void) {
	stblidx = stringtblent::no_string;
}

void token::FloatValue::reset(void) {
	datatype_size = 0;
	floatbit = 0;
	longbit = 0;
	v = 0;
}

void token::IntegerValue::reset(void) {
	longbit = 0;
	datatype_size = 0;
	longlongbit = 0;
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

void source_read_const_int_suffixes(token::IntegerValue &iv,sourcestack::entry &s) {
	int c;

	c = tolower(s.peekc());
	if (c == 'u') { /* u */
		s.getc(); // discard
		iv.signbit = 0;
		iv.unsignbit = 1;
		c = tolower(s.peekc());
		if (c == 'l') { /* ul */
			s.getc();
			iv.longbit = 1;
			c = tolower(s.peekc());
			if (c == 'l') { /* ull */
				s.getc();
				iv.longlongbit = 1;
			}
		}
	}
	else if (c == 'l') { /* l */
		s.getc(); // discard
		iv.signbit = 1;
		iv.unsignbit = 0;
		iv.longbit = 1;
		c = tolower(s.peekc());
		if (c == 'l') { /* ll */
			s.getc();
			iv.longlongbit = 1;
		}
	}
}

void source_read_const_float_suffixes(token::FloatValue &fv,sourcestack::entry &s) {
	int c;

	c = tolower(s.peekc());
	if (c == 'f') {
		s.getc();
		fv.floatbit = 1;
	}
	else if (c == 'l') {
		s.getc();
		fv.longbit = 1;
	}
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
	std::string identifier;
	int c;

	t.token_id = token::tokid::None;

	source_skip_whitespace(s);
	s.getpos(t.pos);
	if (s.eof()) {
		t.token_id = token::tokid::Eof;
		return true;
	}

	c = s.peekc();

	/* identifier parsing, but the identifier followed by specific chars could mean
	 * specific prefixes instead */
	if (isalpha(c) || c == '_') { /* identifiers can start with [a-z_] */
		bool isIdent = true;

		identifier = c;
		s.getc();

		/* the identifier continues until the char is not [a-z0-9_] */
		do {
			c = s.peekc();
			if (isalpha(c) || isdigit(c) || c == '_') {
				identifier += c;
				s.getc();
			}
			else {
				break;
			}
		} while (1);

		/* if specific identifiers and the next char is specific, then it's a prefix.
		 * The C/C++ standard doesn't allow a space between u8 and "string" in u8"string" so neither do we. */
		c = s.peekc();
		if (c == '\"' || c == '\''/*next char begins char literal or string*/) {
			if (identifier == "u" || identifier == "U" || identifier == "u8" || identifier == "L") {
				isIdent = false; /* c contains next char ready for token parsing below */
			}
		}

		if (isIdent) {
			t.token_id = token::tokid::Identifier;
			t.identifier = identifier;
			return true;
		}
	}

	if (c == '{') {
		t.token_id = token::tokid::OpenCurlBrk;
		s.getc();
	}
	else if (c == '}') {
		t.token_id = token::tokid::CloseCurlBrk;
		s.getc();
	}
	else if (c == '[') {
		t.token_id = token::tokid::OpenSqBrk;
		s.getc();
	}
	else if (c == ']') {
		t.token_id = token::tokid::CloseSqBrk;
		s.getc();
	}
	else if (c == '(') {
		t.token_id = token::tokid::OpenParen;
		s.getc();
	}
	else if (c == ')') {
		t.token_id = token::tokid::CloseParen;
		s.getc();
	}
	else if (c == '.') {
		s.getc();
		c = s.peekc();

		if (c == '*') { /* .* */
			t.token_id = token::tokid::PtrToMemDotOp;
			s.getc();
		}
		else { /* . */
			t.token_id = token::tokid::MemberDotOp;
		}
	}
	else if (c == '<') {
		s.getc();
		c = s.peekc();

		if (c == '<') { /* << */
			s.getc();
			c = s.peekc();
			if (c == '=') { /* <<= */
				s.getc();
				t.token_id = token::tokid::LeftShiftAssign;
			}
			else { /* << */
				t.token_id = token::tokid::LeftShift;
			}
		}
		else if (c == '=') { /* <= */
			s.getc();
			c = s.peekc();
			if (c == '>') { /* <=> */
				s.getc();
				t.token_id = token::tokid::StarshipOp;
			}
			else { /* <= */
				t.token_id = token::tokid::LessOrEquOp;
			}
		}
		else { /* < */
			t.token_id = token::tokid::LessThanOp;
		}
	}
	else if (c == '>') {
		s.getc();
		c = s.peekc();

		if (c == '>') { /* >> */
			s.getc();
			c = s.peekc();
			if (c == '=') { /* >>= */
				s.getc();
				t.token_id = token::tokid::RightShiftAssign;
			}
			else { /* >> */
				t.token_id = token::tokid::RightShift;
			}
		}
		else if (c == '=') { /* >= */
			t.token_id = token::tokid::GreaterOrEquOp;
			s.getc();
		}
		else { /* > */
			t.token_id = token::tokid::GreaterThanOp;
		}
	}
	else if (c == '~') { /* ~ */
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
		else if (c == '>') { /* -> operator */
			s.getc(); // discard
			c = s.peekc();
			if (c == '*') { /* ->* operator */
				t.token_id = token::tokid::PtrToMemArrowOp;
				s.getc(); // discard
			}
			else { /* -> operator */
				t.token_id = token::tokid::MemberArrowOp;
			}
		}
		else { /* - operator */
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

				if (identifier == "u") pvlen = 2; /* char16_t */
				else if (identifier == "U") pvlen = 4; /* char32_t */
				else if (identifier == "L") pvlen = string_wchar_size; /* wchar_t */
				// TODO: identifier == "u8", pvlen depends on the length of the char when encoded to UTF-8

				iv.v.u <<= (uint64_t)pvlen * (uint64_t)8u;
				iv.v.u += pv;
			}
		} while(1);

		source_read_const_int_suffixes(iv,s);
		t.token_id = token::tokid::Integer;
		t.u.iv = iv;
	}
	/* string constant */
	else if (c == '\"') {
		const size_t idx = src_stringtbl.size();
		unsigned int pvlen = 1;
		stringtblent ste;
		uint64_t pv;

		t.token_id = token::tokid::String;
		t.u.sv.stblidx = idx;

		s.getc();//discard

		if (identifier == "L")
			pvlen = string_wchar_size;
		else if (identifier == "U")
			pvlen = 4;
		else if (identifier == "u")
			pvlen = 2;

		// TODO: UTF-8 u8

		if (pvlen == 4) {
			std::basic_string<int32_t> str;

			do {
				if (s.peekc() == '\"') {
					s.getc();//discard
					break;
				}
				else {
					if (!source_read_string_esc_char(pv,pvlen,s))
						return false;

					if (pv > (uint64_t)0xFFFFFFFFu)
						return false;

					str += (int32_t)pv;
				}
			} while(1);
			ste.set(str);
		}
		else if (pvlen == 2) {
			std::basic_string<int16_t> str;

			do {
				if (s.peekc() == '\"') {
					s.getc();//discard
					break;
				}
				else {
					if (!source_read_string_esc_char(pv,pvlen,s))
						return false;

					if (pv > (uint64_t)0xFFFFu)
						return false;

					str += (int16_t)pv;
				}
			} while(1);
			ste.set(str);
		}
		else {
			std::basic_string<char> str;
			stringtblent::type_t type = stringtblent::type_t::Char;

			if (identifier == "u8")
				type = stringtblent::type_t::Utf8;

			do {
				if (s.peekc() == '\"') {
					s.getc();//discard
					break;
				}
				else {
					if (!source_read_string_esc_char(pv,pvlen,s))
						return false;

					/* TODO: UTF-8 */
					if (pv > (uint64_t)0xFFu)
						return false;

					str += (char)pv;
				}
			} while(1);
			ste.set(str,type);
		}

		src_stringtbl.push_back(std::move(ste));
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
			fv.reset(); // prepare
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

			source_read_const_float_suffixes(fv,s);
			t.token_id = token::tokid::Float;
			t.u.fv = fv;
		}
		else {
			source_read_const_int_suffixes(iv,s);
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
		case token::tokid::String:
			fprintf(fp,"str");
			if (t.u.sv.stblidx != stringtblent::no_string && t.u.sv.stblidx < src_stringtbl.size()) {
				const stringtblent &ste = src_stringtbl[t.u.sv.stblidx];

				switch (ste.type) {
					case stringtblent::type_t::Char:
						fprintf(stderr,"c=\"%s\"",ste.cstring());
						break;
					case stringtblent::type_t::Utf8:
						fprintf(stderr,"u8=\"%s\"",ste.cstring());
						break;
					case stringtblent::type_t::Char16:
						fprintf(stderr,"u16=\"%s\"",ste.converttoutf().c_str());
						break;
					case stringtblent::type_t::Char32:
						fprintf(stderr,"u32=\"%s\"",ste.converttoutf().c_str());
						break;
					default:
						break;
				};
			}
			else {
				fprintf(stderr,"none");
			}
			break;
		case token::tokid::Identifier:
			fprintf(fp,"ident='%s'",t.identifier.c_str());
			break;
		case token::tokid::Integer:
			fprintf(fp,"int=%llu",(unsigned long long)t.u.iv.v.u);
			if (t.u.iv.unsignbit)
				fprintf(fp,"U");
			if (t.u.iv.longbit) {
				fprintf(fp,"L");
				if (t.u.iv.longlongbit) fprintf(fp,"L");
			}
			break;
		case token::tokid::Float:
			fprintf(fp,"float=%.20Lf",t.u.fv.v);
			if (t.u.fv.floatbit)
				fprintf(fp,"F");
			else if (t.u.fv.longbit)
				fprintf(fp,"L");
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
		case token::tokid::RightShiftAssign:
			fprintf(fp,"rshfasn");
			break;
		case token::tokid::RightShift:
			fprintf(fp,"rshf");
			break;
		case token::tokid::LeftShiftAssign:
			fprintf(fp,"lshfasn");
			break;
		case token::tokid::LeftShift:
			fprintf(fp,"lshf");
			break;
		case token::tokid::LessThanOp:
			fprintf(fp,"lt");
			break;
		case token::tokid::LessOrEquOp:
			fprintf(fp,"ltoe");
			break;
		case token::tokid::GreaterThanOp:
			fprintf(fp,"gt");
			break;
		case token::tokid::GreaterOrEquOp:
			fprintf(fp,"gtoe");
			break;
		case token::tokid::StarshipOp:
			fprintf(fp,"starship");
			break;
		case token::tokid::PtrToMemDotOp:
			fprintf(fp,"ptmdot");
			break;
		case token::tokid::PtrToMemArrowOp:
			fprintf(fp,"ptmarr");
			break;
		case token::tokid::MemberDotOp:
			fprintf(fp,"mbrdot");
			break;
		case token::tokid::MemberArrowOp:
			fprintf(fp,"mbrarr");
			break;
		case token::tokid::OpenParen:
			fprintf(fp,"opar");
			break;
		case token::tokid::CloseParen:
			fprintf(fp,"clopar");
			break;
		case token::tokid::OpenSqBrk:
			fprintf(fp,"osqbrk");
			break;
		case token::tokid::CloseSqBrk:
			fprintf(fp,"closqbrk");
			break;
		case token::tokid::OpenCurlBrk:
			fprintf(fp,"ocrlbrk");
			break;
		case token::tokid::CloseCurlBrk:
			fprintf(fp,"clocrlbrk");
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

