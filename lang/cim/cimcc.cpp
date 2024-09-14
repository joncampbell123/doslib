
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
#include <deque>
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

	struct position_t {
		uint16_t			col;
		uint16_t			row;
		uint32_t			ofs;

		position_t() : col(0),row(0),ofs(0) { }
		position_t(const unsigned int row) : col(1),row(row),ofs(0) { }
	};

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
		unsigned int			flags = 0;
		position_t			pos;

		static const unsigned int	PFL_EOF = 1u << 0u;

		rbuf() { pos.col=1; pos.row=1; pos.ofs=0; }
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
			flags = x.flags; x.flags = 0;
			err   = x.err;   x.err = 0;
			pos   = x.pos;
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

		void discardb(size_t count=1) {
			while (data < end && count > 0) {
				pos_track(*data++);
				count--;
			}
		}

		unsigned char getb(void) {
			if (data < end) { pos_track(*data); return *data++; }
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

		void pos_track(const unsigned char c) {
			pos.ofs++;
			if (c == '\r') {
			}
			else if (c == '\n') {
				pos.col=1;
				pos.row++;
			}
			else if (c < 0x80 || c >= 0xc0) {
				pos.col++;
			}
		}

		void pos_track(const unsigned char *from,const unsigned char *to) {
			while (from < to) pos_track(*from++);
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
				buf.flags |= rbuf::PFL_EOF;
				return 0;
			}
		}
		else if (buf.err) {
			return buf.err;
		}
		else if (buf.flags & rbuf::PFL_EOF) {
			return 0;
		}

		return 1;
	}

	typedef int32_t unicode_char_t;
	static constexpr unicode_char_t unicode_eof = unicode_char_t(-1l);
	static constexpr unicode_char_t unicode_invalid = unicode_char_t(-2l);
	static constexpr unicode_char_t unicode_nothing = unicode_char_t(-3l);

	void utf16_to_str(uint16_t* &w,uint16_t *f,unicode_char_t c) {
		if (c < unicode_char_t(0)) {
			/* do nothing */
		}
		else if (c >= 0x10000l) {
			/* surrogate pair */
			if ((w+2) <= f) {
				*w++ = (uint16_t)((((c - 0x10000l) >> 10l) & 0x3FFl) + 0xD800l);
				*w++ = (uint16_t)(( (c - 0x10000l)         & 0x3FFl) + 0xDC00l);
			}
		}
		else {
			if (w < f) *w++ = (uint16_t)c;
		}
	}

	void utf8_to_str(unsigned char* &w,unsigned char *f,unicode_char_t c) {
		if (c < unicode_char_t(0)) {
			/* do nothing */
		}
		else if (c <= unicode_char_t(0x7Fu)) {
			if (w < f) *w++ = (unsigned char)(c&0xFFu);
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
			unsigned char *wr = w; w += 1+more; assert(w <= f);
			do { wr[more] = (unsigned char)(0x80u | ((unsigned char)(c&0x3F))); c >>= 6u; } while ((--more) != 0);
			assert(uint32_t(c) <= uint32_t((0x80u|(ib>>1u))^0xFFu)); /* 0xC0 0xE0 0xF0 0xF8 0xFC -> 0xE0 0xF0 0xF8 0xFC 0xFE -> 0x1F 0x0F 0x07 0x03 0x01 */
			wr[0] = (unsigned char)(ib | (unsigned char)c);
		}
	}

	std::string utf8_to_str(const unicode_char_t c) {
		unsigned char tmp[64],*w=tmp;

		utf8_to_str(/*&*/w,/*fence*/tmp+sizeof(tmp),c);
		assert(w < (tmp+sizeof(tmp)));
		*w++ = 0;

		return std::string((char*)tmp);
	}

	unicode_char_t getcnu(rbuf &buf,source_file_object &sfo) { /* non-unicode */
		if (buf.data_avail() < 1) rbuf_sfd_refill(buf,sfo);
		if (buf.data_avail() == 0) return unicode_eof;
		return unicode_char_t(buf.getb());
	}

	unicode_char_t p_utf16_decode(const uint16_t* &p,const uint16_t* const f) {
		if (p >= f) return unicode_eof;

		if ((p+2) <= f && (p[0]&0xDC00u) == 0xD800u && (p[1]&0xDC00u) == 0xDC00u) {
			const uint32_t v = uint32_t(((p[0]&0x3FFul) << 10ul) + (p[1]&0x3FFul) + 0x10000ul); p += 2;
			return unicode_char_t(v);
		}

		return unicode_char_t(*p++);
	}

	unicode_char_t p_utf8_decode(const unsigned char* &p,const unsigned char* const f) {
		if (p >= f) return unicode_eof;

		uint32_t v = *p++;
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
			if (p >= f) return unicode_invalid;
			const unsigned char c = *p;
			if ((c&0xC0) != 0x80) return unicode_invalid; /* must be 10xx xxxx */
			p++; v = (v << uint32_t(6u)) + uint32_t(c & 0x3Fu);
		} while ((--more) != 0);

		assert(v <= uint32_t(0x7FFFFFFFu));
		return unicode_char_t(v);
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
		comma,					// 20
		pipeequals,
		caretequals,
		ampersandequals,
		plusequals,
		minusequals,				// 25
		star,
		forwardslash,
		starequals,
		forwardslashequals,
		percent,				// 30
		percentequals,
		exclamationequals,
		exclamation,
		question,
		colon,					// 35
		lessthan,
		lessthanlessthan,
		lessthanlessthanequals,
		lessthanequals,
		lessthanequalsgreaterthan,		// 40
		greaterthan,
		greaterthangreaterthan,
		greaterthangreaterthanequals,
		greaterthanequals,
		minusrightanglebracket,			// 45
		minusrightanglebracketstar,
		period,
		periodstar,
		opensquarebracket,
		closesquarebracket,			// 50
		opencurlybracket,
		closecurlybracket,
		openparenthesis,
		closeparenthesis,
		coloncolon,				// 55
		ppidentifier,
		r_alignas,
		r_alignof,
		r_auto,
		r_bool,					// 60
		r_break,
		r_case,
		r_char,
		r_const,
		r_constexpr,				// 65
		r_continue,
		r_default,
		r_do,
		r_double,
		r_else,					// 70
		r_enum,
		r_extern,
		r_false,
		r_float,
		r_for,					// 75
		r_goto,
		r_if,
		r_inline,
		r_int,
		r_long,					// 80
		r_nullptr,
		r_register,
		r_restrict,
		r_return,
		r_short,				// 85
		r_signed,
		r_sizeof,
		r_static,
		r_static_assert,
		r_struct,				// 90
		r_switch,
		r_thread_local,
		r_true,
		r_typedef,
		r_typeof,				// 95
		r_typeof_unqual,
		r_union,
		r_unsigned,
		r_void,
		r_volatile,				// 100
		r_while,
		r__Alignas,
		r__Alignof,
		r__Atomic,
		r__BitInt,				// 105
		r__Bool,
		r__Complex,
		r__Decimal128,
		r__Decimal32,
		r__Decimal64,				// 110
		r__Generic,
		r__Imaginary,
		r__Noreturn,
		r__Static_assert,
		r__Thread_local,			// 115
		r_char8_t,
		r_char16_t,
		r_char32_t,
		r_consteval,
		r_constinit,				// 120
		r_namespace,
		r_template,
		r_typeid,
		r_typename,
		r_using,				// 125
		r_wchar_t,
		r_ppif,
		r_ppifdef,
		r_ppdefine,
		r_ppundef,				// 130
		r_ppelse,
		backslash,
		r_ppelif,
		r_ppelifdef,
		r_ppifndef,				// 135
		r_ppinclude,
		r_pperror,
		r_ppwarning,
		r_ppline,
		r_pppragma,				// 140
		ellipsis,
		r___LINE__,
		r___FILE__,
		r___VA_OPT__,
		r___VA_ARGS__,				// 145
		opensquarebracketopensquarebracket,
		closesquarebracketclosesquarebracket,
		r_intmax_t,
		r_uintmax_t,
		r_int8_t,				// 150
		r_uint8_t,
		r_int16_t,
		r_uint16_t,
		r_int32_t,
		r_uint32_t,				// 155
		r_int64_t,
		r_uint64_t,
		r_int_least8_t,
		r_uint_least8_t,
		r_int_least16_t,			// 160
		r_uint_least16_t,
		r_int_least32_t,
		r_uint_least32_t,
		r_int_least64_t,
		r_uint_least64_t,			// 165
		r_int_fast8_t,
		r_uint_fast8_t,
		r_int_fast16_t,
		r_uint_fast16_t,
		r_int_fast32_t,				// 170
		r_uint_fast32_t,
		r_int_fast64_t,
		r_uint_fast64_t,
		r_intptr_t,
		r_uintptr_t,				// 175
		r_size_t,
		r_ssize_t,
		r_near,
		r_far,
		r_huge,					// 180
		r___pascal,
		r___watcall,
		r___stdcall,
		r___cdecl,
		r___syscall,				// 185
		r___fastcall,
		r___safecall,
		r___thiscall,
		r___vectorcall,
		r___fortran,				// 190
		r___attribute__,
		r___declspec,
		r_asm, /* GNU asm/__asm__ */
		r___asm, /* MSVC/OpenWatcom _asm __asm */
		r___asm_text,				// 195
		newline,
		pound,
		poundpound,
		backslashnewline,

		__MAX__
	};

