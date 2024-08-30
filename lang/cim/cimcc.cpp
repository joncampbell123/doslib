
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <math.h>

#include <unordered_map>
#include <type_traits>
#include <stdexcept>
#include <vector>
#include <string>
#include <limits>
#include <new>

#ifndef O_BINARY
# define O_BINARY 0
#endif

namespace CIMCC/*TODO: Pick a different name by final release*/ {

	static constexpr int errno_return(int e) {
		return (e >= 0) ? (-e) : (e); /* whether errno values are positive or negative, return as negative */
	}

	//////////////////////////////////////////////////////////////////

#define CIMCC_SOURCE_OBJ_NO_COPY_ONLY_MOVE(sfclass) \
	/* no copy */ \
	sfclass(const sfclass &) = delete; \
	virtual sfclass &operator=(const sfclass &) = delete; \
	/* move allowed */ \
	sfclass(sfclass &&x) { common_move(x); } \
	virtual sfclass &operator=(sfclass &&x) { common_move(x); return *this; }

	struct source_file_object {
		enum {
			IF_BASE=0,
			IF_FD=1
		};

		unsigned int			iface;

		virtual const char*		getname(void);
		virtual ssize_t			read(void *,size_t);
		virtual void			close(void);

						source_file_object(const unsigned int iface=IF_BASE);
		virtual				~source_file_object();

						CIMCC_SOURCE_OBJ_NO_COPY_ONLY_MOVE(source_file_object);
		void				common_move(source_file_object &);
	};

	////////////////////////////////////////////////////////////

	source_file_object::source_file_object(const unsigned int new_iface) : iface(new_iface) {
	}

	source_file_object::~source_file_object() {
	}

	const char* source_file_object::getname(void) {
		return "(null)";
	}

	ssize_t source_file_object::read(void *,size_t) {
		return ssize_t(errno_return(ENOSYS));
	}

	void source_file_object::close(void) {
	}

	void source_file_object::common_move(source_file_object &) {
	}

	//////////////////////////////////////////////////////////////

	struct source_fd : public source_file_object {
		virtual const char*		getname(void);
		virtual ssize_t			read(void *buffer,size_t count);
		virtual void			close(void);

						source_fd();
						source_fd(const int new_fd/*takes ownership*/,const std::string &new_name);
		virtual				~source_fd();

						CIMCC_SOURCE_OBJ_NO_COPY_ONLY_MOVE(source_fd);
		void				common_move(source_fd &x);

		std::string			name;
		int				fd = -1;
	};

	////////////////////////////////////////////////////////////////

	source_fd::source_fd() {
	}

	source_fd::source_fd(const int new_fd/*takes ownership*/,const std::string &new_name) : source_file_object(IF_FD), name(new_name), fd(new_fd) {
	}

	source_fd::~source_fd() {
		close();
	}

	void source_fd::common_move(source_fd &x) {
		name = std::move(x.name);
		fd = x.fd; x.fd = -1;
	}

	const char* source_fd::getname(void) {
		return name.c_str();
	}

	ssize_t source_fd::read(void *buffer,size_t count) {
		if (fd >= 0) {
			const int r = ::read(fd,buffer,count);
			if (r >= 0) {
				assert(size_t(r) <= count);
				return r;
			}
			else {
				return ssize_t(errno_return(r));
			}
		}

		return ssize_t(errno_return(EBADF));
	}

	void source_fd::close(void) {
		if (fd >= 0) {
			::close(fd);
			fd = -1;
		}
	}

	////////////////////////////////////////////////////////////////////

	struct rbuf {
		unsigned char*			base = NULL;
		unsigned char*			data = NULL;
		unsigned char*			end = NULL;
		unsigned char*			fence = NULL;
		int				err = 0;
		bool				eof = false;

		rbuf() { }
		~rbuf() { free(); }

		rbuf(const rbuf &x) = delete;
		rbuf &operator=(const rbuf &x) = delete;

		rbuf(rbuf &&x) { common_move(x); }
		rbuf &operator=(rbuf &&x) { common_move(x); return *this; }

		void common_move(rbuf &x) {
			assert(x.sanity_check());
			base  = x.base;  x.base = NULL;
			data  = x.data;  x.data = NULL;
			end   = x.end;   x.end = NULL;
			fence = x.fence; x.fence = NULL;
			err   = x.err;   x.err = 0;
			eof   = x.eof;   x.eof = false;
			assert(sanity_check());
		}

		size_t buffer_size(void) const {
			return size_t(fence - base);
		}

