
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

			if ((base=data=end=(unsigned char*)::malloc(sz+1)) == NULL)
				return false;

			fence = base + sz;
			*fence = 0;
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
		charliteral,
		strliteral,
		identifier,

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
		"floating",
		"charliteral",
		"strliteral",
		"identifier"
	};

	static const char *token_type_t_str(const token_type_t t) {
		return (t < token_type_t::__MAX__) ? token_type_t_strlist[size_t(t)] : "";
	}

	struct floating_value_t {
		enum class type_t:unsigned char {
			NONE=0,			// 0
			FLOAT,
			DOUBLE,
			LONGDOUBLE,

			__MAX__
		};

		uint64_t				mantissa;
		int32_t					exponent;
		unsigned char				flags;
		type_t					type;

		static constexpr unsigned int		FL_NAN        = (1u << 0u);
		static constexpr unsigned int           FL_OVERFLOW   = (1u << 1u);
		static constexpr unsigned int           FL_NEGATIVE   = (1u << 2u);

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

			s += " t=\"";
			switch (type) {
				case type_t::NONE:       s += "none"; break;
				case type_t::FLOAT:      s += "float"; break;
				case type_t::DOUBLE:     s += "double"; break;
				case type_t::LONGDOUBLE: s += "long double"; break;
				default: break;
			}
			s += "\"";

			if (flags & FL_NEGATIVE) s += " negative";
			if (flags & FL_OVERFLOW) s += " overflow";

			return s;
		}

		void init(void) { flags = 0; mantissa=0; exponent=0; type=type_t::DOUBLE; }

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
		enum class type_t:unsigned char {
			NONE=0,			// 0
			BOOL,
			CHAR,
			SHORT,
			INT,
			LONG,			// 5
			LONGLONG,

			__MAX__
		};

		union {
			uint64_t			u;
			int64_t				v;
		} v;
		unsigned char				flags;
		type_t					type;

		static constexpr unsigned int		FL_SIGNED     = (1u << 0u);
		static constexpr unsigned int		FL_OVERFLOW   = (1u << 1u);

		std::string to_str(void) const {
			std::string s;
			char tmp[192];

			s += "v=";
			if (flags & FL_SIGNED) sprintf(tmp,"%lld/0x%llx",(signed long long)v.v,(unsigned long long)v.u);
			else sprintf(tmp,"%llu/0x%llx",(unsigned long long)v.u,(unsigned long long)v.u);
			s += tmp;

			s += " t=\"";
			switch (type) {
				case type_t::NONE:       s += "none"; break;
				case type_t::BOOL:       s += "bool"; break;
				case type_t::CHAR:       s += "char"; break;
				case type_t::SHORT:      s += "short"; break;
				case type_t::INT:        s += "int"; break;
				case type_t::LONG:       s += "long"; break;
				case type_t::LONGLONG:   s += "longlong"; break;
				default: break;
			}
			s += "\"";

			if (flags & FL_SIGNED) s += " signed";
			if (flags & FL_OVERFLOW) s += " overflow";

			return s;
		}

		void init(void) { flags = FL_SIGNED; type=type_t::INT; v.v=0; }
	};

	struct charstrliteral_t {
		enum type_t {
			CHAR=0,
			UTF8, /* multibyte */
			UNICODE16,
			UNICODE32,

			__MAX__
		};

		type_t			type;
		void*			data;
		size_t			length; /* in bytes */
		size_t			allocated;

		void init(void) { type = type_t::CHAR; data = NULL; length = 0; allocated = 0; }
		void free(void) { if (data) ::free(data); data = NULL; length = 0; allocated = 0; type = type_t::CHAR; }

		const char *as_char(void) const { return (const char*)data; }
		const unsigned char *as_binary(void) const { return (const unsigned char*)data; }
		const unsigned char *as_utf8(void) const { return (const unsigned char*)data; }
		const uint16_t *as_u16(void) const { return (const uint16_t*)data; }
		const uint32_t *as_u32(void) const { return (const uint32_t*)data; }

		size_t units(void) const {
			static constexpr unsigned char unit_size[type_t::__MAX__] = { 1, 1, 2, 4 };
			return length / unit_size[type];
		}

		bool alloc(const size_t sz) {
			if (data != NULL || sz == 0 || sz >= (1024*1024))
				return false;

			data = malloc(sz);
			if (data == NULL)
				return false;

			length = sz;
			allocated = sz;
			return true;
		}

		bool realloc(const size_t sz) {
			if (data == NULL)
				return alloc(sz);

			if (sz > allocated) {
				void *np = ::realloc(data,sz);
				if (np == NULL)
					return false;

				data = np;
				allocated = sz;
			}

			length = sz;
			return true;
		}

		static std::string to_escape8(const unsigned char c) {
			char tmp[32];

			sprintf(tmp,"\\x%02x",c);
			return std::string(tmp);
		}

		bool shrinkfit(void) {
			if (data) {
				if (allocated > length) {
					void *np = ::realloc(data,length);
					if (np == NULL)
						return false;

					data = np;
					allocated = length;
				}
			}

			return true;
		}

		std::string to_str(void) const {
			std::string s;
			char tmp[128];

			if (data) {
				s += "v=\"";
				switch (type) {
					case type_t::CHAR: {
						const unsigned char *p = as_binary();
						const unsigned char *f = p + length;
						while (p < f) {
							if (*p < 0x20 || *p >= 0x80)
								s += to_escape8(*p++);
							else
								s += (char)(*p++);
						}
						break; }
					case type_t::UTF8: {
						const unsigned char *p = as_binary();
						const unsigned char *f = p + length;
						while (p < f) {
							if (*p < 0x20)
								s += to_escape8(*p++);
							else
								s += (char)(*p++); /* assume UTF-8 which we want to emit to STDOUT */
						}
						break; }
					case type_t::UNICODE16: {
						const uint16_t *p = as_u16();
						const uint16_t *f = p + units();
						while (p < f) {
							if (*p < 0x20u)
								s += to_escape8(*p++);
							else
								s += utf8_to_str(*p++); /* TODO: Surrogates? */
						}
						break; }
					case type_t::UNICODE32: {
						const uint32_t *p = as_u32();
						const uint32_t *f = p + units();
						while (p < f) {
							if (*p < 0x20u || *p >= 0x10FFFu)
								s += to_escape8(*p++);
							else
								s += utf8_to_str(*p++);
						}
						break; }
					default:
						break;
				}
				s += "\"";
			}

			s += " t=\"";
			switch (type) {
				case type_t::CHAR:       s += "char"; break;
				case type_t::UTF8:       s += "utf8"; break;
				case type_t::UNICODE16:  s += "unicode16"; break;
				case type_t::UNICODE32:  s += "unicode32"; break;
				default: break;
			}
			s += "\"";

			sprintf(tmp," lenb=%zu lenu=%zu",length,units());
			s += tmp;

			return s;
		}
	};

	struct token_t {
		token_type_t		type = token_type_t::none;

		token_t() { common_init(); }
		~token_t() { common_delete(); }
		token_t(const token_t &t) = delete;
		token_t(token_t &&t) { common_move(t); }
		token_t(const token_type_t t) : type(t) { common_init(); }
		token_t &operator=(const token_t &t) = delete;
		token_t &operator=(token_t &&t) { common_move(t); return *this; }

		union {
			integer_value_t		integer; /* token_type_t::integer */
			floating_value_t	floating; /* token_type_t::floating */
			charstrliteral_t	strliteral; /* token_type_t::charliteral/strliteral/identifier */
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
				case token_type_t::charliteral:
				case token_type_t::strliteral:
				case token_type_t::identifier:
					s += "("; s += v.strliteral.to_str(); s += ")";
					break;
				default:
					break;
			}

			return s;
		}