// str_name[] = "token" str_name_len = strlen("token")
// str_ppname[] = "#token" str_ppname_len = strlen("#token") str_name = str_ppname + 1 = "token" str_name_len = strlen("token")

// identifiers only
#define DEFX(name) static const char str_##name[] = #name; static constexpr size_t str_##name##_len = sizeof(str_##name) - 1
// __identifiers__ only
#define DEFXUU(name) static const char str___##name##__[] = "__" #name "__"; static constexpr size_t str___##name##___len = sizeof(str___##name##__) - 1
// identifiers and/or #identifiers
#define DEFB(name) static const char str_pp##name[] = "#"#name; static constexpr size_t str_pp##name##_len = sizeof(str_pp##name) - 1; static const char * const str_##name = (str_pp##name)+1; static constexpr size_t str_##name##_len = str_pp##name##_len - 1
	DEFX(alignas);
	DEFX(alignof);
	DEFX(auto);
	DEFX(bool);
	DEFX(break);
	DEFX(case);
	DEFX(char);
	DEFX(const);
	DEFX(constexpr);
	DEFX(continue);
	DEFX(default);
	DEFX(do);
	DEFX(double);
	DEFB(else);
	DEFX(enum);
	DEFX(extern);
	DEFX(false); 
	DEFX(float);
	DEFX(for);
	DEFX(goto);
	DEFB(if);
	DEFX(int);
	DEFX(long);
	DEFX(nullptr);
	DEFX(register);
	DEFX(restrict);
	DEFX(return);
	DEFX(short);
	DEFX(signed);
	DEFX(sizeof);
	DEFX(static);
	DEFX(static_assert);
	DEFX(struct);
	DEFX(switch);
	DEFX(thread_local);
	DEFX(true);
	DEFX(typedef);
	DEFX(typeof);
	DEFX(typeof_unqual);
	DEFX(union);
	DEFX(unsigned);
	DEFX(void);
	DEFX(while);
	DEFX(_Alignas);
	DEFX(_Alignof);
	DEFX(_Atomic);
	DEFX(_BitInt);
	DEFX(_Bool);
	DEFX(_Complex);
	DEFX(_Decimal128);
	DEFX(_Decimal32);
	DEFX(_Decimal64);
	DEFX(_Generic);
	DEFX(_Imaginary);
	DEFX(_Noreturn);
	DEFX(_Static_assert);
	DEFX(_Thread_local);
	DEFX(char8_t);
	DEFX(char16_t);
	DEFX(char32_t);
	DEFX(consteval);
	DEFX(constinit);
	DEFX(namespace);
	DEFX(template);
	DEFX(typeid);
	DEFX(typename);
	DEFX(using);
	DEFX(wchar_t);
	DEFB(ifdef);
	DEFB(define);
	DEFB(undef);
	DEFB(elif);
	DEFB(elifdef);
	DEFB(ifndef);
	DEFB(include);
	DEFB(error);
	DEFB(warning);
	DEFB(line);
	DEFB(pragma);
	DEFXUU(LINE);
	DEFXUU(FILE);
	DEFXUU(VA_OPT);
	DEFXUU(VA_ARGS);
	DEFX(intmax_t);
	DEFX(uintmax_t);
	DEFX(int8_t);
	DEFX(uint8_t);
	DEFX(int16_t);
	DEFX(uint16_t);
	DEFX(int32_t);
	DEFX(uint32_t);
	DEFX(int64_t);
	DEFX(uint64_t);
	DEFX(int_least8_t);
	DEFX(uint_least8_t);
	DEFX(int_least16_t);
	DEFX(uint_least16_t);
	DEFX(int_least32_t);
	DEFX(uint_least32_t);
	DEFX(int_least64_t);
	DEFX(uint_least64_t);
	DEFX(int_fast8_t);
	DEFX(uint_fast8_t);
	DEFX(int_fast16_t);
	DEFX(uint_fast16_t);
	DEFX(int_fast32_t);
	DEFX(uint_fast32_t);
	DEFX(int_fast64_t);
	DEFX(uint_fast64_t);
	DEFX(intptr_t);
	DEFX(uintptr_t);
	DEFX(size_t);
	DEFX(ssize_t);
	DEFX(near);
	DEFX(far);
	DEFX(huge);
	DEFX(__pascal);
	DEFX(__watcall);
	DEFX(__stdcall);
	DEFX(__cdecl);
	DEFX(__syscall);
	DEFX(__fastcall);
	DEFX(__safecall);
	DEFX(__thiscall);
	DEFX(__vectorcall);
	DEFX(__fortran);
	DEFX(__attribute__);
	DEFX(__declspec);