		size_t data_offset(void) const {
			return size_t(data - base);
		}

		size_t data_avail(void) const {
			return size_t(end - data);
		}

		size_t can_write(void) const {
			return size_t(fence - end);
		}

		bool sanity_check(void) const {
			return (base <= data) && (data <= end) && (end <= fence);
		}

		void free(void) {
			if (base) {
				::free((void*)base);
				base = data = end = fence = NULL;
			}
		}

		unsigned char peekb(const size_t ofs=0) {
			if ((data+ofs) < end) return data[ofs];
			return 0;
		}

		void discardb(const size_t count=1) {
			if (data < end) {
				if ((data += count) > end)
					data = end;
			}
		}

		unsigned char getb(void) {
			if (data < end) return *data++;
			return 0;
		}

		bool allocate(const size_t sz=4096) {
			if (base != NULL || sz < 64 || sz > 1024*1024)
				return false;

			if ((base=data=end=(unsigned char*)::malloc(sz)) == NULL)
				return false;

			fence = base + sz;
			return true;
		}

		void flush(void) {
			if (data_offset() != 0) {
				const size_t mv = data_avail();

				assert(sanity_check());
				if (mv != 0) memmove(base,data,mv);
				data = base;
				end = data + mv;
				assert(sanity_check());
			}
		}

		void lazy_flush(void) {
			if (data_offset() >= (buffer_size()/2))
				flush();
		}
	};

	static int rbuf_sfd_refill(rbuf &buf,source_file_object &sfo) {
		assert(buf.base != NULL);
		assert(buf.sanity_check());
		buf.lazy_flush();

		const size_t to_rd = buf.can_write();
		if (to_rd != size_t(0)) {
			const ssize_t rd = sfo.read(buf.end,to_rd);
			if (rd > 0) {
				buf.end += rd;
				assert(buf.sanity_check());
			}
			else if (rd < 0) {
				return (buf.err=errno_return(rd));
			}
			else {
				buf.eof = true;
				return 0;
			}
		}
		else if (buf.err) {
			return buf.err;
		}
		else if (buf.eof) {
			return 0;
		}

		return 1;
	}

	typedef int32_t unicode_char_t;
	static constexpr unicode_char_t unicode_eof = unicode_char_t(-1l);
	static constexpr unicode_char_t unicode_invalid = unicode_char_t(-2l);

	void utf8_to_str(char* &w,char *f,unicode_char_t c) {
		if (c < unicode_char_t(0)) {
			/* do nothing */
		}
		else if (c <= unicode_char_t(0x7Fu)) {
			if (w < f) *w++ = (char)(c&0xFFu);
		}
		else if (c <= unicode_char_t(0x7FFFFFFFul)) {
			/* 110x xxxx = 2 (1 more) mask 0x1F bits 5 + 6*1 = 11
			 * 1110 xxxx = 3 (2 more) mask 0x0F bits 4 + 6*2 = 16
			 * 1111 0xxx = 4 (3 more) mask 0x07 bits 3 + 6*3 = 21
			 * 1111 10xx = 5 (4 more) mask 0x03 bits 2 + 6*4 = 26
			 * 1111 110x = 6 (5 more) mask 0x01 bits 1 + 6*5 = 31 */
			unsigned char more = 1;
			{
				uint32_t tmp = uint32_t(c) >> uint32_t(11u);
				while (tmp != 0) { more++; tmp >>= uint32_t(5u); }
				assert(more <= 5);
			}

			const uint8_t ib = 0xFC << (5 - more);
			if ((w+1+more) > f) return;
			char *wr = w; w += 1+more; assert(w <= f);
			do { wr[more] = (char)(0x80u | ((unsigned char)(c&0x3F))); c >>= 6u; } while ((--more) != 0);
			assert(uint32_t(c) <= uint32_t((0x80u|(ib>>1u))^0xFFu)); /* 0xC0 0xE0 0xF0 0xF8 0xFC -> 0xE0 0xF0 0xF8 0xFC 0xFE -> 0x1F 0x0F 0x07 0x03 0x01 */
			wr[0] = (char)(ib | (unsigned char)c);
		}
	}

	std::string utf8_to_str(const unicode_char_t c) {
		char tmp[64],*w=tmp;

		utf8_to_str(/*&*/w,/*fence*/tmp+sizeof(tmp),c);
		assert(w < (tmp+sizeof(tmp)));
		*w++ = 0;

		return std::string(tmp);
	}