private:
		void common_delete(void) {
			switch (type) {
				case token_type_t::charliteral:
				case token_type_t::strliteral:
				case token_type_t::identifier:
					v.strliteral.free();
					break;
				default:
					break;
			}
		}

		void common_init(void) {
			switch (type) {
				case token_type_t::integer:
					v.integer.init();
					break;
				case token_type_t::floating:
					v.floating.init();
					break;
				case token_type_t::charliteral:
				case token_type_t::strliteral:
				case token_type_t::identifier:
					v.strliteral.init();
					break;
				default:
					break;
			}
		}

		void common_move(token_t &x) {
			common_delete();
			type = x.type; x.type = token_type_t::none;
			v = x.v; /* x.type == none so pointers no longer matter */
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

	bool is_identifier_first_char(unsigned char c) {
		return (c == '_') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
	}

	bool is_identifier_char(unsigned char c) {
		return (c == '_') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
	}

	int32_t lgtok_cslitget(rbuf &buf,source_file_object &sfo,const bool unicode=false) {
		int32_t v;

		v = unicode ? getc(buf,sfo) : buf.getb();
		if (v == 0) return int32_t(-1);

		if (v == uint32_t('\\')) {
			v = unicode ? getc(buf,sfo) : buf.getb();

			switch (v) {
				case '\'':
				case '\"':
				case '\?':
				case '\\':
					return v;
				case 'a':
					return int32_t(7);
				case 'b':
					return int32_t(8);
				case 'f':
					return int32_t(12);
				case 'n':
					return int32_t(10);
				case 'r':
					return int32_t(13);
				case 't':
					return int32_t(9);
				case 'v':
					return int32_t(11);
				case '0': case '1': case '2': case '3':
				case '4': case '5': case '6': case '7': {
					v = cc_parsedigit((unsigned char)v,8);
					assert(v >= int32_t(0));

					for (unsigned int c=0;c < 2;c++) {
						const int n = cc_parsedigit(buf.peekb(),8);
						if (n >= 0) {
							v = int32_t(((unsigned int)v << 3u) | (unsigned int)n);
							buf.discardb();
						}
						else {
							break;
						}
					}
					break; }
				case 'x': {
					int n;

					buf.discardb(); v = 0;
					while ((n=cc_parsedigit(buf.peekb(),16)) >= 0) {
						v = int32_t(((unsigned int)v << 4u) | (unsigned int)n);
						buf.discardb();
					}
					break; }
				default:
					return int32_t(-1);
			}
		}

		return v;
	}

	int lgtok_charstrlit(rbuf &buf,source_file_object &sfo,token_t &t,const charstrliteral_t::type_t cslt=charstrliteral_t::type_t::CHAR) {
		unsigned char separator = buf.peekb();

		if (separator == '\'' || separator == '\"') {
			buf.discardb();

			assert(t.type == token_type_t::none);
			if (separator == '\"') t.type = token_type_t::strliteral;
			else t.type = token_type_t::charliteral;
			t.v.strliteral.init();
			t.v.strliteral.type = cslt;

			if (cslt == charstrliteral_t::type_t::CHAR || cslt == charstrliteral_t::type_t::UTF8) { /* char */
				int32_t v;
				unsigned char *p,*f;

				if (!t.v.strliteral.alloc(64))
					return errno_return(ENOMEM);

				p = (unsigned char*)t.v.strliteral.data;
				f = (unsigned char*)t.v.strliteral.data+t.v.strliteral.length;
				do {
					if (buf.peekb() == separator) {
						buf.discardb();
						break;
					}

					if ((p+10) >= f) {
						const size_t wo = size_t(p-t.v.strliteral.as_binary());

						if (!t.v.strliteral.realloc(t.v.strliteral.length+(t.v.strliteral.length/2u)))
							return errno_return(ENOMEM);

						p = (unsigned char*)t.v.strliteral.data+wo;
						f = (unsigned char*)t.v.strliteral.data+t.v.strliteral.length;
					}

					v = lgtok_cslitget(buf,sfo);
					if (cslt == charstrliteral_t::type_t::UTF8) {
						if (v < 0x00) return errno_return(EINVAL);
						char *c = (char*)p; utf8_to_str(c,(char*)f,v); p = (unsigned char*)c;
						assert(p <= f);
					}
					else {
						if (v < 0x00 || v > 0xFF) return errno_return(EINVAL);
						assert((p+1) <= f);
						*p++ = (unsigned char)v;
					}

				} while (1);

				{
					const size_t fo = size_t(p-t.v.strliteral.as_binary());
					assert(fo <= t.v.strliteral.allocated);
					t.v.strliteral.length = fo;
					t.v.strliteral.shrinkfit();
				}
			}
			else if (cslt == charstrliteral_t::type_t::UNICODE16) {
				int32_t v;
				uint16_t *p,*f;

				if (!t.v.strliteral.alloc(64))
					return errno_return(ENOMEM);

				p = (uint16_t*)t.v.strliteral.data;
				f = (uint16_t*)((char*)t.v.strliteral.data+t.v.strliteral.length);
				do {
					if (buf.peekb() == separator) {
						buf.discardb();
						break;
					}

					if ((p+10) >= f) {
						const size_t wo = size_t(p-t.v.strliteral.as_u16());

						if (!t.v.strliteral.realloc(t.v.strliteral.length+(((t.v.strliteral.length+3u)/2u)&(~1u))))
							return errno_return(ENOMEM);

						p = (uint16_t*)t.v.strliteral.data+wo;
						f = (uint16_t*)((char*)t.v.strliteral.data+t.v.strliteral.length);
					}

					v = lgtok_cslitget(buf,sfo);
					if (v < 0x00 || v > 0xFFff) return errno_return(EINVAL);
					assert((p+1) <= f);
					*p++ = (uint16_t)v;
				} while (1);

				{
					const size_t fo = size_t(p-t.v.strliteral.as_u16()) * sizeof(uint16_t);
					assert(fo <= t.v.strliteral.allocated);
					t.v.strliteral.length = fo;
					t.v.strliteral.shrinkfit();
				}
			}
			else if (cslt == charstrliteral_t::type_t::UNICODE32) {
				int32_t v;
				uint32_t *p,*f;

				if (!t.v.strliteral.alloc(64))
					return errno_return(ENOMEM);

				p = (uint32_t*)t.v.strliteral.data;
				f = (uint32_t*)((char*)t.v.strliteral.data+t.v.strliteral.length);
				do {
					if (buf.peekb() == separator) {
						buf.discardb();
						break;
					}

					if ((p+10) >= f) {
						const size_t wo = size_t(p-t.v.strliteral.as_u32());

						if (!t.v.strliteral.realloc(t.v.strliteral.length+(((t.v.strliteral.length+7u)/4u)&(~3u))))
							return errno_return(ENOMEM);

						p = (uint32_t*)t.v.strliteral.data+wo;
						f = (uint32_t*)((char*)t.v.strliteral.data+t.v.strliteral.length);
					}

					v = lgtok_cslitget(buf,sfo);
					if (v < 0x00) return errno_return(EINVAL);
					assert((p+1) <= f);
					*p++ = (uint32_t)v;
				} while (1);

				{
					const size_t fo = size_t(p-t.v.strliteral.as_u32()) * sizeof(uint32_t);
					assert(fo <= t.v.strliteral.allocated);
					t.v.strliteral.length = fo;
					t.v.strliteral.shrinkfit();
				}
			}

			return 1;
		}
		else {
			return errno_return(EINVAL);
		}
	}

	int lgtok_identifier(rbuf &buf,source_file_object &sfo,token_t &t) {
		assert(t.type == token_type_t::none);
		t.type = token_type_t::identifier;
		t.v.strliteral.init();
		t.v.strliteral.type = charstrliteral_t::type_t::CHAR;

		if (!t.v.strliteral.alloc(32))
			return errno_return(ENOMEM);

		unsigned char *p,*f;

		p = (unsigned char*)t.v.strliteral.data;
		f = (unsigned char*)t.v.strliteral.data+t.v.strliteral.length;

		assert(p < f);
		assert(is_identifier_first_char(buf.peekb()));
		rbuf_sfd_refill(buf,sfo);
		*p++ = buf.getb();
		while (is_identifier_char(buf.peekb())) {
			if ((p+1) >= f) {
				const size_t wo = size_t(p-t.v.strliteral.as_binary());

				if (wo >= 120)
					return errno_return(ENAMETOOLONG);

				if (!t.v.strliteral.realloc(t.v.strliteral.length*2u))
					return errno_return(ENOMEM);

				p = (unsigned char*)t.v.strliteral.data+wo;
				f = (unsigned char*)t.v.strliteral.data+t.v.strliteral.length;
			}

			assert((p+1) <= f);
			*p++ = (unsigned char)buf.getb();
			rbuf_sfd_refill(buf,sfo);
		}

		{
			const size_t fo = size_t(p-t.v.strliteral.as_binary());
			assert(fo <= t.v.strliteral.allocated);
			t.v.strliteral.length = fo;
			t.v.strliteral.shrinkfit();
		}

		/* but wait: if the identifier is followed by a quotation mark, and it's some specific sequence,
		 * then it's actually a char/string literal i.e. L"blah" would be a wide char string. No space
		 * allowed. */
		if (buf.peekb() == '\'' || buf.peekb() == '\"') {
			if (t.v.strliteral.length == 2 && !memcmp(t.v.strliteral.data,"u8",2)) {
				t = token_t(); return lgtok_charstrlit(buf,sfo,t,charstrliteral_t::type_t::UTF8);
			}
			else if (t.v.strliteral.length == 1) {
				if (*((const char*)t.v.strliteral.data) == 'U') {
					t = token_t(); return lgtok_charstrlit(buf,sfo,t,charstrliteral_t::type_t::UNICODE32);
				}
				else if (*((const char*)t.v.strliteral.data) == 'u') {
					t = token_t(); return lgtok_charstrlit(buf,sfo,t,charstrliteral_t::type_t::UNICODE16);
				}
				else if (*((const char*)t.v.strliteral.data) == 'L') {
					/* FIXME: A "wide" char varies between targets i.e. Windows wide char is 16 bits,
					 *        Linux wide char is 32 bits. */
					t = token_t(); return lgtok_charstrlit(buf,sfo,t,charstrliteral_t::type_t::UNICODE32);
				}
			}
		}

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
			unsigned char *scan = buf.data;
			while (scan < buf.end && (*scan == '\'' || cc_parsedigit(*scan,base) >= 0)) scan++;
			if (scan < buf.end && (*scan == '.' || tolower((char)(*scan)) == 'e' || tolower((char)(*scan)) == 'f' || tolower((char)(*scan)) == 'd' || tolower((char)(*scan)) == 'p')) {
				/* It's a floating point constant */
				t.type = token_type_t::floating;
				t.v.floating.init();
				scan = buf.data;

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
			case '\'':
			case '\"':
				return lgtok_charstrlit(buf,sfo,t);
			case '0': case '1': case '2': case '3': case '4':
			case '5': case '6': case '7': case '8': case '9':
				return lgtok_number(buf,sfo,t);
			default:
				if (is_identifier_first_char(buf.peekb()))
					return lgtok_identifier(buf,sfo,t);
				else
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