// asm, _asm, __asm, __asm__
	static const char         str___asm__[] = "__asm__";   static constexpr size_t str___asm___len = sizeof(str___asm__) - 1;
	static const char * const str___asm = str___asm__;     static constexpr size_t str___asm_len = sizeof(str___asm__) - 1 - 2;
	static const char * const str__asm = str___asm__+1;    static constexpr size_t str__asm_len = sizeof(str___asm__) - 1 - 2 - 1;
	static const char * const str_asm = str___asm__+2;     static constexpr size_t str_asm_len = sizeof(str___asm__) - 1 - 2 - 2;
// volatile, __volatile__
	static const char         str___volatile__[] = "__volatile__";   static constexpr size_t str___volatile___len = sizeof(str___volatile__) - 1;
	static const char * const str_volatile = str___volatile__+2;     static constexpr size_t str_volatile_len = sizeof(str___volatile__) - 1 - 2 - 2;
// inline, _inline, __inline, __inline__
	static const char         str___inline__[] = "__inline__";   static constexpr size_t str___inline___len = sizeof(str___inline__) - 1;
	static const char * const str___inline = str___inline__;     static constexpr size_t str___inline_len = sizeof(str___inline__) - 1 - 2;
	static const char * const str__inline = str___inline__+1;    static constexpr size_t str__inline_len = sizeof(str___inline__) - 1 - 2 - 1;
	static const char * const str_inline = str___inline__+2;     static constexpr size_t str_inline_len = sizeof(str___inline__) - 1 - 2 - 2;
#undef DEFX

	struct ident2token_t {
		const char*		str;
		uint16_t		len; /* more than enough */
		uint16_t		token; /* make larger in case more than 65535 tokens defined */
	};

#define X(name) { str_##name, str_##name##_len, uint16_t(token_type_t::r_##name) }
#define XAS(name,tok) { str_##name, str_##name##_len, uint16_t(token_type_t::r_##tok) }
#define XUU(name) { str___##name##__, str___##name##___len, uint16_t(token_type_t::r___##name##__) }
	static const ident2token_t ident2tok[] = { // normal tokens
/*                  123456789 */
		X(alignas),
		X(alignof),
		X(auto),
		X(bool),
		X(break),
		X(case),
		X(char),
		X(const),
		X(constexpr),
		X(continue),
		X(default),
		X(do),
		X(double),
		X(else),
		X(enum),
		X(extern),
		X(false), 
		X(float),
		X(for),
		X(goto),
		X(if),
		X(inline),
		X(int),
		X(long),
		X(nullptr),
		X(register),
		X(restrict),
		X(return),
		X(short),
		X(signed),
		X(sizeof),
		X(static),
		X(static_assert),
		X(struct),
		X(switch),
		X(thread_local),
		X(true),
		X(typedef),
		X(typeof),
		X(typeof_unqual),
		X(union),
		X(unsigned),
		X(void),
		X(while),
		X(_Alignas),
		X(_Alignof),
		X(_Atomic),
		X(_BitInt),
		X(_Bool),
		X(_Complex),
		X(_Decimal128),
		X(_Decimal32),
		X(_Decimal64),
		X(_Generic),
		X(_Imaginary),
		X(_Noreturn),
		X(_Static_assert),
		X(_Thread_local),
		X(char8_t),
		X(char16_t),
		X(char32_t),
		X(consteval),
		X(constinit),
		X(namespace),
		X(template),
		X(typeid),
		X(typename),
		X(using),
		X(wchar_t),
		XUU(LINE),
		XUU(FILE),
		XUU(VA_OPT),
		XUU(VA_ARGS),
		X(intmax_t),
		X(uintmax_t),
		X(int8_t),
		X(uint8_t),
		X(int16_t),
		X(uint16_t),
		X(int32_t),
		X(uint32_t),
		X(int64_t),
		X(uint64_t),
		X(int_least8_t),
		X(uint_least8_t),
		X(int_least16_t),
		X(uint_least16_t),
		X(int_least32_t),
		X(uint_least32_t),
		X(int_least64_t),
		X(uint_least64_t),
		X(int_fast8_t),
		X(uint_fast8_t),
		X(int_fast16_t),
		X(uint_fast16_t),
		X(int_fast32_t),
		X(uint_fast32_t),
		X(int_fast64_t),
		X(uint_fast64_t),
		X(intptr_t),
		X(uintptr_t),
		X(size_t),
		X(ssize_t),
		X(near),
		X(far),
		X(huge),
		X(__pascal),
		X(__watcall),
		X(__stdcall),
		X(__cdecl),
		X(__syscall),
		X(__fastcall),
		X(__safecall),
		X(__thiscall),
		X(__vectorcall),
		X(__fortran),
		X(__attribute__),
		X(__declspec),
		XAS(asm,      asm),
		XAS(__asm__,  asm),
		XAS(_asm,     __asm),
		XAS(__asm,    __asm),
		XAS(inline,       inline),
		XAS(_inline,      inline),
		XAS(__inline,     inline),
		XAS(__inline__,   inline),
		XAS(volatile,     volatile),
		XAS(__volatile__, volatile)
	};
	static constexpr size_t ident2tok_length = sizeof(ident2tok) / sizeof(ident2tok[0]);
#undef XUU
#undef XAS
#undef X

#define X(name) { str_##name, str_##name##_len, uint16_t(token_type_t::r_pp##name) }
	static const ident2token_t ppident2tok[] = { // preprocessor tokens
/*                  123456789 */
		X(if),
		X(ifdef),
		X(define),
		X(undef),
		X(else),
		X(elif),
		X(elifdef),
		X(ifndef),
		X(include),
		X(error),
		X(warning),
		X(line),
		X(pragma)
	};
	static constexpr size_t ppident2tok_length = sizeof(ppident2tok) / sizeof(ppident2tok[0]);