	unicode_char_t getcnu(rbuf &buf,source_file_object &sfo) { /* non-unicode */
		if (buf.data_avail() < 1) rbuf_sfd_refill(buf,sfo);
		if (buf.data_avail() == 0) return unicode_eof;
		return unicode_char_t(buf.getb());
	}

	unicode_char_t getc(rbuf &buf,source_file_object &sfo) {
		if (buf.data_avail() < 1) rbuf_sfd_refill(buf,sfo);
		if (buf.data_avail() == 0) return unicode_eof;

		/* 0xxx xxxx                                                    0x00000000-0x0000007F
		 * 110x xxxx 10xx xxxx                                          0x00000080-0x000007FF
		 * 1110 xxxx 10xx xxxx 10xx xxxx                                0x00000800-0x0000FFFF
		 * 1111 0xxx 10xx xxxx 10xx xxxx 10xx xxxx                      0x00010000-0x001FFFFF
		 * 1111 10xx 10xx xxxx 10xx xxxx 10xx xxxx 10xx xxxx            0x00200000-0x03FFFFFF
		 * 1111 110x 10xx xxxx 10xx xxxx 10xx xxxx 10xx xxxx 10xx xxxx  0x04000000-0x7FFFFFFF */

		/* read UTF-8 char */
		uint32_t v = buf.getb();
		if (v <  0x80) return v; /* 0x00-0x7F ASCII char */
		if (v <  0xC0) return unicode_invalid; /* 0x80-0xBF we're in the middle of a UTF-8 char */
		if (v >= 0xFE) return unicode_invalid; /* overlong 1111 1110 or 1111 1111 */

		/* 110x xxxx = 2 (1 more) mask 0x1F bits 5 + 6*1 = 11
		 * 1110 xxxx = 3 (2 more) mask 0x0F bits 4 + 6*2 = 16
		 * 1111 0xxx = 4 (3 more) mask 0x07 bits 3 + 6*3 = 21
		 * 1111 10xx = 5 (4 more) mask 0x03 bits 2 + 6*4 = 26
		 * 1111 110x = 6 (5 more) mask 0x01 bits 1 + 6*5 = 31 */
		unsigned char more = 1;
		for (unsigned char c=(unsigned char)v;(c&0xFFu) >= 0xE0u;) { c <<= 1u; more++; } assert(more <= 5);
		v &= 0x3Fu >> more; /* 1 2 3 4 5 -> 0x1F 0x0F 0x07 0x03 0x01 */

		do {
			const unsigned char c = buf.peekb();
			if ((c&0xC0) != 0x80) return unicode_invalid; /* must be 10xx xxxx */
			buf.discardb(); v = (v << uint32_t(6u)) + uint32_t(c & 0x3Fu);
		} while ((--more) != 0);

		assert(v <= uint32_t(0x7FFFFFFFu));
		return unicode_char_t(v);
	}

	////////////////////////////////////////////////////////////////////

	enum class token_type_t:unsigned int {
		none=0,					// 0
		eof,
		plus,
		plusplus,
		minus,
		minusminus,				// 5
		semicolon,
		equal,
		equalequal,
		tilde,
		ampersand,				// 10
		ampersandampersand,
		pipe,
		pipepipe,
		caret,
		integer,				// 15
		floating,

		__MAX__
	};

	static const char *token_type_t_strlist[size_t(token_type_t::__MAX__)] = {
		"none",					// 0
		"eof",
		"plus",
		"plusplus",
		"minus",
		"minusminus",				// 5
		"semicolon",
		"equal",
		"equalequal",
		"tilde",
		"ampersand",				// 10
		"ampersandampersand",
		"pipe",
		"pipepipe",
		"caret",
		"integer",				// 15
		"floating"
	};

	static const char *token_type_t_str(const token_type_t t) {
		return (t < token_type_t::__MAX__) ? token_type_t_strlist[size_t(t)] : "";
	}

	struct floating_value_t {
		uint64_t				mantissa;
		int32_t					exponent;
		unsigned int				flags;

		static constexpr unsigned int		FL_NAN        = (1u << 0u);
		static constexpr unsigned int           FL_OVERFLOW   = (1u << 1u);
		static constexpr unsigned int           FL_FLOAT      = (1u << 2u);
		static constexpr unsigned int           FL_DOUBLE     = (1u << 3u);
		static constexpr unsigned int           FL_LONG       = (1u << 4u);
		static constexpr unsigned int           FL_NEGATIVE   = (1u << 5u);

		static constexpr uint64_t		mant_msb = uint64_t(1ull) << uint64_t(63ull);

		std::string to_str(void) const {
			std::string s;
			char tmp[256];

			if (!(flags & FL_NAN)) {
				sprintf(tmp,"v=0x%llx e=%ld ",(unsigned long long)mantissa,(long)exponent);
				s += tmp;

				sprintf(tmp,"%.20f",ldexp(double(mantissa),(int)exponent-63));
				s += tmp;
			}
			else {
				s += "NaN";
			}

			if (flags & FL_NEGATIVE) s += " negative";
			if (flags & FL_OVERFLOW) s += " overflow";
			if (flags & FL_FLOAT) s += " float";
			if (flags & FL_DOUBLE) s += " double";
			if (flags & FL_LONG) s += " long";

			return s;
		}

		void init(void) { flags = FL_DOUBLE; mantissa=0; exponent=0; }

		void setsn(const uint64_t m,const int32_t e) {
			mantissa = m;
			exponent = e+63;
			normalize();
		}

		void normalize(void) {
			if (mantissa != uint64_t(0)) {
				while (!(mantissa & mant_msb)) {
					mantissa <<= uint64_t(1ull);
					exponent--;
				}
			}
			else {
				exponent = 0;
			}
		}
	};

	struct integer_value_t {
		union {
			uint64_t			u;
			int64_t				v;
		} v;
		unsigned int				flags;

		static constexpr unsigned int		FL_SIGNED     = (1u << 0u);
		static constexpr unsigned int		FL_LONG       = (1u << 1u);
		static constexpr unsigned int		FL_LONGLONG   = (1u << 2u);
		static constexpr unsigned int		FL_OVERFLOW   = (1u << 3u);

		std::string to_str(void) const {
			std::string s;
			char tmp[192];

			s += "v=";
			if (flags & FL_SIGNED) sprintf(tmp,"%lld/0x%llx",(signed long long)v.v,(unsigned long long)v.u);
			else sprintf(tmp,"%llu/0x%llx",(unsigned long long)v.u,(unsigned long long)v.u);
			s += tmp;

			if (flags & FL_SIGNED) s += " signed";
			if (flags & FL_LONGLONG) s += " llong";
			if (flags & FL_LONG) s += " long";
			if (flags & FL_OVERFLOW) s += " overflow";

			return s;
		}

		void init(void) { flags = FL_SIGNED; v.v=0; }
	};

	struct token_t {
		token_type_t		type = token_type_t::none;

		token_t() { }
		token_t(const token_type_t t) : type(t) { }

		union {
			integer_value_t		integer; /* token_type_t::integer */
			floating_value_t	floating; /* token_type_t::floating */
		} v;

		std::string to_str(void) const {
			std::string s = token_type_t_str(type);

			switch (type) {
				case token_type_t::integer:
					s += "("; s += v.integer.to_str(); s += ")";
					break;
				case token_type_t::floating:
					s += "("; s += v.floating.to_str(); s += ")";
					break;
				default:
					break;
			}

			return s;
		}
	};

	////////////////////////////////////////////////////////////////////

	void eat_whitespace(rbuf &buf,source_file_object &sfo) {
		do {
			if (buf.data_avail() < 1) rbuf_sfd_refill(buf,sfo);
			const unsigned char b = buf.peekb();
			if (b == ' ' || b == '\n' || b == '\t') buf.discardb();
			else break;
		} while (1);
	}

	int cc_parsedigit(unsigned char c,const unsigned char base=10) {
		if (c >= '0' && c <= '9')
			return c - '0';
		else if (c >= 'a' && c < ('a'+base-10))
			return c + 10 - 'a';
		else if (c >= 'A' && c < ('A'+base-10))
			return c + 10 - 'A';

		return -1;
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
					return errno_return(EINVAL);
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
			const unsigned char *scan = buf.data;
			while (scan < buf.end && (*scan == '\'' || cc_parsedigit(*scan,base) >= 0)) scan++;
			if (scan < buf.end && (*scan == '.' || tolower((char)(*scan)) == 'e')) {
				/* TODO: Parse it ourself */
				do {
					if (cc_parsedigit(buf.peekb(),base) >= 0) {
						buf.discardb();
					}
					else if (buf.peekb() == 'e' || buf.peekb() == 'E' || buf.peekb() == '.') {
						buf.discardb();
					}
					else {
						break;
					}
				} while(1);

				t.type = token_type_t::floating;
				t.v.floating.init();
				t.v.floating.setsn(1,0); /* 1 * 2^0 = 1.0 */
				/* It's a floating point constant */
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
				t.v.integer.flags |= integer_value_t::FL_LONG;
				buf.discardb();

				c = (unsigned char)tolower((char)buf.peekb());
				if (c == 'l') {
					t.v.integer.flags &= ~integer_value_t::FL_LONG;
					t.v.integer.flags |= integer_value_t::FL_LONGLONG;
					buf.discardb();
				}
			}
			else {
				break;
			}
		} while(1);