#undef X

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
		"identifier",
		"comma",				// 20
		"pipeequals",
		"caretequals",
		"ampersandequals",
		"plusequals",
		"minusequals",				// 25
		"star",
		"forwardslash",
		"starequals",
		"forwardslashequals",
		"percent",				// 30
		"percentequals",
		"exclamationequals",
		"exclamation",
		"question",
		"colon",				// 35
		"lessthan",
		"lessthanlessthan",
		"lessthanlessthanequals",
		"lessthanequals",
		"lessthanequalsgreaterthan",		// 40
		"greaterthan",
		"greaterthangreaterthan",
		"greaterthangreaterthanequals",
		"greaterthanequals",
		"minusrightanglebracket",		// 45
		"minusrightanglebracketstar",
		"period",
		"periodstar",
		"opensquarebracket",
		"closesquarebracket",			// 50
		"opencurlybracket",
		"closecurlybracket",
		"openparenthesis",
		"closeparenthesis",
		"coloncolon",				// 55
		"ppidentifier",
		str_alignas,
		str_alignof,
		str_auto,
		str_bool,				// 60
		str_break,
		str_case,
		str_char,
		str_const,
		str_constexpr,				// 65
		str_continue,
		str_default,
		str_do,
		str_double,
		str_else,				// 70
		str_enum,
		str_extern,
		str_false,
		str_float,
		str_for,				// 75
		str_goto,
		str_if,
		str___inline__,
		str_int,
		str_long,				// 80
		str_nullptr,
		str_register,
		str_restrict,
		str_return,
		str_short,				// 85
		str_signed,
		str_sizeof,
		str_static,
		str_static_assert,
		str_struct,				// 90
		str_switch,
		str_thread_local,
		str_true,
		str_typedef,
		str_typeof,				// 95
		str_typeof_unqual,
		str_union,
		str_unsigned,
		str_void,
		str___volatile__,			// 100
		str_while,
		str__Alignas,
		str__Alignof,
		str__Atomic,
		str__BitInt,				// 105
		str__Bool,
		str__Complex,
		str__Decimal128,
		str__Decimal32,
		str__Decimal64,				// 110
		str__Generic,
		str__Imaginary,
		str__Noreturn,
		str__Static_assert,
		str__Thread_local,			// 115
		str_char8_t,
		str_char16_t,
		str_char32_t,
		str_consteval,
		str_constinit,				// 120
		str_namespace,
		str_template,
		str_typeid,
		str_typename,
		str_using,				// 125
		str_wchar_t,
		str_ppif,
		str_ppifdef,
		str_ppdefine,
		str_ppundef,				// 130
		str_ppelse,
		"backslash",
		str_ppelif,
		str_ppelifdef,
		str_ppifndef,				// 135
		str_ppinclude,
		str_pperror,
		str_ppwarning,
		str_ppline,
		str_pppragma,				// 140
		"ellipsis",
		str___LINE__,
		str___FILE__,
		str___VA_OPT__,
		str___VA_ARGS__,			// 145
		"opensquarebracketopensquarebracket",
		"closesquarebracketclosesquarebracket",
		str_intmax_t,
		str_uintmax_t,
		str_int8_t,				// 150
		str_uint8_t,
		str_int16_t,
		str_uint16_t,
		str_int32_t,
		str_uint32_t,				// 155
		str_int64_t,
		str_uint64_t,
		str_int_least8_t,
		str_uint_least8_t,
		str_int_least16_t,			// 160
		str_uint_least16_t,
		str_int_least32_t,
		str_uint_least32_t,
		str_int_least64_t,
		str_uint_least64_t,			// 165
		str_int_fast8_t,
		str_uint_fast8_t,
		str_int_fast16_t,
		str_uint_fast16_t,
		str_int_fast32_t,			// 170
		str_uint_fast32_t,
		str_int_fast64_t,
		str_uint_fast64_t,
		str_intptr_t,
		str_uintptr_t,				// 175
		str_size_t,
		str_ssize_t,
		str_near,
		str_far,
		str_huge,				// 180
		str___pascal,
		str___watcall,
		str___stdcall,
		str___cdecl,
		str___syscall,				// 185
		str___fastcall,
		str___safecall,
		str___thiscall,
		str___vectorcall,
		str___fortran,				// 190
		str___attribute__,
		str___declspec,
		"asm",
		"__asm",
		"asm_text",				// 195
		"newline",
		"pound",
		"poundpound",
		"backslashnewline"
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

		bool copy_from(const charstrliteral_t &x) {
			free();
			type = x.type;
			assert(data == NULL);
			assert(length == 0);
			assert(allocated == 0);

			if (x.length != 0) {
				if (!alloc(x.length))
					return false;

				assert(data != NULL);
				assert(x.data != NULL);
				assert(length == x.length);
				memcpy(data,x.data,x.length);
			}

			return true;
		}

		const char *as_char(void) const { return (const char*)data; }
		const unsigned char *as_binary(void) const { return (const unsigned char*)data; }
		const unsigned char *as_utf8(void) const { return (const unsigned char*)data; }
		const uint16_t *as_u16(void) const { return (const uint16_t*)data; }
		const uint32_t *as_u32(void) const { return (const uint32_t*)data; }

		bool operator==(const charstrliteral_t &rhs) const {
			if (length != rhs.length) return false;

			if ((type == type_t::CHAR || type == type_t::UTF8) == (rhs.type == type_t::CHAR || rhs.type == type_t::UTF8)) { /* OK */ }
			else if (type != rhs.type) return false;

			return memcmp(data,rhs.data,length) == 0;
		}
		inline bool operator!=(const charstrliteral_t &rhs) const { return !(*this == rhs); }

		bool operator==(const std::string &rhs) const {
			if (length != rhs.size() || !(type == type_t::CHAR || type == type_t::UTF8)) return false;
			return memcmp(data,rhs.data(),length) == 0;
		}
		inline bool operator!=(const std::string &rhs) const { return !(*this == rhs); }

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

			/* NTS: GNU glibc realloc(data,0) frees the buffer */
			if (sz == 0) {
				free();
			}
			else if (sz > allocated) {
				void *np = ::realloc(data,sz);
				if (np == NULL)
					return false;

				data = np;
				allocated = sz;
			}

			length = sz;
			return true;
		}

		static std::string to_escape(const uint32_t c) {
			char tmp[32];

			sprintf(tmp,"\\x%02lx",(unsigned long)c);
			return std::string(tmp);
		}

		bool shrinkfit(void) {
			if (data) {
				if (allocated > length) {
					/* NTS: GNU glibc realloc(data,0) frees the buffer */
					if (length == 0) {
						free();
					}
					else {
						void *np = ::realloc(data,length);
						if (np == NULL)
							return false;

						data = np;
						allocated = length;
					}
				}
			}

			return true;
		}

		std::string makestring(void) const {
			if (data) {
				std::string s;

				switch (type) {
					case type_t::CHAR: {
						const unsigned char *p = as_binary();
						const unsigned char *f = p + length;
						while (p < f) {
							if (*p < 0x20 || *p >= 0x80)
								s += to_escape(*p++);
							else
								s += (char)(*p++);
						}
						break; }
					case type_t::UTF8: {
						const unsigned char *p = as_binary();
						const unsigned char *f = p + length;
						while (p < f) {
							const int32_t v = p_utf8_decode(p,f);
							if (v < 0l)
								s += "?";
							else if (v < 0x20l || v > 0x10FFFFl)
								s += to_escape(v);
							else
								s += utf8_to_str((uint32_t)v);
						}
						break; }
					case type_t::UNICODE16: {
						const uint16_t *p = as_u16();
						const uint16_t *f = p + units();
						while (p < f) {
							const int32_t v = p_utf16_decode(p,f);
							if (v < 0l)
								s += "?";
							else if (v < 0x20l || v > 0x10FFFFl)
								s += to_escape(v);
							else
								s += utf8_to_str((uint32_t)v);
						}
						break; }
					case type_t::UNICODE32: {
						const uint32_t *p = as_u32();
						const uint32_t *f = p + units();
						while (p < f) {
							if (*p < 0x20u || *p > 0x10FFFFu)
								s += to_escape(*p++);
							else
								s += utf8_to_str(*p++);
						}
						break; }
					default:
						break;
				}

				return s;
			}

			return std::string();
		}

		std::string to_str(void) const {
			std::string s;
			char tmp[128];

			if (data) {
				s += "v=\"";
				s += makestring();
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

	struct identifier_str_t {
		unsigned char*		data;
		size_t			length;

		identifier_str_t() : data(NULL), length(0) { }
		identifier_str_t(const identifier_str_t &x) = delete;
		identifier_str_t &operator=(const identifier_str_t &x) = delete;

		identifier_str_t(identifier_str_t &&x) { common_move(x); }
		identifier_str_t &operator=(identifier_str_t &&x) { common_move(x); return *this; }

		~identifier_str_t() { free(); }

		void free(void) {
			if (data) { ::free(data); data = NULL; }
			length = 0;
		}

		void common_move(identifier_str_t &other) {
			data = other.data; other.data = NULL;
			length = other.length; other.length = 0;
		}

		void take_from(charstrliteral_t &l) {
			free();
			if (l.type == charstrliteral_t::type_t::CHAR || l.type == charstrliteral_t::type_t::UTF8) {
				data = (unsigned char*)l.data; l.data = NULL;
				length = l.length; l.length = 0;
			}
		}

		std::string to_str(void) const {
			if (data != NULL && length != 0) return std::string((char*)data,length);
			return std::string();
		}

		bool operator==(const identifier_str_t &rhs) const {
			if (length != rhs.length) return false;
			if (length != 0) return memcmp(data,rhs.data,length) == 0;
			return true;
		}
		bool operator!=(const identifier_str_t &rhs) const { return !(*this == rhs); }

		bool operator==(const charstrliteral_t &rhs) const {
			if (length != rhs.length) return false;
			if (length != 0) return memcmp(data,rhs.data,length) == 0;
			return true;
		}
		bool operator!=(const charstrliteral_t &rhs) const { return !(*this == rhs); }
	};

	struct token_t {
		token_type_t		type = token_type_t::none;
		position_t		pos;

		token_t() : pos(1) { common_init(); }
		~token_t() { common_delete(); }
		token_t(const token_t &t) { common_copy(t); }
		token_t(token_t &&t) { common_move(t); }
		token_t(const token_type_t t) : type(t) { common_init(); }
		token_t &operator=(const token_t &t) { common_copy(t); return *this; }
		token_t &operator=(token_t &&t) { common_move(t); return *this; }

		union {
			integer_value_t		integer; /* token_type_t::integer */
			floating_value_t	floating; /* token_type_t::floating */
			charstrliteral_t	strliteral; /* token_type_t::charliteral/strliteral/identifier/asm */
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
				case token_type_t::ppidentifier:
				case token_type_t::r___asm_text:
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
				case token_type_t::ppidentifier:
				case token_type_t::r___asm_text:
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
				case token_type_t::ppidentifier:
				case token_type_t::r___asm_text:
					v.strliteral.init();
					break;
				default:
					break;
			}
		}

		void common_copy(const token_t &x) {
			common_delete();
			type = x.type;

			switch (type) {
				case token_type_t::charliteral:
				case token_type_t::strliteral:
				case token_type_t::identifier:
				case token_type_t::ppidentifier:
				case token_type_t::r___asm_text:
					v.strliteral.init();
					v.strliteral.copy_from(x.v.strliteral);
					break;
				default:
					v = x.v;
					break;
			}
		}

		void common_move(token_t &x) {
			common_delete();
			type = x.type; x.type = token_type_t::none;
			pos = x.pos; x.pos = position_t();
			v = x.v; /* x.type == none so pointers no longer matter */
		}
	};

	////////////////////////////////////////////////////////////////////

	bool is_newline(const unsigned char b) {
		return b == '\r' || b == '\n';
	}

	bool is_whitespace(const unsigned char b) {
		return b == ' ' || b == '\t';
	}

	void eat_whitespace(rbuf &buf,source_file_object &sfo) {
		do {
			if (buf.data_avail() < 1) rbuf_sfd_refill(buf,sfo);
			if (is_whitespace(buf.peekb())) buf.discardb();
			else break;
		} while (1);
	}

	void eat_newline(rbuf &buf,source_file_object &sfo) {
		if (buf.data_avail() < 4) rbuf_sfd_refill(buf,sfo);
		if (buf.peekb() == '\r') buf.discardb();
		if (buf.peekb() == '\n') buf.discardb();
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

	bool is_asm_text_first_char(unsigned char c) {
		return (c == '_' || c == '.' || c == '@') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
	}

	bool is_asm_text_char(unsigned char c) {
		return (c == '_' || c == '.' || c == '@') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
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
				case '\r':
					if (buf.peekb() == '\n') buf.discardb(); /* \r\n */
					return unicode_nothing;
				case '\n':
					return unicode_nothing;
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

	template <const charstrliteral_t::type_t cslt,typename ptrat> int lgtok_strlit_wrch(ptrat* &p,ptrat* const f,const unicode_char_t v) = delete;

	template <> int lgtok_strlit_wrch<charstrliteral_t::type_t::CHAR,unsigned char>(unsigned char* &p,unsigned char* const f,const unicode_char_t v) {
		if (v < 0x00 || v > 0xFF)
			return errno_return(EINVAL);

		assert((p+1) <= f);
		*p++ = (unsigned char)v;
		return 1;
	}

	template <> int lgtok_strlit_wrch<charstrliteral_t::type_t::UTF8,unsigned char>(unsigned char* &p,unsigned char* const f,const unicode_char_t v) {
		if (v < 0x00)
			return errno_return(EINVAL);

		utf8_to_str(p,f,v);
		assert(p <= f);
		return 1;
	}

	template <> int lgtok_strlit_wrch<charstrliteral_t::type_t::UNICODE16,uint16_t>(uint16_t* &p,uint16_t* const f,const unicode_char_t v) {
		if (v < 0x00l || v > 0x20FFFFl)
			return errno_return(EINVAL);

		utf16_to_str(p,f,v);
		assert(p <= f);
		return 1;
	}

	template <> int lgtok_strlit_wrch<charstrliteral_t::type_t::UNICODE32,uint32_t>(uint32_t* &p,uint32_t* const f,const unicode_char_t v) {
		if (v < 0x00)
			return errno_return(EINVAL);

		assert((p+1) <= f);
		*p++ = (uint32_t)v;
		return 1;
	}

	template <const charstrliteral_t::type_t cslt,typename ptrat> int lgtok_strlit_common(rbuf &buf,source_file_object &sfo,token_t &t,const unsigned char separator) {
		assert(t.type == token_type_t::charliteral || t.type == token_type_t::strliteral);
		const bool unicode = !(cslt == charstrliteral_t::type_t::CHAR);
		ptrat *p,*f;
		int32_t v;
		int rr;

		if (!t.v.strliteral.alloc(64))
			return errno_return(ENOMEM);

		p = (ptrat*)((char*)t.v.strliteral.data);
		f = (ptrat*)((char*)t.v.strliteral.data+t.v.strliteral.length);
		do {
			if (buf.peekb() == separator) {
				buf.discardb();
				break;
			}

			if ((p+16) >= f) {
				const size_t wo = size_t(p-((ptrat*)t.v.strliteral.data));
				const size_t ns = t.v.strliteral.units() + (t.v.strliteral.units() / 2u);
				assert((t.v.strliteral.units()*sizeof(ptrat)) == t.v.strliteral.length);

				if (!t.v.strliteral.realloc(ns*sizeof(ptrat)))
					return errno_return(ENOMEM);

				p = ((ptrat*)((char*)t.v.strliteral.data)) + wo;
				f =  (ptrat*)((char*)t.v.strliteral.data+t.v.strliteral.length);
			}

			v = lgtok_cslitget(buf,sfo,unicode);
			if (v == unicode_nothing) continue;
			if ((rr=lgtok_strlit_wrch<cslt,ptrat>(p,f,v)) <= 0) return rr;
		} while(1);

		{
			const size_t fo = size_t(p-((ptrat*)t.v.strliteral.data)) * sizeof(ptrat);
			assert(fo <= t.v.strliteral.allocated);
			t.v.strliteral.length = fo;
			t.v.strliteral.shrinkfit();
		}

		return 1;
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

			switch (cslt) {
				case charstrliteral_t::type_t::CHAR:
					return lgtok_strlit_common<charstrliteral_t::type_t::CHAR,unsigned char>(buf,sfo,t,separator);
				case charstrliteral_t::type_t::UTF8:
					return lgtok_strlit_common<charstrliteral_t::type_t::UTF8,unsigned char>(buf,sfo,t,separator);
				case charstrliteral_t::type_t::UNICODE16:
					return lgtok_strlit_common<charstrliteral_t::type_t::UNICODE16,uint16_t>(buf,sfo,t,separator);
				case charstrliteral_t::type_t::UNICODE32:
					return lgtok_strlit_common<charstrliteral_t::type_t::UNICODE32,uint32_t>(buf,sfo,t,separator);
				default:
					break;
			}

			return 1;
		}
		else {
			return errno_return(EINVAL);
		}
	}

	struct lgtok_state_t {
		unsigned int		flags = 0;
		unsigned int		curlies = 0;

		static constexpr unsigned int FL_MSASM = (1u << 0u); /* __asm ... */
		static constexpr unsigned int FL_NEWLINE = (1u << 1u);
	};

	int lgtok_asm_text(lgtok_state_t &lst,rbuf &buf,source_file_object &sfo,token_t &t) {
		(void)lst;

		position_t pos = t.pos;

		assert(t.type == token_type_t::none);
		t.type = token_type_t::r___asm_text;
		t.v.strliteral.init();
		t.v.strliteral.type = charstrliteral_t::type_t::CHAR;

		if (!t.v.strliteral.alloc(32))
			return errno_return(ENOMEM);

		unsigned char *p,*f;

		p = (unsigned char*)t.v.strliteral.data;
		f = (unsigned char*)t.v.strliteral.data+t.v.strliteral.length;

		assert(p < f);
		assert(is_asm_text_first_char(buf.peekb()) || buf.peekb() == '#');
		rbuf_sfd_refill(buf,sfo);
		if (buf.peekb() == '#') {
			t.type = token_type_t::ppidentifier;
			buf.discardb();
			while (is_whitespace(buf.peekb())) {
				if (is_newline(buf.peekb()))
					return errno_return(EINVAL);

				buf.discardb();
			}
		}
		*p++ = buf.getb();
		while (is_asm_text_char(buf.peekb())) {
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
				t = token_t(); t.pos = pos; return lgtok_charstrlit(buf,sfo,t,charstrliteral_t::type_t::UTF8);
			}
			else if (t.v.strliteral.length == 1) {
				if (*((const char*)t.v.strliteral.data) == 'U') {
					t = token_t(); t.pos = pos; return lgtok_charstrlit(buf,sfo,t,charstrliteral_t::type_t::UNICODE32);
				}
				else if (*((const char*)t.v.strliteral.data) == 'u') {
					t = token_t(); t.pos = pos; return lgtok_charstrlit(buf,sfo,t,charstrliteral_t::type_t::UNICODE16);
				}
				else if (*((const char*)t.v.strliteral.data) == 'L') {
					/* FIXME: A "wide" char varies between targets i.e. Windows wide char is 16 bits,
					 *        Linux wide char is 32 bits. */
					t = token_t(); t.pos = pos; return lgtok_charstrlit(buf,sfo,t,charstrliteral_t::type_t::UNICODE32);
				}
			}
		}

		/* it might be _asm or __asm */
		if (	(t.v.strliteral.length == 4 && !memcmp(t.v.strliteral.data,"_asm",4)) ||
			(t.v.strliteral.length == 5 && !memcmp(t.v.strliteral.data,"__asm",5))) {
			t = token_t(token_type_t(token_type_t::r___asm));
		}

		return 1;
	}

	int lgtok_identifier(lgtok_state_t &lst,rbuf &buf,source_file_object &sfo,token_t &t) {
		position_t pos = t.pos;

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
		assert(is_identifier_first_char(buf.peekb()) || buf.peekb() == '#');
		rbuf_sfd_refill(buf,sfo);
		if (buf.peekb() == '#') {
			t.type = token_type_t::ppidentifier;
			buf.discardb();
			while (is_whitespace(buf.peekb())) {
				if (is_newline(buf.peekb()))
					return errno_return(EINVAL);

				buf.discardb();
			}
		}
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
				t = token_t(); t.pos = pos; return lgtok_charstrlit(buf,sfo,t,charstrliteral_t::type_t::UTF8);
			}
			else if (t.v.strliteral.length == 1) {
				if (*((const char*)t.v.strliteral.data) == 'U') {
					t = token_t(); t.pos = pos; return lgtok_charstrlit(buf,sfo,t,charstrliteral_t::type_t::UNICODE32);
				}
				else if (*((const char*)t.v.strliteral.data) == 'u') {
					t = token_t(); t.pos = pos; return lgtok_charstrlit(buf,sfo,t,charstrliteral_t::type_t::UNICODE16);
				}
				else if (*((const char*)t.v.strliteral.data) == 'L') {
					/* FIXME: A "wide" char varies between targets i.e. Windows wide char is 16 bits,
					 *        Linux wide char is 32 bits. */
					t = token_t(); t.pos = pos; return lgtok_charstrlit(buf,sfo,t,charstrliteral_t::type_t::UNICODE32);
				}
			}
		}

		/* it might be a reserved keyword, check */
		if (t.type == token_type_t::ppidentifier) {
			for (const ident2token_t *i2t=ppident2tok;i2t < (ppident2tok+ppident2tok_length);i2t++) {
				if (t.v.strliteral.length == i2t->len) {
					if (!memcmp(t.v.strliteral.data,i2t->str,i2t->len)) {
						t = token_t(token_type_t(i2t->token)); t.pos = pos;
						return 1;
					}
				}
			}
		}
		else {
			for (const ident2token_t *i2t=ident2tok;i2t < (ident2tok+ident2tok_length);i2t++) {
				if (t.v.strliteral.length == i2t->len) {
					if (!memcmp(t.v.strliteral.data,i2t->str,i2t->len)) {
						t = token_t(token_type_t(i2t->token)); t.pos = pos;
						if (t.type == token_type_t::r___asm) {
							lst.flags |= lgtok_state_t::FL_MSASM;
							lst.curlies = 0;
						}

						return 1;
					}
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

	int eat_c_comment(unsigned int level,rbuf &buf,source_file_object &sfo) {
		/* caller already ate the / and the * if level == 1 */
		assert(level == 0 || level == 1);

		do {
			if (buf.data_avail() < 8) rbuf_sfd_refill(buf,sfo);

			if (buf.peekb(0) == '/' && buf.peekb(1) == '*') {
				buf.discardb(2);
				level++;
			}
			else if (buf.peekb(0) == '*' && buf.peekb(1) == '/') {
				buf.discardb(2);
				if (--level == 0) break;
			}
			else {
				buf.discardb();
			}
		} while(1);

		return 1;
	}

	int eat_cpp_comment(rbuf &buf,source_file_object &sfo) {
		/* caller already ate the / and the / */

		do {
			if (buf.data_avail() < 8) rbuf_sfd_refill(buf,sfo);

			if (buf.peekb() == '\r' || buf.peekb() == '\n')
				break;
			else
				buf.discardb();
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
try_again:	t = token_t();

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
				if (lst.flags & lgtok_state_t::FL_MSASM) {
					/* ASM directives like .386p .flat, etc */
					return lgtok_asm_text(lst,buf,sfo,t);
				}
				else {
					t.type = token_type_t::period; buf.discardb();
					if (buf.peekb() == '*') { t.type = token_type_t::periodstar; buf.discardb(); } /* .* */
					else if (buf.peekb(0) == '.' && buf.peekb(1) == '.') { t.type = token_type_t::ellipsis; buf.discardb(2); } /* ... */
				}
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
				if (lst.flags & lgtok_state_t::FL_MSASM) {
					lst.curlies++;
				}
				break;
			case '}':
				t.type = token_type_t::closecurlybracket; buf.discardb();
				if (lst.flags & lgtok_state_t::FL_MSASM) {
					if (--lst.curlies == 0)
						lst.flags &= ~lgtok_state_t::FL_MSASM;
				}
				break;
			case '(':
				t.type = token_type_t::openparenthesis; buf.discardb();
				break;
			case ')':
				t.type = token_type_t::closeparenthesis; buf.discardb();
				break;
			case '<':
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
					return lgtok_identifier(lst,buf,sfo,t);
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
				if (lst.flags & lgtok_state_t::FL_MSASM) {
					if (lst.curlies == 0)
						lst.flags &= ~lgtok_state_t::FL_MSASM;
				}
				break;
			case '0': case '1': case '2': case '3': case '4':
			case '5': case '6': case '7': case '8': case '9':
				return lgtok_number(buf,sfo,t);
			default:
				if ((lst.flags & lgtok_state_t::FL_MSASM) && is_asm_text_first_char(buf.peekb()))
					return lgtok_asm_text(lst,buf,sfo,t);
				else if (is_identifier_first_char(buf.peekb()))
					return lgtok_identifier(lst,buf,sfo,t);
				else
					return errno_return(ESRCH);
		}

		return 1;
	}

	struct pptok_macro_t {
		std::vector<token_t>		tokens;
	};

	struct pptok_state_t {
		struct pptok_macro_ent_t {
			pptok_macro_t		ment;
			identifier_str_t	name;
			pptok_macro_ent_t*	next;
		};

		unsigned int macro_expansion_counter = 0; /* to prevent runaway expansion */
		std::deque<token_t> macro_expansion;

		template <typename idT> const pptok_macro_ent_t* lookup_macro(const idT &i) const {
			const pptok_macro_ent_t *p = macro_buckets[macro_hash_id(i)];
			while (p != NULL) {
				if (p->name == i)
					return p;

				p = p->next;
			}

			return NULL;
		}

		bool create_macro(identifier_str_t &i,pptok_macro_t &m) {
			pptok_macro_ent_t **p = &macro_buckets[macro_hash_id(i)];

			while ((*p) != NULL) {
				if ((*p)->name == i)
					return false; /* already exists */

				p = &((*p)->next);
			}

			(*p) = new pptok_macro_ent_t;
			(*p)->ment = std::move(m);
			(*p)->name = std::move(i);
			(*p)->next = NULL;
			return true;
		}

		bool delete_macro(const identifier_str_t &i) {
			pptok_macro_ent_t **p = &macro_buckets[macro_hash_id(i)];
			while ((*p) != NULL) {
				if ((*p)->name == i) {
					pptok_macro_ent_t* d = *p;
					(*p) = (*p)->next;
					delete d;
					return true;
				}

				p = &((*p)->next);
			}

			return false;
		}

		static constexpr size_t macro_bucket_count = 4096u / sizeof(char*); /* power of 2 */
		static_assert((macro_bucket_count & (macro_bucket_count - 1)) == 0, "must be power of 2"); /* is power of 2 */

		void free_macro_bucket(const unsigned int bucket) {
			pptok_macro_ent_t *d;

			while ((d=macro_buckets[bucket]) != NULL) {
				macro_buckets[bucket] = d->next;
				delete d;
			}
		}

		void free_macros(void) {
			for (unsigned int i=0;i < macro_bucket_count;i++)
				free_macro_bucket(i);
		}

		static uint8_t macro_hash_id(const unsigned char *data,const size_t len) {
			unsigned int h = 0x2222;

			for (size_t c=0;c < len;c++)
				h = (h ^ (h << 9)) + data[c];

			h ^= (h >> 16);
			h ^= ~(h >> 8);
			h ^= (h >> 4) ^ 3;
			return h & (macro_bucket_count - 1u);
		}

		inline uint8_t macro_hash_id(const identifier_str_t &i) const {
			return macro_hash_id(i.data,i.length);
		}

		static uint8_t macro_hash_id(const charstrliteral_t &i) {
			return macro_hash_id((const unsigned char*)i.data,i.length);
		}

		pptok_macro_ent_t* macro_buckets[macro_bucket_count] = { NULL };

		pptok_state_t() { }
		pptok_state_t(const pptok_state_t &) = delete;
		pptok_state_t &operator=(const pptok_state_t &) = delete;
		pptok_state_t(pptok_state_t &&) = delete;
		pptok_state_t &operator=(pptok_state_t &&) = delete;
		~pptok_state_t() { free_macros(); }
	};

	int pptok_lgtok(pptok_state_t &pst,lgtok_state_t &lst,rbuf &buf,source_file_object &sfo,token_t &t) {
		if (!pst.macro_expansion.empty()) {
			t = std::move(pst.macro_expansion.front());
			pst.macro_expansion.pop_front();
			return 1;
		}
		else {
			return lgtok(lst,buf,sfo,t);
		}
	}

	int pptok_undef(pptok_state_t &pst,lgtok_state_t &lst,rbuf &buf,source_file_object &sfo,token_t &t) {
		/* #undef has already been parsed.
		 * the last token we didn't use is left in &t for the caller to parse as most recently obtained,
		 * unless set to token_type_t::none in which case it will fetch another one */
		identifier_str_t s_id;
		pptok_macro_t macro;
		int r;

		(void)pst;

		if ((r=pptok_lgtok(pst,lst,buf,sfo,t)) < 1)
			return r;

		if (t.type != token_type_t::identifier)
			return errno_return(EINVAL);
		s_id.take_from(t.v.strliteral);

		do {
			if ((r=pptok_lgtok(pst,lst,buf,sfo,t)) < 1)
				return r;

			if (t.type == token_type_t::newline) {
				t = token_t();
				break;
			}

			/* ignore trailing tokens */
		} while (1);

#if 1//DEBUG
		fprintf(stderr,"UNDEF '%s'\n",s_id.to_str().c_str());
#endif

		if (!pst.delete_macro(s_id)) {
#if 1//DEBUG
			fprintf(stderr,"  Macro wasn't defined anyway\n");
#endif
		}

		return 1;
	}

	bool pptok_define_allowed_token(const token_t &t) {
		switch (t.type) {
			/* NTS: lgtok() parsing pretty much prevents these tokens entirely within a #define.
			 *      even so, make sure! */
			case token_type_t::r_ppif:
			case token_type_t::r_ppifdef:
			case token_type_t::r_ppdefine:
			case token_type_t::r_ppundef:
			case token_type_t::r_ppelse:
			case token_type_t::r_ppelif:
			case token_type_t::r_ppelifdef:
			case token_type_t::r_ppifndef:
			case token_type_t::r_ppinclude:
			case token_type_t::r_pperror:
			case token_type_t::r_ppwarning:
			case token_type_t::r_ppline:
			case token_type_t::r_pppragma:
				return false;
			default:
				break;
		}

		return true;
	}

	int pptok_define(pptok_state_t &pst,lgtok_state_t &lst,rbuf &buf,source_file_object &sfo,token_t &t) {
		/* #define has already been parsed.
		 * the last token we didn't use is left in &t for the caller to parse as most recently obtained,
		 * unless set to token_type_t::none in which case it will fetch another one */
		identifier_str_t s_id;
		pptok_macro_t macro;
		int r;

		(void)pst;

		if ((r=pptok_lgtok(pst,lst,buf,sfo,t)) < 1)
			return r;

		if (t.type != token_type_t::identifier)
			return errno_return(EINVAL);
		s_id.take_from(t.v.strliteral);

		do {
			if ((r=pptok_lgtok(pst,lst,buf,sfo,t)) < 1)
				return r;

			if (t.type == token_type_t::newline) {
				t = token_t();
				break;
			}
			else if (t.type == token_type_t::backslashnewline) { /* \ + newline continues the macro past newline */
				macro.tokens.push_back(token_t(token_type_t::newline));
			}
			else if (pptok_define_allowed_token(t)) {
				macro.tokens.push_back(std::move(t));
			}
			else {
				return errno_return(EINVAL);
			}
		} while (1);

#if 1//DEBUG
		fprintf(stderr,"MACRO '%s'\n",s_id.to_str().c_str());
		fprintf(stderr,"  tokens:\n");
		for (auto i=macro.tokens.begin();i!=macro.tokens.end();i++)
			fprintf(stderr,"    > %s\n",(*i).to_str().c_str());
#endif

		if (!pst.create_macro(s_id,macro))
			return errno_return(EEXIST);

		return 1;
	}

	int pptok(pptok_state_t &pst,lgtok_state_t &lst,rbuf &buf,source_file_object &sfo,token_t &t) {
		int r;

#define TRY_AGAIN \
		if (t.type != token_type_t::none) \
			goto try_again_w_token; \
		else \
			goto try_again;

try_again:
		if ((r=pptok_lgtok(pst,lst,buf,sfo,t)) < 1)
			return r;

try_again_w_token:
		switch (t.type) {
			case token_type_t::r_ppdefine:
				if ((r=pptok_define(pst,lst,buf,sfo,t)) < 1)
					return r;

				TRY_AGAIN; /* does not fall through */
			case token_type_t::r_ppundef:
				if ((r=pptok_undef(pst,lst,buf,sfo,t)) < 1)
					return r;

				TRY_AGAIN; /* does not fall through */
			case token_type_t::identifier: /* macro substitution */
			case token_type_t::r___asm_text: { /* to allow macros to work with assembly language */
				const pptok_state_t::pptok_macro_ent_t* macro = pst.lookup_macro(t.v.strliteral);
				if (macro) {
#if 1//DEBUG
					fprintf(stderr,"Hello macro '%s'\n",t.v.strliteral.to_str().c_str());
#endif

					if (pst.macro_expansion_counter > 1024) {
						fprintf(stderr,"Too many macro expansions\n");
						return errno_return(ELOOP);
					}

					/* inject tokens from macro */
					for (auto i=macro->ment.tokens.rbegin();i!=macro->ment.tokens.rend();i++)
						pst.macro_expansion.push_front(*i);

					pst.macro_expansion_counter++;
					goto try_again;
				}
				break; }
			default:
				break;
		}

#undef TRY_AGAIN

		return 1;
	}

}

enum test_mode_t {
	TEST_NONE=0,
	TEST_SFO,
	TEST_RBF,
	TEST_RBFGC,
	TEST_RBFGCNU,
	TEST_LGTOK,
	TEST_PPTOK
};

static std::vector<std::string>		main_input_files;
static enum test_mode_t			test_mode = TEST_NONE;

static void help(void) {
	fprintf(stderr,"cimcc [options] [input file [...]]\n");
	fprintf(stderr,"  --test <none|sfo|rbf|rbfgc|rbfgcnu|lgtok|pptok>         Test mode\n");
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
				else if (!strcmp(a,"lgtok"))
					test_mode = TEST_LGTOK;
				else if (!strcmp(a,"pptok"))
					test_mode = TEST_PPTOK;
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
		else if (test_mode == TEST_LGTOK) {
			CIMCC::lgtok_state_t lst;
			CIMCC::token_t tok;
			CIMCC::rbuf rb;
			int r;

			assert(rb.allocate());
			while ((r=CIMCC::lgtok(lst,rb,*sfo,tok)) > 0) {
				printf("Token:");
				if (tok.pos.row > 0) printf(" pos:row=%u,col=%u,ofs=%u",tok.pos.row,tok.pos.col,tok.pos.ofs);
				printf(" %s\n",tok.to_str().c_str());
			}

			if (r < 0) {
				fprintf(stderr,"Read error from %s, error %d\n",sfo->getname(),(int)r);
				return -1;
			}
		}
		else if (test_mode == TEST_PPTOK) {
			CIMCC::lgtok_state_t lst;
			CIMCC::pptok_state_t pst;
			CIMCC::token_t tok;
			CIMCC::rbuf rb;
			int r;

			assert(rb.allocate());
			while ((r=CIMCC::pptok(pst,lst,rb,*sfo,tok)) > 0) {
				printf("Token:");
				if (tok.pos.row > 0) printf(" pos:row=%u,col=%u,ofs=%u",tok.pos.row,tok.pos.col,tok.pos.ofs);
				printf(" %s\n",tok.to_str().c_str());
			}

			if (r < 0) {
				fprintf(stderr,"Read error from %s, error %d\n",sfo->getname(),(int)r);
				return -1;
			}
		}

		assert(sfo != NULL);
		delete sfo;
	}

	if (test_mode == TEST_SFO || test_mode == TEST_LGTOK || test_mode == TEST_RBF || test_mode == TEST_RBFGC || test_mode == TEST_RBFGCNU) return 0;

	return 0;
}