		return 1;
	}

	int lgtok(rbuf &buf,source_file_object &sfo,token_t &t) {
		t = token_t();

		eat_whitespace(buf,sfo);
		if (buf.data_avail() < 8) rbuf_sfd_refill(buf,sfo);
		if (buf.data_avail() == 0) {
			t.type = token_type_t::eof;
			if (buf.err) return buf.err;
			else return 0;
		}

		switch (buf.peekb()) {
			case '+':
				t.type = token_type_t::plus; buf.discardb();
				if (buf.peekb() == '+') { t.type = token_type_t::plusplus; buf.discardb(); }
				break;
			case '-':
				t.type = token_type_t::minus; buf.discardb();
				if (buf.peekb() == '-') { t.type = token_type_t::minusminus; buf.discardb(); }
				break;
			case '=':
				t.type = token_type_t::equal; buf.discardb();
				if (buf.peekb() == '=') { t.type = token_type_t::equalequal; buf.discardb(); }
				break;
			case ';':
				t.type = token_type_t::semicolon; buf.discardb();
				break;
			case '~':
				t.type = token_type_t::tilde; buf.discardb();
				break;
			case '^':
				t.type = token_type_t::caret; buf.discardb();
				break;
			case '&':
				t.type = token_type_t::ampersand; buf.discardb();
				if (buf.peekb() == '&') { t.type = token_type_t::ampersandampersand; buf.discardb(); }
				break;
			case '|':
				t.type = token_type_t::pipe; buf.discardb();
				if (buf.peekb() == '|') { t.type = token_type_t::pipepipe; buf.discardb(); }
				break;
			case '0': case '1': case '2': case '3': case '4':
			case '5': case '6': case '7': case '8': case '9':
				return lgtok_number(buf,sfo,t);
			default:
				return errno_return(ESRCH);
		}

		return 1;
	}

}

enum test_mode_t {
	TEST_NONE=0,
	TEST_SFO,
	TEST_RBF,
	TEST_RBFGC,
	TEST_RBFGCNU,
	TEST_LTOK
};

static std::vector<std::string>		main_input_files;
static enum test_mode_t			test_mode = TEST_NONE;

static void help(void) {
	fprintf(stderr,"cimcc [options] [input file [...]]\n");
	fprintf(stderr,"  --test <none|sfo|rbf|rbfgc|rbfgcnu|ltok>         Test mode\n");
}

static int parse_argv(int argc,char **argv) {
	int i = 1; /* argv[0] is command */
	char *a;

	while (i < argc) {
		a = argv[i++];

		if (*a == '-') {
			do { a++; } while (*a == '-');

			if (!strcmp(a,"h") || !strcmp(a,"help")) {
				help();
				return -1;
			}
			else if (!strcmp(a,"test")) {
				a = argv[i++];
				if (a == NULL) return -1;

				if (!strcmp(a,"sfo"))
					test_mode = TEST_SFO;
				else if (!strcmp(a,"rbf"))
					test_mode = TEST_RBF;
				else if (!strcmp(a,"rbfgc"))
					test_mode = TEST_RBFGC;
				else if (!strcmp(a,"rbfgcnu"))
					test_mode = TEST_RBFGCNU;
				else if (!strcmp(a,"ltok"))
					test_mode = TEST_LTOK;
				else if (!strcmp(a,"none"))
					test_mode = TEST_NONE;
				else
					return -1;
			}
			else {
				fprintf(stderr,"Unknown option '%s'\n",a);
				return -1;
			}
		}
		else {
			if (main_input_files.size() > 1024) return -1;
			main_input_files.push_back(a);
		}
	}

	return 0;
}

int main(int argc,char **argv) {
	if (parse_argv(argc,argv) < 0)
		return 1;

	if (main_input_files.empty()) {
		fprintf(stderr,"No input files\n");
		return 1;
	}

	for (auto mifi=main_input_files.begin();mifi!=main_input_files.end();mifi++) {
		CIMCC::source_file_object *sfo = NULL;

		if (*mifi == "-") {
			struct stat st;

			if (fstat(0/*STDIN*/,&st)) {
				fprintf(stderr,"Cannot stat STDIN\n");
				return 1;
			}
			if (!(S_ISREG(st.st_mode) || S_ISSOCK(st.st_mode) || S_ISFIFO(st.st_mode))) {
				fprintf(stderr,"Cannot use STDIN. Must be a file, socket, or pipe\n");
				return 1;
			}
			sfo = new CIMCC::source_fd(0/*STDIN, takes ownership*/,"<stdin>");
			assert(sfo->iface == CIMCC::source_file_object::IF_FD);
		}
		else {
			int fd = open((*mifi).c_str(),O_RDONLY|O_BINARY);
			if (fd < 0) {
				fprintf(stderr,"Cannot open '%s', '%s'\n",(*mifi).c_str(),strerror(errno));
				return 1;
			}
			sfo = new CIMCC::source_fd(fd/*takes ownership*/,*mifi);
			assert(sfo->iface == CIMCC::source_file_object::IF_FD);
		}

		if (test_mode == TEST_SFO) {
			char tmp[512];
			ssize_t sz;

			while ((sz=sfo->read(tmp,sizeof(tmp))) > 0) {
				if (write(1/*STDOUT*/,tmp,size_t(sz)) != ssize_t(sz))
					return -1;
			}

			if (sz < 0) {
				fprintf(stderr,"Read error from %s, error %d\n",sfo->getname(),(int)sz);
				return -1;
			}
		}
		else if (test_mode == TEST_RBF) {
			CIMCC::rbuf rb;
			assert(rb.allocate(128));

			do {
				int r = rbuf_sfd_refill(rb,*sfo);
				if (r < 0) {
					fprintf(stderr,"RBUF read err from %s, %d\n",sfo->getname(),(int)r);
					return -1;
				}
				else if (r == 0/*EOF*/) {
					break;
				}

				const size_t av = rb.data_avail();
				if (av == 0) {
					fprintf(stderr,"Unexpected data end?\n");
					break;
				}

				if (write(1/*STDOUT*/,rb.data,av) != ssize_t(av)) return -1;
				rb.data += av;
				assert(rb.sanity_check());
			} while (1);
		}
		else if (test_mode == TEST_RBFGC) {
			CIMCC::rbuf rb;
			assert(rb.allocate());

			do {
				CIMCC::unicode_char_t c = CIMCC::getc(rb,*sfo);
				if (c == CIMCC::unicode_eof) {
					assert(rb.data_avail() == 0);
					break;
				}
				else if (c == CIMCC::unicode_invalid) {
					if (write(1/*STDOUT*/,"<INVALID>",9) != 9) return -1;
				}
				else {
					const std::string s = CIMCC::utf8_to_str(c);
					if (write(1/*STDOUT*/,s.data(),s.size()) != ssize_t(s.size())) return -1;
				}
			} while (1);
		}
		else if (test_mode == TEST_RBFGCNU) {
			CIMCC::rbuf rb;
			assert(rb.allocate());

			do {
				CIMCC::unicode_char_t c = CIMCC::getcnu(rb,*sfo);
				if (c == CIMCC::unicode_eof) {
					assert(rb.data_avail() == 0);
					break;
				}
				else if (c == CIMCC::unicode_invalid) {
					abort(); /* should not happen! */
				}
				else {
					assert(c >= CIMCC::unicode_char_t(0l) && c <= CIMCC::unicode_char_t(0xFFul));
					unsigned char cc = (unsigned char)c;
					if (write(1/*STDOUT*/,&cc,1) != ssize_t(1)) return -1;
				}
			} while (1);
		}
		else if (test_mode == TEST_LTOK) {
			CIMCC::token_t tok;
			CIMCC::rbuf rb;
			int r;

			assert(rb.allocate());
			while ((r=CIMCC::lgtok(rb,*sfo,tok)) > 0) printf("Token: %s\n",tok.to_str().c_str());

			if (r < 0) {
				fprintf(stderr,"Read error from %s, error %d\n",sfo->getname(),(int)r);
				return -1;
			}
		}

		assert(sfo != NULL);
		delete sfo;
	}

	if (test_mode == TEST_SFO || test_mode == TEST_LTOK || test_mode == TEST_RBF || test_mode == TEST_RBFGC || test_mode == TEST_RBFGCNU) return 0;

	return 0;
}

