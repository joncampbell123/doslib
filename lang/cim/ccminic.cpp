
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

#include <unordered_map>
#include <type_traits>
#include <stdexcept>
#include <memory>
#include <vector>
#include <string>
#include <limits>
#include <stack>
#include <deque>
#include <new>

#ifndef O_BINARY
# define O_BINARY 0
#endif

namespace CCMiniC {

	static constexpr size_t no_source_file = ~size_t(0);
	static_assert( ~no_source_file == size_t(0), "oops" );

	static constexpr int errno_return(int e) {
		return (e >= 0) ? (-e) : (e); /* whether errno values are positive or negative, return as negative */
	}

	//////////////////////////////////////////////////////////////////

#define CCMiniC_SOURCE_OBJ_NO_COPY_ONLY_MOVE(sfclass) \
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

						CCMiniC_SOURCE_OBJ_NO_COPY_ONLY_MOVE(source_file_object);
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

						CCMiniC_SOURCE_OBJ_NO_COPY_ONLY_MOVE(source_fd);
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

	struct source_file_t {
		std::string		path;
		uint16_t		refcount = 0;

		bool empty(void) const {
			return refcount == 0 && path.empty();
		}
		void clear(void) {
			path.clear();
			refcount = 0;
		}
	};

	std::vector<source_file_t> source_files;

	size_t alloc_source_file(const std::string &path) {
		size_t i=0;

		for (;i < source_files.size();i++) {
			if (source_files[i].path == path)
				return i;
		}

		assert(i == source_files.size());

		{
			source_file_t nt;
			nt.path = path;
			source_files.push_back(std::move(nt));
		}

		return i;
	}

	void clear_source_file(const size_t i) {
		if (i < source_files.size())
			source_files[i].clear();
	}

	void release_source_file(const size_t i) {
		if (i < source_files.size()) {
			if (source_files[i].refcount > 0)
				source_files[i].refcount--;
			if (source_files[i].refcount == 0)
				clear_source_file(i);
		}
	}

	void addref_source_file(const size_t i) {
		if (i < source_files.size())
			source_files[i].refcount++;
	}

	void source_file_refcount_check(void) {
		for (size_t i=0;i < source_files.size();i++) {
			if (source_files[i].refcount != 0) {
				fprintf(stderr,"Leftover refcount=%u for source '%s'\n",
					source_files[i].refcount,
					source_files[i].path.c_str());
			}
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

		size_t				source_file = no_source_file;

		rbuf() { pos.col=1; pos.row=1; pos.ofs=0; }
		~rbuf() { free(); }

		rbuf(const rbuf &x) = delete;
		rbuf &operator=(const rbuf &x) = delete;

		rbuf(rbuf &&x) { common_move(x); }
		rbuf &operator=(rbuf &&x) { common_move(x); return *this; }

		void set_source_file(const size_t i) {
			if (i == source_file) return;

			if (source_file != no_source_file) {
				release_source_file(source_file);
				source_file = no_source_file;
			}

			if (i < source_files.size()) {
				source_file = i;
				addref_source_file(source_file);
			}
		}

		void common_move(rbuf &x) {
			assert(x.sanity_check());
			base  = x.base;  x.base = NULL;
			data  = x.data;  x.data = NULL;
			end   = x.end;   x.end = NULL;
			fence = x.fence; x.fence = NULL;
			flags = x.flags; x.flags = 0;
			err   = x.err;   x.err = 0;
			pos   = x.pos;
			source_file = x.source_file; x.source_file = no_source_file;
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
				set_source_file(no_source_file);
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
	static constexpr unicode_char_t unicode_bad_escape = unicode_char_t(-4l);

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
		r_macro_paramref,			// 200
		r_ppendif,
		r_ppelifndef,
		r_ppembed,
		r_ppinclude_next,
		anglestrliteral,			// 205
		r___func__,
		r___FUNCTION__,
		op_ternary,
		op_comma,
		op_logical_or,				// 210
		op_logical_and,
		op_binary_or,
		op_binary_xor,
		op_binary_and,
		op_equals,				// 215
		op_not_equals,
		op_lessthan,
		op_greaterthan,
		op_lessthan_equals,
		op_greaterthan_equals,			// 220
		op_leftshift,
		op_rightshift,
		op_add,
		op_subtract,
		op_multiply,				// 225
		op_divide,
		op_modulus,
		op_pre_increment,
		op_pre_decrement,
		op_address_of,				// 230
		op_pointer_deref,
		op_negate,
		op_binary_not,
		op_logical_not,
		op_sizeof,				// 235
		op_member_ref,
		op_ptr_ref,
		op_post_increment,
		op_post_decrement,
		op_array_ref,				// 240
		op_assign,
		op_multiply_assign,
		op_divide_assign,
		op_modulus_assign,
		op_add_assign,				// 245
		op_subtract_assign,
		op_leftshift_assign,
		op_rightshift_assign,
		op_and_assign,
		op_xor_assign,				// 250
		op_or_assign,
		op_declaration,
		op_compound_statement,
		op_label,
		op_default_label,			// 255
		op_case_statement,
		op_if_statement,
		op_else_statement,
		op_switch_statement,
		op_break,				// 260
		op_continue,
		op_goto,
		op_return,
		op_while_statement,
		op_do_while_statement,			// 265
		op_for_statement,
		op_none,
		op_function_call,
		op_array_define,
		op_typecast,				// 270
		op_dinit_array,
		op_dinit_field,
		op_gcc_range,
		op_bitfield_range,
		op_symbol,				// 275
		r___int8,
		r___int16,
		r___int32,
		r___int64,
		op_alignof,				// 280

		__MAX__
	};

// str_name[] = "token" str_name_len = strlen("token")
// str_ppname[] = "#token" str_ppname_len = strlen("#token") str_name = str_ppname + 1 = "token" str_name_len = strlen("token")

// identifiers only
#define DEFX(name) static const char str_##name[] = #name; static constexpr size_t str_##name##_len = sizeof(str_##name) - 1
// __identifiers__ only
#define DEFXUU(name) static const char str___##name##__[] = "__" #name "__"; static constexpr size_t str___##name##___len = sizeof(str___##name##__) - 1
// identifiers and/or #identifiers
#define DEFB(name) static const char strpp_##name[] = "#"#name; static constexpr size_t strpp_##name##_len = sizeof(strpp_##name) - 1; static const char * const str_##name = (strpp_##name)+1; static constexpr size_t str_##name##_len = strpp_##name##_len - 1
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
	DEFB(embed);
	DEFB(include_next);
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
	DEFB(endif);
	DEFB(elifndef);
	DEFXUU(func);
	DEFXUU(FUNCTION);
	DEFX(__int8);
	DEFX(__int16);
	DEFX(__int32);
	DEFX(__int64);
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
#define XPP(name) { str_##name, str_##name##_len, uint16_t(token_type_t::r_pp##name) }
	static const ident2token_t ident2tok_pp[] = { // normal tokens, preprocessor time (er, well, actually lgtok time to be used by preprocessor)
		XUU(LINE),
		XUU(FILE),
		XAS(asm,      asm),
		XAS(__asm__,  asm),
		XAS(_asm,     __asm),
		XAS(__asm,    __asm)
	};
	static constexpr size_t ident2tok_pp_length = sizeof(ident2tok_pp) / sizeof(ident2tok_pp[0]);

	static const ident2token_t ident2tok_cc[] = { // normal tokens, compile time
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
		XAS(inline,       inline),
		XAS(_inline,      inline),
		XAS(__inline,     inline),
		XAS(__inline__,   inline),
		XAS(volatile,     volatile),
		XAS(__volatile__, volatile),
		XUU(func),
		XUU(FUNCTION),
		X(__int8),
		X(__int16),
		X(__int32),
		X(__int64)
	};
	static constexpr size_t ident2tok_cc_length = sizeof(ident2tok_cc) / sizeof(ident2tok_cc[0]);

	static const ident2token_t ppident2tok[] = { // preprocessor tokens
		XPP(if),
		XPP(ifdef),
		XPP(define),
		XPP(undef),
		XPP(else),
		XPP(elif),
		XPP(elifdef),
		XPP(ifndef),
		XPP(include),
		XPP(error),
		XPP(warning),
		XPP(line),
		XPP(pragma),
		XPP(endif),
		XPP(elifndef),
		XPP(embed),
		XPP(include_next)
	};
	static constexpr size_t ppident2tok_length = sizeof(ppident2tok) / sizeof(ppident2tok[0]);
#undef XPP
#undef XUU
#undef XAS
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
		strpp_if,
		strpp_ifdef,
		strpp_define,
		strpp_undef,				// 130
		strpp_else,
		"backslash",
		strpp_elif,
		strpp_elifdef,
		strpp_ifndef,				// 135
		strpp_include,
		strpp_error,
		strpp_warning,
		strpp_line,
		strpp_pragma,				// 140
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
		"backslashnewline",
		"macro_paramref",			// 200
		strpp_endif,
		strpp_elifndef,
		strpp_embed,
		strpp_include_next,
		"anglestrliteral",			// 205
		"__func__",
		"__FUNCTION__",
		"op:ternary",
		"op:comma",
		"op:log-or",				// 210
		"op:log-and",
		"op:bin-or",
		"op:bin-xor",
		"op:bin-and",
		"op:equals",				// 215
		"op:notequals",
		"op:lessthan",
		"op:greaterthan",
		"op:lessthan_equals",
		"op:greaterthan_equals",		// 220
		"op:leftshift",
		"op:rightshift",
		"op:add",
		"op:subtract",
		"op:multiply",				// 225
		"op:divide",
		"op:modulus",
		"op:++inc",
		"op:--dec",
		"op:addrof",				// 230
		"op:ptrderef",
		"op:negate",
		"op:bin-not",
		"op:log-not",
		"op:sizeof",				// 235
		"op:member_ref",
		"op:ptr_ref",
		"op:inc++",
		"op:dec--",
		"op:arrayref",				// 240
		"op:assign",
		"op:multiply_assign",
		"op:divide_assign",
		"op:modulus_assign",
		"op:add_assign"	,			// 245
		"op:subtract_assign",
		"op:leftshift_assign",
		"op:rightshift_assign",
		"op:and_assign",
		"op:xor_assign",			// 250
		"op:or_assign",
		"op:declaration",
		"op:compound_statement",
		"op:label",
		"op:default_label",			// 255
		"op:case_statement",
		"op:if_statement",
		"op:else_statement",
		"op:switch_statement",
		"op:break",				// 260
		"op:continue",
		"op:goto",
		"op:return",
		"op:while_statement",
		"op:do_while_statement",		// 265
		"op:for_statement",
		"op:none",
		"op:function_call",
		"op:array_define",
		"op:typecast",				// 270
		"op:dinit_array",
		"op:dinit_field",
		"op:gcc-range",
		"op:bitfield-range",
		"op:symbol",				// 275
		"__int8",
		"__int16",
		"__int32",
		"__int64",
		"op:alignof"				// 280
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

	///////////////////////////////////////////////////////

	template <class obj_t,typename id_t,const id_t none> class obj_pool {
		public:
			obj_pool() { }
			~obj_pool() { }
		public:
			obj_t &operator()(const id_t id) {
				return _lookup(id);
			}
		public:
			id_t alloc(void) {
				obj_t &a = __internal_alloc();
				a.clear().addref();
				return next++;
			}

			void assign(id_t &d,const id_t s) {
				if (d != none) _lookup(d).release();
				d = s;
				if (d != none) _lookup(d).addref();
			}

			void assignmove(id_t &d,id_t &s) {
				if (d != none) _lookup(d).release();
				d = s;
				s = none;
			}

			id_t returnmove(id_t &s) {
				const id_t r = s;
				s = none;
				return r;
			}

			void release(id_t &d) {
				if (d != none) _lookup(d).release();
				d = none;
			}

			void refcount_check(void) {
				for (size_t i=0;i < pool.size();i++) {
					if (pool[i].ref != 0) {
						fprintf(stderr,"Leftover refcount=%u for object '%s'\n",
							pool[i].ref,
							pool[i].to_str().c_str());
					}
				}
			}

			void update_next(obj_t *p) {
				if (p >= &pool[0]) {
					const size_t i = p - &pool[0];
					if (i < pool.size()) next = i;
				}
			}

			obj_t& __internal_alloc() {
#if 1//SET TO ZERO TO MAKE SURE DEALLOCATED NODES STAY DEALLOCATED
				while (next < pool.size() && pool[next].ref != 0)
					next++;
#endif
				if (next == pool.size())
					pool.resize(pool.size()+(pool.size()/2u)+16u);

				assert(next < pool.size());
				assert(pool[next].ref == 0);
				return pool[next];
			}
		public:
			id_t next = id_t(0);
		private:
			inline obj_t &_lookup(const id_t id) {
#if 1//DEBUG
				if (id < pool.size()) {
					if (pool[id].ref == 0)
						throw std::out_of_range("object not initialized");

					return pool[id];
				}

				throw std::out_of_range("object out of range");
#else
				return pool[id];
#endif
			}
		private:
			std::vector<obj_t> pool;
	};

	///////////////////////////////////////////////////////

	struct csliteral_t {
		enum type_t {
			CHAR=0,
			UTF8, /* multibyte */
			UNICODE16,
			UNICODE32,

			__MAX__
		};

		type_t			type = CHAR;
		void*			data = NULL;
		size_t			length = 0; /* in bytes */
		size_t			allocated = 0;
		unsigned int		ref = 0;

		csliteral_t &clear(void) {
			free();
			return *this;
		}

		csliteral_t &addref(void) {
			ref++;
			return *this;
		}

		csliteral_t &release(void) {
			if (ref == 0) throw std::runtime_error("csliteral attempt to release when ref == 0");
			if (--ref == 0) clear();
			return *this;
		}

		void free(void) {
			if (data) ::free(data);
			data = NULL;
			length = 0;
			allocated = 0;
			type = type_t::CHAR;
		}

		bool copy_from(const csliteral_t &x) {
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

		bool operator==(const csliteral_t &rhs) const {
			if (length != rhs.length) return false;

			if ((type == type_t::CHAR || type == type_t::UTF8) == (rhs.type == type_t::CHAR || rhs.type == type_t::UTF8)) { /* OK */ }
			else if (type != rhs.type) return false;

			return memcmp(data,rhs.data,length) == 0;
		}
		inline bool operator!=(const csliteral_t &rhs) const { return !(*this == rhs); }

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

	typedef size_t csliteral_id_t;
	static constexpr csliteral_id_t csliteral_none = ~csliteral_id_t(0u);
	static obj_pool<csliteral_t,csliteral_id_t,csliteral_none> csliteral;

	/////////////////////////////////////////////////

	struct identifier_t {
		unsigned char*		data = NULL;
		size_t			length = 0;
		unsigned int		ref = 0;

		identifier_t &clear(void) {
			free_data();
			return *this;
		}

		identifier_t &addref(void) {
			ref++;
			return *this;
		}

		identifier_t &release(void) {
			if (ref == 0) throw std::runtime_error("identifier attempt to release when ref == 0");
			if (--ref == 0) clear();
			return *this;
		}

		identifier_t() { }
		identifier_t(const identifier_t &x) = delete;
		identifier_t &operator=(const identifier_t &x) = delete;
		identifier_t(identifier_t &&x) { common_move(x); }
		identifier_t &operator=(identifier_t &&x) { common_move(x); return *this; }

		~identifier_t() { free(); }

		void free_data(void) {
			if (data) { ::free(data); data = NULL; }
			length = 0;
		}

		void free(void) {
			if (ref != 0) fprintf(stderr,"WARNING: identifier str free() to object with %u outstanding references\n",ref);
			ref = 0;

			free_data();
		}

		void common_move(identifier_t &other) {
			ref = other.ref; other.ref = 0;
			data = other.data; other.data = NULL;
			length = other.length; other.length = 0;
		}

		bool copy_from(const unsigned char *in_data,const size_t in_length) {
			free_data();

			if (in_length != 0 && in_length < 4096) {
				data = (unsigned char*)::malloc(in_length);
				if (data == NULL) return false;
				memcpy(data,in_data,in_length);
				length = in_length;
			}

			return true;
		}

		std::string to_str(void) const {
			if (data != NULL && length != 0) return std::string((char*)data,length);
			return std::string();
		}

		bool operator==(const identifier_t &rhs) const {
			if (length != rhs.length) return false;
			if (length != 0) return memcmp(data,rhs.data,length) == 0;
			return true;
		}
		bool operator!=(const identifier_t &rhs) const { return !(*this == rhs); }

		bool operator==(const csliteral_t &rhs) const {
			if (length != rhs.length) return false;
			if (length != 0) return memcmp(data,rhs.data,length) == 0;
			return true;
		}
		bool operator!=(const csliteral_t &rhs) const { return !(*this == rhs); }

		bool operator==(const std::string &rhs) const {
			if (length != rhs.length()) return false;
			if (length != 0) return memcmp(data,rhs.data(),length) == 0;
			return true;
		}
		inline bool operator!=(const std::string &rhs) const { return !(*this == rhs); }
	};

	typedef size_t identifier_id_t;
	static constexpr identifier_id_t identifier_none = ~size_t(0u);
	static obj_pool<identifier_t,identifier_id_t,identifier_none> identifier;

	//////////////////////////////////////////////////////

	void CCerr(const position_t &pos,const char *fmt,...) {
		va_list va;

		fprintf(stderr,"Error");
		if (pos.row > 0 || pos.col > 0) fprintf(stderr,"(%d,%d)",pos.row,pos.col);
		fprintf(stderr,": ");

		va_start(va,fmt);
		vfprintf(stderr,fmt,va);
		va_end(va);
		fprintf(stderr,"\n");
	}

#define CCERR_RET(code,pos,...) \
	do { \
		CCerr(pos,__VA_ARGS__); \
		return errno_return(code); \
	} while(0)

	struct declaration_t;

	typedef unsigned int scope_id_t;
	static constexpr unsigned int scope_none = ~((unsigned int)0u);
	static constexpr unsigned int scope_global = 0;

	typedef size_t symbol_id_t;
	static constexpr size_t symbol_none = ~size_t(0);

	/* this is defined here, declared at the bottom, to over come the "incomplete type" on delete issues */
	template <class T> void typ_delete(T *p);

	struct token_t {
		token_type_t		type = token_type_t::none;
		position_t		pos;
		size_t			source_file = no_source_file; /* index into source file array to indicate token source or -1 */

		token_t() : pos(1) { common_init(); }
		~token_t() { common_delete(); }
		token_t(const token_t &t) { common_copy(t); }
		token_t(token_t &&t) { common_move(t); }
		token_t(const token_type_t t) : type(t) { common_init(); }
		token_t(const token_type_t t,const position_t &n_pos,const size_t n_source_file) : type(t), pos(n_pos) { set_source_file(n_source_file); common_init(); }
		token_t &operator=(const token_t &t) { common_copy(t); return *this; }
		token_t &operator=(token_t &&t) { common_move(t); return *this; }

		void set_source_file(const size_t i) {
			if (i == source_file) return;

			if (source_file != no_source_file) {
				release_source_file(source_file);
				source_file = no_source_file;
			}

			if (i < source_files.size()) {
				source_file = i;
				addref_source_file(source_file);
			}
		}

		bool operator==(const token_t &t) const {
			if (type != t.type)
				return false;

			switch (type) {
				case token_type_t::integer:
					if (v.integer.v.u != t.v.integer.v.u)
						return false;
					if (v.integer.flags != t.v.integer.flags)
						return false;
					break;
				default:
					break;
			}

			return true;
		}
		bool operator!=(const token_t &t) const {
			return !(*this == t);
		}

		union v_t {
			integer_value_t		integer; /* token_type_t::integer */
			floating_value_t	floating; /* token_type_t::floating */
			csliteral_id_t		csliteral; /* token_type_t::charliteral/strliteral/identifier/asm */
			size_t			paramref; /* token_type_t::r_macro_paramref */
			declaration_t*		declaration; /* token_type_t::op_declaration */
			identifier_id_t		identifier;
			symbol_id_t		symbol;/* token_type_t::op_symbol */
			scope_id_t		scope; /* token_type_t::op_compound_statement */
			void*			general_ptr; /* for use in clearing general ppinter */
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
				case token_type_t::anglestrliteral:
					s += "("; s += csliteral(v.csliteral).to_str(); s += ")";
					break;
				case token_type_t::r_macro_paramref: {
					char tmp[64];
					sprintf(tmp,"(%u)",(unsigned int)v.paramref);
					s += tmp;
					break; }
				case token_type_t::identifier:
				case token_type_t::ppidentifier:
				case token_type_t::r___asm_text:
					s += "("; if (v.identifier != identifier_none) s += identifier(v.identifier).to_str(); s += ")";
					break;
				case token_type_t::op_compound_statement:
					s += "("; if (v.scope != scope_none) s += std::string("scope #")+std::to_string((unsigned long)v.scope); s += ")";
					break;
				case token_type_t::op_symbol:
					s += "("; if (v.symbol != symbol_none) s += std::string("symbol #")+std::to_string((unsigned long)v.symbol); s += ")";
					break;
				default:
					break;
			}

			return s;
		}

		void clear_v(void) {
			common_delete();
		}

private:
		void common_delete(void) {
			switch (type) {
				case token_type_t::charliteral:
				case token_type_t::strliteral:
				case token_type_t::anglestrliteral:
					csliteral.release(v.csliteral);
					break;
				case token_type_t::op_declaration:
					typ_delete(v.declaration);
					break;
				case token_type_t::identifier:
				case token_type_t::ppidentifier:
				case token_type_t::r___asm_text:
					identifier.release(v.identifier);
					break;
				default:
					break;
			}

			set_source_file(no_source_file);
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
				case token_type_t::anglestrliteral:
					v.csliteral = csliteral_none;
					break;
				case token_type_t::op_declaration:
					v.general_ptr = NULL;
					static_assert( offsetof(v_t,general_ptr) == offsetof(v_t,declaration), "oops" );
					break;
				case token_type_t::identifier:
				case token_type_t::ppidentifier:
				case token_type_t::r___asm_text:
					v.identifier = identifier_none;
					break;
				case token_type_t::op_compound_statement:
					v.scope = scope_none;
					break;
				case token_type_t::op_symbol:
					v.symbol = symbol_none;
					break;
				default:
					break;
			}
		}

		void common_copy(const token_t &x) {
			assert(&x != this);

			common_delete();

			set_source_file(x.source_file);
			type = x.type;
			pos = x.pos;

			common_init();

			switch (type) {
				case token_type_t::charliteral:
				case token_type_t::strliteral:
				case token_type_t::anglestrliteral:
					csliteral.assign(/*to*/v.csliteral,/*from*/x.v.csliteral);
					break;
				case token_type_t::op_declaration:
					throw std::runtime_error("Copy constructor not available");
					break;
				case token_type_t::identifier:
				case token_type_t::ppidentifier:
				case token_type_t::r___asm_text:
					identifier.assign(/*to*/v.identifier,/*from*/x.v.identifier);
					break;
				default:
					v = x.v;
					break;
			}
		}

		void common_move(token_t &x) {
			common_delete();

			source_file = x.source_file; x.source_file = no_source_file;
			type = x.type; x.type = token_type_t::none;
			pos = x.pos; x.pos = position_t();

			common_init();

			switch (type) {
				case token_type_t::charliteral:
				case token_type_t::strliteral:
				case token_type_t::anglestrliteral:
					csliteral.assignmove(/*to*/v.csliteral,/*from*/x.v.csliteral);
					break;
				case token_type_t::identifier:
				case token_type_t::ppidentifier:
				case token_type_t::r___asm_text:
					identifier.assignmove(/*to*/v.identifier,/*from*/x.v.identifier);
					break;
				case token_type_t::op_compound_statement:
					v.scope = x.v.scope; x.v.scope = scope_none;
					break;
				case token_type_t::op_symbol:
					v.symbol = x.v.symbol; x.v.symbol = symbol_none;
					break;
				default:
					v = x.v;
					memset(&x.v,0,sizeof(x.v)); /* x.type == none so pointers no longer matter */
					break;
			}
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
		return (c == '_' || c == '.' || c == '@') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
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
					int n,count=0;

					v = 0;
					while ((n=cc_parsedigit(buf.peekb(),16)) >= 0) {
						v = int32_t(((unsigned int)v << 4u) | (unsigned int)n);
						buf.discardb();
						count++;
					}
					if (count < 2) return unicode_eof;
					break; }
				default:
					return unicode_bad_escape;
			}
		}

		return v;
	}

	template <const csliteral_t::type_t cslt,typename ptrat> int lgtok_strlit_wrch(ptrat* &p,ptrat* const f,const unicode_char_t v) = delete;

	template <> int lgtok_strlit_wrch<csliteral_t::type_t::CHAR,unsigned char>(unsigned char* &p,unsigned char* const f,const unicode_char_t v) {
		if (v < 0x00 || v > 0xFF)
			return errno_return(EINVAL);

		assert((p+1) <= f);
		*p++ = (unsigned char)v;
		return 1;
	}

	template <> int lgtok_strlit_wrch<csliteral_t::type_t::UTF8,unsigned char>(unsigned char* &p,unsigned char* const f,const unicode_char_t v) {
		if (v < 0x00)
			return errno_return(EINVAL);

		utf8_to_str(p,f,v);
		assert(p <= f);
		return 1;
	}

	template <> int lgtok_strlit_wrch<csliteral_t::type_t::UNICODE16,uint16_t>(uint16_t* &p,uint16_t* const f,const unicode_char_t v) {
		if (v < 0x00l || v > 0x20FFFFl)
			return errno_return(EINVAL);

		utf16_to_str(p,f,v);
		assert(p <= f);
		return 1;
	}

	template <> int lgtok_strlit_wrch<csliteral_t::type_t::UNICODE32,uint32_t>(uint32_t* &p,uint32_t* const f,const unicode_char_t v) {
		if (v < 0x00)
			return errno_return(EINVAL);

		assert((p+1) <= f);
		*p++ = (uint32_t)v;
		return 1;
	}

	template <const csliteral_t::type_t cslt,typename ptrat> int lgtok_strlit_common_inner(rbuf &buf,source_file_object &sfo,const unsigned char separator,ptrat* &p,ptrat* const &f) {
		const bool unicode = !(cslt == csliteral_t::type_t::CHAR);
		int32_t v;
		int rr;

		do {
			if (buf.peekb() == separator) {
				buf.discardb();
				break;
			}

			if ((p+16) >= f)
				CCERR_RET(ENAMETOOLONG,buf.pos,"String constant is too long");

			v = lgtok_cslitget(buf,sfo,unicode);
			if (v == unicode_nothing) continue;
			if (v == unicode_bad_escape) CCERR_RET(EINVAL,buf.pos,"Invalid escape");
			if ((rr=lgtok_strlit_wrch<cslt,ptrat>(p,f,v)) <= 0) CCERR_RET(rr,buf.pos,"invalid encoding for string");
		} while(1);

		return 1;
	}

	template <const csliteral_t::type_t cslt,typename ptrat> int lgtok_strlit_common(rbuf &buf,source_file_object &sfo,token_t &t,const unsigned char separator) {
		assert(t.type == token_type_t::charliteral || t.type == token_type_t::strliteral || t.type == token_type_t::anglestrliteral);
		ptrat *p,*f;
		int rr;

		if (!csliteral(t.v.csliteral).alloc((4096+20)*sizeof(ptrat)))
			return errno_return(ENOMEM);

		p = (ptrat*)((char*)csliteral(t.v.csliteral).data);
		f = (ptrat*)((char*)csliteral(t.v.csliteral).data+csliteral(t.v.csliteral).length);

		rr = lgtok_strlit_common_inner<cslt,ptrat>(buf,sfo,separator,p,f);

		{
			const size_t fo = size_t(p-((ptrat*)csliteral(t.v.csliteral).data)) * sizeof(ptrat);
			assert(fo <= csliteral(t.v.csliteral).allocated);
			csliteral(t.v.csliteral).length = fo;
			csliteral(t.v.csliteral).shrinkfit();
		}

		return rr;
	}

	bool is_hchar(unsigned char c) {
		return isalpha((char)c) || isdigit((char)c) || c == '.' || c == ',' || c == '/' || c == '\\';
	}

	bool looks_like_arrowstr(rbuf &buf,source_file_object &sfo) {
		rbuf_sfd_refill(buf,sfo);

		unsigned char *p = buf.data,*f = buf.end;

		if (p >= f) return false;
		if (*p != '<') return false;
		p++;

		while (p < f && is_hchar(*p)) p++;

		if (p >= f) return false;
		if (*p != '>') return false;
		p++;

		return true;
	}

	int lgtok_charstrlit(rbuf &buf,source_file_object &sfo,token_t &t,const csliteral_t::type_t cslt=csliteral_t::type_t::CHAR) {
		unsigned char separator = buf.peekb();

		if (separator == '\'' || separator == '\"' || separator == '<') {
			buf.discardb();

			assert(t.type == token_type_t::none);
			if (separator == '\"') t.type = token_type_t::strliteral;
			else if (separator == '<') t.type = token_type_t::anglestrliteral;
			else t.type = token_type_t::charliteral;
			t.v.csliteral = csliteral.alloc();
			csliteral(t.v.csliteral).type = cslt;

			if (separator == '<') separator = '>';

			switch (cslt) {
				case csliteral_t::type_t::CHAR:
					return lgtok_strlit_common<csliteral_t::type_t::CHAR,unsigned char>(buf,sfo,t,separator);
				case csliteral_t::type_t::UTF8:
					return lgtok_strlit_common<csliteral_t::type_t::UTF8,unsigned char>(buf,sfo,t,separator);
				case csliteral_t::type_t::UNICODE16:
					return lgtok_strlit_common<csliteral_t::type_t::UNICODE16,uint16_t>(buf,sfo,t,separator);
				case csliteral_t::type_t::UNICODE32:
					return lgtok_strlit_common<csliteral_t::type_t::UNICODE32,uint32_t>(buf,sfo,t,separator);
				default:
					break;
			}

			return 1;
		}
		else {
			abort();//should not happen
		}
	}

	struct lgtok_state_t {
		unsigned int		flags = FL_NEWLINE;
		unsigned int		curlies = 0;

		static constexpr unsigned int FL_MSASM = (1u << 0u); /* __asm ... */
		static constexpr unsigned int FL_NEWLINE = (1u << 1u);
		static constexpr unsigned int FL_ARROWSTR = (1u << 2u); /* <string> in #include <string> */
	};

	int lgtok_asm_text(lgtok_state_t &lst,rbuf &buf,source_file_object &sfo,token_t &t) {
		(void)lst;

		unsigned char data[128];
		unsigned char *p = data;
		unsigned char *f = data + sizeof(data);

		assert(t.type == token_type_t::none);

		assert(p < f);
		assert(is_asm_text_first_char(buf.peekb()) || buf.peekb() == '#');
		rbuf_sfd_refill(buf,sfo);
		*p++ = buf.getb();
		while (is_asm_text_char(buf.peekb())) {
			if ((p+1) >= f)
				CCERR_RET(ENAMETOOLONG,t.pos,"Identifier name too long");

			assert((p+1) <= f);
			*p++ = (unsigned char)buf.getb();
			rbuf_sfd_refill(buf,sfo);
		}

		const size_t length = size_t(p-data);
		assert(length != 0);

		/* it might be _asm or __asm */
		if (	(length == 4 && !memcmp(data,"_asm",4)) ||
			(length == 5 && !memcmp(data,"__asm",5))) {
			t.type = token_type_t(token_type_t::r___asm);
		}

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

	bool pptok_define_asm_allowed_token(const token_t &t);

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

try_again:	t = token_t();
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
				case '+':
					t.type = token_type_t::plus; buf.discardb();
					break;
				case '-':
					t.type = token_type_t::minus; buf.discardb();
					break;
				case '*':
					t.type = token_type_t::star; buf.discardb();
					break;
				case '/':
					t.type = token_type_t::forwardslash; buf.discardb();
					break;
				case '%':
					t.type = token_type_t::percent; buf.discardb();
					break;
				case '!':
					t.type = token_type_t::exclamation; buf.discardb();
					break;
				case '=':
					t.type = token_type_t::equal; buf.discardb();
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
					break;
				case '^':
					t.type = token_type_t::caret; buf.discardb();
					break;
				case '&':
					t.type = token_type_t::ampersand; buf.discardb();
					break;
				case '|':
					t.type = token_type_t::pipe; buf.discardb();
					break;
				case '.':
					if (lst_was_flags & lgtok_state_t::FL_NEWLINE) {
						/* ASM directive i.e. .386p */
						goto asm_directive;
					}
					else {
						t.type = token_type_t::period; buf.discardb();
					}
					break;
				case ',':
					t.type = token_type_t::comma; buf.discardb();
					break;
				case '[':
					t.type = token_type_t::opensquarebracket; buf.discardb();
					break;
				case ']':
					t.type = token_type_t::closesquarebracket; buf.discardb();
					break;
				case '{':
					t.type = token_type_t::opencurlybracket; buf.discardb();
					lst.curlies++;
					break;
				case '}':
					t.type = token_type_t::closecurlybracket; buf.discardb();
					if (lst.curlies == 0)
						return errno_return(EINVAL);
					if (--lst.curlies == 0)
						lst.flags &= ~lgtok_state_t::FL_MSASM;
					break;
				case '(':
					t.type = token_type_t::openparenthesis; buf.discardb();
					break;
				case ')':
					t.type = token_type_t::closeparenthesis; buf.discardb();
					break;
				case '<':
					t.type = token_type_t::lessthan; buf.discardb();
					break;
				case '>':
					t.type = token_type_t::greaterthan; buf.discardb();
					break;
				case '\'':
				case '\"':
					return lgtok_charstrlit(buf,sfo,t);
				case '\\':
					if (is_newline(buf.peekb(1))) {
						/* stop and switch back to normal tokenizing.
						 * as far as I know you're not allowed to \n
						 * __asm blocks like this even within scope.
						 * doing this allows \n escapes in a #define
						 * macro */
						if (lst.curlies == 0) {
							lst.flags &= ~lgtok_state_t::FL_MSASM;
							goto try_again;
						}

						t.type = token_type_t::backslashnewline;
						buf.discardb(2); /* the '\\' and the newline */
						return 1;
					}

					t.type = token_type_t::backslash; buf.discardb();
					break;
				case '#':
					if (lst_was_flags & lgtok_state_t::FL_NEWLINE) {
						CCERR_RET(EINVAL,buf.pos,"Preprocessor directives not allowed in __asm blocks");
					}
					else {
						t.type = token_type_t::pound; buf.discardb();
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
					if (lst.curlies == 0)
						lst.flags &= ~lgtok_state_t::FL_MSASM;
					break;
				default:
				asm_directive:
					if (is_asm_text_first_char(buf.peekb()))
						return lgtok_asm_text(lst,buf,sfo,t);
					else
						CCERR_RET(ESRCH,buf.pos,"Unexpected symbol at");
					break;
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

	struct pptok_macro_t {
		std::vector<token_t>		tokens;
		std::vector<identifier_id_t>	parameters;
		unsigned int			flags = 0;

		static constexpr unsigned int	FL_PARENTHESIS = 1u << 0u;
		static constexpr unsigned int	FL_VARIADIC = 1u << 1u;
		static constexpr unsigned int	FL_NO_VA_ARGS = 1u << 2u; /* set if GNU args... used instead */
		static constexpr unsigned int	FL_STRINGIFY = 1u << 3u; /* uses #parameter to stringify */

		pptok_macro_t() { }
		pptok_macro_t(const pptok_macro_t &) = delete;
		pptok_macro_t &operator=(const pptok_macro_t &) = delete;
		pptok_macro_t(pptok_macro_t &&x) { common_move(x); }
		pptok_macro_t &operator=(pptok_macro_t &&x) { common_move(x); return *this; }
		~pptok_macro_t() {
			for (auto &pid : parameters) identifier.release(pid);
			parameters.clear();
		}

		void common_move(pptok_macro_t &o) {
			flags = o.flags; o.flags = 0;
			tokens = std::move(o.tokens);
			parameters = std::move(o.parameters);
		}
	};

	struct pptok_state_t {
		struct pptok_macro_ent_t {
			pptok_macro_t		ment;
			identifier_id_t		name = identifier_none;
			pptok_macro_ent_t*	next;

			pptok_macro_ent_t() { }
			pptok_macro_ent_t(const pptok_macro_ent_t &) = delete;
			pptok_macro_ent_t &operator=(const pptok_macro_ent_t &) = delete;
			pptok_macro_ent_t(pptok_macro_ent_t &&) = delete;
			pptok_macro_ent_t &operator=(pptok_macro_ent_t &&) = delete;
			~pptok_macro_ent_t() {
				identifier.release(name);
			}
		};
		struct cond_block_t {
			enum {
				WAITING=0,	/* false, waiting for elseif/else */
				IS_TRUE,	/* true, next elseif/else switches to done. #else && WAITING == IS_TRUE */
				DONE		/* state was IS_TRUE, skip anything else */
			};
			unsigned char		state = WAITING;
			unsigned char		flags = 0;
			static constexpr unsigned char FL_ELSE = 1u << 0u;
		};

		/* recursion stack, for #include */
		struct include_t {
			CCMiniC::source_file_object*	sfo = NULL;
			CCMiniC::rbuf			rb;
		};

		std::stack<include_t>		include_stk;
		std::stack<cond_block_t>	cond_block;

		void include_push(CCMiniC::source_file_object *sfo) {
			if (sfo != NULL) {
				include_t i;
				i.sfo = sfo;
				i.rb.set_source_file(alloc_source_file(sfo->getname()));
				assert(i.rb.allocate());
				include_stk.push(std::move(i));
			}
		}

		void include_pop(void) {
			if (!include_stk.empty()) {
				include_t &e = include_stk.top();
				e.rb.set_source_file(no_source_file);
				if (e.sfo) delete e.sfo;
				include_stk.pop();
			}
		}

		void include_popall(void) {
			while (!include_stk.empty()) {
				include_t &e = include_stk.top();
				e.rb.set_source_file(no_source_file);
				if (e.sfo) delete e.sfo;
				include_stk.pop();
			}
		}

		bool condb_true(void) const {
			if (cond_block.empty())
				return true;
			else
				return cond_block.top().state == cond_block_t::IS_TRUE;
		}

		unsigned int macro_expansion_counter = 0; /* to prevent runaway expansion */
		std::deque<token_t> macro_expansion;

		const pptok_macro_ent_t* lookup_macro(const identifier_id_t &i) const {
			const pptok_macro_ent_t *p = macro_buckets[macro_hash_id(i)];
			while (p != NULL) {
				if (identifier(p->name) == identifier(i))
					return p;

				p = p->next;
			}

			return NULL;
		}

		bool create_macro(const identifier_id_t i,pptok_macro_t &m) {
			pptok_macro_ent_t **p = &macro_buckets[macro_hash_id(i)];

			while ((*p) != NULL) {
				if (identifier((*p)->name) == identifier(i)) {
					/* already exists. */
					/* this is an error, unless the attempt re-states the same macro and definition,
					 * which is not an error.
					 *
					 * not an error:
					 *
					 * #define X 5
					 * #define X 5
					 *
					 * error:
					 *
					 * #define X 5
					 * #define X 4 */
					if (	(*p)->ment.tokens == m.tokens &&
						(*p)->ment.parameters == m.parameters &&
						(*p)->ment.flags == m.flags)
						return true;

					return false;
				}

				p = &((*p)->next);
			}

			(*p) = new pptok_macro_ent_t;
			(*p)->ment = std::move(m);
			identifier.assign(/*to*/(*p)->name,/*from*/i);
			(*p)->next = NULL;
			return true;
		}

		bool delete_macro(const identifier_id_t i) {
			pptok_macro_ent_t **p = &macro_buckets[macro_hash_id(i)];
			while ((*p) != NULL) {
				if (identifier((*p)->name) == identifier(i)) {
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

		inline uint8_t macro_hash_id(const identifier_t &i) const {
			return macro_hash_id(i.data,i.length);
		}

		inline uint8_t macro_hash_id(const identifier_id_t &i) const {
			return macro_hash_id(identifier(i));
		}

		static uint8_t macro_hash_id(const csliteral_t &i) {
			return macro_hash_id((const unsigned char*)i.data,i.length);
		}

		pptok_macro_ent_t* macro_buckets[macro_bucket_count] = { NULL };

		pptok_state_t() { }
		pptok_state_t(const pptok_state_t &) = delete;
		pptok_state_t &operator=(const pptok_state_t &) = delete;
		pptok_state_t(pptok_state_t &&) = delete;
		pptok_state_t &operator=(pptok_state_t &&) = delete;
		~pptok_state_t() { include_popall(); free_macros(); }
	};

	int pptok_lgtok(pptok_state_t &pst,lgtok_state_t &lst,rbuf &buf,source_file_object &sfo,token_t &t) {
		if (!pst.macro_expansion.empty()) {
			t = std::move(pst.macro_expansion.front());
			pst.macro_expansion.pop_front();
			return 1;
		}
		else {
			pst.macro_expansion_counter = 0;
			return lgtok(lst,buf,sfo,t);
		}
	}

	void pptok_lgtok_ungetch(pptok_state_t &pst,token_t &t) {
		pst.macro_expansion.push_front(std::move(t));
	}

	int pptok_undef(pptok_state_t &pst,lgtok_state_t &lst,rbuf &buf,source_file_object &sfo,token_t &t) {
		/* #undef has already been parsed.
		 * the last token we didn't use is left in &t for the caller to parse as most recently obtained,
		 * unless set to token_type_t::none in which case it will fetch another one */
		identifier_id_t s_id = identifier_none;
		pptok_macro_t macro;
		int r;

		(void)pst;

		if ((r=pptok_lgtok(pst,lst,buf,sfo,t)) < 1)
			return r;

		if (t.type != token_type_t::identifier || t.v.identifier == identifier_none)
			CCERR_RET(EINVAL,t.pos,"Identifier expected");

		identifier.assign(/*to*/s_id,/*from*/t.v.identifier);

		do {
			if ((r=pptok_lgtok(pst,lst,buf,sfo,t)) < 1)
				return r;

			if (t.type == token_type_t::newline) {
				t = token_t();
				break;
			}

			/* ignore trailing tokens */
		} while (1);

#if 0//DEBUG
		for (size_t i=0;i < pst.cond_block.size();i++) fprintf(stderr,"  ");
		fprintf(stderr,"UNDEF '%s'\n",s_id.to_str().c_str());
#endif

		if (!pst.delete_macro(s_id)) {
#if 0//DEBUG
			fprintf(stderr,"  Macro wasn't defined anyway\n");
#endif
		}

		identifier.release(s_id);
		return 1;
	}

	int pptok_eval_expr(integer_value_t &r,std::deque<token_t>::iterator &ib,std::deque<token_t>::iterator ie,const bool subexpr=false) {
		std::stack< std::pair<unsigned char,token_t> > os;
		std::stack<integer_value_t> vs;
		bool expect_op2 = false;
		int er;

		enum {
			NONE=0,
			LOG_OR, /* || */
			LOG_AND, /* && */
			BIT_OR, /* | */
			BIT_XOR, /* ^ */
			BIT_AND, /* & */
			EQU, /* == != */
			CMP, /* < <= > >= */
			SHF, /* << >> */
			AS, /* + - */
			MDR, /* * / % */
			NOT, /* ! ~ */
			NEG /* leading + - */
		};

		if (ib == ie) return errno_return(EINVAL);

		while (ib != ie || !os.empty()) {
			unsigned char lev = NONE;

			if (ib != ie) {
				if (subexpr && (*ib).type == token_type_t::closeparenthesis) {
					/* step past close parenthesis and force loop to end and pop stacks */
					ib++;
					ie = ib;
				}
				else {
					switch ((*ib).type) {
						case token_type_t::plus:
							if (expect_op2) {
								expect_op2 = false; lev = AS; break;
							}
							else {
								lev = NEG; break;
							}
						case token_type_t::minus:
							if (expect_op2) {
								expect_op2 = false; lev = AS; break;
							}
							else {
								lev = NEG; break;
							}
						case token_type_t::tilde:
							lev = NOT; break;
							continue; /* loop while loop again */
						case token_type_t::exclamation:
							lev = NOT; break;
							continue; /* loop while loop again */
						case token_type_t::integer:
							if (expect_op2 == true) return errno_return(EINVAL);
							vs.push((*ib).v.integer); ib++;
							expect_op2 = true;
							continue;
						case token_type_t::openparenthesis:
							if (expect_op2 == true) return errno_return(EINVAL);
							ib++;
							if ((er=pptok_eval_expr(r,ib,ie,/*subexpr*/true)) < 1)
								return er;
							vs.push(r);
							expect_op2 = true;
							continue;
						case token_type_t::pipepipe:
							expect_op2 = false; lev = LOG_OR; break;
						case token_type_t::ampersandampersand:
							expect_op2 = false; lev = LOG_AND; break;
						case token_type_t::pipe:
							expect_op2 = false; lev = BIT_OR; break;
						case token_type_t::caret:
							expect_op2 = false; lev = BIT_XOR; break;
						case token_type_t::ampersand:
							expect_op2 = false; lev = BIT_AND; break;
						case token_type_t::equalequal:
						case token_type_t::exclamationequals:
							expect_op2 = false; lev = EQU; break;
						case token_type_t::lessthan:
						case token_type_t::greaterthan:
						case token_type_t::lessthanequals:
						case token_type_t::greaterthanequals:
							expect_op2 = false; lev = CMP; break;
						case token_type_t::lessthanlessthan:
						case token_type_t::greaterthangreaterthan:
							expect_op2 = false; lev = SHF; break;
						case token_type_t::star:
						case token_type_t::forwardslash:
						case token_type_t::percent:
							expect_op2 = false; lev = MDR; break;
						default:
							return errno_return(EINVAL);
					}
				}
			}

			while (!os.empty()) {
				if (lev <= os.top().first) {
					if (os.top().first == NOT || os.top().first == NEG) {
						if (vs.size() > 0) {
							integer_value_t &a = vs.top();

							/* does not pop, just modifies in place */
							switch (os.top().second.type) {
								case token_type_t::plus:
									/* nothing */
									break;
								case token_type_t::minus:
									a.v.v = -a.v.v;
									break;
								case token_type_t::tilde:
									a.v.u = ~a.v.u;
									break;
								case token_type_t::exclamation:
									a.v.u = (a.v.u == 0) ? 1 : 0;
									break;
								default:
									return errno_return(EINVAL);
							}

#if 0//DEBUG
							fprintf(stderr,"Reduce op %u <= %u: %s\n",lev,os.top().first,os.top().second.to_str().c_str());
#endif

							os.pop();
						}
						else {
							break;
						}
					}
					else {
						if (vs.size() < 2) return errno_return(EINVAL);

						/* a, b pushed to stack in a, b order so pops off b, a */
						integer_value_t b = vs.top(); vs.pop();
						integer_value_t a = vs.top(); vs.pop();

						/* a OP b,
						 * put result in a */
						switch (os.top().second.type) {
							case token_type_t::pipepipe:
								a.v.u = ((a.v.u != 0) || (b.v.u != 0)) ? 1 : 0;
								break;
							case token_type_t::ampersandampersand:
								a.v.u = ((a.v.u != 0) && (b.v.u != 0)) ? 1 : 0;
								break;
							case token_type_t::pipe:
								a.v.u = a.v.u | b.v.u;
								break;
							case token_type_t::caret:
								a.v.u = a.v.u ^ b.v.u;
								break;
							case token_type_t::ampersand:
								a.v.u = a.v.u & b.v.u;
								break;
							case token_type_t::equalequal:
								a.v.u = (a.v.u == b.v.u) ? 1 : 0;
								break;
							case token_type_t::exclamationequals:
								a.v.u = (a.v.u != b.v.u) ? 1 : 0;
								break;
							case token_type_t::lessthan:
								a.v.u = (a.v.v < b.v.v) ? 1 : 0;
								break;
							case token_type_t::greaterthan:
								a.v.u = (a.v.v > b.v.v) ? 1 : 0;
								break;
							case token_type_t::lessthanequals:
								a.v.u = (a.v.v <= b.v.v) ? 1 : 0;
								break;
							case token_type_t::greaterthanequals:
								a.v.u = (a.v.v >= b.v.v) ? 1 : 0;
								break;
							case token_type_t::lessthanlessthan:
								a.v.u = a.v.v << b.v.v;
								break;
							case token_type_t::greaterthangreaterthan:
								a.v.u = a.v.v >> b.v.v;
								break;
							case token_type_t::plus:
								a.v.u = a.v.v + b.v.v;
								break;
							case token_type_t::minus:
								a.v.u = a.v.v - b.v.v;
								break;
							case token_type_t::star:
								a.v.u = a.v.v * b.v.v;
								break;
							case token_type_t::forwardslash:
								if (b.v.v == 0) return errno_return(EINVAL);
								a.v.u = a.v.v / b.v.v;
								break;
							case token_type_t::percent:
								if (b.v.v == 0) return errno_return(EINVAL);
								a.v.u = a.v.v % b.v.v;
								break;
							default:
								return errno_return(EINVAL);
						}

#if 0//DEBUG
						fprintf(stderr,"Reduce op %u <= %u: %s\n",lev,os.top().first,os.top().second.to_str().c_str());
#endif

						vs.push(a);
						os.pop();
					}
				}
				else {
					break;
				}
			}

			if (lev != NONE) {
				assert(ib != ie);
				os.push( std::pair<unsigned char,token_t>(lev,*ib) );
				ib++;
			}
		}

		if (!os.empty())
			return errno_return(EINVAL);
		if (vs.size() != 1)
			return errno_return(EINVAL);

		r = vs.top();
		return 1;
	}

	int pptok_macro_expansion(const pptok_state_t::pptok_macro_ent_t* macro,pptok_state_t &pst,lgtok_state_t &lst,rbuf &buf,source_file_object &sfo,token_t &t);

	int pptok_line(pptok_state_t &pst,lgtok_state_t &lst,rbuf &buf,source_file_object &sfo,token_t &t) {
		std::string msg;
		int r;

		(void)pst;

		/* line number */
		if ((r=pptok_lgtok(pst,lst,buf,sfo,t)) < 1)
			return r;

		/* canonical GCC behavior: The number parameter can be a macro */
		if (t.type == token_type_t::identifier) {
			const pptok_state_t::pptok_macro_ent_t* macro = pst.lookup_macro(t.v.identifier);
			if (macro) {
				if ((r=pptok_macro_expansion(macro,pst,lst,buf,sfo,t)) < 1) /* which affects pptok_lgtok() */
					return r;

				pst.macro_expansion_counter++;
				if ((r=pptok_lgtok(pst,lst,buf,sfo,t)) < 1)
					return r;
			}
		}

		if (t.type != token_type_t::integer)
			return errno_return(EINVAL);
		if (t.v.integer.v.v < 0ll || t.v.integer.v.v > 0xFFFFll)
			return errno_return(EINVAL);

		const unsigned int row = (unsigned int)t.v.integer.v.u;
		std::string name;

		if ((r=pptok_lgtok(pst,lst,buf,sfo,t)) < 1)
			return r;

		/* optional quoted filename */
		if (t.type == token_type_t::strliteral) {
			name = csliteral(t.v.csliteral).makestring();
			if ((r=pptok_lgtok(pst,lst,buf,sfo,t)) < 1)
				return r;

			/* TODO: This needs to become the token source name */
		}

		do {
			if (t.type == token_type_t::newline) {
				t = token_t();
				break;
			}
			else if (t.type == token_type_t::backslashnewline) {
				continue;
			}

			if ((r=pptok_lgtok(pst,lst,buf,sfo,t)) < 1)
				return r;
		} while(1);

		buf.pos.row = row;
		return 1;
	}

	int pptok_errwarn(pptok_state_t &pst,lgtok_state_t &lst,rbuf &buf,source_file_object &sfo,token_t &t,const bool is_err) {
		const int line = t.pos.row;
		std::string msg;
		int r;

		(void)pst;

		do {
			if ((r=pptok_lgtok(pst,lst,buf,sfo,t)) < 1)
				return r;

			if (t.type == token_type_t::newline) {
				t = token_t();
				break;
			}
			else if (t.type == token_type_t::backslashnewline) {
				continue;
			}
			else if (t.type == token_type_t::strliteral) {
				if (!msg.empty()) msg += " ";
				msg += csliteral(t.v.csliteral).makestring();
			}
			else if (t.type == token_type_t::identifier) {
				if (!msg.empty()) msg += " ";
				msg += identifier(t.v.identifier).to_str();
			}
			else {
				if (!msg.empty()) msg += " ";
				msg += token_type_t_str(t.type);
			}
		} while (1);

		printf("%s(%d): %s\n",is_err?"Error":"Warning",line,msg.c_str());
		return 1;
	}

	int pptok_if(pptok_state_t &pst,lgtok_state_t &lst,rbuf &buf,source_file_object &sfo,token_t &t,const bool is_if) {
		/* #if has already been parsed.
		 * the last token we didn't use is left in &t for the caller to parse as most recently obtained,
		 * unless set to token_type_t::none in which case it will fetch another one */
		std::deque<token_t> expr;
		int r;

		(void)pst;

		do {
			if ((r=pptok_lgtok(pst,lst,buf,sfo,t)) < 1)
				return r;

			if (t.type == token_type_t::newline) {
				t = token_t();
				break;
			}
			else if (t.type == token_type_t::backslashnewline) {
				continue;
			}
			else if (t.type == token_type_t::floating) {
				return errno_return(EINVAL);
			}
			else if (t.type == token_type_t::strliteral) {
				return errno_return(EINVAL);
			}
			else if (t.type == token_type_t::charliteral) {
				return errno_return(EINVAL);
			}
			else if (t.type == token_type_t::r_true || t.type == token_type_t::r_false) {
				const bool res = (t.type == token_type_t::r_true);
				t = token_t(token_type_t::integer);
				t.v.integer.init();
				t.v.integer.v.u = (res > 0) ? 1 : 0;
			}
			else if (t.type == token_type_t::identifier) {
				if (identifier(t.v.identifier) == "defined") { /* defined(MACRO) */
					int paren = 0;
					int res = -1;

					do {
						if ((r=pptok_lgtok(pst,lst,buf,sfo,t)) < 1)
							return r;

						if (t.type == token_type_t::openparenthesis) {
							paren++;
						}
						else if (t.type == token_type_t::closeparenthesis) {
							if (paren == 0) return errno_return(EINVAL);
							paren--;
							if (paren == 0) break;
						}
						else if (t.type == token_type_t::identifier) {
							if (res >= 0) return errno_return(EINVAL);
							res = (pst.lookup_macro(t.v.identifier) != NULL) ? 1 : 0;
						}
						else if (t.type == token_type_t::newline) {
							pptok_lgtok_ungetch(pst,t);
							break;
						}
						else {
							break;
						}
					} while (1);

					if (res < 0)
						return errno_return(EINVAL);

					t = token_t(token_type_t::integer);
					t.v.integer.init();
					t.v.integer.v.u = (res > 0) ? 1 : 0;
				}
				else {
					const pptok_state_t::pptok_macro_ent_t* macro = pst.lookup_macro(t.v.identifier);
					if (macro) {
						if ((r=pptok_macro_expansion(macro,pst,lst,buf,sfo,t)) < 1) /* which affects pptok_lgtok() */
							return r;

						pst.macro_expansion_counter++;
						continue;
					}
					else {
						t = token_t(token_type_t::integer);
						t.v.integer.init();
						t.v.integer.v.u = 0;
					}
				}
			}

			expr.push_back(std::move(t));
		} while (1);

#if 0//DEBUG
		for (size_t i=0;i < pst.cond_block.size();i++) fprintf(stderr,"  ");
		fprintf(stderr,"%s (SUBST1)\n",is_if?"IF":"ELIF");
		for (auto i=expr.begin();i!=expr.end();i++) fprintf(stderr,"  > %s\n",(*i).to_str().c_str());
#endif

		integer_value_t rv;
		rv.init();

		{
			std::deque<token_t>::iterator ib = expr.begin();

			if ((r=pptok_eval_expr(rv,ib,expr.end())) < 1)
				return r;

			if (ib != expr.end())
				return errno_return(EINVAL);
		}

#if 0//DEBUG
		fprintf(stderr,"%s Result %s\n",is_if?"IF":"ELIF",rv.to_str().c_str());
#endif

		if (expr.empty())
			return errno_return(EINVAL);

		const bool cond = (rv.v.u != 0);

		if (is_if) {
			pptok_state_t::cond_block_t cb;

			/* if the condition matches and the parent condition if any is true */
			if (pst.condb_true())
				cb.state = cond ? pptok_state_t::cond_block_t::IS_TRUE : pptok_state_t::cond_block_t::WAITING;
			else
				cb.state = pptok_state_t::cond_block_t::DONE; /* never will be true */

			pst.cond_block.push(std::move(cb));
		}
		else {
			if (pst.cond_block.empty())
				return errno_return(EINVAL); /* #elifdef to what?? */

			auto &ent = pst.cond_block.top();
			if (ent.flags & pptok_state_t::cond_block_t::FL_ELSE) return errno_return(EINVAL); /* #elif cannot follow #else */

			if (ent.state == pptok_state_t::cond_block_t::WAITING) {
				if (cond) ent.state = pptok_state_t::cond_block_t::IS_TRUE;
			}
			else if (ent.state == pptok_state_t::cond_block_t::IS_TRUE) {
				ent.state = pptok_state_t::cond_block_t::DONE;
			}
		}

		return 1;
	}

	int pptok_ifdef(pptok_state_t &pst,lgtok_state_t &lst,rbuf &buf,source_file_object &sfo,token_t &t,const bool is_ifdef,const bool match_cond) {
		/* #ifdef has already been parsed.
		 * the last token we didn't use is left in &t for the caller to parse as most recently obtained,
		 * unless set to token_type_t::none in which case it will fetch another one */
		identifier_id_t s_id = identifier_none;
		pptok_macro_t macro;
		int r;

		(void)pst;

		if ((r=pptok_lgtok(pst,lst,buf,sfo,t)) < 1)
			return r;

		if (t.type != token_type_t::identifier || t.v.identifier == identifier_none)
			CCERR_RET(EINVAL,t.pos,"Identifier expected");

		identifier.assign(/*to*/s_id,/*from*/t.v.identifier);

		do {
			if ((r=pptok_lgtok(pst,lst,buf,sfo,t)) < 1)
				return r;

			if (t.type == token_type_t::newline) {
				t = token_t();
				break;
			}

			/* ignore trailing tokens */
		} while (1);

#if 0//DEBUG
		for (size_t i=0;i < pst.cond_block.size();i++) fprintf(stderr,"  ");
		fprintf(stderr,"%s '%s'\n",is_ifdef?(match_cond?"IFDEF":"IFNDEF"):(match_cond?"ELIFDEF":"ELIFNDEF"),s_id.to_str().c_str());
#endif

		const bool cond = (pst.lookup_macro(s_id) != NULL) == match_cond;

		if (is_ifdef) {
			pptok_state_t::cond_block_t cb;

			/* if the condition matches and the parent condition if any is true */
			if (pst.condb_true())
				cb.state = cond ? pptok_state_t::cond_block_t::IS_TRUE : pptok_state_t::cond_block_t::WAITING;
			else
				cb.state = pptok_state_t::cond_block_t::DONE; /* never will be true */

			pst.cond_block.push(std::move(cb));
		}
		else {
			if (pst.cond_block.empty())
				return errno_return(EINVAL); /* #elifdef to what?? */

			auto &ent = pst.cond_block.top();
			if (ent.flags & pptok_state_t::cond_block_t::FL_ELSE) return errno_return(EINVAL); /* #elifdef,etc cannot follow #else */

			if (ent.state == pptok_state_t::cond_block_t::WAITING) {
				if (cond) ent.state = pptok_state_t::cond_block_t::IS_TRUE;
			}
			else if (ent.state == pptok_state_t::cond_block_t::IS_TRUE) {
				ent.state = pptok_state_t::cond_block_t::DONE;
			}
		}

		identifier.release(s_id);
		return 1;
	}

	int pptok_else(pptok_state_t &pst,lgtok_state_t &lst,rbuf &buf,source_file_object &sfo,token_t &t) {
		/* #else has already been parsed.
		 * the last token we didn't use is left in &t for the caller to parse as most recently obtained,
		 * unless set to token_type_t::none in which case it will fetch another one */
		pptok_macro_t macro;
		int r;

		(void)pst;

		do {
			if ((r=pptok_lgtok(pst,lst,buf,sfo,t)) < 1)
				return r;

			if (t.type == token_type_t::newline) {
				t = token_t();
				break;
			}

			/* ignore trailing tokens */
		} while (1);

#if 0//DEBUG
		for (size_t i=0;i < pst.cond_block.size();i++) fprintf(stderr,"  ");
		fprintf(stderr,"ELSE condempty=%u\n",pst.cond_block.empty());
#endif

		if (pst.cond_block.empty())
			return errno_return(EINVAL); /* #else to what?? */

		auto &ent = pst.cond_block.top();
		if (ent.flags & pptok_state_t::cond_block_t::FL_ELSE) return errno_return(EINVAL); /* #else cannot follow #else */
		ent.flags |= pptok_state_t::cond_block_t::FL_ELSE;

		/* if nothing else was true, this block is true
		 * if the previous block was true, this is false */
		if (ent.state == pptok_state_t::cond_block_t::WAITING)
			ent.state = pptok_state_t::cond_block_t::IS_TRUE;
		else if (ent.state == pptok_state_t::cond_block_t::IS_TRUE)
			ent.state = pptok_state_t::cond_block_t::DONE;
		return 1;
	}

	int pptok_endif(pptok_state_t &pst,lgtok_state_t &lst,rbuf &buf,source_file_object &sfo,token_t &t) {
		/* #endif has already been parsed.
		 * the last token we didn't use is left in &t for the caller to parse as most recently obtained,
		 * unless set to token_type_t::none in which case it will fetch another one */
		pptok_macro_t macro;
		int r;

		(void)pst;

		do {
			if ((r=pptok_lgtok(pst,lst,buf,sfo,t)) < 1)
				return r;

			if (t.type == token_type_t::newline) {
				t = token_t();
				break;
			}

			/* ignore trailing tokens */
		} while (1);

#if 0//DEBUG
		for (size_t i=1;i < pst.cond_block.size();i++) fprintf(stderr,"  ");
		fprintf(stderr,"ENDIF\n");
#endif

		if (pst.cond_block.empty())
			return errno_return(EPIPE);

		pst.cond_block.pop();
		return 1;
	}

	bool pptok_define_asm_allowed_token(const token_t &t) {
		switch (t.type) {
			/* NTS: lgtok() parsing pretty much prevents these tokens entirely within a #define.
			 *      even so, make sure! */
			case token_type_t::r_ppif:
			case token_type_t::r_ppifdef:
			case token_type_t::r_ppelse:
			case token_type_t::r_ppelif:
			case token_type_t::r_ppelifdef:
			case token_type_t::r_ppendif:
			case token_type_t::r_ppifndef:
			case token_type_t::r_ppelifndef:
				return false;
			default:
				break;
		}

		return true;
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
			case token_type_t::r_ppendif:
			case token_type_t::r_ppifndef:
			case token_type_t::r_ppelifndef:
			case token_type_t::r_ppinclude:
			case token_type_t::r_pperror:
			case token_type_t::r_ppwarning:
			case token_type_t::r_ppline:
			case token_type_t::r_pppragma:
			case token_type_t::r_ppembed:
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
		identifier_id_t s_id_va_args_subst = identifier_none;
		identifier_id_t s_id = identifier_none;
		pptok_macro_t macro;
		int r;

		(void)pst;

		if ((r=pptok_lgtok(pst,lst,buf,sfo,t)) < 1)
			return r;

		if (t.type != token_type_t::identifier || t.v.identifier == identifier_none)
			CCERR_RET(EINVAL,t.pos,"Identifier expected");

		identifier.assign(/*to*/s_id,/*from*/t.v.identifier);

		/* if the next character is '(' (without a space), it's a parameter list.
		 * a space and then '(' doesn't count. that's how GCC behaves, anyway. */
		if (buf.peekb() == '(') {
			buf.discardb(); /* don't bother with parsing as token */

			macro.flags |= pptok_macro_t::FL_PARENTHESIS;

			do {
				identifier_id_t s_p = identifier_none;

				if ((r=pptok_lgtok(pst,lst,buf,sfo,t)) < 1)
					return r;
				if (t.type == token_type_t::newline)
					CCERR_RET(EINVAL,t.pos,"Unexpected newline");
				if (t.type == token_type_t::closeparenthesis)
					break;
				if (macro.flags & pptok_macro_t::FL_VARIADIC) /* you can't put additional params past the ... or even a comma */
					CCERR_RET(EINVAL,t.pos,"Cannot define macro parameters after variadic ellipsis");
				if (t.type == token_type_t::ellipsis) {
					macro.flags |= pptok_macro_t::FL_VARIADIC;
					continue;
				}
				if (t.type != token_type_t::identifier)
					CCERR_RET(EINVAL,t.pos,"Identifier expected");

				eat_whitespace(buf,sfo);

				/* GNU GCC arg... variadic macros that predate __VA_ARGS__ */
				if (buf.peekb(0) == '.' && buf.peekb(1) == '.' && buf.peekb(2) == '.') {
					identifier.assign(/*to*/s_id_va_args_subst,/*from*/t.v.identifier);
					macro.flags |= pptok_macro_t::FL_VARIADIC | pptok_macro_t::FL_NO_VA_ARGS;
					buf.discardb(3);
				}
				else {
					identifier.assign(/*to*/s_p,/*from*/t.v.identifier);
					macro.parameters.push_back(s_p);
				}

				if (buf.peekb() == ',') { buf.discardb(); }
				else if (buf.peekb() == ')') { buf.discardb(); break; }
				else CCERR_RET(EINVAL,buf.pos,"Unexpected characters in macro parameter list");
			} while (1);
		}

		do {
			if ((r=pptok_lgtok(pst,lst,buf,sfo,t)) < 1)
				return r;

			if (t.type == token_type_t::identifier || t.type == token_type_t::r___asm_text) {
				if (identifier(t.v.identifier) == "__VA_ARGS__") {
					if (macro.flags & pptok_macro_t::FL_VARIADIC) {
						t = token_t(token_type_t::r___VA_ARGS__,t.pos,t.source_file);
					}
				}
				else if (identifier(t.v.identifier) == "__VA_OPT__") {
					if (macro.flags & pptok_macro_t::FL_VARIADIC) {
						t = token_t(token_type_t::r___VA_OPT__,t.pos,t.source_file);
					}
				}
			}

			if (t.type == token_type_t::newline) {
				t = token_t();
				break;
			}
			else if (t.type == token_type_t::backslashnewline) { /* \ + newline continues the macro past newline */
				macro.tokens.push_back(std::move(token_t(token_type_t::newline,t.pos,t.source_file)));
			}
			else if (t.type == token_type_t::identifier || t.type == token_type_t::r___asm_text) {
				/* if the identifier matches a paraemeter then put in a parameter reference,
				 * else pass the identifier along. */

				if (!macro.tokens.empty() && macro.tokens[macro.tokens.size()-1u].type == token_type_t::pound)
					macro.flags |= pptok_macro_t::FL_STRINGIFY;

				if (s_id_va_args_subst != identifier_none && identifier(s_id_va_args_subst) == identifier(t.v.identifier)) {
					/* GNU arg... variadic convert to __VA_ARGS__ */
					macro.tokens.push_back(std::move(token_t(token_type_t::r___VA_ARGS__,t.pos,t.source_file)));
				}
				else {
					for (auto pi=macro.parameters.begin();pi!=macro.parameters.end();pi++) {
						if (identifier(*pi) == identifier(t.v.identifier)) {
							t = token_t(token_type_t::r_macro_paramref);
							t.v.paramref = size_t(pi - macro.parameters.begin());
							break;
						}
					}
				}

				macro.tokens.push_back(std::move(t));
			}
			else if (pptok_define_allowed_token(t)) {
				if (t.type == token_type_t::r___VA_ARGS__) {
					if (macro.flags & pptok_macro_t::FL_NO_VA_ARGS) {
						/* do not expand __VA_ARGS__ if GNU args... syntax used */
						continue;
					}
					else if (macro.tokens.size() >= 2) {
						const size_t i = macro.tokens.size() - 2;
						/* GNU extension: expression, ##__VA_ARGS__ which means the same thing as expression __VA_OPT__(,) __VA_ARGS__ */
						if (macro.tokens[i].type == token_type_t::comma && macro.tokens[i+1].type == token_type_t::poundpound) {
							/* convert ,##__VA_ARGS__ to __VA__OPT__(,) __VA_ARGS__ and then continue to bottom to add __VA_ARGS__ */
							macro.tokens.resize(i+4);
							macro.tokens[i+0] = std::move(token_t(token_type_t::r___VA_OPT__));
							macro.tokens[i+1] = std::move(token_t(token_type_t::openparenthesis));
							macro.tokens[i+2] = std::move(token_t(token_type_t::comma,t.pos,t.source_file));
							macro.tokens[i+3] = std::move(token_t(token_type_t::closeparenthesis));
						}
					}
				}

				macro.tokens.push_back(std::move(t));
			}
			else {
				return errno_return(EINVAL);
			}
		} while (1);

#if 0//DEBUG
		fprintf(stderr,"MACRO '%s'",s_id.to_str().c_str());
		if (macro.flags & pptok_macro_t::FL_PARENTHESIS) fprintf(stderr," PARENTHESIS");
		if (macro.flags & pptok_macro_t::FL_VARIADIC) fprintf(stderr," VARIADIC");
		if (macro.flags & pptok_macro_t::FL_NO_VA_ARGS) fprintf(stderr," NO_VA_ARGS");
		if (macro.flags & pptok_macro_t::FL_STRINGIFY) fprintf(stderr," STRINGIFY");
		fprintf(stderr,"\n");
		fprintf(stderr,"  parameters:\n");
		for (auto i=macro.parameters.begin();i!=macro.parameters.end();i++)
			fprintf(stderr,"    > %s\n",(*i).to_str().c_str());
		fprintf(stderr,"  tokens:\n");
		for (auto i=macro.tokens.begin();i!=macro.tokens.end();i++)
			fprintf(stderr,"    > %s\n",(*i).to_str().c_str());
#endif

		// NTS:
		//  Open Watcom header rel/h/win/win16.h
		//    WM_SPOOLERSTATUS is defined twice, but with the exact same value.
		//    many compilers overlook #defining a macro multiple times IF each
		//    time the value is exactly the same. We need to do that too:
		//    If the macro exists but the tokens are the same, silently ignore
		//    it instead of throwing an error.
		if (!pst.create_macro(s_id,macro))
			return errno_return(EEXIST);

		identifier.release(s_id_va_args_subst);
		identifier.release(s_id);
		return 1;
	}

	int pptok_macro_expansion(const pptok_state_t::pptok_macro_ent_t* macro,pptok_state_t &pst,lgtok_state_t &lst,rbuf &buf,source_file_object &sfo,token_t &t) {
		/* caller just parsed the identifier token */
		int r;

#if 0//DEBUG
		fprintf(stderr,"Hello macro '%s'\n",t.v.strliteral.to_str().c_str());
#endif

		if (pst.macro_expansion_counter > 1024)
			CCERR_RET(ELOOP,buf.pos,"Too many macro expansions");

		std::vector< std::vector<token_t> > params;
		std::vector< rbuf > params_str;

		if (macro->ment.flags & pptok_macro_t::FL_PARENTHESIS) {
			if ((r=pptok_lgtok(pst,lst,buf,sfo,t)) < 1)
				return r;
			if (t.type != token_type_t::openparenthesis)
				return errno_return(EINVAL);

			if (macro->ment.flags & pptok_macro_t::FL_STRINGIFY) {
				/* parse every arg as a string, then convert to tokens, but preserve the string
				 * so the code below can properly expand #parameter to string */

				/* NTS: #__VA_ARGS__ is valid, and it stringifies into every parameter past the
				 *      last named parameter INCLUDING the commas!
				 *
				 *      i.e. #define XYZ(a,...) #a #__VA_ARGS__
				 *           XYZ(a,b,c,d) would become "ab,c,d" */

				do {
					eat_whitespace(buf,sfo);
					if (buf.peekb() == ')') {
						buf.discardb();
						break;
					}

					bool allow_commas = false;
					unsigned int paren = 0;
					rbuf arg_str;

					/* Apparently in variadic macros, #__VA_ARGS__ expands to the extra parameters INCLUDING commas */
					if ((macro->ment.flags & pptok_macro_t::FL_VARIADIC) && params_str.size() >= macro->ment.parameters.size())
						allow_commas = true;

					/* if you're calling a macro with 4096 chars in it you are using too much, split it up! */
					arg_str.pos = buf.pos;
					arg_str.set_source_file(buf.source_file);
					if (!arg_str.allocate(4096))
						return errno_return(ENOMEM);

					do {
						if (buf.data_avail() < 1) rbuf_sfd_refill(buf,sfo);
						if (buf.data_avail() == 0) return errno_return(EINVAL);

						unsigned char c = buf.peekb();
						if (c == ',' && paren == 0 && !allow_commas) {
							buf.discardb();
							break;
						}

						/* string constants are copied as-is and then stringified like that,
						 * except that within strings, comma and parens are part of the string,
						 * however escapes like \t and \n are also stringified as-is HOWEVER
						 * the backslash even if copied still works as an escape i.e you can
						 * use \" to put \" in the stringified string without closing the string. */
						if (c == '\"' || c == '\'') {
							const unsigned char ec = c;
							if (arg_str.end >= arg_str.fence) return errno_return(ENOSPC);
							*(arg_str.end++) = c;
							buf.discardb();

							do {
								if (buf.data_avail() < 1) rbuf_sfd_refill(buf,sfo);
								if (buf.data_avail() == 0) return errno_return(EINVAL);
								c = buf.getb();
								if (arg_str.end >= arg_str.fence) return errno_return(ENOSPC);
								*(arg_str.end++) = c;

								if (c == ec) {
									break;
								}
								else if (c == '\\') {
									if (buf.data_avail() < 1) rbuf_sfd_refill(buf,sfo);
									if (buf.data_avail() == 0) return errno_return(EINVAL);
									c = buf.getb();
									if (arg_str.end >= arg_str.fence) return errno_return(ENOSPC);
									*(arg_str.end++) = c;
								}
							} while (1);

							continue;
						}

						if (c == '(') {
							paren++;
						}
						else if (c == ')') {
							if (paren == 0) break; /* leave it for the outer loop */
							paren--;
						}

						if (arg_str.end >= arg_str.fence) return errno_return(ENOSPC);
						*(arg_str.end++) = c;
						buf.discardb();

						if (c == ' ') eat_whitespace(buf,sfo);
					} while (1);

					while (arg_str.end > arg_str.data && is_whitespace(*(arg_str.end-1)))
						arg_str.end--;

					params_str.push_back(std::move(arg_str));
				} while (1);

#if 0//DEBUG
				fprintf(stderr,"  Parameters filled in at call (stringify):\n");
				for (auto i=params_str.begin();i!=params_str.end();i++) {
					assert((*i).data != NULL);
					fprintf(stderr,"    [%u]\n",(unsigned int)(i-params_str.begin()));
					fprintf(stderr,"      \"");
					fwrite((*i).data,(*i).data_avail(),1,stderr);
					fprintf(stderr,"\"\n");
				}
#endif

				for (auto i=params_str.begin();i!=params_str.end();i++) {
					source_file_object subsfo;
					std::vector<token_t> arg;
					auto &subbuf = *i;
					position_t saved_pos = subbuf.pos;

					eat_whitespace(subbuf,subsfo);
					while (subbuf.data_avail() > 0) {
						if ((r=pptok_lgtok(pst,lst,subbuf,subsfo,t)) < 1)
							return r;
						if (!pptok_define_allowed_token(t))
							return errno_return(EINVAL);

						arg.push_back(std::move(t));
						eat_whitespace(subbuf,subsfo);
					}

					params.push_back(std::move(arg));
					subbuf.data = subbuf.base;
					subbuf.pos = saved_pos;
				}
			}
			else {
				std::vector<token_t> arg;
				unsigned int paren = 0;

				do {
					if ((r=pptok_lgtok(pst,lst,buf,sfo,t)) < 1)
						return r;
					if (!pptok_define_allowed_token(t))
						return errno_return(EINVAL);

					if (t.type == token_type_t::closeparenthesis) {
						if (paren == 0) {
							if (!params.empty() || !arg.empty()) {
								params.push_back(std::move(arg)); arg.clear();
							}
							break;
						}
						else {
							paren--;
							arg.push_back(std::move(t));
						}
					}
					else if (t.type == token_type_t::openparenthesis) {
						paren++;
						arg.push_back(std::move(t));
					}
					else if (t.type == token_type_t::comma && paren == 0) {
						params.push_back(std::move(arg)); arg.clear();
					}
					else {
						arg.push_back(std::move(t));
					}
				} while (1);
			}

#if 0//DEBUG
			fprintf(stderr,"  Parameters filled in at call:\n");
			for (auto i=params.begin();i!=params.end();i++) {
				auto &ent = *i;

				fprintf(stderr,"    [%u]\n",(unsigned int)(i-params.begin()));
				for (auto j=ent.begin();j!=ent.end();j++)
					fprintf(stderr,"      %s\n",(*j).to_str().c_str());
			}

			if (params.size() < macro->ment.parameters.size())
				return errno_return(EPIPE);
			if (params.size() > macro->ment.parameters.size() && !(macro->ment.flags & pptok_macro_t::FL_VARIADIC))
				return errno_return(E2BIG);
#endif
		}

		/* inject tokens from macro */
		std::vector<token_t> out;
		for (auto i=macro->ment.tokens.begin();i!=macro->ment.tokens.end();i++) {
go_again:
			/* check for macro param stringification */
			if ((*i).type == token_type_t::pound) {
				auto i2 = i; i2++; /* assume i != macro->ment.tokens.end() */
				if (i2 != macro->ment.tokens.end()) {
					if ((*i2).type == token_type_t::r_macro_paramref) {
						/* #param stringification */
						i++; /* step forward to ref */
						assert((*i).v.paramref < params_str.size());
						const auto &rb = params_str[(*i).v.paramref];

						if (rb.data_avail() > 0) {
							token_t st;
							st.type = token_type_t::strliteral;
							st.set_source_file(rb.source_file);
							st.v.csliteral = csliteral.alloc();
							st.pos = rb.pos;

							if (!csliteral(st.v.csliteral).alloc(rb.data_avail()))
								return errno_return(ENOMEM);

							assert(csliteral(st.v.csliteral).length == rb.data_avail());
							memcpy(csliteral(st.v.csliteral).data,rb.data,rb.data_avail());
							out.push_back(std::move(st));
						}

						continue;
					}
					else if ((*i2).type == token_type_t::r___VA_ARGS__) {
						/* #__VA_ARGS__ stringification */
						i++; /* step forward to ref */

						if (macro->ment.flags & pptok_macro_t::FL_VARIADIC) {
							for (size_t pi=macro->ment.parameters.size();pi < params_str.size();pi++) {
								const auto &rb = params_str[pi];
								token_t st;

								st.type = token_type_t::strliteral;
								st.set_source_file(rb.source_file);
								st.v.csliteral = csliteral.alloc();
								st.pos = rb.pos;

								if (!csliteral(st.v.csliteral).alloc(rb.data_avail()))
									return errno_return(ENOMEM);

								assert(csliteral(st.v.csliteral).length == rb.data_avail());
								memcpy(csliteral(st.v.csliteral).data,rb.data,rb.data_avail());
								out.push_back(std::move(st));
							}

							continue;
						}
					}
				}
			}

			if ((*i).type == token_type_t::r_macro_paramref) {
				assert((*i).v.paramref < params.size());
				const auto &param = params[(*i).v.paramref];
				for (auto j=param.begin();j!=param.end();j++)
					out.push_back(*j);
			}
			else if ((*i).type == token_type_t::r___VA_OPT__) {
				i++; if (i == macro->ment.tokens.end()) return errno_return(EINVAL); /* skip __VA_OPT__ */
				if ((*i).type != token_type_t::openparenthesis) return errno_return(EINVAL);
				i++; if (i == macro->ment.tokens.end()) return errno_return(EINVAL); /* skip opening paren */

				unsigned int sparen = 0;
				bool do_copy = false;

				if ((macro->ment.flags & pptok_macro_t::FL_VARIADIC) && params.size() > macro->ment.parameters.size())
					do_copy = true;

				do {
					if (i == macro->ment.tokens.end()) return errno_return(EINVAL);
					const auto &current = (*i); i++;

					if (current.type == token_type_t::closeparenthesis) {
						if (sparen == 0) {
							break;
						}
						else {
							sparen--;
							if (do_copy) out.push_back(current);
						}
					}
					else if (current.type == token_type_t::openparenthesis) {
						sparen++;
						if (do_copy) out.push_back(current);
					}
					else if (current.type == token_type_t::pound && i != macro->ment.tokens.end() &&
						(*i).type == token_type_t::r_macro_paramref) {
						const auto &pncurrent = (*i); i++;
						assert(pncurrent.v.paramref < params_str.size());
						const auto &rb = params_str[pncurrent.v.paramref];

						if (rb.data_avail() > 0) {
							token_t st;
							st.type = token_type_t::strliteral;
							st.set_source_file(rb.source_file);
							st.v.csliteral = csliteral.alloc();
							st.pos = rb.pos;

							if (!csliteral(st.v.csliteral).alloc(rb.data_avail()))
								return errno_return(ENOMEM);

							assert(csliteral(st.v.csliteral).length == rb.data_avail());
							memcpy(csliteral(st.v.csliteral).data,rb.data,rb.data_avail());
							out.push_back(std::move(st));
						}
					}
					else if (current.type == token_type_t::r_macro_paramref) {
						assert(current.v.paramref < params.size());
						const auto &param = params[current.v.paramref];
						for (auto j=param.begin();j!=param.end();j++)
							out.push_back(*j);
					}
					else if (current.type == token_type_t::r___VA_OPT__) {
						/* GCC doesn't allow it, neither do we */
						fprintf(stderr,"You can't use __VA_OPT__ inside __VA_OPT__\n");
						return errno_return(EINVAL);
					}
					else {
						if (do_copy) out.push_back(current);
					}
				} while (1);

				/* this is a for loop that will do i++, jump back to beginning */
				if (i == macro->ment.tokens.end())
					break;
				else
					goto go_again;
			}
			else if ((*i).type == token_type_t::r___VA_ARGS__) {
				if (macro->ment.flags & pptok_macro_t::FL_VARIADIC) {
					for (size_t pi=macro->ment.parameters.size();pi < params.size();pi++) {
						if (pi != macro->ment.parameters.size())
							out.push_back(std::move(token_t(token_type_t::comma,(*i).pos,(*i).source_file)));

						const auto &param = params[pi];
						for (auto j=param.begin();j!=param.end();j++)
							out.push_back(*j);
					}
				}
			}
			else {
				out.push_back(*i);
			}
		}

		for (auto i=out.rbegin();i!=out.rend();i++)
			pst.macro_expansion.push_front(std::move(*i));

		return 1;
	}

	static constexpr unsigned int CBIS_USER_HEADER = (1u << 0u);
	static constexpr unsigned int CBIS_SYS_HEADER = (1u << 1u);
	static constexpr unsigned int CBIS_NEXT = (1u << 2u);

	static void path_slash_translate(std::string &path) {
		for (auto &c : path) {
			if (c == '\\') c = '/';
		}
	}

	static std::string path_combine(const std::string &base,const std::string &rel) {
		if (!base.empty() && !rel.empty())
			return base + "/" + rel;
		if (!rel.empty())
			return rel;

		return std::string();
	}

	std::vector<std::string> cb_include_search_paths;

	typedef bool (*cb_include_accept_path_t)(const std::string &p);

	static bool cb_include_accept_path_default(const std::string &/*p*/) {
		return true;
	}

	cb_include_accept_path_t cb_include_accept_path = cb_include_accept_path_default;

	typedef CCMiniC::source_file_object* (*cb_include_search_t)(CCMiniC::pptok_state_t &pst,CCMiniC::lgtok_state_t &lst,const CCMiniC::token_t &t,unsigned int fl);

	static CCMiniC::source_file_object* cb_include_search_default(CCMiniC::pptok_state_t &/*pst*/,CCMiniC::lgtok_state_t &/*lst*/,const CCMiniC::token_t &t,unsigned int fl) {
		CCMiniC::source_file_object *sfo = NULL;

		if (fl & CBIS_USER_HEADER) {
			std::string path = csliteral(t.v.csliteral).makestring(); path_slash_translate(path);
			if (!path.empty() && cb_include_accept_path(path) && access(path.c_str(),R_OK) >= 0) {
				const int fd = open(path.c_str(),O_RDONLY|O_BINARY);
				if (fd >= 0) {
					sfo = new CCMiniC::source_fd(fd/*takes ownership*/,path);
					assert(sfo->iface == CCMiniC::source_file_object::IF_FD);
					return sfo;
				}
			}
		}

		for (auto ipi=cb_include_search_paths.begin();ipi!=cb_include_search_paths.end();ipi++) {
			std::string path = path_combine(*ipi,csliteral(t.v.csliteral).makestring()); path_slash_translate(path);
			if (!path.empty() && cb_include_accept_path(path) && access(path.c_str(),R_OK) >= 0) {
				const int fd = open(path.c_str(),O_RDONLY|O_BINARY);
				if (fd >= 0) {
					sfo = new CCMiniC::source_fd(fd/*takes ownership*/,path);
					assert(sfo->iface == CCMiniC::source_file_object::IF_FD);
					return sfo;
				}
			}
		}

		return NULL;
	}

	cb_include_search_t cb_include_search = cb_include_search_default;

	int pptok(pptok_state_t &pst,lgtok_state_t &lst,rbuf &buf,source_file_object &sfo,token_t &t) {
		int r;

#define TRY_AGAIN \
		if (t.type != token_type_t::none) \
			goto try_again_w_token; \
		else \
			goto try_again;

try_again:
		if (!pst.include_stk.empty()) {
			CCMiniC::pptok_state_t::include_t &i = pst.include_stk.top();
			if ((r=pptok_lgtok(pst,lst,i.rb,*i.sfo,t)) < 0)
				return r;

			if (r == 0) {
				pst.include_pop();
				goto try_again;
			}
		}
		else {
			if ((r=pptok_lgtok(pst,lst,buf,sfo,t)) < 1)
				return r;
		}

try_again_w_token:
		switch (t.type) {
			case token_type_t::r___LINE__:
				if (!pst.condb_true())
					goto try_again;
				t.type = token_type_t::integer;
				t.v.integer.init();
				t.v.integer.v.u = t.pos.row;
				break;
			case token_type_t::r___FILE__:
				if (!pst.condb_true())
					goto try_again;
				t.type = token_type_t::strliteral;
				t.v.csliteral = csliteral.alloc();
				{
					const char *name = sfo.getname();
					if (name) {
						const size_t l = strlen(name);

						if (!csliteral(t.v.csliteral).alloc(l))
							return errno_return(ENOMEM);

						assert(csliteral(t.v.csliteral).length == l);
						memcpy(csliteral(t.v.csliteral).data,name,l);
					}
				}
				break;
			case token_type_t::r_ppdefine:
				if (!pst.condb_true())
					goto try_again;
				if ((r=pptok_define(pst,lst,buf,sfo,t)) < 1)
					return r;

				TRY_AGAIN; /* does not fall through */
			case token_type_t::r_ppundef:
				if (!pst.condb_true())
					goto try_again;
				if ((r=pptok_undef(pst,lst,buf,sfo,t)) < 1)
					return r;

				TRY_AGAIN; /* does not fall through */
			case token_type_t::r_ppifdef:
				if ((r=pptok_ifdef(pst,lst,buf,sfo,t,true,true)) < 1)
					return r;

				TRY_AGAIN; /* does not fall through */
			case token_type_t::r_ppifndef:
				if ((r=pptok_ifdef(pst,lst,buf,sfo,t,true,false)) < 1)
					return r;

				TRY_AGAIN; /* does not fall through */
			case token_type_t::r_ppelifdef:
				if ((r=pptok_ifdef(pst,lst,buf,sfo,t,false,true)) < 1)
					return r;

				TRY_AGAIN; /* does not fall through */
			case token_type_t::r_ppelifndef:
				if ((r=pptok_ifdef(pst,lst,buf,sfo,t,false,false)) < 1)
					return r;

				TRY_AGAIN; /* does not fall through */
			case token_type_t::r_ppelse:
				if ((r=pptok_else(pst,lst,buf,sfo,t)) < 1)
					return r;

				TRY_AGAIN; /* does not fall through */
			case token_type_t::r_ppendif:
				if ((r=pptok_endif(pst,lst,buf,sfo,t)) < 1)
					return r;

				TRY_AGAIN; /* does not fall through */
			case token_type_t::r_ppif:
				if ((r=pptok_if(pst,lst,buf,sfo,t,true)) < 1)
					return r;

				TRY_AGAIN; /* does not fall through */
			case token_type_t::r_ppelif:
				if ((r=pptok_if(pst,lst,buf,sfo,t,false)) < 1)
					return r;

				TRY_AGAIN; /* does not fall through */
			case token_type_t::r_pperror:
			case token_type_t::r_ppwarning:
				if (!pst.condb_true())
					goto try_again;
				if ((r=pptok_errwarn(pst,lst,buf,sfo,t,t.type == token_type_t::r_pperror)) < 1)
					return r;

				TRY_AGAIN; /* does not fall through */
			case token_type_t::r_ppline:
				if (!pst.condb_true())
					goto try_again;
				if ((r=pptok_line(pst,lst,buf,sfo,t)) < 1)
					return r;

				TRY_AGAIN; /* does not fall through */
			case token_type_t::r_ppinclude: {
				if (!pst.condb_true())
					goto try_again;
				if ((r=pptok_lgtok(pst,lst,buf,sfo,t)) < 1)
					return r;
				if (t.type == token_type_t::strliteral) {
					CCMiniC::source_file_object *sfo = cb_include_search(pst,lst,t,CBIS_USER_HEADER);
					if (sfo == NULL) {
						fprintf(stderr,"Unable to #include \"%s\"\n",csliteral(t.v.csliteral).makestring().c_str());
						return errno_return(ENOENT);
					}
					pst.include_push(sfo);
					goto try_again;
				}
				else if (t.type == token_type_t::anglestrliteral) {
					CCMiniC::source_file_object *sfo = cb_include_search(pst,lst,t,CBIS_SYS_HEADER);
					if (sfo == NULL) sfo = cb_include_search(pst,lst,t,CBIS_USER_HEADER);
					if (sfo == NULL) {
						fprintf(stderr,"Unable to #include <%s>\n",csliteral(t.v.csliteral).makestring().c_str());
						return errno_return(ENOENT);
					}
					pst.include_push(sfo);
					goto try_again;
				}
				goto try_again_w_token; }
			case token_type_t::r_ppinclude_next: {
				if (!pst.condb_true())
					goto try_again;
				if ((r=pptok_lgtok(pst,lst,buf,sfo,t)) < 1)
					return r;
				if (t.type == token_type_t::strliteral) {
					CCMiniC::source_file_object *sfo = cb_include_search(pst,lst,t,CBIS_USER_HEADER|CBIS_NEXT);
					if (sfo == NULL) {
						fprintf(stderr,"Unable to #include_next \"%s\"\n",csliteral(t.v.csliteral).makestring().c_str());
						return errno_return(ENOENT);
					}
					pst.include_push(sfo);
					goto try_again;
				}
				else if (t.type == token_type_t::anglestrliteral) {
					CCMiniC::source_file_object *sfo = cb_include_search(pst,lst,t,CBIS_SYS_HEADER|CBIS_NEXT);
					if (sfo == NULL) sfo = cb_include_search(pst,lst,t,CBIS_USER_HEADER|CBIS_NEXT);
					if (sfo == NULL) {
						fprintf(stderr,"Unable to #include_next <%s>\n",csliteral(t.v.csliteral).makestring().c_str());
						return errno_return(ENOENT);
					}
					pst.include_push(sfo);
					goto try_again;
				}
				goto try_again_w_token; }
			case token_type_t::identifier: /* macro substitution */
			case token_type_t::r___asm_text: { /* to allow macros to work with assembly language */
				if (!pst.condb_true())
					goto try_again;

				const pptok_state_t::pptok_macro_ent_t* macro = pst.lookup_macro(t.v.identifier);
				if (macro) {
					if ((r=pptok_macro_expansion(macro,pst,lst,buf,sfo,t)) < 1)
						return r;

					pst.macro_expansion_counter++;
					goto try_again;
				}
				break; }
			default:
				if (!pst.condb_true())
					goto try_again;

				break;
		}

#undef TRY_AGAIN

		return 1;
	}

	int lctok(pptok_state_t &pst,lgtok_state_t &lst,rbuf &buf,source_file_object &sfo,token_t &t) {
		int r;

		if ((r=pptok(pst,lst,buf,sfo,t)) < 1)
			return r;

		/* it might be a reserved keyword, check */
		if (t.type == token_type_t::identifier) {
			for (const ident2token_t *i2t=ident2tok_cc;i2t < (ident2tok_cc+ident2tok_cc_length);i2t++) {
				if (identifier(t.v.identifier).length == i2t->len) {
					if (!memcmp(identifier(t.v.identifier).data,i2t->str,i2t->len)) {
						t.clear_v();
						t.type = token_type_t(i2t->token);
						return 1;
					}
				}
			}
		}

		return 1;
	}

	///////////////////////////////////////

	enum storage_class_idx_t {
		SCI_TYPEDEF,		// 0
		SCI_EXTERN,
		SCI_STATIC,
		SCI_AUTO,
		SCI_REGISTER,
		SCI_CONSTEXPR,		// 5
		SCI_INLINE,
		SCI_CONSTEVAL,
		SCI_CONSTINIT,

		SCI__MAX
	};

	const char *storage_class_idx_t_str[SCI__MAX] = {
		"typedef",		// 0
		"extern",
		"static",
		"auto",
		"register",
		"constexpr",		// 5
		"inline",
		"consteval",
		"constinit"
	};

	typedef unsigned int storage_class_t;

#define X(c) static constexpr storage_class_t SC_##c = 1u << SCI_##c
	X(TYPEDEF);			// 0
	X(EXTERN);
	X(STATIC);
	X(AUTO);
	X(REGISTER);
	X(CONSTEXPR);			// 5
	X(INLINE);
	X(CONSTEVAL);
	X(CONSTINIT);
#undef X

	///////////////////////////////////////

	/* this is a bitmask because you can specify more than one, these are indexes */
	enum type_specifier_idx_t {
		TSI_VOID=0,		// 0
		TSI_CHAR,
		TSI_SHORT,
		TSI_INT,
		TSI_LONG,
		TSI_FLOAT,		// 5
		TSI_DOUBLE,
		TSI_SIGNED,
		TSI_UNSIGNED,
		TSI_LONGLONG,
		TSI_ENUM,		// 10
		TSI_STRUCT,
		TSI_UNION,
		TSI_MATCH_TYPEDEF,
		TSI_MATCH_BUILTIN,
		TSI_SZ8,		// 15
		TSI_SZ16,
		TSI_SZ32,
		TSI_SZ64,

		TSI__MAX
	};

	const char *type_specifier_idx_t_str[TSI__MAX] = {
		"void",			// 0
		"char",
		"short",
		"int",
		"long",
		"float",		// 5
		"double",
		"signed",
		"unsigned",
		"longlong",
		"enum",			// 10
		"struct",
		"union",
		"matches-typedef",
		"matches-builtin",
		"sz8",			// 15
		"sz16",
		"sz32",
		"sz64"
	};

	typedef unsigned int type_specifier_t;

#define X(c) static constexpr type_specifier_t TS_##c = 1u << TSI_##c
	X(VOID);			// 0
	X(CHAR);
	X(SHORT);
	X(INT);
	X(LONG);
	X(FLOAT);			// 5
	X(DOUBLE);
	X(SIGNED);
	X(UNSIGNED);
	X(LONGLONG);
	X(ENUM);			// 10
	X(STRUCT);
	X(UNION);
	X(MATCH_TYPEDEF);
	X(MATCH_BUILTIN);
	X(SZ8);				// 15
	X(SZ16);
	X(SZ32);
	X(SZ64);
#undef X

	///////////////////////////////////////

	template <typename T> constexpr bool only_one_bit_set(const T &t) {
		return (t & (t - T(1u))) == T(0u);
	}

	/* this is a bitmask because you can specify more than one, these are indexes */
	enum type_qualifier_idx_t {
		TQI_CONST=0,		// 0
		TQI_VOLATILE,
		TQI_NEAR,
		TQI_FAR,
		TQI_HUGE,
		TQI_RESTRICT,		// 5

		TQI__MAX
	};

	const char *type_qualifier_idx_t_str[TSI__MAX] = {
		"const",		// 0
		"volatile",
		"near",
		"far",
		"huge",
		"restrict"		// 5
	};

	typedef unsigned int type_qualifier_t;

#define X(c) static constexpr type_qualifier_t TQ_##c = 1u << TQI_##c
	X(CONST);
	X(VOLATILE);
	X(NEAR);			// 5
	X(FAR);
	X(HUGE);
	X(RESTRICT);
#undef X

	///////////////////////////////////////

	typedef uint64_t data_size_t;
	static constexpr data_size_t data_size_none = ~data_size_t(0u);
	static_assert( ~data_size_none == data_size_t(0u), "oops" );

	typedef uint64_t data_offset_t;
	static constexpr data_offset_t data_offset_none = ~data_offset_t(0u);
	static_assert( ~data_offset_none == data_offset_t(0u), "oops" );

	typedef uint64_t addrmask_t;
	static constexpr addrmask_t addrmask_none(0u);

	static constexpr addrmask_t addrmask_make(const addrmask_t sz/*must be power of 2*/) {
		return ~(sz - addrmask_t(1u));
	}

	///////////////////////////////////////

	struct data_type_t {
		data_size_t			size;
		addrmask_t			align;
	};

	struct data_var_type_t {
		data_type_t			t;
		type_specifier_t		ts;
	};

	struct data_ptr_type_t {
		data_type_t			t;
		type_qualifier_t		tq;
	};

	struct data_type_set_ptr_t {
		data_ptr_type_t			dt_ptr;
		data_ptr_type_t			dt_ptr_near;
		data_ptr_type_t			dt_ptr_far;
		data_ptr_type_t			dt_ptr_huge;
		data_var_type_t			dt_size_t;
		data_var_type_t			dt_intptr_t;
	};

	struct data_type_set_t {
		/* base integer/float types */
		data_var_type_t			dt_bool;
		data_var_type_t			dt_char;
		data_var_type_t			dt_short;
		data_var_type_t			dt_int;
		data_var_type_t			dt_long;
		data_var_type_t			dt_longlong;
		data_var_type_t			dt_float;
		data_var_type_t			dt_double;
		data_var_type_t			dt_longdouble;
	};

	/* default */
	const data_type_set_t data_types_default = {
		{ { /*size*/sizeof(char),      /*align*/addrmask_make(alignof(char))      }, TS_CHAR                 }, /* bool */
		{ { /*size*/sizeof(char),      /*align*/addrmask_make(alignof(char))      }, TS_CHAR                 }, /* char */
		{ { /*size*/sizeof(short),     /*align*/addrmask_make(alignof(short))     }, TS_SHORT                }, /* short */
		{ { /*size*/sizeof(int),       /*align*/addrmask_make(alignof(int))       }, TS_INT                  }, /* int */
		{ { /*size*/sizeof(long),      /*align*/addrmask_make(alignof(long))      }, TS_LONG                 }, /* long */
		{ { /*size*/sizeof(long long), /*align*/addrmask_make(alignof(long long)) }, TS_LONGLONG             }, /* longlong */
		{ { /*size*/sizeof(float),     /*align*/addrmask_make(alignof(float))     }, TS_FLOAT                }, /* float */
		{ { /*size*/sizeof(double),    /*align*/addrmask_make(alignof(double))    }, TS_DOUBLE               }, /* double */
		{ { /*size*/sizeof(double),    /*align*/addrmask_make(alignof(double))    }, TS_DOUBLE               }  /* longdouble */
	};

	const data_type_set_ptr_t data_ptr_types_default = {
		{ { /*size*/sizeof(void*),     /*align*/addrmask_make(alignof(void*))     }, TQ_NEAR                 }, /* ptr */
		{ { /*size*/sizeof(void*),     /*align*/addrmask_make(alignof(void*))     }, TQ_NEAR                 }, /* near ptr */
		{ { /*size*/sizeof(void*),     /*align*/addrmask_make(alignof(void*))     }, TQ_NEAR                 }, /* far ptr */
		{ { /*size*/sizeof(void*),     /*align*/addrmask_make(alignof(void*))     }, TQ_NEAR                 }, /* huge ptr */
		{ { /*size*/sizeof(uintptr_t), /*align*/addrmask_make(alignof(uintptr_t)) }, TS_LONG                 }, /* size_t/ssize_t */
		{ { /*size*/sizeof(uintptr_t), /*align*/addrmask_make(alignof(uintptr_t)) }, TS_LONG                 }  /* intptr_t/uintptr_t */
	};

	/* 16-bit segmented x86 (MS-DOS, Windows, OS/2, etc) */
	const data_type_set_t data_types_intel16 = {
		{ { /*size*/1u,                /*align*/addrmask_make(1u)                 }, TS_CHAR                 }, /* bool */
		{ { /*size*/1u,                /*align*/addrmask_make(1u)                 }, TS_CHAR                 }, /* char */
		{ { /*size*/2u,                /*align*/addrmask_make(2u)                 }, TS_SHORT                }, /* short */
		{ { /*size*/2u,                /*align*/addrmask_make(2u)                 }, TS_INT                  }, /* int */
		{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TS_LONG                 }, /* long */
		{ { /*size*/8u,                /*align*/addrmask_make(8u)                 }, TS_LONGLONG             }, /* longlong */
		{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TS_FLOAT                }, /* float */
		{ { /*size*/8u,                /*align*/addrmask_make(8u)                 }, TS_DOUBLE               }, /* double */
		{ { /*size*/10u,               /*align*/addrmask_make(2u)                 }, TS_LONG|TS_DOUBLE       }  /* longdouble */
	};

	const data_type_set_ptr_t data_ptr_types_intel16_small = {
		{ { /*size*/2u,                /*align*/addrmask_make(2u)                 }, TQ_NEAR                 }, /* ptr */
		{ { /*size*/2u,                /*align*/addrmask_make(2u)                 }, TQ_NEAR                 }, /* near ptr */
		{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TQ_FAR                  }, /* far ptr */
		{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TQ_HUGE                 }, /* huge ptr */
		{ { /*size*/2u,                /*align*/addrmask_make(2u)                 }, TS_INT                  }, /* size_t/ssize_t */
		{ { /*size*/2u,                /*align*/addrmask_make(2u)                 }, TS_INT                  }  /* intptr_t/uintptr_t */
	};

	const data_type_set_ptr_t data_ptr_types_intel16_big = {
		{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TQ_FAR                  }, /* ptr */
		{ { /*size*/2u,                /*align*/addrmask_make(2u)                 }, TQ_NEAR                 }, /* near ptr */
		{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TQ_FAR                  }, /* far ptr */
		{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TQ_HUGE                 }, /* huge ptr */
		{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TS_LONG                 }, /* size_t/ssize_t */
		{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TS_LONG                 }  /* intptr_t/uintptr_t */
	};

	const data_type_set_ptr_t data_ptr_types_intel16_huge = {
		{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TQ_HUGE                 }, /* ptr */
		{ { /*size*/2u,                /*align*/addrmask_make(2u)                 }, TQ_NEAR                 }, /* near ptr */
		{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TQ_FAR                  }, /* far ptr */
		{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TQ_HUGE                 }, /* huge ptr */
		{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TS_LONG                 }, /* size_t/ssize_t */
		{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TS_LONG                 }  /* intptr_t/uintptr_t */
	};

	/* Open Watcom definitions:
	 *   small = small code + small data
	 *   medium = big code + small data
	 *   compact = small code + big data
	 *   large = big code + big data
	 *   huge = big code + hude data
	 *
	 * "Huge" pointers in Open Watcom are like 32-bit pointers in a flat memory space,
	 * which are converted on the fly to 16:16 segmented real mode addresses. The code
	 * runs slower, but this also allows converting existing code not written for the
	 * 16-bit segmented model to run anyway. */

	/* 32-bit segmented or flat x86 (MS-DOS, Windows, OS/2, etc) */
	const data_type_set_t data_types_intel32 = {
		{ { /*size*/1u,                /*align*/addrmask_make(1u)                 }, TS_CHAR                 }, /* bool */
		{ { /*size*/1u,                /*align*/addrmask_make(1u)                 }, TS_CHAR                 }, /* char */
		{ { /*size*/2u,                /*align*/addrmask_make(2u)                 }, TS_SHORT                }, /* short */
		{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TS_INT                  }, /* int */
		{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TS_LONG                 }, /* long */
		{ { /*size*/8u,                /*align*/addrmask_make(8u)                 }, TS_LONGLONG             }, /* longlong */
		{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TS_FLOAT                }, /* float */
		{ { /*size*/8u,                /*align*/addrmask_make(8u)                 }, TS_DOUBLE               }, /* double */
		{ { /*size*/10u,               /*align*/addrmask_make(4u)                 }, TS_LONG|TS_DOUBLE       }  /* longdouble */
	};

	/* 32-bit segmented (though perfectly usable with flat memory models too) */
	const data_type_set_ptr_t data_ptr_types_intel32_segmented = {
		{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TQ_NEAR                 }, /* ptr */
		{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TQ_NEAR                 }, /* near ptr */
		{ { /*size*/6u,                /*align*/addrmask_make(8u)                 }, TQ_FAR                  }, /* far ptr */
		{ { /*size*/6u,                /*align*/addrmask_make(8u)                 }, TQ_HUGE                 }, /* huge ptr */
		{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TS_LONG                 }, /* size_t/ssize_t */
		{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TS_LONG                 }  /* intptr_t/uintptr_t */
	};

	/* 32-bit flat memory models such as Linux i386 where you do not need far pointers, EVER */
	const data_type_set_ptr_t data_ptr_types_intel32_flat = {
		{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TQ_NEAR                 }, /* ptr */
		{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TQ_NEAR                 }, /* near ptr */
		{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TQ_NEAR                 }, /* far ptr */
		{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TQ_NEAR                 }, /* huge ptr */
		{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TS_LONG                 }, /* size_t/ssize_t */
		{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TS_LONG                 }  /* intptr_t/uintptr_t */
	};

	/* 64-bit flat x86_64 (Linux, etc) */
	const data_type_set_t data_types_intel64 = {
		{ { /*size*/1u,                /*align*/addrmask_make(1u)                 }, TS_CHAR                 }, /* bool */
		{ { /*size*/1u,                /*align*/addrmask_make(1u)                 }, TS_CHAR                 }, /* char */
		{ { /*size*/2u,                /*align*/addrmask_make(2u)                 }, TS_SHORT                }, /* short */
		{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TS_INT                  }, /* int */
		{ { /*size*/8u,                /*align*/addrmask_make(8u)                 }, TS_LONG                 }, /* long */
		{ { /*size*/8u,                /*align*/addrmask_make(8u)                 }, TS_LONGLONG             }, /* longlong */
		{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TS_FLOAT                }, /* float */
		{ { /*size*/8u,                /*align*/addrmask_make(8u)                 }, TS_DOUBLE               }, /* double */
		{ { /*size*/10u,               /*align*/addrmask_make(8u)                 }, TS_LONG|TS_DOUBLE       }  /* longdouble */
	};

	/* 64-bit flat x86_64 (Windows) */
	const data_type_set_t data_types_intel64_windows = {
		{ { /*size*/1u,                /*align*/addrmask_make(1u)                 }, TS_CHAR                 }, /* bool */
		{ { /*size*/1u,                /*align*/addrmask_make(1u)                 }, TS_CHAR                 }, /* char */
		{ { /*size*/2u,                /*align*/addrmask_make(2u)                 }, TS_SHORT                }, /* short */
		{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TS_INT                  }, /* int */
		{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TS_LONG                 }, /* long */
		{ { /*size*/8u,                /*align*/addrmask_make(8u)                 }, TS_LONGLONG             }, /* longlong */
		{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TS_FLOAT                }, /* float */
		{ { /*size*/8u,                /*align*/addrmask_make(8u)                 }, TS_DOUBLE               }, /* double */
		{ { /*size*/10u,               /*align*/addrmask_make(8u)                 }, TS_LONG|TS_DOUBLE       }  /* longdouble */
	};

	/* 64-bit flat memory model */
	const data_type_set_ptr_t data_ptr_types_intel64_flat = {
		{ { /*size*/8u,                /*align*/addrmask_make(8u)                 }, TQ_NEAR                 }, /* ptr */
		{ { /*size*/8u,                /*align*/addrmask_make(8u)                 }, TQ_NEAR                 }, /* near ptr */
		{ { /*size*/8u,                /*align*/addrmask_make(8u)                 }, TQ_NEAR                 }, /* far ptr */
		{ { /*size*/8u,                /*align*/addrmask_make(8u)                 }, TQ_NEAR                 }, /* huge ptr */
		{ { /*size*/8u,                /*align*/addrmask_make(8u)                 }, TS_LONGLONG             }, /* size_t/ssize_t */
		{ { /*size*/8u,                /*align*/addrmask_make(8u)                 }, TS_LONGLONG             }  /* intptr_t/uintptr_t */
	};

	/* data types */
	data_type_set_t				data_types = data_types_default;
	data_type_set_ptr_t			data_types_ptr_code = data_ptr_types_default;
	data_type_set_ptr_t			data_types_ptr_data = data_ptr_types_default;
	data_type_set_ptr_t			data_types_ptr_stack = data_ptr_types_default;

	///////////////////////////////////////

	typedef unsigned int ast_node_id_t;
	static constexpr ast_node_id_t ast_node_none = ~ast_node_id_t(0u);

	struct ast_node_t {
		token_t			t;
		ast_node_id_t		next = ast_node_none;
		ast_node_id_t		child = ast_node_none;
		unsigned int		ref = 0;

		ast_node_t&		set_child(const ast_node_id_t n);
		ast_node_t&		set_next(const ast_node_id_t n);
		ast_node_t&		addref(void);
		ast_node_t&		release(void);
		ast_node_t&		clear_and_move_assign(token_t &tt);
		ast_node_t&		clear(const token_type_t t);

		ast_node_t() { }
		ast_node_t(const ast_node_t &) = delete;
		ast_node_t &operator=(const ast_node_t &) = delete;
		ast_node_t(ast_node_t &&x) { common_move(x); }
		ast_node_t &operator=(ast_node_t &&x) { common_move(x); return *this; }

		static void arraycopy(std::vector<ast_node_id_t> &d,const std::vector<ast_node_id_t> &s);
		static void arrayrelease(std::vector<ast_node_id_t> &d);

		void common_move(ast_node_t &o) {
			t = std::move(o.t);
			ref = o.ref; o.ref = 0;
			next = o.next; o.next = ast_node_none;
			child = o.child; o.child = ast_node_none;
		}

		std::string to_str(void) const {
			return std::string(); // TODO
		}
	};

	using ast_node_pool_t = obj_pool<ast_node_t,ast_node_id_t,ast_node_none>;
	class ast_node_pool : public ast_node_pool_t {
		public:
			using pool_t = ast_node_pool_t;
		public:
			ast_node_pool() : pool_t() { }
			~ast_node_pool() { }
		public:
			ast_node_id_t alloc(token_type_t t=token_type_t::none) {
				ast_node_t &a = __internal_alloc();
				a.clear(t).addref();
				return next++;
			}

			ast_node_id_t alloc(token_t &t) {
				ast_node_t &a = __internal_alloc();
				a.clear_and_move_assign(t).addref();
				return next++;
			}
	};

	static ast_node_pool ast_node;

	void ast_node_t::arraycopy(std::vector<ast_node_id_t> &d,const std::vector<ast_node_id_t> &s) {
		for (auto &id : d)
			if (id != ast_node_none)
				ast_node(id).release();

		d = s;

		for (auto &id : d)
			if (id != ast_node_none)
				ast_node(id).addref();
	}

	void ast_node_t::arrayrelease(std::vector<ast_node_id_t> &d) {
		for (auto &id : d)
			if (id != ast_node_none)
				ast_node(id).release();
	}

	ast_node_t &ast_node_t::set_child(const ast_node_id_t n) {
		if (child != n) ast_node.assign(/*to*/child,/*from*/n);
		return *this;
	}

	ast_node_t &ast_node_t::set_next(const ast_node_id_t n) {
		if (next != n) ast_node.assign(/*to*/next,/*from*/n);
		return *this;
	}

	ast_node_t &ast_node_t::clear_and_move_assign(token_t &tt) {
		ref = 0;
		t = std::move(tt);
		set_next(ast_node_none);
		set_child(ast_node_none);
		ast_node.update_next(this);
		return *this;
	}

	ast_node_t &ast_node_t::clear(const token_type_t tt) {
		ref = 0;
		t = token_t(tt);
		set_next(ast_node_none);
		set_child(ast_node_none);
		ast_node.update_next(this);
		return *this;
	}

	ast_node_t &ast_node_t::addref(void) {
		ref++;
		return *this;
	}

	ast_node_t &ast_node_t::release(void) {
		if (ref == 0) throw std::runtime_error("ast_node attempt to release when ref == 0");
		if (--ref == 0) clear(token_type_t::none);
		return *this;
	}

	//////////////////////////////////////////////////

	struct declaration_specifiers_t {
		storage_class_t				storage_class = 0;
		type_specifier_t			type_specifier = 0;
		type_qualifier_t			type_qualifier = 0;
		symbol_id_t				type_identifier_symbol = symbol_none;
		data_size_t				size = data_size_none;
		addrmask_t				align = addrmask_none;
		std::vector<symbol_id_t>		enum_list;
		unsigned int				count = 0;

		bool empty(void) const {
			return 	storage_class == 0 && type_specifier == 0 && type_qualifier == 0 &&
				type_identifier_symbol == symbol_none && enum_list.empty() && count == 0 && align == addrmask_none && size == data_size_none;
		}

		declaration_specifiers_t() { }
		declaration_specifiers_t(const declaration_specifiers_t &x) { common_copy(x); }
		declaration_specifiers_t &operator=(const declaration_specifiers_t &x) { common_copy(x); return *this; }
		declaration_specifiers_t(declaration_specifiers_t &&x) { common_move(x); }
		declaration_specifiers_t &operator=(declaration_specifiers_t &&x) { common_move(x); return *this; }

		~declaration_specifiers_t() {
		}

		void common_move(declaration_specifiers_t &o) {
			storage_class = o.storage_class; o.storage_class = 0;
			type_specifier = o.type_specifier; o.type_specifier = 0;
			type_qualifier = o.type_qualifier; o.type_qualifier = 0;
			type_identifier_symbol = o.type_identifier_symbol;
			size = o.size; o.size = data_size_none;
			align = o.align; o.align = addrmask_none;
			enum_list = std::move(o.enum_list);
			count = o.count; o.count = 0;
		}

		void common_copy(const declaration_specifiers_t &o) {
			storage_class = o.storage_class;
			type_specifier = o.type_specifier;
			type_qualifier = o.type_qualifier;
			type_identifier_symbol = o.type_identifier_symbol;
			size = o.size;
			align = o.align;
			enum_list = o.enum_list;
			count = o.count;
		}
	};

	struct pointer_t {
		type_qualifier_t			tq = 0;

		bool operator==(const pointer_t &o) const {
			return tq == o.tq;
		}

		bool operator!=(const pointer_t &o) const {
			return !(*this == o);
		}
	};

	struct parameter_t;

	struct ddip_t {
		std::vector<pointer_t>		ptr;
		std::vector<ast_node_id_t>	arraydef;
		std::vector<parameter_t>	parameters;
		unsigned int			dd_flags = 0;

		ddip_t() { }
		ddip_t(const ddip_t &x) { common_copy(x); }
		ddip_t &operator=(const ddip_t &x) { common_copy(x); return *this; }
		ddip_t(ddip_t &&x) { common_move(x); }
		ddip_t &operator=(ddip_t &&x) { common_move(x); return *this; }

		~ddip_t() {
			ast_node_t::arrayrelease(arraydef);
		}

		void common_copy(const ddip_t &o) {
			ptr = o.ptr;
			ast_node_t::arraycopy(arraydef,o.arraydef);
			parameters = o.parameters;
			dd_flags = o.dd_flags;
		}

		void common_move(ddip_t &o) {
			ptr = std::move(o.ptr);
			arraydef = std::move(o.arraydef);
			parameters = std::move(o.parameters);
			dd_flags = o.dd_flags; o.dd_flags = 0;
		}

		bool empty(void) const {
			return ptr.empty() && arraydef.empty() && parameters.empty() && dd_flags == 0;
		}
	};

	class ddip_list_t : public std::vector<ddip_t> {
	public:
		using BT = std::vector<ddip_t>;
	public:
		ddip_list_t() : BT() { }
		~ddip_list_t() { }
	public:
		void addcombine(ddip_t &&x);
		void addcombine(const ddip_t &x);
		ddip_t *funcparampair(void);
	};

	struct declarator_t {
		static constexpr unsigned int FL_FUNCTION = 1u << 0u; /* it saw () */
		static constexpr unsigned int FL_FUNCTION_POINTER = 1u << 1u; /* it saw () but it's a function pointer, not a function, hence actually a variable */
		static constexpr unsigned int FL_ELLIPSIS = 1u << 2u; /* we saw ellipsis ... in the parameter list */

		ddip_list_t ddip;
		identifier_id_t name = identifier_none;
		symbol_id_t symbol = symbol_none;
		unsigned int flags = 0;

		ast_node_id_t expr = ast_node_none; /* function body if FL_FUNCTION, else initval */
		ast_node_id_t bitfield_expr = ast_node_none;

		declarator_t() { }
		declarator_t(const declarator_t &x) { common_copy(x); }
		declarator_t &operator=(const declarator_t &x) { common_copy(x); return *this; }
		declarator_t(declarator_t &&x) { common_move(x); }
		declarator_t &operator=(declarator_t &&x) { common_move(x); return *this; }

		void common_copy(const declarator_t &o) {
			ddip = o.ddip;
			identifier.assign(/*to*/name,/*from*/o.name);
			symbol = o.symbol;
			flags = o.flags;
			ast_node.assign(/*to*/expr,/*from*/o.expr);
			ast_node.assign(/*to*/bitfield_expr,/*from*/o.bitfield_expr);
		}

		void common_move(declarator_t &o) {
			ddip = std::move(o.ddip);
			identifier.assignmove(/*to*/name,/*from*/o.name);
			symbol = o.symbol; o.symbol = symbol_none;
			flags = o.flags; o.flags = 0;
			ast_node.assignmove(/*to*/expr,/*from*/o.expr);
			ast_node.assignmove(/*to*/bitfield_expr,/*from*/o.bitfield_expr);
		}

		~declarator_t() {
			identifier.release(name);
			ast_node.release(expr);
			ast_node.release(bitfield_expr);
		}
	};

	ddip_t *ddip_list_t::funcparampair(void) {
		if (!empty()) {
			size_t si = size() - 1u;
			do {
				ddip_t &p = (*this)[si];
				if (p.dd_flags & declarator_t::FL_FUNCTION)
					return &p;
			} while ((si--) != 0u);
		}

		return NULL;
	}

	bool ptrmergeable(const ddip_t &to,const ddip_t &from) {
		if (!to.parameters.empty() || !from.parameters.empty())
			return false;
		if (!to.arraydef.empty() || !from.arraydef.empty())
			return false;
		if (to.dd_flags != 0 || from.dd_flags != 0)
			return false;

		if (!to.ptr.empty() || !from.ptr.empty())
			return true;

		return false;
	}

	bool arraymergeable(const ddip_t &to,const ddip_t &from) {
		if (!to.parameters.empty() || !from.parameters.empty())
			return false;
		if (!to.ptr.empty() || !from.ptr.empty())
			return false;
		if (to.dd_flags != 0 || from.dd_flags != 0)
			return false;

		if (!to.arraydef.empty() || !from.arraydef.empty())
			return true;

		return false;
	}

	void ddip_list_t::addcombine(ddip_t &&x) {
		if (x.empty())
			return;

		if (!empty()) {
			ddip_t &top = back();
			if (ptrmergeable(top,x)) {
				for (auto &p : x.ptr)
					top.ptr.push_back(std::move(p));
				return;
			}
			if (arraymergeable(top,x)) {
				for (auto &a : x.arraydef) {
					top.arraydef.push_back(a);
					a = ast_node_none;
				}
				return;
			}
		}

		push_back(std::move(x));
	}

	void ddip_list_t::addcombine(const ddip_t &x) {
		if (x.empty())
			return;

		if (!empty()) {
			ddip_t &top = back();
			if (ptrmergeable(top,x)) {
				for (const auto &p : x.ptr)
					top.ptr.push_back(p);
				return;
			}
			if (arraymergeable(top,x)) {
				for (const auto &a : x.arraydef) {
					top.arraydef.push_back(a);
					if (a != ast_node_none) ast_node(a).addref();
				}
				return;
			}
		}

		push_back(x);
	}

	struct declaration_t {
		declaration_specifiers_t	spec;
		std::vector<declarator_t>	declor;

		declarator_t &new_declarator(void) {
			const size_t i = declor.size();
			declor.resize(i+1);
			return declor[i];
		}

		declaration_t() { }
		declaration_t(const declaration_t &) = delete;
		declaration_t &operator=(const declaration_t &) = delete;
		declaration_t(declaration_t &&x) { common_move(x); }
		declaration_t &operator=(declaration_t &&x) { common_move(x); return *this; }

		void common_move(declaration_t &o) {
			spec = std::move(o.spec);
			declor = std::move(o.declor);
		}

		~declaration_t() {
		}
	};

	struct parameter_t {
		declaration_specifiers_t		spec;
		declarator_t				decl;

		parameter_t() { }
		parameter_t(const parameter_t &x) { common_copy(x); }
		parameter_t &operator=(const parameter_t &x) { common_copy(x); return *this; }
		parameter_t(parameter_t &&x) { common_move(x); }
		parameter_t &operator=(parameter_t &&x) { common_move(x); return *this; }
		~parameter_t() { }

		void common_copy(const parameter_t &o) {
			spec = o.spec;
			decl = o.decl;
		}

		void common_move(parameter_t &o) {
			spec = std::move(o.spec);
			decl = std::move(o.decl);
		}
	};

	struct cc_state_t {
		cc_state_t() {
			assert(scopes.empty());
			assert(scope_global == scope_id_t(0));
			scopes.resize(scope_global+1); // make sure global scope exists

			assert(scope_stack.empty());
			scope_stack.push_back(scope_global);
		}

		struct enumerator_t {
			identifier_id_t				name = identifier_none;
			ast_node_id_t				expr = ast_node_none;
			position_t				pos;

			enumerator_t() { }
			enumerator_t(const enumerator_t &x) { common_copy(x); }
			enumerator_t &operator=(const enumerator_t &x) { common_copy(x); return *this; }
			enumerator_t(enumerator_t &&x) { common_move(x); }
			enumerator_t &operator=(enumerator_t &&x) { common_move(x); return *this; }

			void common_copy(const enumerator_t &o) {
				identifier.assign(/*to*/name,/*from*/o.name);
				ast_node.assign(/*to*/expr,/*from*/o.expr);
				pos = pos;
			}

			void common_move(enumerator_t &o) {
				identifier.assignmove(/*to*/name,/*from*/o.name);
				ast_node.assignmove(/*to*/expr,/*from*/o.expr);
				pos = std::move(pos);
			}

			~enumerator_t() {
				identifier.release(name);
				ast_node.release(expr);
			}
		};

		struct structfield_t {
			declaration_specifiers_t		spec;
			ddip_list_t				ddip;
			identifier_id_t				name = identifier_none;
			ast_node_id_t				bitfield_expr = ast_node_none;

			structfield_t() { }
			structfield_t(const structfield_t &) = delete;
			structfield_t &operator=(const structfield_t &) = delete;
			structfield_t(structfield_t &&x) { common_move(x); }
			structfield_t &operator=(structfield_t &&x) { common_move(x); return *this; }

			~structfield_t() {
				identifier.release(name);
				ast_node.release(bitfield_expr);
			}

			void common_move(structfield_t &x) {
				spec = std::move(x.spec);
				ddip = std::move(x.ddip);
				identifier.assignmove(/*to*/name,/*from*/x.name);
				ast_node.assignmove(/*to*/bitfield_expr,/*from*/x.bitfield_expr);
			}
		};

		struct symbol_t {
			enum type_t {
				NONE=0,
				VARIABLE,
				FUNCTION,
				TYPEDEF,
				STRUCT,
				UNION,
				CONST,
				ENUM
			};

			static constexpr unsigned int FL_DEFINED = 1u << 0u; /* function body, variable without extern */
			static constexpr unsigned int FL_DECLARED = 1u << 1u; /* function without body, variable with extern */
			static constexpr unsigned int FL_PARAMETER = 1u << 2u; /* within scope of function, parameter value */
			static constexpr unsigned int FL_STACK = 1u << 3u; /* exists on the stack */
			static constexpr unsigned int FL_ELLIPSIS = 1u << 4u; /* function has ellipsis param */
			static constexpr unsigned int FL_FUNCTION_POINTER = 1u << 5u; /* it's a variable you can can call like a function, a function pointer */

			declaration_specifiers_t		spec;
			ddip_list_t				ddip;
			std::vector<structfield_t>		fields;
			identifier_id_t				name = identifier_none;
			ast_node_id_t				expr = ast_node_none; /* variable init, function body, etc */
			scope_id_t				scope = scope_none;
			scope_id_t				parent_of_scope = scope_none;
			enum type_t				sym_type = NONE;
			unsigned int				flags = 0;

			symbol_t() { }
			symbol_t(const symbol_t &) = delete;
			symbol_t &operator=(const symbol_t &) = delete;
			symbol_t(symbol_t &&x) { common_move(x); }
			symbol_t &operator=(symbol_t &&x) { common_move(x); return *this; }

			~symbol_t() {
				ast_node.release(expr);
				identifier.release(name);
			}

			void common_move(symbol_t &x) {
				spec = std::move(x.spec);
				ddip = std::move(x.ddip);
				fields = std::move(x.fields);
				identifier.assignmove(/*to*/name,/*from*/x.name);
				ast_node.assignmove(/*to*/expr,/*from*/x.expr);
				scope = x.scope; x.scope = scope_none;
				parent_of_scope = x.parent_of_scope; x.parent_of_scope = scope_none;
				sym_type = x.sym_type; x.sym_type = NONE;
				flags = x.flags; x.flags = 0;
			}

			bool identifier_exists(const identifier_id_t &id) {
				if (id != identifier_none) {
					for (const auto &f : fields) {
						if (f.name != identifier_none) {
							if (identifier(f.name) == identifier(id))
								return true;
						}
					}
				}

				return false;
			}
		};

		struct scope_t {
			struct decl_t {
				declaration_specifiers_t	spec;
				declarator_t			declor;
			};

			ast_node_id_t				root = ast_node_none;
			std::vector<decl_t>			localdecl;
			std::vector<symbol_id_t>		symbols;

			scope_t() { }
			scope_t(const scope_t &) = delete;
			scope_t &operator=(const scope_t &) = delete;
			scope_t(scope_t &&x) { common_move(x); }
			scope_t &operator=(scope_t &&x) { common_move(x); return *this; }

			~scope_t() {
				ast_node.release(root);
			}

			decl_t &new_localdecl(void) {
				const size_t r = localdecl.size();
				localdecl.resize(r+1u);
				return localdecl[r];
			}

			void common_move(scope_t &x) {
				ast_node.assignmove(/*to*/root,/*from*/x.root);
				localdecl = std::move(x.localdecl);
				symbols = std::move(x.symbols);
			}
		};

		void debug_dump_ast(const std::string prefix,ast_node_id_t r);
		void debug_dump_declaration_specifiers(const std::string prefix,declaration_specifiers_t &ds);
		void debug_dump_declarator(const std::string prefix,declarator_t &declr,const std::string &name=std::string());
		void debug_dump_declaration(const std::string prefix,declaration_t &decl,const std::string &name=std::string());
		void debug_dump_pointer(const std::string prefix,std::vector<pointer_t> &ptr,const std::string &name=std::string());
		void debug_dump_direct_declarator(const std::string prefix,declarator_t &ddecl,const std::string &name=std::string());
		void debug_dump_arraydef(const std::string prefix,std::vector<ast_node_id_t> &arraydef,const std::string &name=std::string());
		void debug_dump_parameter(const std::string prefix,parameter_t &p,const std::string &name=std::string());
		void debug_dump_structfield(const std::string prefix,structfield_t &f,const std::string &name=std::string());
		void debug_dump_ddip(const std::string prefix,ddip_t &ddip,const std::string &name=std::string());
		void debug_dump_ddip(const std::string prefix,ddip_list_t &ddip,const std::string &name=std::string());
		void debug_dump_symbol(const std::string prefix,symbol_t &sym,const std::string &name=std::string());
		void debug_dump_symbol_table(const std::string prefix,const std::string &name=std::string());
		void debug_dump_scope(const std::string prefix,scope_t &sco,const std::string &name=std::string());
		void debug_dump_scope_table(const std::string prefix,const std::string &name=std::string());
		void debug_dump_enumerator(const std::string prefix,enumerator_t &en);

		std::string ddip_list_to_str(const ddip_list_t &dl) {
			std::string r;
			size_t i=0;

			if (!dl.empty()) {
				while (i < dl.size()) {
					if (i != 0)
						r += "(";

					const auto &e = dl[i++];
					for (const auto &p : e.ptr) {
						r += "*";
						for (unsigned int x=0;x < TQI__MAX;x++) { if (p.tq&(1u<<x)) r += std::string(" ") + type_qualifier_idx_t_str[x]; }
						if (p.tq) r += " ";
					}
				}

				assert(i != 0u);

				do {
					const auto &e = dl[--i];

					if (e.dd_flags & declarator_t::FL_FUNCTION) {
						if (!r.empty()) r += " ";
						r += "(";
						for (size_t pi=0;pi < e.parameters.size();pi++) {
							auto &p = e.parameters[pi];
							std::string sr;

							for (unsigned int i=0;i < TSI__MAX;i++) {
								if (p.spec.type_specifier&(1u<<i)) {
									if (!sr.empty()) sr += " ";
									sr += type_specifier_idx_t_str[i];
								}
							}
							for (unsigned int i=0;i < TQI__MAX;i++) {
								if (p.spec.type_qualifier&(1u<<i)) {
									if (!sr.empty()) sr += " ";
									sr += type_qualifier_idx_t_str[i];
								}
							}
							if (p.spec.type_identifier_symbol != symbol_none) {
								symbol_t &sym = symbol(p.spec.type_identifier_symbol);
								if (!sr.empty()) sr += " ";
								if (sym.name != identifier_none)
									sr += identifier(sym.name).to_str();
								else
									sr += "<anon>";
							}

							sr += ddip_list_to_str(p.decl.ddip);

							if (pi != 0) r += ",";
							r += sr;
						}
						r += ")";
					}

					for (const auto &a : e.arraydef) {
						r += "[";
						if (a != ast_node_none) {
							ast_node_t &an = ast_node(a);
							if (an.t.type == token_type_t::integer)
								r += std::to_string(an.t.v.integer.v.v);
							else
								r += "<expr>";
						}
						r += "]";
					}

					if (i != 0)
						r += ")";
				} while (i != 0u);
			}

			return r;
		}

		CCMiniC::lgtok_state_t	lst;
		CCMiniC::pptok_state_t	pst;
		rbuf*			buf = NULL;
		source_file_object*	sfo = NULL;
		int			err = 0;

		std::vector<token_t>	tq;
		size_t			tq_tail = 0;

		static constexpr unsigned int DECLSPEC_OPTIONAL = 1u << 0u;
		static constexpr unsigned int DECLSPEC_ALLOW_DEF = 1u << 1u; /* allow definitions i.e. struct { ... } union { .... } enum { .... } */

		static constexpr unsigned int DIRDECL_ALLOW_ABSTRACT = 1u << 0u;
		static constexpr unsigned int DIRDECL_NO_IDENTIFIER = 1u << 1u;

		std::vector<symbol_t>	symbols;

		std::vector<scope_t>	scopes;
		std::vector<scope_id_t>	scope_stack;

		bool			ignore_whitespace = true;

		void tq_ft(void) {
			token_t t(token_type_t::eof);
			int r;

			do {
				if (err == 0) {
					if ((r=lctok(pst,lst,*buf,*sfo,t)) < 1)
						err = r;
				}

				if (ignore_whitespace && err == 0) {
					if (t.type == token_type_t::newline)
						continue;
				}

				break;
			} while (1);

			tq.push_back(std::move(t));
		}

		void tq_refill(const size_t i=1) {
			while (tq.size() < (i+tq_tail))
				tq_ft();
		}

		const token_t &tq_peek(const size_t i=0) {
			tq_refill(i+1);
			assert((tq_tail+i) < tq.size());
			return tq[tq_tail+i];
		}

		void tq_discard(const size_t i=1) {
			tq_refill(i);
			tq_tail += i;
			assert(tq_tail <= tq.size());

			if (tq_tail == tq.size()) {
				tq_tail = 0;
				tq.clear();
			}
		}

		token_t &tq_get(void) {
			tq_refill();
			assert(tq_tail < tq.size());
			return tq[tq_tail++];
		}

		int chkerr(void) {
			const token_t &t = tq_peek();
			if (t.type == token_type_t::none || t.type == token_type_t::eof || err < 0)
				return err; /* 0 or negative */

			return 1;
		}

		scope_t &scope(scope_id_t id) {
#if 1//DEBUG
			if (id < scopes.size())
				return scopes[id];

			throw std::out_of_range("scope out of range");
#else
			return scopes[id];
#endif
		}

		scope_id_t new_scope(void) {
			const scope_id_t r = scopes.size();
			scopes.resize(r+1u);
			return r;
		}

		scope_id_t push_new_scope(void) {
			const scope_id_t r = new_scope();
			scope_stack.push_back(r);
			return r;
		}

		void pop_scope(void) {
#if 1//DEBUG
			if (scope_stack.empty()) throw std::runtime_error("Attempt to pop scope stack when empty");
			if (scope_stack.size() == 1) throw std::runtime_error("Attempt to pop global scope");
			assert(scope_stack[0] == scope_global);
#endif
			scope_stack.pop_back();
		}

		scope_id_t current_scope(void) {
#if 1//DEBUG
			assert(!scope_stack.empty());
#endif
			return scope_stack[scope_stack.size()-1u];
		}

		/* symbols are never removed from the vector, and they are always added in increasing ID order */
		symbol_id_t new_symbol(const identifier_id_t name) {
			const symbol_id_t r = symbols.size();
			symbols.resize(r+1u);
			identifier.assign(/*to*/symbols[r].name,/*from*/name);
			return r;
		}

		symbol_t &symbol(symbol_id_t id) {
#if 1//DEBUG
			if (id < symbols.size())
				return symbols[id];

			throw std::out_of_range("symbol out of range");
#else
			return symbols[id];
#endif
		}

		bool sym_match_type(const symbol_t::type_t at,const symbol_t::type_t bt) {
			if (at == bt)
				return true;

			const bool atf = (at == symbol_t::FUNCTION || at == symbol_t::VARIABLE || at == symbol_t::TYPEDEF || at == symbol_t::ENUM || at == symbol_t::CONST);
			const bool btf = (bt == symbol_t::FUNCTION || bt == symbol_t::VARIABLE || bt == symbol_t::TYPEDEF || bt == symbol_t::ENUM || bt == symbol_t::CONST);
			if ((atf || btf) && atf == btf)
				return true;

			return false;
		}

		symbol_id_t lookup_symbol(scope_t &sco,const identifier_id_t name,const symbol_t::type_t st) {
			if (name == identifier_none)
				return symbol_none;

#if 0//DEBUG
			fprintf(stderr,"lookup '%s' scope#%lu:\n",identifier(name).to_str().c_str(),(unsigned long)(&sco-&scopes[0]));
#endif

			for (const auto &sid : sco.symbols) {
				symbol_t &chk_s = symbol(sid);

				if (chk_s.name != identifier_none) {
					if (sym_match_type(chk_s.sym_type,st) && identifier(name) == identifier(chk_s.name))
						return sid;
				}
			}

#if 0//DEBUG
			fprintf(stderr,"lookup '%s' scope#%lu not found\n",identifier(name).to_str().c_str(),(unsigned long)(&sco-&scopes[0]));
#endif

			return symbol_none;
		}

		/* this automatically uses the scope_stack and current_scope() return value */
		symbol_id_t lookup_symbol(const identifier_id_t name,const symbol_t::type_t st) {
			if (name == identifier_none)
				return symbol_none;

#if 0//DEBUG
			fprintf(stderr,"lookup '%s'\n",identifier(name).to_str().c_str());
			fprintf(stderr,"scope:");
			for (auto i=scope_stack.rbegin();i!=scope_stack.rend();i++)
				fprintf(stderr," #%lu",(unsigned long)(*i));
			fprintf(stderr,"\n");
#endif

			symbol_id_t sid;
			for (auto i=scope_stack.rbegin();i!=scope_stack.rend();i++)
				if ((sid=lookup_symbol(scope(*i),name,st)) != symbol_none)
					return sid;

			return symbol_none;
		}

		symbol_t::type_t classify_symbol(const declaration_specifiers_t &spec,const declarator_t &declor) {
			if ((declor.flags & (declarator_t::FL_FUNCTION|declarator_t::FL_FUNCTION_POINTER)) == declarator_t::FL_FUNCTION)/* function, but not function pointer */
				return symbol_t::FUNCTION;
			else if (spec.storage_class & SC_TYPEDEF)
				return symbol_t::TYPEDEF;
			else
				return symbol_t::VARIABLE;
		}

		struct symbol_lookup_t {
			symbol_t::type_t st = symbol_t::NONE;
			scope_id_t cursco = scope_none;
			symbol_id_t sid = symbol_none;
			unsigned int flags = 0;
			position_t pos;
		};

		int prep_symbol_lookup(symbol_lookup_t &sl,const declaration_specifiers_t &spec,const declarator_t &declor) {
			if (sl.st == symbol_t::NONE) sl.st = classify_symbol(spec,declor);
			sl.cursco = current_scope();

			if (spec.storage_class & SC_TYPEDEF)
				sl.flags |= symbol_t::FL_DECLARED | symbol_t::FL_DEFINED;
			else if (sl.st == symbol_t::CONST)
				sl.flags |= symbol_t::FL_DECLARED | symbol_t::FL_DEFINED;
			else {
				sl.flags |= symbol_t::FL_DECLARED;
				switch (sl.st) {
					case symbol_t::VARIABLE:
						if (!(spec.storage_class & SC_EXTERN))
							sl.flags |= symbol_t::FL_DEFINED;

						/* if declared as const/constexpr/constinit and defined and the expression has been reduced to an integer, mark as const */
						if (sl.flags & symbol_t::FL_DEFINED) {
							if ((spec.storage_class & (SC_CONSTEXPR|SC_CONSTINIT|SC_CONSTEVAL)) != 0 || (spec.type_qualifier & TQ_CONST)) {
								if (declor.expr != ast_node_none) {
									ast_node_t &an = ast_node(declor.expr);
									if (an.child == ast_node_none && an.next == ast_node_none) {
										if (an.t.type == token_type_t::integer) {
											sl.st = symbol_t::CONST;
										}
									}
								}
							}
						}
						break;
					case symbol_t::FUNCTION:
						sl.cursco = scope_global; /* All functions are global scope in C. We're not a C++ compiler. */
						if (declor.expr != ast_node_none)
							sl.flags |= symbol_t::FL_DEFINED;
						break;
					case symbol_t::ENUM:
						sl.flags |= symbol_t::FL_DEFINED;
						break;
					default:
						break;
				}
			}

			if (declor.flags & declarator_t::FL_FUNCTION_POINTER)
				sl.flags |= symbol_t::FL_FUNCTION_POINTER;
			if (declor.flags & declarator_t::FL_ELLIPSIS)
				sl.flags |= symbol_t::FL_ELLIPSIS;

			return 1;
		}

		bool do_local_symbol_lookup(symbol_lookup_t &sl,const declaration_specifiers_t &spec,const declarator_t &declor) {
			(void)spec;

			assert(sl.sid == symbol_none);
			assert(sl.st != symbol_t::NONE);

			if ((sl.sid=lookup_symbol(scope(sl.cursco),declor.name,sl.st)) != symbol_none)
				return true;

			return false;
		}

		int check_symbol_param_match(symbol_lookup_t &sl,const ddip_list_t &p1,const ddip_list_t &p2) {
			if (p1.size() != p2.size())
				CCERR_RET(EINVAL,sl.pos,"Parameter type does not match");

			for (size_t pi=0;pi < p1.size();pi++) {
				auto &e1 = p1[pi];
				auto &e2 = p2[pi];

				if (e1.ptr.size() != e2.ptr.size())
					CCERR_RET(EINVAL,sl.pos,"Parameter type does not match");
				if (e1.arraydef.size() != e2.arraydef.size())
					CCERR_RET(EINVAL,sl.pos,"Parameter type does not match");
				if (e1.parameters.size() != e2.parameters.size())
					CCERR_RET(EINVAL,sl.pos,"Parameter type does not match");

				for (size_t ai=0;ai < e1.arraydef.size();ai++) {
					if ((e1.arraydef[ai] == ast_node_none) != (e2.arraydef[ai] == ast_node_none))
						CCERR_RET(EINVAL,sl.pos,"Parameter type does not match");

					const ast_node_t &a1 = ast_node(e1.arraydef[ai]);
					const ast_node_t &a2 = ast_node(e2.arraydef[ai]);

					if (a1.t != a2.t)
						CCERR_RET(EINVAL,sl.pos,"Parameter type does not match");
				}
			}

			return 1;
		}

		int check_symbol_param_match(symbol_lookup_t &sl,const ddip_list_t &p) {
			int r;

			assert(sl.sid != symbol_none);

			symbol_t &chk_s = symbol(sl.sid);

			/* exception: if the function was without any parameters, and you specify parameters, then take the parameters */
			if (chk_s.ddip.empty())
				chk_s.ddip = p;
			else if ((r=check_symbol_param_match(sl,chk_s.ddip,p)) < 1)
				return r;

			return 1;
		}

		int check_symbol_lookup_match(symbol_lookup_t &sl,const declaration_specifiers_t &spec,const declarator_t &declor) {
			assert(sl.sid != symbol_none);
			assert(sl.st != symbol_t::NONE);

			symbol_t &chk_s = symbol(sl.sid);

			(void)declor;

			/* check for re-declaration of the same symbol with the same specification.
			 * perhaps variables once declared extern that are re-declared non-extern (but not static).
			 * function declaration re-declared, or perhaps a declaration turned into a definition with a function body.
			 * that's fine. */
			if (chk_s.sym_type == sl.st) {
				if (sl.st == symbol_t::VARIABLE) {
					const unsigned int fl_chk =
						symbol_t::FL_FUNCTION_POINTER|
						symbol_t::FL_DECLARED|
						symbol_t::FL_ELLIPSIS;

					const storage_class_t sc_chk =
						~(SC_EXTERN);

					if ((chk_s.flags&fl_chk) == (sl.flags&fl_chk) &&
						(chk_s.spec.storage_class&sc_chk) == (spec.storage_class&sc_chk) &&
						chk_s.spec.type_specifier == spec.type_specifier &&
						chk_s.spec.type_qualifier == spec.type_qualifier) {
						/* you can change EXTERN to non-EXTERN, or just declare EXTERN again, that's it */
						if (chk_s.spec.storage_class & SC_EXTERN) {
							if ((spec.storage_class & SC_EXTERN) == 0)
								chk_s.spec = spec;
							if (sl.flags & symbol_t::FL_DEFINED)
								chk_s.flags |= symbol_t::FL_DEFINED;

							return 1;
						}
					}
				}
				else if (sl.st == symbol_t::FUNCTION || sl.st == symbol_t::STRUCT || sl.st == symbol_t::UNION) {
					const unsigned int fl_chk =
						symbol_t::FL_FUNCTION_POINTER|
						symbol_t::FL_DECLARED|
						symbol_t::FL_ELLIPSIS;

					if ((chk_s.flags&fl_chk) == (sl.flags&fl_chk) &&
						chk_s.spec.storage_class == spec.storage_class &&
						chk_s.spec.type_specifier == spec.type_specifier &&
						chk_s.spec.type_qualifier == spec.type_qualifier) {
						if (sl.flags & symbol_t::FL_DEFINED) {
							if (chk_s.flags & symbol_t::FL_DEFINED) /* cannot define it again */
								goto exists;

							chk_s.flags |= symbol_t::FL_DEFINED;
						}

						return 1;
					}
				}
			}

exists:
			CCERR_RET(EEXIST,sl.pos,"Symbol already exists");
		}

		/* this automatically uses the scope_stack and current_scope() return value */
		int add_symbol(symbol_lookup_t &sl,const declaration_specifiers_t &spec,const declarator_t &declor) {
			assert(sl.sid == symbol_none);
			sl.sid = new_symbol(declor.name);
			symbol_t &sym = symbol(sl.sid);
			sym.spec = spec;
			sym.scope = sl.cursco;
			scope(sym.scope).symbols.push_back(sl.sid);
			sym.flags = sl.flags;
			sym.ddip = declor.ddip;
			sym.sym_type = sl.st;
			ast_node.assign(/*to*/sym.expr,/*from*/declor.expr);
			return 1;
		}

		static constexpr size_t ptr_deref_sizeof_addressof = ~size_t(0);
		static_assert( (~ptr_deref_sizeof_addressof) == size_t(0), "oops" );

		int typeid_or_expr_parse(ast_node_id_t &aroot);
		bool ast_constexpr_sizeof(token_t &r,token_t &op,size_t ptr_deref=0);
		bool ast_constexpr_alignof(token_t &r,token_t &op,size_t ptr_deref=0);
		data_size_t calc_sizeof(declaration_specifiers_t &spec,ddip_list_t &ddip,size_t ptr_deref=0);
		data_size_t calc_sizeof(declaration_specifiers_t &spec,declarator_t &decl,size_t ptr_deref=0);
		addrmask_t calc_alignof(declaration_specifiers_t &spec,ddip_list_t &ddip,size_t ptr_deref=0);
		addrmask_t calc_alignof(declaration_specifiers_t &spec,declarator_t &decl,size_t ptr_deref=0);
		int direct_declarator_inner_parse(ddip_list_t &dp,declarator_t &dd,position_t &pos,unsigned int flags=0);
		int direct_declarator_parse(declaration_specifiers_t &ds,declarator_t &dd,unsigned int flags=0);
		int declaration_inner_parse(declaration_specifiers_t &spec,declarator_t &declor);
		int declarator_parse(declaration_specifiers_t &ds,declarator_t &declor);
		int declaration_specifiers_parse(declaration_specifiers_t &ds,const unsigned int declspec = 0);
		int enumerator_list_parse(declaration_specifiers_t &ds,std::vector<symbol_id_t> &enum_list);
		int struct_declarator_parse(const symbol_id_t sid,declaration_specifiers_t &ds,declarator_t &declor);
		void ast_node_reduce(ast_node_id_t &eroot,const std::string &prefix=std::string());
		int struct_bitfield_validate(token_t &t,bool first_of_range=false);
		bool declaration_specifiers_check(const unsigned int token_offset=0);
		int compound_statement(ast_node_id_t &aroot,ast_node_id_t &nroot);
		int struct_declaration_parse(const symbol_id_t sid,const token_type_t &tt);
		int multiplicative_expression(ast_node_id_t &aroot);
		int exclusive_or_expression(ast_node_id_t &aroot);
		int inclusive_or_expression(ast_node_id_t &aroot);
		int conditional_expression(ast_node_id_t &aroot);
		int logical_and_expression(ast_node_id_t &aroot);
		int logical_or_expression(ast_node_id_t &aroot);
		int assignment_expression(ast_node_id_t &aroot);
		int relational_expression(ast_node_id_t &aroot);
		int pointer_parse(std::vector<pointer_t> &ptr);
		int expression_statement(ast_node_id_t &aroot);
		int declaration_parse(declaration_t &declion);
		int additive_expression(ast_node_id_t &aroot);
		int equality_expression(ast_node_id_t &aroot);
		int postfix_expression(ast_node_id_t &aroot);
		int primary_expression(ast_node_id_t &aroot);
		int shift_expression(ast_node_id_t &aroot);
		int unary_expression(ast_node_id_t &aroot);
		int cast_expression(ast_node_id_t &aroot);
		int compound_statement_declarators(void);
		int and_expression(ast_node_id_t &aroot);
		int xor_expression(ast_node_id_t &aroot);
		int or_expression(ast_node_id_t &aroot);
		int initializer(ast_node_id_t &aroot);
		int expression(ast_node_id_t &aroot);
		int statement(ast_node_id_t &aroot);
		int external_declaration(void);
		int translation_unit(void);
	};

	bool is_ast_constexpr(token_t &t) {
		switch (t.type) {
			case token_type_t::integer:
				return true;
			default:
				break;
		};

		return false;
	}

	bool ast_constexpr_to_bool(integer_value_t &iv) {
		return iv.v.u != 0ull;
	}

	bool ast_constexpr_to_bool(token_t &t) {
		switch (t.type) {
			case token_type_t::integer:
				return ast_constexpr_to_bool(t.v.integer);
			default:
				break;
		};

		return false;
	}

	addrmask_t cc_state_t::calc_alignof(declaration_specifiers_t &spec,ddip_list_t &ddip,size_t ptr_deref) {
		addrmask_t data_talign = addrmask_none;
		addrmask_t data_calcalign;

		if (spec.align != addrmask_none)
			data_talign = spec.align;
		else if (spec.type_specifier & TS_SZ8)
			data_talign = addrmask_make(1);
		else if (spec.type_specifier & TS_SZ16)
			data_talign = addrmask_make(2);
		else if (spec.type_specifier & TS_SZ32)
			data_talign = addrmask_make(4);
		else if (spec.type_specifier & TS_SZ64)
			data_talign = addrmask_make(8);
		else if (spec.type_specifier & TS_CHAR)
			data_talign = data_types.dt_char.t.align;
		else if (spec.type_specifier & TS_SHORT)
			data_talign = data_types.dt_short.t.align;
		else if (spec.type_specifier & TS_INT)
			data_talign = data_types.dt_int.t.align;
		else if (spec.type_specifier & TS_LONG)
			data_talign = data_types.dt_long.t.align;
		else if (spec.type_specifier & TS_LONGLONG)
			data_talign = data_types.dt_longlong.t.align;
		else if (spec.type_specifier & TS_FLOAT)
			data_talign = data_types.dt_float.t.align;
		else if (spec.type_specifier & TS_DOUBLE)
			data_talign = data_types.dt_double.t.align;
		else if ((spec.type_specifier & (TS_LONG|TS_DOUBLE)) == (TS_LONG|TS_DOUBLE))
			data_talign = data_types.dt_longdouble.t.align;
		else if (spec.type_specifier & (TS_ENUM|TS_MATCH_TYPEDEF|TS_STRUCT|TS_UNION)) {
			if (spec.type_identifier_symbol == symbol_none)
				return addrmask_none;

			symbol_t &sym = symbol(spec.type_identifier_symbol);
			if ((data_talign=calc_alignof(sym.spec,sym.ddip)) == symbol_none)
				return addrmask_none;
		}

		data_calcalign = data_talign;

		if (ptr_deref == ptr_deref_sizeof_addressof) {
			/* & address of? That's a pointer, unconditionally */
			data_calcalign = data_types_ptr_data.dt_ptr.t.size;
		}
		else {
			if (!ddip.empty()) {
				size_t i = ddip.size() - 1u;
				do {
					auto &ent = ddip[i];

					if ((ent.dd_flags & (declarator_t::FL_FUNCTION|declarator_t::FL_FUNCTION_POINTER)) == declarator_t::FL_FUNCTION)
						return addrmask_none;

					if (!ent.ptr.empty()) {
						data_calcalign = data_types_ptr_data.dt_ptr.t.align;
						if (ptr_deref != 0u) {
							if (ptr_deref > ent.ptr.size()) { /* dereferencing more than what is in this level, process as a pointer, continue on */
								ptr_deref -= ent.ptr.size();
								continue;
							}
							else if (ptr_deref == ent.ptr.size()) { /* dereferencing exactly what is in this level, return a pointer and count, and stop */
								data_calcalign = data_talign;
								ptr_deref = 0;
								i = 0; /* break out of the loop at the end by setting i == 0 */
							}
							else { /* dereferencing less than this level, return a pointer, but not the count */
								ptr_deref = 0;
								break; /* break out of the loop now */
							}
						}
					}
				} while ((i--) != 0u);
			}

			/* if dereferncing too much, return no size */
			if (ptr_deref != 0u)
				return addrmask_none;
		}

#if 0
		fprintf(stderr,"dbg final: calcalign=%zu count=%zu\n",data_calcalign,count);
#endif

		if (data_calcalign != addrmask_none)
			return (~data_calcalign) + addrmask_t(1u);

		return addrmask_none;
	}

	addrmask_t cc_state_t::calc_alignof(declaration_specifiers_t &spec,declarator_t &decl,size_t ptr_deref) {
		if ((decl.flags & (declarator_t::FL_FUNCTION|declarator_t::FL_FUNCTION_POINTER)) == declarator_t::FL_FUNCTION)
			return addrmask_none;

		return calc_alignof(spec,decl.ddip,ptr_deref);
	}

	data_size_t cc_state_t::calc_sizeof(declaration_specifiers_t &spec,ddip_list_t &ddip,size_t ptr_deref) {
		data_size_t data_tsz = data_size_none;
		data_size_t count,data_calcsz;

		if (spec.size != data_size_none)
			data_tsz = spec.size;
		else if (spec.type_specifier & TS_SZ8)
			data_tsz = 1;
		else if (spec.type_specifier & TS_SZ16)
			data_tsz = 2;
		else if (spec.type_specifier & TS_SZ32)
			data_tsz = 4;
		else if (spec.type_specifier & TS_SZ64)
			data_tsz = 8;
		else if (spec.type_specifier & TS_CHAR)
			data_tsz = data_types.dt_char.t.size;
		else if (spec.type_specifier & TS_SHORT)
			data_tsz = data_types.dt_short.t.size;
		else if (spec.type_specifier & TS_INT)
			data_tsz = data_types.dt_int.t.size;
		else if (spec.type_specifier & TS_LONG)
			data_tsz = data_types.dt_long.t.size;
		else if (spec.type_specifier & TS_LONGLONG)
			data_tsz = data_types.dt_longlong.t.size;
		else if (spec.type_specifier & TS_FLOAT)
			data_tsz = data_types.dt_float.t.size;
		else if (spec.type_specifier & TS_DOUBLE)
			data_tsz = data_types.dt_double.t.size;
		else if ((spec.type_specifier & (TS_LONG|TS_DOUBLE)) == (TS_LONG|TS_DOUBLE))
			data_tsz = data_types.dt_longdouble.t.size;
		else if (spec.type_specifier & (TS_ENUM|TS_MATCH_TYPEDEF|TS_STRUCT|TS_UNION)) {
			if (spec.type_identifier_symbol == symbol_none)
				return data_size_none;

			symbol_t &sym = symbol(spec.type_identifier_symbol);
			if ((data_tsz=calc_sizeof(sym.spec,sym.ddip)) == symbol_none)
				return data_size_none;
		}

		count = 1;
		data_calcsz = data_tsz;

		if (ptr_deref == ptr_deref_sizeof_addressof) {
			/* & address of? That's a pointer, unconditionally */
			data_calcsz = data_types_ptr_data.dt_ptr.t.size;
		}
		else {
			if (!ddip.empty()) {
				size_t i = ddip.size() - 1u;
				do {
					auto &ent = ddip[i];

					if ((ent.dd_flags & (declarator_t::FL_FUNCTION|declarator_t::FL_FUNCTION_POINTER)) == declarator_t::FL_FUNCTION)
						return data_size_none;

					if (data_calcsz != data_size_none)
						data_calcsz *= count;

					count = 1;
					if (!ent.ptr.empty()) {
						data_calcsz = data_types_ptr_data.dt_ptr.t.size;
						if (ptr_deref != 0u) {
							if (ptr_deref > ent.ptr.size()) { /* dereferencing more than what is in this level, process as a pointer, continue on */
								ptr_deref -= ent.ptr.size();
								continue;
							}
							else if (ptr_deref == ent.ptr.size()) { /* dereferencing exactly what is in this level, return a pointer and count, and stop */
								data_calcsz = data_tsz;
								ptr_deref = 0;
								i = 0; /* break out of the loop at the end by setting i == 0 */
							}
							else { /* dereferencing less than this level, return a pointer, but not the count */
								ptr_deref = 0;
								break; /* break out of the loop now */
							}
						}
					}

#if 0
					fprintf(stderr,"dbg: calcsz=%zu count=%zu\n",data_calcsz,count);
					debug_dump_ddip("  ",ent);
#endif

					if (!ent.arraydef.empty()) {
						for (const auto &a : ent.arraydef) {
							if (a == ast_node_none)
								return data_size_none;

							const auto &an = ast_node(a);
							if (an.t.type != token_type_t::integer)
								return data_size_none;
							if (an.t.v.integer.v.v < 1ll)
								return data_size_none;
							if (an.t.v.integer.v.u >= (0x8000000000000000ull / count))
								return data_size_none;

							count *= an.t.v.integer.v.u;
						}
					}
				} while ((i--) != 0u);
			}

			/* if dereferncing too much, return no size */
			if (ptr_deref != 0u)
				return data_size_none;
		}

#if 0
		fprintf(stderr,"dbg final: calcsz=%zu count=%zu\n",data_calcsz,count);
#endif

		if (data_calcsz != data_size_none)
			return data_calcsz * count;

		return data_size_none;
	}

	data_size_t cc_state_t::calc_sizeof(declaration_specifiers_t &spec,declarator_t &decl,size_t ptr_deref) {
		if ((decl.flags & (declarator_t::FL_FUNCTION|declarator_t::FL_FUNCTION_POINTER)) == declarator_t::FL_FUNCTION)
			return data_size_none;

		return calc_sizeof(spec,decl.ddip,ptr_deref);
	}

	bool cc_state_t::ast_constexpr_sizeof(token_t &r,token_t &op,size_t ptr_deref) {
		switch (op.type) {
			case token_type_t::op_symbol:
			{
				if (op.v.symbol != symbol_none) {
					auto &sym = symbol(op.v.symbol);
					if (sym.sym_type == symbol_t::FUNCTION || sym.sym_type == symbol_t::NONE)
						return false;

					data_size_t sz = calc_sizeof(sym.spec,sym.ddip,ptr_deref);
					if (sz != data_size_none) {
						r = token_t(token_type_t::integer);
						r.v.integer.v.u = sz;
						r.v.integer.flags = 0;
						return true;
					}
				}
				break;
			}
			case token_type_t::op_declaration:
			{
				declaration_t *decl = op.v.declaration;
				assert(decl != NULL);

				if (decl->declor.size() == 1) {
					data_size_t sz = calc_sizeof(decl->spec,decl->declor[0]);
					if (sz != data_size_none) {
						r = token_t(token_type_t::integer);
						r.v.integer.v.u = sz;
						r.v.integer.flags = 0;
						return true;
					}
				}
				break;
			}
			default:
				break;
		};

		return false;
	}

	bool cc_state_t::ast_constexpr_alignof(token_t &r,token_t &op,size_t ptr_deref) {
		switch (op.type) {
			case token_type_t::op_symbol:
			{
				if (op.v.symbol != symbol_none) {
					auto &sym = symbol(op.v.symbol);
					if (sym.sym_type == symbol_t::FUNCTION || sym.sym_type == symbol_t::NONE)
						return false;

					addrmask_t sz = calc_alignof(sym.spec,sym.ddip,ptr_deref);
					if (sz != addrmask_none) {
						r = token_t(token_type_t::integer);
						r.v.integer.v.u = sz;
						r.v.integer.flags = 0;
						return true;
					}
				}
				break;
			}
			case token_type_t::op_declaration:
			{
				declaration_t *decl = op.v.declaration;
				assert(decl != NULL);

				if (decl->declor.size() == 1) {
					addrmask_t sz = calc_alignof(decl->spec,decl->declor[0]);
					if (sz != addrmask_none) {
						r = token_t(token_type_t::integer);
						r.v.integer.v.u = sz;
						r.v.integer.flags = 0;
						return true;
					}
				}
				break;
			}
			default:
				break;
		};

		return false;
	}

	bool ast_constexpr_leftshift(token_t &r,token_t &op1,token_t &op2) {
		/* TODO: type promotion/conversion */
		if (op1.type == op2.type) {
			switch (op1.type) {
				case token_type_t::integer:
					/* negative or over-large shift? NOPE.
					 * That is undefined behavior. On Intel CPUs other than the 8086 for example,
					 * the shift instructions only use the number of LSB bits relevent to the data
					 * type being shifted. For example shifting an 8-bit value only uses the low 3 bits (0-7),
					 * 16-bit the low 4 bits (0-15), 32-bit the low 5 bits (0-31), and so on.
					 * TODO: If too large for the target interger data type, not the 64-bit integers we use here. */
					if (op2.v.integer.v.v < 0ll || op2.v.integer.v.v >= 63ll)
						return false;

					r = op1;
					if (op1.v.integer.flags & integer_value_t::FL_SIGNED)
						r.v.integer.v.u = op1.v.integer.v.v << op2.v.integer.v.v;
					else
						r.v.integer.v.u = op1.v.integer.v.u << op2.v.integer.v.u;
					return true;
				default:
					break;
			};
		}

		return false;
	}

	bool ast_constexpr_rightshift(token_t &r,token_t &op1,token_t &op2) {
		/* TODO: type promotion/conversion */
		if (op1.type == op2.type) {
			switch (op1.type) {
				case token_type_t::integer:
					/* negative or over-large shift? NOPE.
					 * That is undefined behavior. On Intel CPUs other than the 8086 for example,
					 * the shift instructions only use the number of LSB bits relevent to the data
					 * type being shifted. For example shifting an 8-bit value only uses the low 3 bits (0-7),
					 * 16-bit the low 4 bits (0-15), 32-bit the low 5 bits (0-31), and so on.
					 * TODO: If too large for the target interger data type, not the 64-bit integers we use here. */
					if (op2.v.integer.v.v < 0ll || op2.v.integer.v.v >= 63ll)
						return false;

					r = op1;
					if (op1.v.integer.flags & integer_value_t::FL_SIGNED)
						r.v.integer.v.u = op1.v.integer.v.v >> op2.v.integer.v.v;
					else
						r.v.integer.v.u = op1.v.integer.v.u >> op2.v.integer.v.u;
					return true;
				default:
					break;
			};
		}

		return false;
	}

	bool ast_constexpr_lessthan_equals(token_t &r,token_t &op1,token_t &op2) {
		/* TODO: type promotion/conversion */
		if (op1.type == op2.type) {
			switch (op1.type) {
				case token_type_t::integer:
					r = op1;
					if (op1.v.integer.flags & integer_value_t::FL_SIGNED)
						r.v.integer.v.u = (op1.v.integer.v.v <= op2.v.integer.v.v) ? 1 : 0;
					else
						r.v.integer.v.u = (op1.v.integer.v.u <= op2.v.integer.v.u) ? 1 : 0;
					return true;
				default:
					break;
			};
		}

		return false;
	}

	bool ast_constexpr_greaterthan_equals(token_t &r,token_t &op1,token_t &op2) {
		/* TODO: type promotion/conversion */
		if (op1.type == op2.type) {
			switch (op1.type) {
				case token_type_t::integer:
					r = op1;
					if (op1.v.integer.flags & integer_value_t::FL_SIGNED)
						r.v.integer.v.u = (op1.v.integer.v.v >= op2.v.integer.v.v) ? 1 : 0;
					else
						r.v.integer.v.u = (op1.v.integer.v.u >= op2.v.integer.v.u) ? 1 : 0;
					return true;
				default:
					break;
			};
		}

		return false;
	}

	bool ast_constexpr_lessthan(token_t &r,token_t &op1,token_t &op2) {
		/* TODO: type promotion/conversion */
		if (op1.type == op2.type) {
			switch (op1.type) {
				case token_type_t::integer:
					r = op1;
					if (op1.v.integer.flags & integer_value_t::FL_SIGNED)
						r.v.integer.v.u = (op1.v.integer.v.v < op2.v.integer.v.v) ? 1 : 0;
					else
						r.v.integer.v.u = (op1.v.integer.v.u < op2.v.integer.v.u) ? 1 : 0;
					return true;
				default:
					break;
			};
		}

		return false;
	}

	bool ast_constexpr_greaterthan(token_t &r,token_t &op1,token_t &op2) {
		/* TODO: type promotion/conversion */
		if (op1.type == op2.type) {
			switch (op1.type) {
				case token_type_t::integer:
					r = op1;
					if (op1.v.integer.flags & integer_value_t::FL_SIGNED)
						r.v.integer.v.u = (op1.v.integer.v.v > op2.v.integer.v.v) ? 1 : 0;
					else
						r.v.integer.v.u = (op1.v.integer.v.u > op2.v.integer.v.u) ? 1 : 0;
					return true;
				default:
					break;
			};
		}

		return false;
	}

	bool ast_constexpr_equals(token_t &r,token_t &op1,token_t &op2) {
		/* TODO: type promotion/conversion */
		if (op1.type == op2.type) {
			switch (op1.type) {
				case token_type_t::integer:
					r = op1;
					r.v.integer.v.u = (op1.v.integer.v.u == op2.v.integer.v.u) ? 1 : 0;
					return true;
				default:
					break;
			};
		}

		return false;
	}

	bool ast_constexpr_not_equals(token_t &r,token_t &op1,token_t &op2) {
		/* TODO: type promotion/conversion */
		if (op1.type == op2.type) {
			switch (op1.type) {
				case token_type_t::integer:
					r = op1;
					r.v.integer.v.u = (op1.v.integer.v.u != op2.v.integer.v.u) ? 1 : 0;
					return true;
				default:
					break;
			};
		}

		return false;
	}

	bool ast_constexpr_binary_not(token_t &r,token_t &op) {
		/* TODO: type promotion/conversion */
		switch (op.type) {
			case token_type_t::integer:
				r = op;
				r.v.integer.v.u = ~r.v.integer.v.u;
				return true;
			default:
				break;
		};

		return false;
	}

	bool ast_constexpr_logical_not(token_t &r,token_t &op) {
		/* TODO: type promotion/conversion */
		switch (op.type) {
			case token_type_t::integer:
				r = op;
				r.v.integer.v.u = ast_constexpr_to_bool(op.v.integer) ? 0 : 1;
				return true;
			default:
				break;
		};

		return false;
	}

	bool ast_constexpr_negate(token_t &r,token_t &op) {
		/* TODO: type promotion/conversion */
		switch (op.type) {
			case token_type_t::integer:
				r = op;
				r.v.integer.v.v = -r.v.integer.v.v;
				r.v.integer.flags |= integer_value_t::FL_SIGNED;
				return true;
			default:
				break;
		};

		return false;
	}

	bool ast_constexpr_add(token_t &r,token_t &op1,token_t &op2) {
		/* TODO: type promotion/conversion */
		if (op1.type == op2.type) {
			switch (op1.type) {
				case token_type_t::integer:
					r = op1;
					r.v.integer.v.u += op2.v.integer.v.u;
					return true;
				default:
					break;
			};
		}

		return false;
	}

	bool ast_constexpr_subtract(token_t &r,token_t &op1,token_t &op2) {
		/* TODO: type promotion/conversion */
		if (op1.type == op2.type) {
			switch (op1.type) {
				case token_type_t::integer:
					r = op1;
					r.v.integer.v.u -= op2.v.integer.v.u;
					return true;
				default:
					break;
			};
		}

		return false;
	}

	bool ast_constexpr_logical_or(token_t &r,token_t &op1,token_t &op2) {
		/* TODO: type promotion/conversion */
		if (op1.type == op2.type) {
			switch (op1.type) {
				case token_type_t::integer:
					r = op1;
					r.v.integer.v.u = (ast_constexpr_to_bool(op1.v.integer) || ast_constexpr_to_bool(op2.v.integer)) ? 1 : 0;
					return true;
				default:
					break;
			};
		}

		return false;
	}

	bool ast_constexpr_binary_or(token_t &r,token_t &op1,token_t &op2) {
		/* TODO: type promotion/conversion */
		if (op1.type == op2.type) {
			switch (op1.type) {
				case token_type_t::integer:
					r = op1;
					r.v.integer.v.v |= op2.v.integer.v.v;
					return true;
				default:
					break;
			};
		}

		return false;
	}

	bool ast_constexpr_binary_xor(token_t &r,token_t &op1,token_t &op2) {
		/* TODO: type promotion/conversion */
		if (op1.type == op2.type) {
			switch (op1.type) {
				case token_type_t::integer:
					r = op1;
					r.v.integer.v.v ^= op2.v.integer.v.v;
					return true;
				default:
					break;
			};
		}

		return false;
	}

	bool ast_constexpr_logical_and(token_t &r,token_t &op1,token_t &op2) {
		/* TODO: type promotion/conversion */
		if (op1.type == op2.type) {
			switch (op1.type) {
				case token_type_t::integer:
					r = op1;
					r.v.integer.v.u = (ast_constexpr_to_bool(op1.v.integer) && ast_constexpr_to_bool(op2.v.integer)) ? 1 : 0;
					return true;
				default:
					break;
			};
		}

		return false;
	}

	bool ast_constexpr_binary_and(token_t &r,token_t &op1,token_t &op2) {
		/* TODO: type promotion/conversion */
		if (op1.type == op2.type) {
			switch (op1.type) {
				case token_type_t::integer:
					r = op1;
					r.v.integer.v.v &= op2.v.integer.v.v;
					return true;
				default:
					break;
			};
		}

		return false;
	}

	bool ast_constexpr_multiply(token_t &r,token_t &op1,token_t &op2) {
		/* TODO: type promotion/conversion */
		if (op1.type == op2.type) {
			switch (op1.type) {
				case token_type_t::integer:
					r = op1;
					if (op1.v.integer.flags & integer_value_t::FL_SIGNED)
						r.v.integer.v.v *= op2.v.integer.v.v;
					else
						r.v.integer.v.u *= op2.v.integer.v.u;
					return true;
				default:
					break;
			};
		}

		return false;
	}

	bool ast_constexpr_divide(token_t &r,token_t &op1,token_t &op2) {
		/* TODO: type promotion/conversion */
		if (op1.type == op2.type) {
			switch (op1.type) {
				case token_type_t::integer:
					/* divide by zero? NOPE! */
					if (op2.v.integer.v.u == 0ull)
						return false;

					r = op1;
					if (op1.v.integer.flags & integer_value_t::FL_SIGNED)
						r.v.integer.v.v /= op2.v.integer.v.v;
					else
						r.v.integer.v.u /= op2.v.integer.v.u;
					return true;
				default:
					break;
			};
		}

		return false;
	}

	bool ast_constexpr_modulus(token_t &r,token_t &op1,token_t &op2) {
		/* TODO: type promotion/conversion */
		if (op1.type == op2.type) {
			switch (op1.type) {
				case token_type_t::integer:
					/* divide by zero? NOPE! */
					if (op2.v.integer.v.u == 0ull)
						return false;

					r = op1;
					if (op1.v.integer.flags & integer_value_t::FL_SIGNED)
						r.v.integer.v.v %= op2.v.integer.v.v;
					else
						r.v.integer.v.u %= op2.v.integer.v.u;
					return true;
				default:
					break;
			};
		}

		return false;
	}

	void cc_state_t::ast_node_reduce(ast_node_id_t &eroot,const std::string &prefix) { /* destructive reduce */
#define OP_ONE_PARAM_TEVAL ast_node_id_t op1 = erootnode.child

#define OP_TWO_PARAM_TEVAL ast_node_id_t op1 = erootnode.child; \
                     ast_node_id_t op2 = ast_node(op1).next

		if (eroot == ast_node_none)
			return;

#if 1//DEBUG
		if (prefix.empty()) {
			fprintf(stderr,"%senum expr (reducing node#%lu):\n",prefix.c_str(),(unsigned long)eroot);
			debug_dump_ast(prefix+"  ",eroot);
		}
#endif

again:
		for (ast_node_id_t n=eroot;n!=ast_node_none;n=ast_node(n).next)
			ast_node_reduce(ast_node(n).child,prefix+"  ");

		if (eroot != ast_node_none) {
			/* WARNING: stale references will occur if any code during this switch statement creates new AST nodes */
			ast_node_t &erootnode = ast_node(eroot);
			switch (erootnode.t.type) {
				case token_type_t::op_sizeof:
					{
						size_t ptrdref = 0;
						OP_ONE_PARAM_TEVAL;
						do {
							assert(op1 != ast_node_none);
							ast_node_t &an = ast_node(op1);

							if (an.t.type == token_type_t::op_binary_not) {
								op1 = an.child;
							}
							else if (an.t.type == token_type_t::op_pointer_deref) {
								op1 = an.child;
								ptrdref++;
							}
							else if (an.t.type == token_type_t::op_address_of) {
								op1 = an.child;
								ptrdref = ptr_deref_sizeof_addressof;

								while (op1 != ast_node_none && ast_node(op1).t.type == token_type_t::op_address_of)
									op1 = ast_node(op1).child;

								break;
							}
							else {
								break;
							}
						} while(1);
						if (ast_constexpr_sizeof(erootnode.t,ast_node(op1).t,ptrdref)) {
							erootnode.set_child(ast_node_none);
							goto again;
						}
						break;
					}

				case token_type_t::op_alignof:
					{
						size_t ptrdref = 0;
						OP_ONE_PARAM_TEVAL;
						do {
							assert(op1 != ast_node_none);
							ast_node_t &an = ast_node(op1);

							if (an.t.type == token_type_t::op_binary_not) {
								op1 = an.child;
							}
							else if (an.t.type == token_type_t::op_pointer_deref) {
								op1 = an.child;
								ptrdref++;
							}
							else if (an.t.type == token_type_t::op_address_of) {
								op1 = an.child;
								ptrdref = ptr_deref_sizeof_addressof;

								while (op1 != ast_node_none && ast_node(op1).t.type == token_type_t::op_address_of)
									op1 = ast_node(op1).child;

								break;
							}
							else {
								break;
							}
						} while(1);
						if (ast_constexpr_alignof(erootnode.t,ast_node(op1).t,ptrdref)) {
							erootnode.set_child(ast_node_none);
							goto again;
						}
						break;
					}

				case token_type_t::op_negate:
					{
						OP_ONE_PARAM_TEVAL;
						if (is_ast_constexpr(ast_node(op1).t)) {
							if (ast_constexpr_negate(erootnode.t,ast_node(op1).t)) {
								erootnode.set_child(ast_node_none);
								goto again;
							}
						}
						break;
					}

				case token_type_t::op_binary_not:
					{
						OP_ONE_PARAM_TEVAL;
						if (is_ast_constexpr(ast_node(op1).t)) {
							if (ast_constexpr_binary_not(erootnode.t,ast_node(op1).t)) {
								erootnode.set_child(ast_node_none);
								goto again;
							}
						}
						break;
					}

				case token_type_t::op_logical_not:
					{
						OP_ONE_PARAM_TEVAL;
						if (is_ast_constexpr(ast_node(op1).t)) {
							if (ast_constexpr_logical_not(erootnode.t,ast_node(op1).t)) {
								erootnode.set_child(ast_node_none);
								goto again;
							}
						}
						break;
					}

				case token_type_t::op_leftshift:
					{
						OP_TWO_PARAM_TEVAL;
						if (is_ast_constexpr(ast_node(op1).t) && is_ast_constexpr(ast_node(op2).t)) {
							if (ast_constexpr_leftshift(erootnode.t,ast_node(op1).t,ast_node(op2).t)) {
								erootnode.set_child(ast_node_none);
								goto again;
							}
						}
						break;
					}

				case token_type_t::op_rightshift:
					{
						OP_TWO_PARAM_TEVAL;
						if (is_ast_constexpr(ast_node(op1).t) && is_ast_constexpr(ast_node(op2).t)) {
							if (ast_constexpr_rightshift(erootnode.t,ast_node(op1).t,ast_node(op2).t)) {
								erootnode.set_child(ast_node_none);
								goto again;
							}
						}
						break;
					}

				case token_type_t::op_lessthan_equals:
					{
						OP_TWO_PARAM_TEVAL;
						if (is_ast_constexpr(ast_node(op1).t) && is_ast_constexpr(ast_node(op2).t)) {
							if (ast_constexpr_lessthan_equals(erootnode.t,ast_node(op1).t,ast_node(op2).t)) {
								erootnode.set_child(ast_node_none);
								goto again;
							}
						}
						break;
					}

				case token_type_t::op_greaterthan_equals:
					{
						OP_TWO_PARAM_TEVAL;
						if (is_ast_constexpr(ast_node(op1).t) && is_ast_constexpr(ast_node(op2).t)) {
							if (ast_constexpr_greaterthan_equals(erootnode.t,ast_node(op1).t,ast_node(op2).t)) {
								erootnode.set_child(ast_node_none);
								goto again;
							}
						}
						break;
					}

				case token_type_t::op_lessthan:
					{
						OP_TWO_PARAM_TEVAL;
						if (is_ast_constexpr(ast_node(op1).t) && is_ast_constexpr(ast_node(op2).t)) {
							if (ast_constexpr_lessthan(erootnode.t,ast_node(op1).t,ast_node(op2).t)) {
								erootnode.set_child(ast_node_none);
								goto again;
							}
						}
						break;
					}

				case token_type_t::op_greaterthan:
					{
						OP_TWO_PARAM_TEVAL;
						if (is_ast_constexpr(ast_node(op1).t) && is_ast_constexpr(ast_node(op2).t)) {
							if (ast_constexpr_greaterthan(erootnode.t,ast_node(op1).t,ast_node(op2).t)) {
								erootnode.set_child(ast_node_none);
								goto again;
							}
						}
						break;
					}

				case token_type_t::op_equals:
					{
						OP_TWO_PARAM_TEVAL;
						if (is_ast_constexpr(ast_node(op1).t) && is_ast_constexpr(ast_node(op2).t)) {
							if (ast_constexpr_equals(erootnode.t,ast_node(op1).t,ast_node(op2).t)) {
								erootnode.set_child(ast_node_none);
								goto again;
							}
						}
						break;
					}

				case token_type_t::op_not_equals:
					{
						OP_TWO_PARAM_TEVAL;
						if (is_ast_constexpr(ast_node(op1).t) && is_ast_constexpr(ast_node(op2).t)) {
							if (ast_constexpr_not_equals(erootnode.t,ast_node(op1).t,ast_node(op2).t)) {
								erootnode.set_child(ast_node_none);
								goto again;
							}
						}
						break;
					}

				case token_type_t::op_add:
					{
						OP_TWO_PARAM_TEVAL;
						if (is_ast_constexpr(ast_node(op1).t) && is_ast_constexpr(ast_node(op2).t)) {
							if (ast_constexpr_add(erootnode.t,ast_node(op1).t,ast_node(op2).t)) {
								erootnode.set_child(ast_node_none);
								goto again;
							}
						}
						break;
					}

				case token_type_t::op_subtract:
					{
						OP_TWO_PARAM_TEVAL;
						if (is_ast_constexpr(ast_node(op1).t) && is_ast_constexpr(ast_node(op2).t)) {
							if (ast_constexpr_subtract(erootnode.t,ast_node(op1).t,ast_node(op2).t)) {
								erootnode.set_child(ast_node_none);
								goto again;
							}
						}
						break;
					}

				case token_type_t::op_logical_or:
					{
						OP_TWO_PARAM_TEVAL;
						if (is_ast_constexpr(ast_node(op1).t) && is_ast_constexpr(ast_node(op2).t)) {
							if (ast_constexpr_logical_or(erootnode.t,ast_node(op1).t,ast_node(op2).t)) {
								erootnode.set_child(ast_node_none);
								goto again;
							}
						}
						break;
					}

				case token_type_t::op_binary_or:
					{
						OP_TWO_PARAM_TEVAL;
						if (is_ast_constexpr(ast_node(op1).t) && is_ast_constexpr(ast_node(op2).t)) {
							if (ast_constexpr_binary_or(erootnode.t,ast_node(op1).t,ast_node(op2).t)) {
								erootnode.set_child(ast_node_none);
								goto again;
							}
						}
						break;
					}

				case token_type_t::op_binary_xor:
					{
						OP_TWO_PARAM_TEVAL;
						if (is_ast_constexpr(ast_node(op1).t) && is_ast_constexpr(ast_node(op2).t)) {
							if (ast_constexpr_binary_xor(erootnode.t,ast_node(op1).t,ast_node(op2).t)) {
								erootnode.set_child(ast_node_none);
								goto again;
							}
						}
						break;
					}

				case token_type_t::op_logical_and:
					{
						OP_TWO_PARAM_TEVAL;
						if (is_ast_constexpr(ast_node(op1).t) && is_ast_constexpr(ast_node(op2).t)) {
							if (ast_constexpr_logical_and(erootnode.t,ast_node(op1).t,ast_node(op2).t)) {
								erootnode.set_child(ast_node_none);
								goto again;
							}
						}
						break;
					}

				case token_type_t::op_binary_and:
					{
						OP_TWO_PARAM_TEVAL;
						if (is_ast_constexpr(ast_node(op1).t) && is_ast_constexpr(ast_node(op2).t)) {
							if (ast_constexpr_binary_and(erootnode.t,ast_node(op1).t,ast_node(op2).t)) {
								erootnode.set_child(ast_node_none);
								goto again;
							}
						}
						break;
					}

				case token_type_t::op_multiply:
					{
						OP_TWO_PARAM_TEVAL;
						if (is_ast_constexpr(ast_node(op1).t) && is_ast_constexpr(ast_node(op2).t)) {
							if (ast_constexpr_multiply(erootnode.t,ast_node(op1).t,ast_node(op2).t)) {
								erootnode.set_child(ast_node_none);
								goto again;
							}
						}
						break;
					}

				case token_type_t::op_divide:
					{
						OP_TWO_PARAM_TEVAL;
						if (is_ast_constexpr(ast_node(op1).t) && is_ast_constexpr(ast_node(op2).t)) {
							if (ast_constexpr_divide(erootnode.t,ast_node(op1).t,ast_node(op2).t)) {
								erootnode.set_child(ast_node_none);
								goto again;
							}
						}
						break;
					}

				case token_type_t::op_modulus:
					{
						OP_TWO_PARAM_TEVAL;
						if (is_ast_constexpr(ast_node(op1).t) && is_ast_constexpr(ast_node(op2).t)) {
							if (ast_constexpr_modulus(erootnode.t,ast_node(op1).t,ast_node(op2).t)) {
								erootnode.set_child(ast_node_none);
								goto again;
							}
						}
						break;
					}

				case token_type_t::op_comma:
					{
						OP_TWO_PARAM_TEVAL;
						if (is_ast_constexpr(ast_node(op1).t)) {
							ast_node_id_t nn = ast_node.returnmove(erootnode.next);
							op1 = ast_node.returnmove(erootnode.child);
							op2 = ast_node.returnmove(ast_node(op1).next);

							ast_node.assignmove(eroot,op2);

							ast_node(eroot).set_next(nn);
							ast_node.release(op2);
							ast_node.release(op1);
							ast_node.release(nn);
							goto again;
						}
						break;
					}

				case token_type_t::op_ternary:
					{
						if (is_ast_constexpr(ast_node(erootnode.child).t)) {
							ast_node_id_t nn = ast_node.returnmove(erootnode.next);
							ast_node_id_t cn = ast_node.returnmove(erootnode.child);
							ast_node_id_t tc = ast_node.returnmove(ast_node(cn).next);
							ast_node_id_t fc = ast_node.returnmove(ast_node(tc).next);

							if (ast_constexpr_to_bool(ast_node(cn).t))
								ast_node.assignmove(eroot,tc);
							else
								ast_node.assignmove(eroot,fc);

							ast_node(eroot).set_next(nn);
							ast_node.release(fc);
							ast_node.release(tc);
							ast_node.release(cn);
							ast_node.release(nn);
							goto again;
						}
						break;
					}

				case token_type_t::op_symbol:
					{
						symbol_t &sym = symbol(ast_node(eroot).t.v.symbol);
						if (sym.sym_type == symbol_t::CONST) {
							if (sym.expr != ast_node_none) {
								/* non-destructive copy the token from the symbol.
								 * this will not work if the node has children or sibling (next) */
								if (ast_node(sym.expr).child == ast_node_none && ast_node(sym.expr).next == ast_node_none) {
									ast_node(eroot).t = ast_node(sym.expr).t;
									goto again;
								}
							}
						}
						break;
					}

				default:
					{
						for (ast_node_id_t n=eroot;n!=ast_node_none;n=ast_node(n).next)
							ast_node_reduce(ast_node(n).next,"  ");
						break;
					}
			}
		}

#if 1//DEBUG
		if (prefix.empty()) {
			fprintf(stderr,"%senum expr (reduce-complete):\n",prefix.c_str());
			debug_dump_ast(prefix+"  ",eroot);
		}
#endif
#undef OP_TWO_PARAM_TEVAL
#undef OP_ONE_PARAM_TEVAL
	}

	int cc_state_t::enumerator_list_parse(declaration_specifiers_t &spec,std::vector<symbol_id_t> &enum_list) {
		integer_value_t iv;
		int r;

		iv.init();
		iv.v.u = 0;
		iv.flags = integer_value_t::FL_SIGNED;

		if ((spec.type_specifier & (TS_INT|TS_LONG|TS_LONGLONG|TS_CHAR)) == 0)
			CCERR_RET(EINVAL,tq_peek().pos,"Enumeration list must use integer type");

		if (spec.type_specifier & TS_UNSIGNED)
			iv.flags &= ~integer_value_t::FL_SIGNED;

		/* caller already ate the { */

		do {
			if (tq_peek().type == token_type_t::closecurlybracket) {
				tq_discard();
				break;
			}

			enumerator_t en;

			en.pos = tq_peek().pos;
			if (tq_peek().type != token_type_t::identifier || tq_peek().v.identifier == identifier_none)
				CCERR_RET(EINVAL,tq_peek().pos,"Identifier expected");

			identifier.assign(/*to*/en.name,/*from*/tq_get().v.identifier);

			if (tq_peek().type == token_type_t::equal) {
				tq_discard();

				if ((r=conditional_expression(en.expr)) < 1)
					return r;

				ast_node_reduce(en.expr);

				/* the expression must reduce to an integer, or else it's an error */
				ast_node_t &an = ast_node(en.expr);
				if (an.t.type == token_type_t::integer) {
					iv.v = an.t.v.integer.v;
				}
				else {
					CCERR_RET(EINVAL,en.pos,"Enumeration constant is not a constant integer expression");
				}
			}
			else {
				en.expr = ast_node.alloc(token_type_t::integer);
				ast_node(en.expr).t.v.integer = iv;
			}

			iv.v.u++;

			{
				declarator_t declor;
				symbol_lookup_t sl;

				sl.pos = en.pos;
				sl.st = symbol_t::CONST;
				ast_node.assignmove(/*to*/declor.expr,/*from*/en.expr);
				identifier.assignmove(/*to*/declor.name,/*from*/en.name);
				if ((r=prep_symbol_lookup(sl,spec,declor)) < 1)
					return r;
				if (do_local_symbol_lookup(sl,spec,declor)) {
					if ((r=check_symbol_lookup_match(sl,spec,declor)) < 1)
						return r;
				}
				else if ((r=add_symbol(sl,spec,declor)) < 1) {
					return r;
				}

				enum_list.push_back(sl.sid);
			}
			if (tq_peek().type == token_type_t::closecurlybracket) {
				tq_discard();
				break;
			}
			else if (tq_peek().type == token_type_t::comma) {
				tq_discard();
			}
			else {
				return errno_return(EINVAL);
			}
		} while (1);

		return 1;
	}

	bool cc_state_t::declaration_specifiers_check(const unsigned int token_offset) {
		const token_t &t = tq_peek(token_offset);

		switch (t.type) {
			case token_type_t::r_typedef: return true;
			case token_type_t::r_extern: return true;
			case token_type_t::r_static: return true;
			case token_type_t::r_auto: return true;
			case token_type_t::r_register: return true;
			case token_type_t::r_constexpr: return true;
			case token_type_t::r_void: return true;
			case token_type_t::r_char: return true;
			case token_type_t::r_short: return true;
			case token_type_t::r_int: return true;
			case token_type_t::r_float: return true;
			case token_type_t::r_double: return true;
			case token_type_t::r_signed: return true;
			case token_type_t::r_unsigned: return true;
			case token_type_t::r_const: return true;
			case token_type_t::r_volatile: return true;
			case token_type_t::r_near: return true;
			case token_type_t::r_far: return true;
			case token_type_t::r_huge: return true;
			case token_type_t::r_restrict: return true;
			case token_type_t::r_long: return true;
			case token_type_t::r_enum: return true;
			case token_type_t::r_struct: return true;
			case token_type_t::r_union: return true;
			case token_type_t::r_inline: return true;
			case token_type_t::r_consteval: return true;
			case token_type_t::r_constinit: return true;
			case token_type_t::r_size_t: return true;
			case token_type_t::r_ssize_t: return true;
			case token_type_t::r___int8: return true;
			case token_type_t::r___int16: return true;
			case token_type_t::r___int32: return true;
			case token_type_t::r___int64: return true;
			case token_type_t::r_int8_t: return true;
			case token_type_t::r_int16_t: return true;
			case token_type_t::r_int32_t: return true;
			case token_type_t::r_int64_t: return true;
			case token_type_t::r_uint8_t: return true;
			case token_type_t::r_uint16_t: return true;
			case token_type_t::r_uint32_t: return true;
			case token_type_t::r_uint64_t: return true;
			case token_type_t::r_uintptr_t: return true;
			case token_type_t::r_intptr_t: return true;
			case token_type_t::r_alignas: return true;
			case token_type_t::r__Alignas: return true;

			case token_type_t::identifier:
				{
					symbol_id_t sid;

					if ((sid=lookup_symbol(t.v.identifier,symbol_t::TYPEDEF)) == symbol_none)
						return false;

					if (symbol(sid).sym_type != symbol_t::TYPEDEF)
						return false;
				}
				return true;

			default: break;
		};

		return false;
	}

	int cc_state_t::declaration_specifiers_parse(declaration_specifiers_t &ds,const unsigned int declspec) {
		const position_t pos = tq_peek().pos;
		type_specifier_t builtin_ts = 0;
		int r;

		do {
			const token_t &t = tq_peek();

#define XCHK(d,m) \
	if (d&m) { \
		CCERR_RET(EINVAL,t.pos,"declarator specifier '%s' already specified",token_type_t_str(t.type)); \
	} \
	else { \
		ds.count++; \
		d|=m; \
	}
#define X(d,m) \
	{ \
		XCHK(d,m); \
		tq_discard(); \
		continue; \
	}

			switch (t.type) {
				case token_type_t::r_typedef:		X(ds.storage_class,SC_TYPEDEF);
				case token_type_t::r_extern:		X(ds.storage_class,SC_EXTERN);
				case token_type_t::r_static:		X(ds.storage_class,SC_STATIC);
				case token_type_t::r_auto:		X(ds.storage_class,SC_AUTO);
				case token_type_t::r_register:		X(ds.storage_class,SC_REGISTER);
				case token_type_t::r_constexpr:		X(ds.storage_class,SC_CONSTEXPR);
				case token_type_t::r_inline:		X(ds.storage_class,SC_INLINE);
				case token_type_t::r_consteval:		X(ds.storage_class,SC_CONSTEVAL);
				case token_type_t::r_constinit:		X(ds.storage_class,SC_CONSTINIT);
				case token_type_t::r_void:		X(ds.type_specifier,TS_VOID);
				case token_type_t::r_char:		X(ds.type_specifier,TS_CHAR);
				case token_type_t::r_short:		X(ds.type_specifier,TS_SHORT);
				case token_type_t::r_int:		X(ds.type_specifier,TS_INT);
				case token_type_t::r_float:		X(ds.type_specifier,TS_FLOAT);
				case token_type_t::r_double:		X(ds.type_specifier,TS_DOUBLE);
				case token_type_t::r_signed:		X(ds.type_specifier,TS_SIGNED);
				case token_type_t::r_unsigned:		X(ds.type_specifier,TS_UNSIGNED);
				case token_type_t::r_const:		X(ds.type_qualifier,TQ_CONST);
				case token_type_t::r_volatile:		X(ds.type_qualifier,TQ_VOLATILE);
				case token_type_t::r_near:		X(ds.type_qualifier,TQ_NEAR);
				case token_type_t::r_far:		X(ds.type_qualifier,TQ_FAR);
				case token_type_t::r_huge:		X(ds.type_qualifier,TQ_HUGE);
				case token_type_t::r_restrict:		X(ds.type_qualifier,TQ_RESTRICT);

				case token_type_t::r_long:
					ds.count++;
					if (ds.type_specifier & TS_LONG) {
						/* second "long" promote to "long long" because GCC allows it too i.e. "long int long" is the same as "long long int" */
						if (ds.type_specifier & TS_LONGLONG)
							CCERR_RET(EINVAL,pos,"declarator specifier 'long long' already specified");

						ds.type_specifier = (ds.type_specifier & (~TS_LONG)) | TS_LONGLONG;
						tq_discard();
					}
					else {
						if (ds.type_specifier & TS_LONG)
							CCERR_RET(EINVAL,pos,"declarator specifier 'long' already specified");

						ds.type_specifier |= TS_LONG;
						tq_discard();
					}
					continue;

#define ONLYSPECALLOWED if (ds.type_specifier != 0) goto common_error
				case token_type_t::r_size_t:
					builtin_ts = data_types_ptr_data.dt_size_t.ts; ONLYSPECALLOWED; ds.type_specifier |= TS_UNSIGNED; goto common_builtin;
				case token_type_t::r_ssize_t:
					builtin_ts = data_types_ptr_data.dt_size_t.ts; ONLYSPECALLOWED; ds.type_specifier |= TS_SIGNED; goto common_builtin;
				case token_type_t::r___int8:
					builtin_ts = TS_INT | TS_SZ8; goto common_builtin;
				case token_type_t::r___int16:
					builtin_ts = TS_INT | TS_SZ16; goto common_builtin;
				case token_type_t::r___int32:
					builtin_ts = TS_INT | TS_SZ32; goto common_builtin;
				case token_type_t::r___int64:
					builtin_ts = TS_INT | TS_SZ64; goto common_builtin;
				case token_type_t::r_int8_t:
					builtin_ts = TS_INT | TS_SZ8; ONLYSPECALLOWED; ds.type_specifier |= TS_SIGNED; goto common_builtin;
				case token_type_t::r_int16_t:
					builtin_ts = TS_INT | TS_SZ16; ONLYSPECALLOWED; ds.type_specifier |= TS_SIGNED; goto common_builtin;
				case token_type_t::r_int32_t:
					builtin_ts = TS_INT | TS_SZ32; ONLYSPECALLOWED; ds.type_specifier |= TS_SIGNED; goto common_builtin;
				case token_type_t::r_int64_t:
					builtin_ts = TS_INT | TS_SZ64; ONLYSPECALLOWED; ds.type_specifier |= TS_SIGNED; goto common_builtin;
				case token_type_t::r_uint8_t:
					builtin_ts = TS_INT | TS_SZ8; ONLYSPECALLOWED; ds.type_specifier |= TS_UNSIGNED; goto common_builtin;
				case token_type_t::r_uint16_t:
					builtin_ts = TS_INT | TS_SZ16; ONLYSPECALLOWED; ds.type_specifier |= TS_UNSIGNED; goto common_builtin;
				case token_type_t::r_uint32_t:
					builtin_ts = TS_INT | TS_SZ32; ONLYSPECALLOWED; ds.type_specifier |= TS_UNSIGNED; goto common_builtin;
				case token_type_t::r_uint64_t:
					builtin_ts = TS_INT | TS_SZ64; ONLYSPECALLOWED; ds.type_specifier |= TS_UNSIGNED; goto common_builtin;
				case token_type_t::r_uintptr_t:
					builtin_ts = data_types_ptr_data.dt_intptr_t.ts; ONLYSPECALLOWED; ds.type_specifier |= TS_UNSIGNED; goto common_builtin;
				case token_type_t::r_intptr_t:
					builtin_ts = data_types_ptr_data.dt_intptr_t.ts; ONLYSPECALLOWED; ds.type_specifier |= TS_SIGNED; goto common_builtin;

#undef ONLYSPECALLOWED
common_builtin:
					if (ds.type_specifier & TS_MATCH_BUILTIN)
						break;

					ds.type_specifier |= TS_MATCH_BUILTIN;
					tq_discard();
					ds.count++;
					continue;
common_error:
					CCERR_RET(EINVAL,pos,"Extra specifiers for builtin type");

				case token_type_t::r_alignas:
				case token_type_t::r__Alignas:
					tq_discard();
					ds.count++;

					if (ds.align != addrmask_none)
						CCERR_RET(EINVAL,pos,"Alignas already specified");

					{
						ast_node_id_t expr = ast_node_none;

						if ((r=typeid_or_expr_parse(expr)) < 1)
							return r;

						ast_node_reduce(expr);

						ast_node_t &an = ast_node(expr);
						if (an.t.type == token_type_t::integer) {
							if (an.t.v.integer.v.u != 0) {
								if (an.t.v.integer.v.u & (an.t.v.integer.v.u - 1ull))
									CCERR_RET(EINVAL,pos,"Alignas expression not a power of 2");

								ds.align = addrmask_make(an.t.v.integer.v.u);
							}
						}
						else if (an.t.type == token_type_t::op_declaration) {
							declaration_t *decl = an.t.v.declaration;

							assert(decl != NULL);
							if (decl->declor.size() != 1)
								CCERR_RET(EINVAL,pos,"Unexpected number of declor");

							addrmask_t align = calc_alignof(decl->spec,decl->declor[0]);
							if (align == addrmask_none)
								CCERR_RET(EINVAL,pos,"Unable to determine alignof for alignas");

							ds.align = ~(align - addrmask_t(1u));
						}
						else if (an.t.type == token_type_t::op_symbol) {
							assert(an.t.v.symbol != symbol_none);
							auto &sym = symbol(an.t.v.symbol);
							if (sym.sym_type == symbol_t::FUNCTION || sym.sym_type == symbol_t::NONE)
								CCERR_RET(EINVAL,pos,"alignas for function or none");

							addrmask_t align = calc_alignof(sym.spec,sym.ddip);
							if (align == addrmask_none)
								CCERR_RET(EINVAL,pos,"Unable to determine alignof for alignas");

							ds.align = ~(align - addrmask_t(1u));
						}

						ast_node(expr).release();
					}

					continue;

				case token_type_t::identifier:
					if (ds.type_specifier & TS_MATCH_TYPEDEF)
						break;

					{
						symbol_id_t sid;

						if ((sid=lookup_symbol(t.v.identifier,symbol_t::TYPEDEF)) == symbol_none)
							break;

						if (symbol(sid).sym_type != symbol_t::TYPEDEF)
							break;

						ds.type_specifier |= TS_MATCH_TYPEDEF;
						ds.type_identifier_symbol = sid;
						tq_discard();
						ds.count++;
					}
					continue;

				case token_type_t::r_enum:
					ds.count++;
					if (ds.type_specifier & TS_ENUM)
						CCERR_RET(EINVAL,pos,"declarator specifier 'enum' already specified");

					ds.type_specifier |= TS_ENUM;
					tq_discard();

					/* enum { list }
					 * enum identifier { list }
					 * enum identifier */

					{
						declaration_specifiers_t eds;
						declarator_t declor;
						symbol_lookup_t sl;

						eds.count = 3;
						eds.type_specifier = TS_INT;
						eds.storage_class = SC_CONSTEXPR;
						eds.type_qualifier = TQ_CONST;

						if (tq_peek().type == token_type_t::identifier)
							identifier.assign(/*to*/declor.name,/*from*/tq_get().v.identifier);

						if (tq_peek().type == token_type_t::colon) {
							tq_discard();

							eds.type_specifier &= ~TS_INT;
							if ((r=declaration_specifiers_parse(eds)) < 1)
								return r;
						}

						sl.pos = pos;
						sl.st = symbol_t::ENUM;
						if (tq_peek().type == token_type_t::opencurlybracket) {
							tq_discard();

							if (!(declspec & DECLSPEC_ALLOW_DEF))
								CCERR_RET(EINVAL,pos,"not allowed to define types here");

							sl.flags = symbol_t::FL_DEFINED|symbol_t::FL_DECLARED;
							if ((r=prep_symbol_lookup(sl,eds,declor)) < 1)
								return r;
							if (do_local_symbol_lookup(sl,eds,declor)) {
								if ((r=check_symbol_lookup_match(sl,eds,declor)) < 1)
									return r;
							}
							else if ((r=add_symbol(sl,eds,declor)) < 1) {
								return r;
							}

							if ((r=enumerator_list_parse(eds,ds.enum_list)) < 1)
								return r;

							ds.type_identifier_symbol = sl.sid;
						}
						else if (declor.name != identifier_none) {
							if ((sl.sid=lookup_symbol(declor.name,sl.st)) == symbol_none)
								CCERR_RET(ENOENT,sl.pos,"No such enum");

							ds.type_identifier_symbol = sl.sid;
						}
					}
					continue;

				case token_type_t::r_struct:
					ds.count++;
					if (ds.type_specifier & TS_STRUCT)
						CCERR_RET(EINVAL,pos,"declarator specifier 'struct' already specified");

					ds.type_specifier |= TS_STRUCT;
					tq_discard();

					/* struct { list }
					 * struct identifier { list }
					 * struct identifier */

					{
						declarator_t declor;
						symbol_lookup_t sl;

						if (tq_peek().type == token_type_t::identifier)
							identifier.assign(/*to*/declor.name,/*from*/tq_get().v.identifier);

						sl.pos = pos;
						sl.st = symbol_t::STRUCT;
						if (tq_peek().type == token_type_t::opencurlybracket) {
							tq_discard();

							if (!(declspec & DECLSPEC_ALLOW_DEF))
								CCERR_RET(EINVAL,pos,"not allowed to define types here");

							sl.flags = symbol_t::FL_DEFINED|symbol_t::FL_DECLARED;
							if ((r=prep_symbol_lookup(sl,ds,declor)) < 1)
								return r;
							if (do_local_symbol_lookup(sl,ds,declor)) {
								if ((r=check_symbol_lookup_match(sl,ds,declor)) < 1)
									return r;
							}
							else if ((r=add_symbol(sl,ds,declor)) < 1) {
								return r;
							}

							do {
								if (tq_peek().type == token_type_t::closecurlybracket) {
									tq_discard();
									break;
								}

								if ((r=struct_declaration_parse(sl.sid,token_type_t::r_struct)) < 1)
									return r;
							} while(1);

							ds.type_identifier_symbol = sl.sid;
						}
						else if (declor.name != identifier_none) {
							sl.flags = symbol_t::FL_DECLARED;
							if ((r=prep_symbol_lookup(sl,ds,declor)) < 1)
								return r;
							if (do_local_symbol_lookup(sl,ds,declor)) {
								if ((r=check_symbol_lookup_match(sl,ds,declor)) < 1)
									return r;
							}
							else if ((r=add_symbol(sl,ds,declor)) < 1) {
								return r;
							}

							ds.type_identifier_symbol = sl.sid;
						}
					}
					continue;

				case token_type_t::r_union:
					ds.count++;

					if (ds.type_specifier & TS_UNION)
						CCERR_RET(EINVAL,pos,"declarator specifier 'union' already specified");

					ds.type_specifier |= TS_UNION;
					tq_discard();

					/* union { list }
					 * union identifier { list }
					 * union identifier */

					{
						declarator_t declor;
						symbol_lookup_t sl;

						if (tq_peek().type == token_type_t::identifier)
							identifier.assign(/*to*/declor.name,/*from*/tq_get().v.identifier);

						sl.pos = pos;
						sl.st = symbol_t::UNION;
						if (tq_peek().type == token_type_t::opencurlybracket) {
							tq_discard();

							if (!(declspec & DECLSPEC_ALLOW_DEF))
								CCERR_RET(EINVAL,pos,"not allowed to define types here");

							sl.flags = symbol_t::FL_DEFINED|symbol_t::FL_DECLARED;
							if ((r=prep_symbol_lookup(sl,ds,declor)) < 1)
								return r;
							if (do_local_symbol_lookup(sl,ds,declor)) {
								if ((r=check_symbol_lookup_match(sl,ds,declor)) < 1)
									return r;
							}
							else if ((r=add_symbol(sl,ds,declor)) < 1) {
								return r;
							}

							do {
								if (tq_peek().type == token_type_t::closecurlybracket) {
									tq_discard();
									break;
								}

								if ((r=struct_declaration_parse(sl.sid,token_type_t::r_union)) < 1)
									return r;
							} while(1);

							ds.type_identifier_symbol = sl.sid;
						}
						else if (declor.name != identifier_none) {
							sl.flags = symbol_t::FL_DECLARED;
							if ((r=prep_symbol_lookup(sl,ds,declor)) < 1)
								return r;
							if (do_local_symbol_lookup(sl,ds,declor)) {
								if ((r=check_symbol_lookup_match(sl,ds,declor)) < 1)
									return r;
							}
							else if ((r=add_symbol(sl,ds,declor)) < 1) {
								return r;
							}

							ds.type_identifier_symbol = sl.sid;
						}
					}
					continue;

				default: break;
			}
#undef XCHK
#undef X

			break;
		} while (1);

		/* unless told otherwise, it is an error for this code not to parse any tokens */
		if (!(declspec & DECLSPEC_OPTIONAL) && ds.count == 0)
			CCERR_RET(EINVAL,pos,"Declaration specifiers expected");

		/* unless asked not to parse type specifiers, it is an error not to specify one.
		 * You can't say "static x" for example */
		if (!(declspec & DECLSPEC_OPTIONAL) && ds.type_specifier == 0)
			CCERR_RET(EINVAL,pos,"Type specifiers expected. Specify a type here");

		/* sanity check */
		{
			const type_qualifier_t mm_t = ds.type_qualifier & (TQ_NEAR|TQ_FAR|TQ_HUGE); /* only one of */
			if (mm_t && !only_one_bit_set(mm_t))
				CCERR_RET(EINVAL,pos,"Multiple storage classes specified");

			const storage_class_t sc_t = ds.storage_class & (SC_TYPEDEF|SC_EXTERN|SC_STATIC|SC_AUTO|SC_REGISTER); /* only one of */
			if (sc_t && !only_one_bit_set(sc_t))
				CCERR_RET(EINVAL,pos,"Multiple storage classes specified");

			const storage_class_t ce_t = ds.storage_class & (SC_CONSTEXPR|SC_CONSTEVAL|SC_CONSTINIT); /* only one of */
			if (ce_t && !only_one_bit_set(ce_t))
				CCERR_RET(EINVAL,pos,"Multiple storage classes specified");
		}

		/* "long int" -> "long"
		 * "long long int" -> "long"
		 * "signed long long int" -> "signed long long"
		 * "short int" -> "short"
		 *
		 * The "int" part is redundant */
		{
			const type_specifier_t t = ds.type_specifier & ~(TS_SIGNED|TS_UNSIGNED);
			if (t == (TS_LONG|TS_INT) || t == (TS_LONGLONG|TS_INT) || t == (TS_SHORT|TS_INT))
				ds.type_specifier &= ~TS_INT;
		}

		/* skip type specifier checks if and only "long double" because "long double" would fail the below checks */
		if (ds.type_specifier != (TS_LONG|TS_DOUBLE)) {
			const type_specifier_t sign_t = ds.type_specifier & (TS_SIGNED|TS_UNSIGNED); /* only one of */
			if (sign_t && !only_one_bit_set(sign_t))
				CCERR_RET(EINVAL,pos,"Multiple type specifiers (signed/unsigned)");

			const type_specifier_t intlen_t = ds.type_specifier & (TS_VOID|TS_CHAR|TS_SHORT|TS_INT|TS_LONG|TS_LONGLONG|TS_ENUM|TS_STRUCT|TS_UNION|TS_MATCH_TYPEDEF|TS_MATCH_BUILTIN); /* only one of */
			if (intlen_t && !only_one_bit_set(intlen_t))
				CCERR_RET(EINVAL,pos,"Multiple type specifiers (int/char/void)");

			const type_specifier_t floattype_t = ds.type_specifier & (TS_FLOAT|TS_DOUBLE); /* only one of */
			if (floattype_t && !only_one_bit_set(floattype_t))
				CCERR_RET(EINVAL,pos,"Multiple type specifiers (float)");

			if (intlen_t && floattype_t)
				CCERR_RET(EINVAL,pos,"Multiple type specifiers (float+int)");

			if (floattype_t && sign_t)
				CCERR_RET(EINVAL,pos,"Multiple type specifiers (float+signed/unsigned)");
		}

		if (ds.type_specifier & TS_MATCH_BUILTIN) {
			if ((builtin_ts & (TS_UNSIGNED|TS_SIGNED)) == 0)
				builtin_ts |= ds.type_specifier & (TS_UNSIGNED|TS_SIGNED);

			ds.type_specifier = builtin_ts;
		}

#if 0//DEBUG
		fprintf(stderr,"DEBUG %s:%d:\n",__FUNCTION__,__LINE__);
		debug_dump_declaration_specifiers("  ",ds);
#endif

		return 1;
	}

	int cc_state_t::pointer_parse(std::vector<pointer_t> &ptr) {
		int r;

#if 0//DEBUG
		fprintf(stderr,"%s():parsing\n",__FUNCTION__);
#endif

		/* pointer handling
		 *
		 *   <nothing>
		 *   *
		 *   **
		 *   * const *
		 *   *** const * const
		 *
		 *   and so on */
		while (tq_peek().type == token_type_t::star) {
			declaration_specifiers_t ds;
			pointer_t p;

			tq_discard();

			if ((r=declaration_specifiers_parse(ds,DECLSPEC_OPTIONAL)) < 1)
				return r;

			assert(ds.type_specifier == 0 && ds.storage_class == 0);
			assert(ds.type_identifier_symbol == symbol_none);
			p.tq = ds.type_qualifier;

			ptr.push_back(std::move(p));
		}

		if (!ptr.empty()) {
#if 0//DEBUG
			fprintf(stderr,"DEBUG %s:%d:\n",__FUNCTION__,__LINE__);
			debug_dump_pointer("  ",ptr);
#endif
		}

		return 1;
	}

	int cc_state_t::direct_declarator_inner_parse(ddip_list_t &dp,declarator_t &dd,position_t &pos,unsigned int flags) {
		int r;

		/* [*] identifier [arraydefs]
		 * ( [*] identifier [arraydefs] )
		 * ( [*] identifier ) [arraydefs]
		 * ( [*] ( [*] identifier [arraydefs] ) )
		 * ( [*] ( [*] identifier ) [arraydefs] )
		 * ( [*] ( [*] identifier ) ) [arraydefs]
		 *
		 * This parses one level within parens.
		 *
		 * apparently postfix operators (arraydefs) within a layer take priority over the pointer.
		 *
		 * int *ar[4];     ar is an array of 4 pointers to int
		 * int (*ar)[4];   ar is a pointer to an array of 4 ints */
		ddip_list_t sdp;
		ddip_t tdp;

		if ((r=pointer_parse(tdp.ptr)) < 1)
			return r;

		if (tq_peek().type == token_type_t::openparenthesis) {
			tq_discard();

			/* WARNING: pushes to vector which causes reallocation which will make references to array elements stale */
			if ((r=direct_declarator_inner_parse(sdp,dd,pos,flags)) < 1)
				return r;

			if (tq_get().type != token_type_t::closeparenthesis)
				CCERR_RET(EINVAL,tq_peek().pos,"Closing parenthesis expected");
		}
		else {
			assert(dd.name == identifier_none);
			if (tq_peek().type == token_type_t::identifier && tq_peek().v.identifier != identifier_none) {
				if (flags & DIRDECL_NO_IDENTIFIER) CCERR_RET(EINVAL,tq_peek().pos,"Identifier not allowed here");
				identifier.assign(/*to*/dd.name,/*from*/tq_get().v.identifier);
			}
			else if (flags & DIRDECL_ALLOW_ABSTRACT) {
				dd.name = identifier_none;
			}
			else {
				CCERR_RET(EINVAL,tq_peek().pos,"Identifier expected");
			}
		}

		while (tq_peek().type == token_type_t::opensquarebracket) {
			tq_discard();

			/* NTS: "[]" is acceptable */
			ast_node_id_t expr = ast_node_none;
			if (tq_peek().type != token_type_t::closesquarebracket) {
				if ((r=conditional_expression(expr)) < 1)
					return r;

				ast_node_reduce(expr);
			}

			tdp.arraydef.push_back(std::move(expr));
			if (tq_get().type != token_type_t::closesquarebracket)
				CCERR_RET(EINVAL,tq_peek().pos,"Closing square bracket expected");
		}

		if (tq_peek().type == token_type_t::openparenthesis) {
			tq_discard();

			/* NTS: "()" is acceptable */
			tdp.dd_flags |= declarator_t::FL_FUNCTION;

			/* if we're at the () in (*function)() then this is a function pointer */
			if (sdp.size() == 1)
				if (!sdp[0].ptr.empty())
					tdp.dd_flags |= declarator_t::FL_FUNCTION_POINTER;

			if (tq_peek().type != token_type_t::closeparenthesis) {
				do {
					if (tq_peek().type == token_type_t::ellipsis) {
						tq_discard();

						tdp.dd_flags |= declarator_t::FL_ELLIPSIS;

						/* At least one paremter is required for ellipsis! */
						if (tdp.parameters.empty()) {
							CCerr(pos,"Variadic functions must have at least one named parameter");
							return errno_return(EINVAL);
						}

						break;
					}

					parameter_t p;

					if ((r=declaration_specifiers_parse(p.spec,DECLSPEC_OPTIONAL)) < 1)
						return r;

					if ((r=direct_declarator_parse(p.spec,p.decl,DIRDECL_ALLOW_ABSTRACT)) < 1)
						return r;

					/* do not allow using the same name again */
					if (p.decl.name != identifier_none) {
						for (const auto &chk_p : tdp.parameters) {
							if (chk_p.decl.name != identifier_none) {
								if (identifier(chk_p.decl.name) == identifier(p.decl.name)) {
									CCerr(pos,"Parameter '%s' already defined",identifier(p.decl.name).to_str().c_str());
									return errno_return(EEXIST);
								}
							}
						}

						if (tq_peek().type == token_type_t::equal) {
							/* if no declaration specifiers were given (just a bare identifier
							 * aka the old 1980s syntax), then you shouldn't be allowed to define
							 * a default value. */
							if (p.spec.empty())
								return errno_return(EINVAL);

							tq_discard();
							if ((r=initializer(p.decl.expr)) < 1)
								return r;
						}
					}

					tdp.parameters.push_back(std::move(p));
					if (tq_peek().type == token_type_t::comma) {
						tq_discard();
						continue;
					}

					break;
				} while (1);
			}

			if (tq_get().type != token_type_t::closeparenthesis)
				CCERR_RET(EINVAL,tq_peek().pos,"Expected closing parenthesis");
		}

		dp.addcombine(std::move(tdp));
		for (auto &s : sdp)
			dp.addcombine(std::move(s));

		return 1;
	}

	int cc_state_t::direct_declarator_parse(declaration_specifiers_t &ds,declarator_t &dd,unsigned int flags) {
		position_t pos = tq_peek().pos;
		int r;

		/* direct declarator
		 *
		 *   IDENTIFIER
		 *   ( declarator )
		 *
		 *   followed by
		 *
		 *   <nothing>
		 *   [ constant_expression ]
		 *   [ ]
		 *   ( parameter_type_list )
		 *   ( identifier_list )                      <- the old C function syntax you'd see from 1980s code
		 *   ( ) */

		/* if the declaration specifier is an enum, struct, or union, then you're allowed not to specify an identifier */
		if (ds.type_specifier & (TS_ENUM|TS_STRUCT|TS_UNION))
			flags |= DIRDECL_ALLOW_ABSTRACT;

		if ((r=direct_declarator_inner_parse(dd.ddip,dd,pos,flags)) < 1)
			return r;

		/* typedef subst */
		if (ds.type_specifier & TS_MATCH_TYPEDEF) {
			symbol_t &sym = symbol(ds.type_identifier_symbol);
			assert(sym.sym_type == symbol_t::TYPEDEF);

			/* do not replace storage_class */

			/* copy type_specifier */
			ds.type_specifier = sym.spec.type_specifier;

			/* merge type_qualifier */
			{
				const type_qualifier_t msk = TQ_CONST|TQ_VOLATILE|TQ_RESTRICT;
				ds.type_qualifier |= sym.spec.type_qualifier & msk;
			}
			{
				const type_qualifier_t msk = TQ_NEAR|TQ_FAR|TQ_HUGE;
				if ((ds.type_qualifier&msk) == 0)
					ds.type_qualifier |= sym.spec.type_qualifier & msk;
			}

			ds.type_identifier_symbol = sym.spec.type_identifier_symbol;

			for (auto &tde : sym.ddip)
				dd.ddip.addcombine(tde);
		}

		std::vector<parameter_t> *parameters = NULL;
		{
			ddip_t *f = dd.ddip.funcparampair();
			if (f) {
				dd.flags |= f->dd_flags;
				parameters = &f->parameters;
			}
		}

		/* parameter validation:
		 * you can have either all parameter_type parameters,
		 * or identifier_list parameters.
		 * Do not allow a mix of them. */
		if (dd.flags & declarator_t::FL_FUNCTION) {
			int type = -1; /* -1 = no spec  0 = old identifier only  1 = new parameter type */
			int cls;

			assert(parameters != NULL);
			for (const auto &p : *parameters) {
				if (p.spec.empty())
					cls = 0;
				else
					cls = 1;

				if (type < 0) {
					type = cls;
				}
				else if (type != cls) {
					CCerr(pos,"Mixed parameter style not allowed");
					return errno_return(EINVAL);
				}
			}

			/* if old-school identifier only, then after the parenthesis there will be declarations of the parameters i.e.
			 *
			 * int func(a,b,c)
			 *   int a;
			 *   float b;
			 *   const char *c;
			 * {
			 * }
			 *
			 * apparently GCC also allows:
			 *
			 * int func(a,b,c);
			 *
			 * int func(a,b,c)
			 *   int a;
			 *   int b;
			 *   int c;
			 * {
			 * }
			 *
			 * However that's pretty useless and we're not going to allow that.
			 */
			if (type == 0) {
				do {
					if (tq_peek().type == token_type_t::opencurlybracket || tq_peek().type == token_type_t::semicolon) {
						break;
					}
					else {
						declaration_specifiers_t s_spec;

						if ((r=declaration_specifiers_parse(s_spec)) < 1)
							return r;
						if ((r=chkerr()) < 1)
							return r;

						do {
							declarator_t d;

							if ((r=declaration_inner_parse(s_spec,d)) < 1)
								return r;

							/* check */
							{
								/* the name must match a parameter and it must not already have been given it's type */
								size_t i=0;
								while (i < (*parameters).size()) {
									parameter_t &chk_p = (*parameters)[i];

									if (identifier(d.name) == identifier(chk_p.decl.name))
										break;

									i++;
								}

								/* no match---fail */
								if (i == (*parameters).size()) {
									CCerr(pos,"No such parameter '%s' in identifier list",csliteral(d.name).makestring().c_str());
									return errno_return(ENOENT);
								}

								parameter_t &fp = (*parameters)[i];
								if (fp.spec.empty() && fp.decl.ddip.empty()) {
									fp.spec = s_spec;
									fp.decl.ddip = std::move(d.ddip);
								}
								else {
									CCerr(pos,"Identifier already given type");
									return errno_return(EALREADY);
								}

								/* do not allow initializer for parameter type declaration */
								if (d.expr != ast_node_none) {
									CCerr(pos,"Initializer value not permitted here");
									return errno_return(EINVAL);
								}
							}

							if (tq_peek().type == token_type_t::comma) {
								tq_discard();
								continue;
							}

							break;
						} while(1);

						if (tq_peek().type == token_type_t::semicolon) {
							tq_discard();
							continue;
						}

						break;
					}
				} while (1);

				if (tq_peek().type != token_type_t::opencurlybracket && !(*parameters).empty()) {
					/* no body of the function? */
					CCerr(pos,"Identifier-only parameter list only permitted if the function has a body");
					return errno_return(EINVAL);
				}
			}
		}

#if 0//DEBUG
		fprintf(stderr,"DEBUG %s:%d:\n",__FUNCTION__,__LINE__);
		debug_dump_direct_declarator("  ",dd);
#endif

		if (dd.flags & declarator_t::FL_FUNCTION) {
			/* any parameter not yet described, is an error */
			assert(parameters != NULL);
			for (auto &p : *parameters) {
				if (p.spec.empty()) {
					CCerr(pos,"Parameter '%s' is missing type",identifier(p.decl.name).to_str().c_str());
					debug_dump_parameter("  ",p);
					return errno_return(EINVAL);
				}
			}
		}

		return 1;
	}

	int cc_state_t::declarator_parse(declaration_specifiers_t &ds,declarator_t &declor) {
		int r;

		if ((r=direct_declarator_parse(ds,declor)) < 1)
			return r;

		return 1;
	}

	int cc_state_t::struct_bitfield_validate(token_t &t,bool first_of_range) {
		if (t.type == token_type_t::integer) {
			if (t.v.integer.v.v < (first_of_range ? 0ll : 1ll) || t.v.integer.v.v > 255ll)
				CCERR_RET(EINVAL,tq_peek().pos,"Bitfield value out of range");
		}
		else {
			CCERR_RET(EINVAL,tq_peek().pos,"Bitfield range is not a constant integer expression");
		}

		return 1;
	}

	int cc_state_t::struct_declarator_parse(const symbol_id_t sid,declaration_specifiers_t &ds,declarator_t &declor) {
		int r;

		if ((r=direct_declarator_parse(ds,declor)) < 1)
			return r;

		if (tq_peek().type == token_type_t::colon) {
			tq_discard();

			/* This compiler extension: Take the GCC range syntax and make our own
			 * extension that allows specifying the explicit range of bits that make
			 * up the bitfield, rather than just a length and having to count bits.
			 *
			 * [expr]     1-bit field at bit number (expr)
			 * [a ... b]  bit field starting at a, extends to b inclusive.
			 *            For example [ 4 ... 7 ] means a 4-bit field, LSB at bit 4,
			 *            MSB at bit 7. */
			if (tq_peek().type == token_type_t::opensquarebracket) {
				tq_discard();

				declor.bitfield_expr = ast_node.alloc(token_type_t::op_bitfield_range);

				{
					ast_node_id_t expr = ast_node_none;
					if ((r=conditional_expression(expr)) < 1)
						return r;

					ast_node_reduce(expr);
					ast_node(declor.bitfield_expr).set_child(expr); ast_node(expr).release();

					if (tq_peek().type == token_type_t::ellipsis) {
						tq_discard();

						ast_node_id_t expr2 = ast_node_none;
						if ((r=conditional_expression(expr2)) < 1)
							return r;

						ast_node_reduce(expr2);
						ast_node(expr).set_next(expr2); ast_node(expr2).release();

						if ((r=struct_bitfield_validate(ast_node(expr).t,true)) < 1)
							return r;
						if ((r=struct_bitfield_validate(ast_node(expr2).t)) < 1)
							return r;
					}
					else {
						if ((r=struct_bitfield_validate(ast_node(expr).t)) < 1)
							return r;
					}
				}

				if (tq_get().type != token_type_t::closesquarebracket)
					CCERR_RET(EINVAL,tq_peek().pos,"Closing square bracket expected");
			}
			else {
				if ((r=conditional_expression(declor.bitfield_expr)) < 1)
					return r;

				ast_node_reduce(declor.bitfield_expr);

				if ((r=struct_bitfield_validate(ast_node(declor.bitfield_expr).t)) < 1)
					return r;
			}
		}

		/* No, you cannot typedef wihtin a C struct (but you can a C++ struct) */
		if (ds.storage_class & SC_TYPEDEF)
			CCERR_RET(EINVAL,tq_peek().pos,"Cannot typedef in a struct or union");

		/* anon enums and unions are OK, GCC allows it */
		if (declor.name == identifier_none) {
			if (ds.type_specifier & TS_ENUM)
				return 1;
			else if (ds.type_specifier & TS_UNION)
				{ /* None. Later stages of struct/union handling will merge anon union fields in and detect conflicts later. */ }
			else
				CCERR_RET(EINVAL,tq_peek().pos,"Anonymous struct field not allowed here");
		}

		symbol_t &sym = symbol(sid);

		if (sym.identifier_exists(declor.name))
			CCERR_RET(EEXIST,tq_peek().pos,"Struct/union field already defined");

		const size_t sfi = sym.fields.size();
		sym.fields.resize(sfi + 1u);
		structfield_t &sf = sym.fields[sfi];
		sf.spec = ds;
		sf.ddip = declor.ddip;
		identifier.assign(/*to*/sf.name,/*from*/declor.name);
		ast_node.assign(/*to*/sf.bitfield_expr,/*from*/declor.bitfield_expr);
		return 1;
	}

	void cc_state_t::debug_dump_enumerator(const std::string prefix,enumerator_t &en) {
		fprintf(stderr,"%s",prefix.c_str());

		if (en.name != identifier_none) fprintf(stderr,"'%s'",identifier(en.name).to_str().c_str());
		else fprintf(stderr,"?");
		fprintf(stderr,"\n");

		if (en.expr != ast_node_none) {
			fprintf(stderr,"%s  expr:\n",prefix.c_str());
			debug_dump_ast(prefix+"    ",en.expr);
		}
	}

	void cc_state_t::debug_dump_declaration_specifiers(const std::string prefix,declaration_specifiers_t &ds) {
		if (ds.empty()) return;

		fprintf(stderr,"%sdeclaration_specifiers:",prefix.c_str());
		for (unsigned int i=0;i < SCI__MAX;i++) { if (ds.storage_class&(1u<<i)) fprintf(stderr," %s",storage_class_idx_t_str[i]); }
		for (unsigned int i=0;i < TSI__MAX;i++) { if (ds.type_specifier&(1u<<i)) fprintf(stderr," %s",type_specifier_idx_t_str[i]); }
		for (unsigned int i=0;i < TQI__MAX;i++) { if (ds.type_qualifier&(1u<<i)) fprintf(stderr," %s",type_qualifier_idx_t_str[i]); }
		if (ds.type_identifier_symbol != symbol_none) {
			symbol_t &sym = symbol(ds.type_identifier_symbol);
			fprintf(stderr," symbol#%lu",(unsigned long)ds.type_identifier_symbol);
			if (sym.name != identifier_none)
				fprintf(stderr," '%s'",identifier(sym.name).to_str().c_str());
			else
				fprintf(stderr," <anon>");
		}
		fprintf(stderr,"\n");

		if (ds.align != addrmask_none)
			fprintf(stderr,"%s  alignment: 0x%llx (%llu)\n",prefix.c_str(),(unsigned long long)(~ds.align) + 1ull,(unsigned long long)(~ds.align) + 1ull);

		if (ds.size != data_size_none)
			fprintf(stderr,"%s  size: 0x%llx (%llu)\n",prefix.c_str(),(unsigned long long)ds.size,(unsigned long long)ds.size);

		if (!ds.enum_list.empty()) {
			fprintf(stderr,"%s  enum_list:\n",prefix.c_str());
			for (auto &sid : ds.enum_list)
				debug_dump_symbol(prefix+"    ",symbol(sid));
		}
	}

	void cc_state_t::debug_dump_pointer(const std::string prefix,std::vector<pointer_t> &ptr,const std::string &name) {
		if (ptr.empty()) return;

		fprintf(stderr,"%s%s%spointer(s):",prefix.c_str(),name.c_str(),name.empty()?"":" ");
		for (auto i=ptr.begin();i!=ptr.end();i++) {
			fprintf(stderr," *");
			for (unsigned int x=0;x < TQI__MAX;x++) { if ((*i).tq&(1u<<x)) fprintf(stderr," %s",type_qualifier_idx_t_str[x]); }
		}
		fprintf(stderr,"\n");
	}

	void cc_state_t::debug_dump_arraydef(const std::string prefix,std::vector<ast_node_id_t> &arraydef,const std::string &name) {
		if (arraydef.empty()) return;

		fprintf(stderr,"%s%s%sarraydef(s):\n",prefix.c_str(),name.c_str(),name.empty()?"":" ");
		for (const auto &expr : arraydef) {
			if (expr != ast_node_none) debug_dump_ast(prefix+"  ",expr);
			else fprintf(stderr,"%s  <none>\n",prefix.c_str());
		}
	}

	void cc_state_t::debug_dump_parameter(const std::string prefix,parameter_t &p,const std::string &name) {
		fprintf(stderr,"%s%s%sparameter:\n",prefix.c_str(),name.c_str(),name.empty()?"":" ");
		debug_dump_declaration_specifiers(prefix+"  ",p.spec);

		if (p.decl.name != identifier_none)
			fprintf(stderr,"%s  identifier: '%s'\n",prefix.c_str(),identifier(p.decl.name).to_str().c_str());

		if (p.decl.symbol != symbol_none)
			fprintf(stderr,"%s  symbol: #%lu\n",prefix.c_str(),(unsigned long)p.decl.symbol);

		debug_dump_ddip(prefix+"  ",p.decl.ddip);

		if (p.decl.expr != ast_node_none) {
			fprintf(stderr,"%s  expr:\n",prefix.c_str());
			debug_dump_ast(prefix+"    ",p.decl.expr);
		}
	}

	void cc_state_t::debug_dump_ddip(const std::string prefix,ddip_t &ddip,const std::string &name) {
		fprintf(stderr,"%s%s%sptr/array pair:\n",prefix.c_str(),name.c_str(),name.empty()?"":" ");
		debug_dump_pointer(prefix+"  ",ddip.ptr);
		debug_dump_arraydef(prefix+"  ",ddip.arraydef);

		if (ddip.dd_flags & declarator_t::FL_FUNCTION_POINTER)
			fprintf(stderr,"%s  function pointer:\n",prefix.c_str());
		else if (ddip.dd_flags & declarator_t::FL_FUNCTION)
			fprintf(stderr,"%s  function:\n",prefix.c_str());

		if (ddip.dd_flags & declarator_t::FL_ELLIPSIS)
			fprintf(stderr,"%s  parameter ... (ellipsis)\n",prefix.c_str());

		for (auto &ppp : ddip.parameters)
			debug_dump_parameter(prefix+"  ",ppp);
	}

	void cc_state_t::debug_dump_ddip(const std::string prefix,ddip_list_t &ddip,const std::string &name) {
		if (ddip.empty())
			return;

		fprintf(stderr,"%s%s%sptr/array pairs:\n",prefix.c_str(),name.c_str(),name.empty()?"":" ");

		if (!ddip.empty())
			fprintf(stderr,"%s  representation: %s\n",prefix.c_str(),ddip_list_to_str(ddip).c_str());

		for (auto &sdip : ddip)
			debug_dump_ddip(prefix+"  ",sdip);
	}

	void cc_state_t::debug_dump_direct_declarator(const std::string prefix,declarator_t &ddecl,const std::string &name) {
		if (ddecl.flags & declarator_t::FL_FUNCTION_POINTER)
			fprintf(stderr,"%s%s%sfunction pointer direct declarator:\n",prefix.c_str(),name.c_str(),name.empty()?"":" ");
		else if (ddecl.flags & declarator_t::FL_FUNCTION)
			fprintf(stderr,"%s%s%sfunction direct declarator:\n",prefix.c_str(),name.c_str(),name.empty()?"":" ");
		else
			fprintf(stderr,"%s%s%sdirect declarator:\n",prefix.c_str(),name.c_str(),name.empty()?"":" ");

		if (ddecl.name != identifier_none)
			fprintf(stderr,"%s  identifier: '%s'\n",prefix.c_str(),identifier(ddecl.name).to_str().c_str());

		if (ddecl.symbol != symbol_none)
			fprintf(stderr,"%s  symbol: #%lu\n",prefix.c_str(),(unsigned long)ddecl.symbol);

		debug_dump_ddip(prefix+"  ",ddecl.ddip);

		if (ddecl.flags & declarator_t::FL_ELLIPSIS)
			fprintf(stderr,"%s  parameter ... (ellipsis)\n",prefix.c_str());
	}

	void cc_state_t::debug_dump_declarator(const std::string prefix,declarator_t &declr,const std::string &name) {
		if (declr.flags & declarator_t::FL_FUNCTION_POINTER)
			fprintf(stderr,"%s%s%sfunction pointer declarator:\n",prefix.c_str(),name.c_str(),name.empty()?"":" ");
		else if (declr.flags & declarator_t::FL_FUNCTION)
			fprintf(stderr,"%s%s%sfunction declarator:\n",prefix.c_str(),name.c_str(),name.empty()?"":" ");
		else
			fprintf(stderr,"%s%s%sdeclarator:\n",prefix.c_str(),name.c_str(),name.empty()?"":" ");

		debug_dump_direct_declarator(prefix+"  ",declr);

		if (declr.expr != ast_node_none) {
			fprintf(stderr,"%s  expr:\n",prefix.c_str());
			debug_dump_ast(prefix+"    ",declr.expr);
		}

		if (declr.bitfield_expr != ast_node_none) {
			fprintf(stderr,"%s  bitfield:\n",prefix.c_str());
			debug_dump_ast(prefix+"    ",declr.bitfield_expr);
		}
	}

	void cc_state_t::debug_dump_declaration(const std::string prefix,declaration_t &decl,const std::string &name) {
		fprintf(stderr,"%s%s%sdeclaration:{\n",prefix.c_str(),name.c_str(),name.empty()?"":" ");
		debug_dump_declaration_specifiers(prefix+"  ",decl.spec);

		for (auto &declr : decl.declor)
			debug_dump_declarator(prefix+"  ",declr);

		fprintf(stderr,"%s}\n",prefix.c_str());
	}

	void cc_state_t::debug_dump_ast(const std::string prefix,ast_node_id_t r) {
		unsigned int count = 0;

		while (r != ast_node_none) {
			const auto &n = ast_node(r);
			fprintf(stderr,"%s%s[%u]\n",prefix.c_str(),n.t.to_str().c_str(),count);

			if (n.t.type == token_type_t::op_declaration) {
				if (n.t.v.declaration)
					debug_dump_declaration(prefix+"  ",*n.t.v.declaration);
			}
			else if (n.t.type == token_type_t::op_symbol) {
				if (n.t.v.symbol != symbol_none) {
					symbol_t &sym = symbol(n.t.v.symbol);
					fprintf(stderr,"%s  identifier: ",prefix.c_str());
					if (sym.name != identifier_none) fprintf(stderr,"%s\n",identifier(sym.name).to_str().c_str());
					else fprintf(stderr,"<anon>\n");
				}
			}

			debug_dump_ast(prefix+"  ",n.child);
			r = n.next;
			count++;
		}
	}

	void cc_state_t::debug_dump_scope(const std::string prefix,scope_t &sco,const std::string &name) {
		fprintf(stderr,"%s%s%sscope:\n",prefix.c_str(),name.c_str(),name.empty()?"":" ");

		for (auto sid : sco.symbols) {
			symbol_t &sym = symbol(sid);
			debug_dump_symbol(prefix+"  ",sym);
		}

		for (auto &decl : sco.localdecl) {
			fprintf(stderr,"%s  decl:\n",prefix.c_str());
			debug_dump_declaration_specifiers(prefix+"    ",decl.spec);
			debug_dump_declarator(prefix+"    ",decl.declor);
		}

		if (sco.root != ast_node_none) {
			fprintf(stderr,"%s  expr:\n",prefix.c_str());
			debug_dump_ast(prefix+"    ",sco.root);
		}
	}

	void cc_state_t::debug_dump_scope_table(const std::string prefix,const std::string &name) {
		fprintf(stderr,"%s%s%sscope table:\n",prefix.c_str(),name.c_str(),name.empty()?"":" ");
		for (auto si=scopes.begin();si!=scopes.end();si++)
			debug_dump_scope(prefix+"  ",*si,std::string("#")+std::to_string((unsigned int)(si-scopes.begin())));
	}

	void cc_state_t::debug_dump_structfield(const std::string prefix,structfield_t &field,const std::string &name) {
		fprintf(stderr,"%s%s%sfield:",prefix.c_str(),name.c_str(),name.empty()?"":" ");
		if (field.name != identifier_none) fprintf(stderr," '%s'",identifier(field.name).to_str().c_str());
		else fprintf(stderr," <anon>");
		fprintf(stderr,"\n");

		debug_dump_declaration_specifiers(prefix+"  ",field.spec);
		debug_dump_ddip(prefix+"  ",field.ddip);

		if (field.bitfield_expr != ast_node_none) {
			fprintf(stderr,"%s  bitfield expr:\n",prefix.c_str());
			debug_dump_ast(prefix+"    ",field.bitfield_expr);
		}
	}

	void cc_state_t::debug_dump_symbol(const std::string prefix,symbol_t &sym,const std::string &name) {
		if (sym.sym_type == cc_state_t::symbol_t::NONE)
			return;

		fprintf(stderr,"%s%s%ssymbol#%lu",prefix.c_str(),name.c_str(),name.empty()?"":" ",size_t(&sym-&symbols[0]));
		switch (sym.sym_type) {
			case cc_state_t::symbol_t::VARIABLE: fprintf(stderr," variable"); break;
			case cc_state_t::symbol_t::FUNCTION: fprintf(stderr," function"); break;
			case cc_state_t::symbol_t::TYPEDEF: fprintf(stderr," typedef"); break;
			case cc_state_t::symbol_t::STRUCT: fprintf(stderr," struct"); break;
			case cc_state_t::symbol_t::UNION: fprintf(stderr," union"); break;
			case cc_state_t::symbol_t::CONST: fprintf(stderr," const"); break;
			case cc_state_t::symbol_t::ENUM: fprintf(stderr," enum"); break;
			default: break;
		};
		if (sym.name != identifier_none) fprintf(stderr," '%s'",identifier(sym.name).to_str().c_str());
		else fprintf(stderr," <anon>");
		if (sym.flags & cc_state_t::symbol_t::FL_DEFINED) fprintf(stderr," DEFINED");
		if (sym.flags & cc_state_t::symbol_t::FL_DECLARED) fprintf(stderr," DECLARED");
		if (sym.flags & cc_state_t::symbol_t::FL_PARAMETER) fprintf(stderr," PARAM");
		if (sym.flags & cc_state_t::symbol_t::FL_STACK) fprintf(stderr," STACK");
		if (sym.flags & cc_state_t::symbol_t::FL_ELLIPSIS) fprintf(stderr," ELLIPSIS");
		if (sym.flags & cc_state_t::symbol_t::FL_FUNCTION_POINTER) fprintf(stderr," FUNCTION-POINTER");

		if (sym.scope == scope_none) fprintf(stderr," scope:none");
		else if (sym.scope == scope_global) fprintf(stderr," scope:global");
		else fprintf(stderr," scope:%lu",(unsigned long)sym.scope);

		if (sym.parent_of_scope != scope_none) fprintf(stderr," parent-of-scope:%lu",(unsigned long)sym.parent_of_scope);

		fprintf(stderr,"\n");

		debug_dump_declaration_specifiers(prefix+"  ",sym.spec);
		debug_dump_ddip(prefix+"  ",sym.ddip);

		for (auto &f : sym.fields)
			debug_dump_structfield(prefix+"  ",f);

		if (sym.parent_of_scope != scope_none) {
			scope_t &sco = scope(sym.parent_of_scope);
			for (auto &decl : sco.localdecl) {
				fprintf(stderr,"%s  decl (as parent of scope):\n",prefix.c_str());
				debug_dump_declaration_specifiers(prefix+"    ",decl.spec);
				debug_dump_declarator(prefix+"    ",decl.declor);
			}
		}

		if (sym.expr != ast_node_none) {
			fprintf(stderr,"%s  expr:\n",prefix.c_str());
			debug_dump_ast(prefix+"    ",sym.expr);
		}
	}

	void cc_state_t::debug_dump_symbol_table(const std::string prefix,const std::string &name) {
		fprintf(stderr,"%s%s%ssymbol table:\n",prefix.c_str(),name.c_str(),name.empty()?"":" ");
		for (auto &symbol : symbols) debug_dump_symbol(prefix+"  ",symbol);
	}

	int cc_state_t::primary_expression(ast_node_id_t &aroot) {
		int r;

		if (	tq_peek().type == token_type_t::strliteral ||
			tq_peek().type == token_type_t::charliteral ||
			tq_peek().type == token_type_t::integer ||
			tq_peek().type == token_type_t::floating) {
			assert(aroot == ast_node_none);
			aroot = ast_node.alloc(tq_get());
		}
		else if (tq_peek().type == token_type_t::identifier) {
			/* if we can make it a symbol ref, do it */
			symbol_id_t sid;

			assert(aroot == ast_node_none);
			if ((sid=lookup_symbol(tq_peek().v.identifier,symbol_t::VARIABLE)) != symbol_none) {
				token_t srt = std::move(tq_get());
				srt.clear_v();
				srt.type = token_type_t::op_symbol;
				srt.v.symbol = sid;
				aroot = ast_node.alloc(srt);
			}
			else {
// If it's not in the symbol table, it's unknown, not defined
// aroot = ast_node.alloc(tq_get());
				if (tq_peek().v.identifier != identifier_none)
					CCERR_RET(ENOENT,tq_peek().pos,"'%s' not declared in this scope",identifier(tq_peek().v.identifier).to_str().c_str());
				else
					CCERR_RET(ENOENT,tq_peek().pos,"<anon> not declared in this scope");
			}
		}
		else if (tq_peek().type == token_type_t::openparenthesis) {
			tq_discard();

			assert(aroot == ast_node_none);
			if ((r=expression(aroot)) < 1)
				return r;

			if (tq_get().type != token_type_t::closeparenthesis)
				CCERR_RET(EINVAL,tq_peek().pos,"Subexpression is missing closing parenthesis");
		}
		else {
			CCERR_RET(EINVAL,tq_peek().pos,"Expected primary expression");
		}

		return 1;
	}

	int cc_state_t::postfix_expression(ast_node_id_t &aroot) {
#define nextexpr primary_expression
		int r;

		if ((r=nextexpr(aroot)) < 1)
			return r;

		do {
			if (tq_peek().type == token_type_t::openparenthesis) {
				tq_discard();

				ast_node_id_t expr = aroot; aroot = ast_node_none;

				aroot = ast_node.alloc(token_type_t::op_function_call);
				ast_node(aroot).set_child(expr); ast_node(expr).release();

				do {
					if (tq_peek().type == token_type_t::closeparenthesis)
						break;

					ast_node_id_t nexpr = ast_node_none;
					if ((r=assignment_expression(nexpr)) < 1)
						return r;

					ast_node(expr).set_next(nexpr); ast_node(nexpr).release();
					expr = nexpr;

					if (tq_peek().type == token_type_t::closeparenthesis) {
						break;
					}
					else if (tq_peek().type == token_type_t::comma) {
						tq_discard();
						continue;
					}
					else {
						return errno_return(EINVAL);
					}
				} while (1);

				if (tq_get().type != token_type_t::closeparenthesis)
					return errno_return(EINVAL);
			}
			else if (tq_peek().type == token_type_t::opensquarebracket) {
				tq_discard();

				ast_node_id_t expr1 = aroot; aroot = ast_node_none;

				aroot = ast_node.alloc(token_type_t::op_array_ref);
				ast_node(aroot).set_child(expr1); ast_node(expr1).release();

				ast_node_id_t expr2 = ast_node_none;
				if ((r=expression(expr2)) < 1)
					return r;

				ast_node(expr1).set_next(expr2); ast_node(expr2).release();

				if (tq_get().type != token_type_t::closesquarebracket)
					return errno_return(EINVAL);
			}
			else if (tq_peek().type == token_type_t::period) {
				tq_discard();

				ast_node_id_t expr1 = aroot; aroot = ast_node_none;

				aroot = ast_node.alloc(token_type_t::op_member_ref);
				ast_node(aroot).set_child(expr1); ast_node(expr1).release();

				ast_node_id_t expr2 = ast_node_none;
				if ((r=postfix_expression(expr2)) < 1)
					return r;

				ast_node(expr1).set_next(expr2); ast_node(expr2).release();
			}
			else if (tq_peek().type == token_type_t::minusrightanglebracket) {
				tq_discard();

				ast_node_id_t expr1 = aroot; aroot = ast_node_none;

				aroot = ast_node.alloc(token_type_t::op_ptr_ref);
				ast_node(aroot).set_child(expr1); ast_node(expr1).release();

				ast_node_id_t expr2 = ast_node_none;
				if ((r=postfix_expression(expr2)) < 1)
					return r;

				ast_node(expr1).set_next(expr2); ast_node(expr2).release();
			}
			else if (tq_peek().type == token_type_t::plusplus) {
				tq_discard();

				ast_node_id_t expr1 = aroot; aroot = ast_node_none;

				aroot = ast_node.alloc(token_type_t::op_post_increment);
				ast_node(aroot).set_child(expr1); ast_node(expr1).release();
			}
			else if (tq_peek().type == token_type_t::minusminus) {
				tq_discard();

				ast_node_id_t expr1 = aroot; aroot = ast_node_none;

				aroot = ast_node.alloc(token_type_t::op_post_decrement);
				ast_node(aroot).set_child(expr1); ast_node(expr1).release();
			}
			else {
				break;
			}
		} while (1);
#undef nextexpr
		return 1;
	}

	int cc_state_t::typeid_or_expr_parse(ast_node_id_t &aroot) {
		bool typespec = false;
		int depth = 0;
		int r;

		if (tq_peek().type == token_type_t::openparenthesis) {
			tq_discard();
			depth++;
		}

		if (depth == 1)
			typespec = declaration_specifiers_check();

		if (typespec) {
			std::unique_ptr<declaration_t> declion(new declaration_t);

			if ((r=chkerr()) < 1)
				return r;
			if ((r=declaration_specifiers_parse((*declion).spec)) < 1)
				return r;

			declarator_t &declor = (*declion).new_declarator();

			if ((r=direct_declarator_parse((*declion).spec,declor,DIRDECL_ALLOW_ABSTRACT|DIRDECL_NO_IDENTIFIER)) < 1)
				return r;

			ast_node_id_t decl = ast_node.alloc(token_type_t::op_declaration);
			assert(ast_node(decl).t.type == token_type_t::op_declaration);
			ast_node(decl).t.v.declaration = declion.release();

			if (aroot == ast_node_none) {
				aroot = decl;
			}
			else {
				ast_node(aroot).set_child(decl); ast_node(decl).release();
			}
		}
		else {
			ast_node_id_t expr = ast_node_none;
			if ((r=unary_expression(expr)) < 1)
				return r;

			if (aroot == ast_node_none) {
				aroot = expr;
			}
			else {
				ast_node(aroot).set_child(expr); ast_node(expr).release();
			}
		}

		while (depth > 0 && tq_peek().type == token_type_t::closeparenthesis) {
			tq_discard();
			depth--;
		}

		return 1;
	}

	int cc_state_t::unary_expression(ast_node_id_t &aroot) {
#define nextexpr postfix_expression
		int r;

		if (tq_peek().type == token_type_t::plusplus) {
			tq_discard();

			assert(aroot == ast_node_none);
			aroot = ast_node.alloc(token_type_t::op_pre_increment);

			ast_node_id_t expr = ast_node_none;
			if ((r=unary_expression(expr)) < 1)
				return r;

			ast_node(aroot).set_child(expr); ast_node(expr).release();
		}
		else if (tq_peek().type == token_type_t::minusminus) {
			tq_discard();

			assert(aroot == ast_node_none);
			aroot = ast_node.alloc(token_type_t::op_pre_decrement);

			ast_node_id_t expr = ast_node_none;
			if ((r=unary_expression(expr)) < 1)
				return r;

			ast_node(aroot).set_child(expr); ast_node(expr).release();
		}
		else if (tq_peek().type == token_type_t::ampersand) {
			tq_discard();

			assert(aroot == ast_node_none);
			aroot = ast_node.alloc(token_type_t::op_address_of);

			ast_node_id_t expr = ast_node_none;
			if ((r=cast_expression(expr)) < 1)
				return r;

			ast_node(aroot).set_child(expr); ast_node(expr).release();
		}
		else if (tq_peek().type == token_type_t::ampersandampersand) {
			tq_discard();

			assert(aroot == ast_node_none);
			aroot = ast_node.alloc(token_type_t::op_address_of);

			ast_node_id_t aroot2 = ast_node.alloc(token_type_t::op_address_of);

			ast_node_id_t expr = ast_node_none;
			if ((r=cast_expression(expr)) < 1)
				return r;

			ast_node(aroot).set_child(aroot2); ast_node(aroot2).release();
			ast_node(aroot2).set_child(expr); ast_node(expr).release();
		}
		else if (tq_peek().type == token_type_t::star) {
			tq_discard();

			assert(aroot == ast_node_none);
			aroot = ast_node.alloc(token_type_t::op_pointer_deref);

			ast_node_id_t expr = ast_node_none;
			if ((r=cast_expression(expr)) < 1)
				return r;

			ast_node(aroot).set_child(expr); ast_node(expr).release();
		}
		else if (tq_peek().type == token_type_t::plus) {
			tq_discard();

			/* So basically +4, right? Which we can just treat as a no-op */
			if ((r=cast_expression(aroot)) < 1)
				return r;
		}
		else if (tq_peek().type == token_type_t::minus) {
			tq_discard();

			assert(aroot == ast_node_none);
			aroot = ast_node.alloc(token_type_t::op_negate);

			ast_node_id_t expr = ast_node_none;
			if ((r=cast_expression(expr)) < 1)
				return r;

			ast_node(aroot).set_child(expr); ast_node(expr).release();
		}
		else if (tq_peek().type == token_type_t::tilde) {
			tq_discard();

			assert(aroot == ast_node_none);
			aroot = ast_node.alloc(token_type_t::op_binary_not);

			ast_node_id_t expr = ast_node_none;
			if ((r=cast_expression(expr)) < 1)
				return r;

			ast_node(aroot).set_child(expr); ast_node(expr).release();
		}
		else if (tq_peek().type == token_type_t::exclamation) {
			tq_discard();

			assert(aroot == ast_node_none);
			aroot = ast_node.alloc(token_type_t::op_logical_not);

			ast_node_id_t expr = ast_node_none;
			if ((r=cast_expression(expr)) < 1)
				return r;

			ast_node(aroot).set_child(expr); ast_node(expr).release();
		}
		else if (tq_peek().type == token_type_t::r_sizeof) {
			tq_discard();

			assert(aroot == ast_node_none);
			aroot = ast_node.alloc(token_type_t::op_sizeof);

			if ((r=typeid_or_expr_parse(aroot)) < 1)
				return r;
		}
		else if (tq_peek().type == token_type_t::r_alignof) {

			tq_discard();

			assert(aroot == ast_node_none);
			aroot = ast_node.alloc(token_type_t::op_alignof);

			if ((r=typeid_or_expr_parse(aroot)) < 1)
				return r;
		}
		else {
			if ((r=nextexpr(aroot)) < 1)
				return r;
		}
#undef nextexpr
		return 1;
	}

	int cc_state_t::cast_expression(ast_node_id_t &aroot) {
#define nextexpr unary_expression
		int r;

		if (tq_peek(0).type == token_type_t::openparenthesis && declaration_specifiers_check(1)) {
			tq_discard(); /* toss the open parenthesis */

			std::unique_ptr<declaration_t> declion(new declaration_t);

			if ((r=chkerr()) < 1)
				return r;
			if ((r=declaration_specifiers_parse((*declion).spec)) < 1)
				return r;

			declarator_t &declor = (*declion).new_declarator();

			if ((r=direct_declarator_parse((*declion).spec,declor,DIRDECL_ALLOW_ABSTRACT|DIRDECL_NO_IDENTIFIER)) < 1)
				return r;

			aroot = ast_node.alloc(token_type_t::op_typecast);

			ast_node_id_t decl = ast_node.alloc(token_type_t::op_declaration);
			assert(ast_node(decl).t.type == token_type_t::op_declaration);
			ast_node(decl).t.v.declaration = declion.release();

			ast_node(aroot).set_child(decl); ast_node(decl).release();

			if (tq_get().type != token_type_t::closeparenthesis)
				return errno_return(EINVAL);

			ast_node_id_t nxt = ast_node_none;
			if ((r=cast_expression(nxt)) < 1)
				return r;

			ast_node(decl).set_next(nxt); ast_node(nxt).release();
			return 1;
		}

		if ((r=nextexpr(aroot)) < 1)
			return r;

#undef nextexpr
		return 1;
	}

	int cc_state_t::multiplicative_expression(ast_node_id_t &aroot) {
#define nextexpr cast_expression
		int r;

		if ((r=nextexpr(aroot)) < 1)
			return r;

		do {
			if (tq_peek().type == token_type_t::star) {
				tq_discard();

				ast_node_id_t expr1 = aroot; aroot = ast_node_none;

				aroot = ast_node.alloc(token_type_t::op_multiply);
				ast_node(aroot).set_child(expr1); ast_node(expr1).release();

				ast_node_id_t expr2 = ast_node_none;
				if ((r=nextexpr(expr2)) < 1)
					return r;

				ast_node(expr1).set_next(expr2); ast_node(expr2).release();
			}
			else if (tq_peek().type == token_type_t::forwardslash) {
				tq_discard();

				ast_node_id_t expr1 = aroot; aroot = ast_node_none;

				aroot = ast_node.alloc(token_type_t::op_divide);
				ast_node(aroot).set_child(expr1); ast_node(expr1).release();

				ast_node_id_t expr2 = ast_node_none;
				if ((r=nextexpr(expr2)) < 1)
					return r;

				ast_node(expr1).set_next(expr2); ast_node(expr2).release();
			}
			else if (tq_peek().type == token_type_t::percent) {
				tq_discard();

				ast_node_id_t expr1 = aroot; aroot = ast_node_none;

				aroot = ast_node.alloc(token_type_t::op_modulus);
				ast_node(aroot).set_child(expr1); ast_node(expr1).release();

				ast_node_id_t expr2 = ast_node_none;
				if ((r=nextexpr(expr2)) < 1)
					return r;

				ast_node(expr1).set_next(expr2); ast_node(expr2).release();
			}
			else {
				break;
			}
		} while (1);
#undef nextexpr
		return 1;
	}

	int cc_state_t::additive_expression(ast_node_id_t &aroot) {
#define nextexpr multiplicative_expression
		int r;

		if ((r=nextexpr(aroot)) < 1)
			return r;

		do {
			if (tq_peek().type == token_type_t::plus) {
				tq_discard();

				ast_node_id_t expr1 = aroot; aroot = ast_node_none;

				aroot = ast_node.alloc(token_type_t::op_add);
				ast_node(aroot).set_child(expr1); ast_node(expr1).release();

				ast_node_id_t expr2 = ast_node_none;
				if ((r=nextexpr(expr2)) < 1)
					return r;

				ast_node(expr1).set_next(expr2); ast_node(expr2).release();
			}
			else if (tq_peek().type == token_type_t::minus) {
				tq_discard();

				ast_node_id_t expr1 = aroot; aroot = ast_node_none;

				aroot = ast_node.alloc(token_type_t::op_subtract);
				ast_node(aroot).set_child(expr1); ast_node(expr1).release();

				ast_node_id_t expr2 = ast_node_none;
				if ((r=nextexpr(expr2)) < 1)
					return r;

				ast_node(expr1).set_next(expr2); ast_node(expr2).release();
			}
			else {
				break;
			}
		} while (1);
#undef nextexpr
		return 1;
	}

	int cc_state_t::shift_expression(ast_node_id_t &aroot) {
#define nextexpr additive_expression
		int r;

		if ((r=nextexpr(aroot)) < 1)
			return r;

		do {
			if (tq_peek().type == token_type_t::lessthanlessthan) {
				tq_discard();

				ast_node_id_t expr1 = aroot; aroot = ast_node_none;

				aroot = ast_node.alloc(token_type_t::op_leftshift);
				ast_node(aroot).set_child(expr1); ast_node(expr1).release();

				ast_node_id_t expr2 = ast_node_none;
				if ((r=nextexpr(expr2)) < 1)
					return r;

				ast_node(expr1).set_next(expr2); ast_node(expr2).release();
			}
			else if (tq_peek().type == token_type_t::greaterthangreaterthan) {
				tq_discard();

				ast_node_id_t expr1 = aroot; aroot = ast_node_none;

				aroot = ast_node.alloc(token_type_t::op_rightshift);
				ast_node(aroot).set_child(expr1); ast_node(expr1).release();

				ast_node_id_t expr2 = ast_node_none;
				if ((r=nextexpr(expr2)) < 1)
					return r;

				ast_node(expr1).set_next(expr2); ast_node(expr2).release();
			}
			else {
				break;
			}
		} while (1);
#undef nextexpr
		return 1;
	}

	int cc_state_t::relational_expression(ast_node_id_t &aroot) {
#define nextexpr shift_expression
		int r;

		if ((r=nextexpr(aroot)) < 1)
			return r;

		do {
			if (tq_peek().type == token_type_t::lessthan) {
				tq_discard();

				ast_node_id_t expr1 = aroot; aroot = ast_node_none;

				aroot = ast_node.alloc(token_type_t::op_lessthan);
				ast_node(aroot).set_child(expr1); ast_node(expr1).release();

				ast_node_id_t expr2 = ast_node_none;
				if ((r=nextexpr(expr2)) < 1)
					return r;

				ast_node(expr1).set_next(expr2); ast_node(expr2).release();
			}
			else if (tq_peek().type == token_type_t::greaterthan) {
				tq_discard();

				ast_node_id_t expr1 = aroot; aroot = ast_node_none;

				aroot = ast_node.alloc(token_type_t::op_greaterthan);
				ast_node(aroot).set_child(expr1); ast_node(expr1).release();

				ast_node_id_t expr2 = ast_node_none;
				if ((r=nextexpr(expr2)) < 1)
					return r;

				ast_node(expr1).set_next(expr2); ast_node(expr2).release();
			}
			else if (tq_peek().type == token_type_t::lessthanequals) {
				tq_discard();

				ast_node_id_t expr1 = aroot; aroot = ast_node_none;

				aroot = ast_node.alloc(token_type_t::op_lessthan_equals);
				ast_node(aroot).set_child(expr1); ast_node(expr1).release();

				ast_node_id_t expr2 = ast_node_none;
				if ((r=nextexpr(expr2)) < 1)
					return r;

				ast_node(expr1).set_next(expr2); ast_node(expr2).release();
			}
			else if (tq_peek().type == token_type_t::greaterthanequals) {
				tq_discard();

				ast_node_id_t expr1 = aroot; aroot = ast_node_none;

				aroot = ast_node.alloc(token_type_t::op_greaterthan_equals);
				ast_node(aroot).set_child(expr1); ast_node(expr1).release();

				ast_node_id_t expr2 = ast_node_none;
				if ((r=nextexpr(expr2)) < 1)
					return r;

				ast_node(expr1).set_next(expr2); ast_node(expr2).release();
			}
			else {
				break;
			}
		} while (1);
#undef nextexpr
		return 1;
	}

	int cc_state_t::equality_expression(ast_node_id_t &aroot) {
#define nextexpr relational_expression
		int r;

		if ((r=nextexpr(aroot)) < 1)
			return r;

		do {
			if (tq_peek().type == token_type_t::equalequal) {
				tq_discard();

				ast_node_id_t expr1 = aroot; aroot = ast_node_none;

				aroot = ast_node.alloc(token_type_t::op_equals);
				ast_node(aroot).set_child(expr1); ast_node(expr1).release();

				ast_node_id_t expr2 = ast_node_none;
				if ((r=nextexpr(expr2)) < 1)
					return r;

				ast_node(expr1).set_next(expr2); ast_node(expr2).release();
			}
			else if (tq_peek().type == token_type_t::exclamationequals) {
				tq_discard();

				ast_node_id_t expr1 = aroot; aroot = ast_node_none;

				aroot = ast_node.alloc(token_type_t::op_not_equals);
				ast_node(aroot).set_child(expr1); ast_node(expr1).release();

				ast_node_id_t expr2 = ast_node_none;
				if ((r=nextexpr(expr2)) < 1)
					return r;

				ast_node(expr1).set_next(expr2); ast_node(expr2).release();
			}
			else {
				break;
			}
		} while (1);
#undef nextexpr
		return 1;
	}

	int cc_state_t::and_expression(ast_node_id_t &aroot) {
#define nextexpr equality_expression
		int r;

		if ((r=nextexpr(aroot)) < 1)
			return r;

		while (tq_peek().type == token_type_t::ampersand) {
			tq_discard();

			ast_node_id_t expr1 = aroot; aroot = ast_node_none;

			aroot = ast_node.alloc(token_type_t::op_binary_and);
			ast_node(aroot).set_child(expr1); ast_node(expr1).release();

			ast_node_id_t expr2 = ast_node_none;
			if ((r=nextexpr(expr2)) < 1)
				return r;

			ast_node(expr1).set_next(expr2); ast_node(expr2).release();
		}
#undef nextexpr
		return 1;
	}

	int cc_state_t::exclusive_or_expression(ast_node_id_t &aroot) {
#define nextexpr and_expression
		int r;

		if ((r=nextexpr(aroot)) < 1)
			return r;

		while (tq_peek().type == token_type_t::caret) {
			tq_discard();

			ast_node_id_t expr1 = aroot; aroot = ast_node_none;

			aroot = ast_node.alloc(token_type_t::op_binary_xor);
			ast_node(aroot).set_child(expr1); ast_node(expr1).release();

			ast_node_id_t expr2 = ast_node_none;
			if ((r=nextexpr(expr2)) < 1)
				return r;

			ast_node(expr1).set_next(expr2); ast_node(expr2).release();
		}
#undef nextexpr
		return 1;
	}

	int cc_state_t::inclusive_or_expression(ast_node_id_t &aroot) {
#define nextexpr exclusive_or_expression
		int r;

		if ((r=nextexpr(aroot)) < 1)
			return r;

		while (tq_peek().type == token_type_t::pipe) {
			tq_discard();

			ast_node_id_t expr1 = aroot; aroot = ast_node_none;

			aroot = ast_node.alloc(token_type_t::op_binary_or);
			ast_node(aroot).set_child(expr1); ast_node(expr1).release();

			ast_node_id_t expr2 = ast_node_none;
			if ((r=nextexpr(expr2)) < 1)
				return r;

			ast_node(expr1).set_next(expr2); ast_node(expr2).release();
		}
#undef nextexpr
		return 1;
	}

	int cc_state_t::logical_and_expression(ast_node_id_t &aroot) {
#define nextexpr inclusive_or_expression
		int r;

		if ((r=nextexpr(aroot)) < 1)
			return r;

		while (tq_peek().type == token_type_t::ampersandampersand) {
			tq_discard();

			ast_node_id_t expr1 = aroot; aroot = ast_node_none;

			aroot = ast_node.alloc(token_type_t::op_logical_and);
			ast_node(aroot).set_child(expr1); ast_node(expr1).release();

			ast_node_id_t expr2 = ast_node_none;
			if ((r=nextexpr(expr2)) < 1)
				return r;

			ast_node(expr1).set_next(expr2); ast_node(expr2).release();
		}
#undef nextexpr
		return 1;
	}

	int cc_state_t::logical_or_expression(ast_node_id_t &aroot) {
#define nextexpr logical_and_expression
		int r;

		if ((r=nextexpr(aroot)) < 1)
			return r;

		while (tq_peek().type == token_type_t::pipepipe) {
			tq_discard();

			ast_node_id_t expr1 = aroot; aroot = ast_node_none;

			aroot = ast_node.alloc(token_type_t::op_logical_or);
			ast_node(aroot).set_child(expr1); ast_node(expr1).release();

			ast_node_id_t expr2 = ast_node_none;
			if ((r=nextexpr(expr2)) < 1)
				return r;

			ast_node(expr1).set_next(expr2); ast_node(expr2).release();
		}
#undef nextexpr
		return 1;
	}

	int cc_state_t::conditional_expression(ast_node_id_t &aroot) {
#define nextexpr logical_or_expression
		int r;

		if ((r=nextexpr(aroot)) < 1)
			return r;

		if (tq_peek().type == token_type_t::question) {
			ast_node_id_t cond_expr = aroot; aroot = ast_node_none;
			tq_discard();

			ast_node_id_t true_expr = ast_node_none;
			if ((r=expression(true_expr)) < 1)
				return r;

			if (tq_get().type != token_type_t::colon)
				return errno_return(EINVAL);

			ast_node_id_t false_expr = ast_node_none;
			if ((r=conditional_expression(false_expr)) < 1)
				return r;

			aroot = ast_node.alloc(token_type_t::op_ternary);
			ast_node(aroot).set_child(cond_expr); ast_node(cond_expr).release();
			ast_node(cond_expr).set_next(true_expr); ast_node(true_expr).release();
			ast_node(true_expr).set_next(false_expr); ast_node(false_expr).release();
		}
#undef nextexpr
		return 1;
	}

	int cc_state_t::assignment_expression(ast_node_id_t &aroot) {
#define nextexpr conditional_expression
		ast_node_id_t to = ast_node_none,from = ast_node_none;
		int r;

		if ((r=nextexpr(aroot)) < 1)
			return r;

		switch (tq_peek().type) {
			case token_type_t::equal:
				tq_discard();
				if ((r=assignment_expression(from)) < 1)
					return r;
				to = aroot; aroot = ast_node.alloc(token_type_t::op_assign);
				ast_node(aroot).set_child(to); ast_node(to).release();
				ast_node(to).set_next(from); ast_node(from).release();
				break;
			case token_type_t::starequals:
				tq_discard();
				if ((r=assignment_expression(from)) < 1)
					return r;
				to = aroot; aroot = ast_node.alloc(token_type_t::op_multiply_assign);
				ast_node(aroot).set_child(to); ast_node(to).release();
				ast_node(to).set_next(from); ast_node(from).release();
				break;
			case token_type_t::forwardslashequals:
				tq_discard();
				if ((r=assignment_expression(from)) < 1)
					return r;
				to = aroot; aroot = ast_node.alloc(token_type_t::op_divide_assign);
				ast_node(aroot).set_child(to); ast_node(to).release();
				ast_node(to).set_next(from); ast_node(from).release();
				break;
			case token_type_t::percentequals:
				tq_discard();
				if ((r=assignment_expression(from)) < 1)
					return r;
				to = aroot; aroot = ast_node.alloc(token_type_t::op_modulus_assign);
				ast_node(aroot).set_child(to); ast_node(to).release();
				ast_node(to).set_next(from); ast_node(from).release();
				break;
			case token_type_t::plusequals:
				tq_discard();
				if ((r=assignment_expression(from)) < 1)
					return r;
				to = aroot; aroot = ast_node.alloc(token_type_t::op_add_assign);
				ast_node(aroot).set_child(to); ast_node(to).release();
				ast_node(to).set_next(from); ast_node(from).release();
				break;
			case token_type_t::minusequals:
				tq_discard();
				if ((r=assignment_expression(from)) < 1)
					return r;
				to = aroot; aroot = ast_node.alloc(token_type_t::op_subtract_assign);
				ast_node(aroot).set_child(to); ast_node(to).release();
				ast_node(to).set_next(from); ast_node(from).release();
				break;
			case token_type_t::lessthanlessthanequals:
				tq_discard();
				if ((r=assignment_expression(from)) < 1)
					return r;
				to = aroot; aroot = ast_node.alloc(token_type_t::op_leftshift_assign);
				ast_node(aroot).set_child(to); ast_node(to).release();
				ast_node(to).set_next(from); ast_node(from).release();
				break;
			case token_type_t::greaterthangreaterthanequals:
				tq_discard();
				if ((r=assignment_expression(from)) < 1)
					return r;
				to = aroot; aroot = ast_node.alloc(token_type_t::op_rightshift_assign);
				ast_node(aroot).set_child(to); ast_node(to).release();
				ast_node(to).set_next(from); ast_node(from).release();
				break;
			case token_type_t::ampersandequals:
				tq_discard();
				if ((r=assignment_expression(from)) < 1)
					return r;
				to = aroot; aroot = ast_node.alloc(token_type_t::op_and_assign);
				ast_node(aroot).set_child(to); ast_node(to).release();
				ast_node(to).set_next(from); ast_node(from).release();
				break;
			case token_type_t::caretequals:
				tq_discard();
				if ((r=assignment_expression(from)) < 1)
					return r;
				to = aroot; aroot = ast_node.alloc(token_type_t::op_xor_assign);
				ast_node(aroot).set_child(to); ast_node(to).release();
				ast_node(to).set_next(from); ast_node(from).release();
				break;
			case token_type_t::pipeequals:
				tq_discard();
				if ((r=assignment_expression(from)) < 1)
					return r;
				to = aroot; aroot = ast_node.alloc(token_type_t::op_or_assign);
				ast_node(aroot).set_child(to); ast_node(to).release();
				ast_node(to).set_next(from); ast_node(from).release();
				break;
			default:
				break;
		};

#undef nextexpr
		return 1;
	}

	int cc_state_t::expression(ast_node_id_t &aroot) {
#define nextexpr assignment_expression
		int r;

		if ((r=nextexpr(aroot)) < 1)
			return r;

		while (tq_peek().type == token_type_t::comma) {
			tq_discard();

			ast_node_id_t expr1 = aroot; aroot = ast_node_none;

			aroot = ast_node.alloc(token_type_t::op_comma);
			ast_node(aroot).set_child(expr1); ast_node(expr1).release();

			ast_node_id_t expr2 = ast_node_none;
			if ((r=nextexpr(expr2)) < 1)
				return r;

			ast_node(expr1).set_next(expr2); ast_node(expr2).release();
		}
#undef nextexpr
		return 1;
	}

	int cc_state_t::initializer(ast_node_id_t &aroot) {
		int r;

		assert(aroot == ast_node_none);

		/* the equals sign has already been consumed */

		if (tq_peek().type == token_type_t::opencurlybracket) {
			tq_discard();

			ast_node_id_t nex = ast_node_none;
			aroot = ast_node.alloc(token_type_t::op_array_define);

			do {
				if (tq_peek().type == token_type_t::closecurlybracket)
					break;

				ast_node_id_t expr = ast_node_none,s_expr = ast_node_none;
				bool c99_dinit = false;

				if (tq_peek(0).type == token_type_t::identifier && tq_peek(1).type == token_type_t::colon) {
					/* field: expression
					 *
					 * means the same as .field = expression but is an old GCC syntax */
					ast_node_id_t asq = ast_node.alloc(tq_get()); /* and then the ident */
					ast_node_id_t op = ast_node.alloc(token_type_t::op_dinit_field);
					ast_node(op).set_child(asq); ast_node(asq).release();
					tq_discard();
					s_expr = asq;
					expr = op;

					ast_node_id_t i_expr = ast_node_none;
					if ((r=initializer(i_expr)) < 1)
						return r;

					assert(expr != ast_node_none);
					assert(s_expr != ast_node_none);
					ast_node(s_expr).set_next(i_expr); ast_node(i_expr).release();
				}
				else {
					do {
						if (tq_peek().type == token_type_t::opensquarebracket) {
							tq_discard();

							ast_node_id_t asq = ast_node_none;
							if ((r=conditional_expression(asq)) < 1)
								return r;

							ast_node_reduce(asq);

							if (tq_peek().type == token_type_t::ellipsis) {
								/* GNU GCC extension: first ... last ranges */
								/* valid in case statements:
								 *   case 1 ... 3:
								 * valid in designated initializers:
								 *   int[] = { [1 ... 3] = 5 } */
								tq_discard();

								ast_node_id_t op1 = asq;
								ast_node_id_t op2 = ast_node_none;
								asq = ast_node.alloc(token_type_t::op_gcc_range);

								if ((r=conditional_expression(op2)) < 1)
									return r;

								ast_node_reduce(op2);

								ast_node(asq).set_child(op1); ast_node(op1).release();
								ast_node(op1).set_next(op2); ast_node(op2).release();
							}

							if (tq_get().type != token_type_t::closesquarebracket)
								return errno_return(EINVAL);

							ast_node_id_t op = ast_node.alloc(token_type_t::op_dinit_array);
							ast_node(op).set_child(asq); ast_node(asq).release();

							if (expr == ast_node_none) {
								expr = op;
							}
							else {
								ast_node(s_expr).set_next(op); ast_node(op).release();
							}
							s_expr = asq;

							c99_dinit = true;
						}
						else if (tq_peek(0).type == token_type_t::period && tq_peek(1).type == token_type_t::identifier) {
							tq_discard(); /* eat the '.' */

							ast_node_id_t asq = ast_node.alloc(tq_get()); /* and then the ident */
							ast_node_id_t op = ast_node.alloc(token_type_t::op_dinit_field);
							ast_node(op).set_child(asq); ast_node(asq).release();

							if (expr == ast_node_none) {
								expr = op;
							}
							else {
								ast_node(s_expr).set_next(op); ast_node(op).release();
							}
							s_expr = asq;

							c99_dinit = true;
						}
						else {
							break;
						}
					} while (1);

					if (c99_dinit) {
						if (tq_get().type != token_type_t::equal)
							CCERR_RET(EINVAL,tq_peek().pos,"Expected equal sign");

						ast_node_id_t i_expr = ast_node_none;
						if ((r=initializer(i_expr)) < 1)
							return r;

						assert(expr != ast_node_none);
						assert(s_expr != ast_node_none);
						ast_node(s_expr).set_next(i_expr); ast_node(i_expr).release();
					}
					else {
						if ((r=initializer(expr)) < 1)
							return r;
					}
				}

				if (nex == ast_node_none)
					ast_node(aroot).set_child(expr);
				else
					ast_node(nex).set_next(expr);

				ast_node(expr).release();
				nex = expr;

				if (tq_peek().type == token_type_t::comma)
					tq_discard();
				else if (tq_peek().type == token_type_t::closecurlybracket)
					break;
				else
					return errno_return(EINVAL);
			} while (1);

			if (tq_get().type != token_type_t::closecurlybracket)
				return errno_return(EINVAL);
		}
		else {
			if ((r=assignment_expression(aroot)) < 1)
				return r;
		}

#if 0//DEBUG
		fprintf(stderr,"init AST:\n");
		debug_dump_ast("  ",aroot);
#endif

		ast_node_reduce(aroot);

		return 1;
	}

	int cc_state_t::compound_statement_declarators(void) {
		scope_id_t scope_id = current_scope();
		scope_t &sco = scope(scope_id);
		int r;

		/* caller already ate the { */

		/* declarator (variables) phase.
		 * Use C rules where they are only allowed at the top of the compound statement.
		 * Not C++ rules where you can just do it wherever. */
		do {
			if (tq_peek().type == token_type_t::eof || tq_peek().type == token_type_t::none)
				CCERR_RET(EINVAL,tq_peek().pos,"Missing closing curly brace } in compound statement");

			if (tq_peek().type == token_type_t::semicolon)
				break;

			if (tq_peek().type == token_type_t::closecurlybracket)
				return 1; /* do not discard, let the calling function take care of it */

			if (!declaration_specifiers_check())
				break;

			declaration_specifiers_t spec;

			if ((r=chkerr()) < 1)
				return r;
			if ((r=declaration_specifiers_parse(spec,DECLSPEC_ALLOW_DEF)) < 1)
				return r;

			do {
				declarator_t declor;
				symbol_lookup_t sl;

				sl.pos = tq_peek().pos;
				if ((r=declaration_inner_parse(spec,declor)) < 1)
					return r;

				if (declor.name != identifier_none) {
					if ((r=prep_symbol_lookup(sl,spec,declor)) < 1)
						return r;
					if (do_local_symbol_lookup(sl,spec,declor)) {
						if ((r=check_symbol_lookup_match(sl,spec,declor)) < 1)
							return r;
						if ((r=check_symbol_param_match(sl,declor.ddip)) < 1)
							return r;
					}
					else {
						if ((r=add_symbol(sl,spec,declor)) < 1)
							return r;

						scope_t::decl_t &sldef = sco.new_localdecl();
						sldef.spec = spec;
						sldef.declor = std::move(declor); /* this is the last time this function will use it, std::move it */
						sldef.declor.symbol = sl.sid;
					}
				}

				if (tq_peek().type == token_type_t::comma) {
					tq_discard();
					continue;
				}

				break;
			} while(1);

			if (tq_peek().type == token_type_t::semicolon) {
				tq_discard();
				continue;
			}

			CCERR_RET(EINVAL,tq_peek().pos,"Missing semicolon");
		} while (1);

		return 1;
	}

	int cc_state_t::expression_statement(ast_node_id_t &aroot) {
		int r;

		if ((r=statement(aroot)) < 1)
			return r;

		if (aroot == ast_node_none)
			aroot = ast_node.alloc(token_type_t::op_none);

		return 1;
	}

	int cc_state_t::statement(ast_node_id_t &aroot) {
		int r;

		if (tq_peek().type == token_type_t::eof || tq_peek().type == token_type_t::none)
			return errno_return(EINVAL);

		if (tq_peek().type == token_type_t::semicolon) {
			tq_discard();
		}
		else if (tq_peek().type == token_type_t::opencurlybracket) {
			tq_discard();

			push_new_scope();

			/* a compound statement within a compound statement */
			ast_node_id_t cur = ast_node_none;
			ast_node_id_t curnxt = ast_node_none;
			if ((r=compound_statement(cur,curnxt)) < 1) {
				pop_scope();
				return r;
			}

			aroot = ast_node.alloc(token_type_t::op_compound_statement);
			ast_node(aroot).set_child(cur); ast_node(cur).release();
			ast_node(aroot).t.v.scope = current_scope();

			pop_scope();
		}
		else if (tq_peek(0).type == token_type_t::r_switch && tq_peek(1).type == token_type_t::openparenthesis) {
			tq_discard(2);

			ast_node_id_t expr = ast_node_none;
			if ((r=expression(expr)) < 1)
				return r;

			if (tq_get().type != token_type_t::closeparenthesis)
				return errno_return(EINVAL);

			ast_node_id_t stmt = ast_node_none;
			if ((r=statement(stmt)) < 1)
				return r;

			aroot = ast_node.alloc(token_type_t::op_switch_statement);
			ast_node(aroot).set_child(expr); ast_node(expr).release();
			ast_node(expr).set_next(stmt); ast_node(stmt).release();
		}
		else if (tq_peek(0).type == token_type_t::r_if && tq_peek(1).type == token_type_t::openparenthesis) {
			tq_discard(2);

			ast_node_id_t expr = ast_node_none;
			if ((r=expression(expr)) < 1)
				return r;

			if (tq_get().type != token_type_t::closeparenthesis)
				return errno_return(EINVAL);

			ast_node_id_t stmt = ast_node_none;
			if ((r=statement(stmt)) < 1)
				return r;

			aroot = ast_node.alloc(token_type_t::op_if_statement);
			ast_node(aroot).set_child(expr); ast_node(expr).release();
			ast_node(expr).set_next(stmt); ast_node(stmt).release();

			if (tq_peek().type == token_type_t::r_else) {
				tq_discard();

				ast_node_id_t elst = ast_node_none;
				if ((r=statement(elst)) < 1)
					return r;

				ast_node(stmt).set_next(elst); ast_node(elst).release();
			}
		}
		else if (tq_peek(0).type == token_type_t::r_while && tq_peek(1).type == token_type_t::openparenthesis) {
			tq_discard(2);

			ast_node_id_t expr = ast_node_none;
			if ((r=expression(expr)) < 1)
				return r;

			if (tq_get().type != token_type_t::closeparenthesis)
				return errno_return(EINVAL);

			ast_node_id_t stmt = ast_node_none;
			if ((r=statement(stmt)) < 1)
				return r;

			aroot = ast_node.alloc(token_type_t::op_while_statement);
			ast_node(aroot).set_child(expr); ast_node(expr).release();
			ast_node(expr).set_next(stmt); ast_node(stmt).release();
		}
		else if (tq_peek(0).type == token_type_t::r_for && tq_peek(1).type == token_type_t::openparenthesis) {
			tq_discard(2);

			ast_node_id_t initexpr = ast_node_none;
			ast_node_id_t loopexpr = ast_node_none;
			ast_node_id_t iterexpr = ast_node_none;
			ast_node_id_t stmt = ast_node_none;

			if ((r=expression_statement(initexpr)) < 1)
				return r;

			if ((r=expression_statement(loopexpr)) < 1)
				return r;

			if (tq_peek().type != token_type_t::closeparenthesis) {
				if ((r=expression(iterexpr)) < 1)
					return r;
			}
			if (iterexpr == ast_node_none)
				iterexpr = ast_node.alloc(token_type_t::op_none);

			if (tq_get().type != token_type_t::closeparenthesis)
				return errno_return(EINVAL);

			if ((r=statement(stmt)) < 1)
				return r;

			aroot = ast_node.alloc(token_type_t::op_for_statement);
			ast_node(aroot).set_child(initexpr); ast_node(initexpr).release();
			ast_node(initexpr).set_next(loopexpr); ast_node(loopexpr).release();
			ast_node(loopexpr).set_next(iterexpr); ast_node(iterexpr).release();
			if (stmt != ast_node_none) { ast_node(iterexpr).set_next(stmt); ast_node(stmt).release(); }
		}
		else if (tq_peek(0).type == token_type_t::identifier && tq_peek(1).type == token_type_t::colon) {
			ast_node_id_t label = ast_node.alloc(tq_get()); /* eats tq_peek(0); */
			aroot = ast_node.alloc(token_type_t::op_label);
			ast_node(aroot).set_child(label); ast_node(label).release();
			tq_discard(); /* eats the ':' */
		}
		else if (tq_peek(0).type == token_type_t::r_default && tq_peek(1).type == token_type_t::colon) {
			aroot = ast_node.alloc(token_type_t::op_default_label);
			tq_discard(2);
		}
		else if (tq_peek().type == token_type_t::r_case) { /* case constant_expression : */
			tq_discard();
			ast_node_id_t expr = ast_node_none;

			if ((r=conditional_expression(expr)) < 1)
				return r;

			if (tq_peek().type == token_type_t::ellipsis) {
				/* GNU GCC extension: first ... last ranges */
				/* valid in case statements:
				 *   case 1 ... 3:
				 * valid in designated initializers:
				 *   int[] = { [1 ... 3] = 5 } */
				tq_discard();

				ast_node_id_t op1 = expr;
				ast_node_id_t op2 = ast_node_none;
				expr = ast_node.alloc(token_type_t::op_gcc_range);

				if ((r=conditional_expression(op2)) < 1)
					return r;

				ast_node(expr).set_child(op1); ast_node(op1).release();
				ast_node(op1).set_next(op2); ast_node(op2).release();
			}

			if (tq_get().type != token_type_t::colon)
				return errno_return(EINVAL);

			aroot = ast_node.alloc(token_type_t::op_case_statement);
			ast_node(aroot).set_child(expr); ast_node(expr).release();
		}
		else {
			if (tq_peek().type == token_type_t::r_break) {
				aroot = ast_node.alloc(token_type_t::op_break);
				tq_discard();
			}
			else if (tq_peek().type == token_type_t::r_continue) {
				aroot = ast_node.alloc(token_type_t::op_continue);
				tq_discard();
			}
			else if (tq_peek(0).type == token_type_t::r_goto && tq_peek(1).type == token_type_t::identifier) {
				tq_discard();
				ast_node_id_t label = ast_node.alloc(tq_get());
				aroot = ast_node.alloc(token_type_t::op_goto);
				ast_node(aroot).set_child(label); ast_node(label).release();
			}
			else if (tq_peek().type == token_type_t::r_do) {
				tq_discard();

				ast_node_id_t stmt = ast_node_none;
				if ((r=statement(stmt)) < 1)
					return r;

				if (tq_get().type != token_type_t::r_while)
					return errno_return(EINVAL);

				if (tq_get().type != token_type_t::openparenthesis)
					return errno_return(EINVAL);

				ast_node_id_t expr = ast_node_none;
				if ((r=expression(expr)) < 1)
					return r;

				if (tq_get().type != token_type_t::closeparenthesis)
					return errno_return(EINVAL);

				aroot = ast_node.alloc(token_type_t::op_do_while_statement);
				ast_node(aroot).set_child(stmt); ast_node(stmt).release();
				ast_node(stmt).set_next(expr); ast_node(expr).release();
			}
			else if (tq_peek().type == token_type_t::r_return) {
				aroot = ast_node.alloc(token_type_t::op_return);
				tq_discard();

				if (tq_peek().type != token_type_t::semicolon) {
					ast_node_id_t expr = ast_node_none;
					if ((r=expression(expr)) < 1)
						return r;

					ast_node(aroot).set_child(expr); ast_node(expr).release();
				}
			}
			else {
				if ((r=expression(aroot)) < 1)
					return r;
			}

			if (tq_peek().type == token_type_t::semicolon)
				tq_discard();
			else
				return errno_return(EINVAL);
		}

		return 1;
	}

	int cc_state_t::compound_statement(ast_node_id_t &aroot,ast_node_id_t &nroot) {
		ast_node_id_t nxt;
		int r;

		assert(current_scope() != scope_global);
		assert(current_scope() != scope_none);

		assert(
			(aroot == ast_node_none && nroot == ast_node_none) ||
			(aroot != ast_node_none && nroot == ast_node_none) ||
			(aroot != ast_node_none && nroot != ast_node_none)
		);

		/* caller already ate the { */

		if ((r=compound_statement_declarators()) < 1)
			return r;

		/* OK, now statements */
		do {
			if (tq_peek().type == token_type_t::eof || tq_peek().type == token_type_t::none)
				return errno_return(EINVAL);

			if (tq_peek().type == token_type_t::closecurlybracket) {
				tq_discard();
				break;
			}

			nxt = ast_node_none;
			if ((r=statement(nxt)) < 1)
				return r;

			/* NTS: statement() can leave the node unset if an empty statement like a lone ';' */
			if (nxt != ast_node_none) {
				if (aroot == ast_node_none)
					aroot = nxt;
				else
					{ ast_node(nroot).set_next(nxt); ast_node(nxt).release(); }

				nroot = nxt;
			}
		} while (1);

		ast_node.assign(/*to*/scope(current_scope()).root,/*from*/aroot);

#if 0//DEBUG
		fprintf(stderr,"compound declarator, scope %u:\n",(unsigned int)current_scope());
		debug_dump_ast("  ",aroot);
#endif

		return 1;
	}

	int cc_state_t::declaration_inner_parse(declaration_specifiers_t &spec,declarator_t &declor) {
		int r;

		if ((r=declarator_parse(spec,declor)) < 1)
			return r;
		if ((r=chkerr()) < 1)
			return r;

		if (tq_peek().type == token_type_t::equal) {
			tq_discard();

			if ((r=initializer(declor.expr)) < 1)
				return r;
		}

#if 0//DEBUG
		fprintf(stderr,"DEBUG %s:%d:\n",__FUNCTION__,__LINE__);
		debug_dump_declaration_specifiers("  ",spec); // this is the only other param
		debug_dump_declarator("  ",declor);
		for (auto &p : parameters)
			debug_dump_parameter("  ",p);
#endif

		return 1;
	}

	int cc_state_t::declaration_parse(declaration_t &declion) {
		int r,count = 0;

#if 0//DEBUG
		fprintf(stderr,"%s(line %d) begin parsing\n",__FUNCTION__,__LINE__);
#endif

		if ((r=declaration_specifiers_parse(declion.spec,DECLSPEC_ALLOW_DEF)) < 1)
			return r;
		if ((r=chkerr()) < 1)
			return r;

		do {
			symbol_lookup_t sl;
			declarator_t &declor = declion.new_declarator();

			sl.pos = tq_peek().pos;
			if ((r=declaration_inner_parse(declion.spec,declor)) < 1)
				return r;

			if (tq_peek().type == token_type_t::opencurlybracket && (declor.flags & declarator_t::FL_FUNCTION)) {
				tq_discard();

				if (declor.flags & declarator_t::FL_FUNCTION_POINTER)
					CCERR_RET(EINVAL,tq_peek().pos,"Function body not allowed for function pointers");
				if (declor.expr != ast_node_none)
					CCERR_RET(EINVAL,tq_peek().pos,"Function body cannot coexist with initializer expression");
				if (count != 0)
					CCERR_RET(EINVAL,tq_peek().pos,"Function body not allowed here");

				sl.flags |= symbol_t::FL_DEFINED;
				if ((r=prep_symbol_lookup(sl,declion.spec,declor)) < 1)
					return r;
				if (do_local_symbol_lookup(sl,declion.spec,declor)) {
					if ((r=check_symbol_lookup_match(sl,declion.spec,declor)) < 1)
						return r;
					if ((r=check_symbol_param_match(sl,declor.ddip)) < 1)
						return r;
				}
				else if ((r=add_symbol(sl,declion.spec,declor)) < 1) {
					return r;
				}

				/* start the new scope, tell compound_statement() we already did it,
				 * so that we can register the function parameters as part of the new scope */
				push_new_scope();

				/* add it to the symbol table */
				{
					ddip_t *fp = declor.ddip.funcparampair();

					if (fp) {
						for (auto &p : fp->parameters) {
							/* if a parameter was given without a name, don't register a symbol */
							if (p.decl.name == identifier_none)
								continue;

							symbol_lookup_t sl;

							sl.flags |= symbol_t::FL_PARAMETER | symbol_t::FL_DEFINED;
							if ((r=prep_symbol_lookup(sl,p.spec,p.decl)) < 1)
								return r;
							if (do_local_symbol_lookup(sl,p.spec,p.decl)) {
								if ((r=check_symbol_lookup_match(sl,p.spec,p.decl)) < 1)
									return r;
								if ((r=check_symbol_param_match(sl,p.decl.ddip)) < 1)
									return r;
							}
							else if ((r=add_symbol(sl,p.spec,p.decl)) < 1) {
								return r;
							}

							p.decl.symbol = sl.sid;
						}
					}
				}

				{
					/* look it up again, compound_statement() could very well have added symbols
					 * causing reallocation and the reference above would become invalid */
					symbol_t &sym = symbol(sl.sid);
					sym.parent_of_scope = current_scope();
					sym.ddip = declor.ddip;
				}

				ast_node_id_t fbroot = ast_node_none,fbrootnext = ast_node_none;
				if ((r=compound_statement(fbroot,fbrootnext)) < 1) {
					pop_scope();
					return r;
				}

				{
					/* once the compound statment ends, no more declarators.
					 * you can't do "int f() { },g() { }" */
					symbol_t &sym = symbol(sl.sid);
					ast_node.assign(/*to*/declor.expr,/*from*/fbroot);
					ast_node.assignmove(/*to*/sym.expr,/*from*/fbroot);
				}

				pop_scope();
				return 1;
			}

			/* add it to the symbol table */
			if (declor.name != identifier_none) {
				if ((r=prep_symbol_lookup(sl,declion.spec,declor)) < 1)
					return r;
				if (do_local_symbol_lookup(sl,declion.spec,declor)) {
					if ((r=check_symbol_lookup_match(sl,declion.spec,declor)) < 1)
						return r;
					if ((r=check_symbol_param_match(sl,declor.ddip)) < 1)
						return r;
				}
				else {
					if ((r=add_symbol(sl,declion.spec,declor)) < 1)
						return r;
				}
			}

			count++;
			if (tq_peek().type == token_type_t::comma) {
				tq_discard();
				continue;
			}

			break;
		} while(1);

#if 0//DEBUG
		fprintf(stderr,"DEBUG %s:%d:\n",__FUNCTION__,__LINE__);
		debug_dump_declaration("  ",declion);
#endif

		if (tq_peek().type == token_type_t::semicolon) {
			tq_discard();
			return 1;
		}

		CCERR_RET(EINVAL,tq_peek().pos,"Missing semicolon");
	}

	int cc_state_t::struct_declaration_parse(const symbol_id_t sid,const token_type_t &tt) {
		declaration_specifiers_t spec;
		int r,count = 0;

		(void)tt;

#if 0//DEBUG
		fprintf(stderr,"%s(line %d) begin parsing\n",__FUNCTION__,__LINE__);
#endif

		if ((r=declaration_specifiers_parse(spec,DECLSPEC_ALLOW_DEF)) < 1)
			return r;
		if ((r=chkerr()) < 1)
			return r;

#if 0//DEBUG
		fprintf(stderr,"DEBUG %s:%d %s:\n",__FUNCTION__,__LINE__,token_type_t_str(tt));
		debug_dump_declaration_specifiers("  ",spec);
#endif

		do {
			declarator_t declor;

			if ((r=struct_declarator_parse(sid,spec,declor)) < 1)
				return r;
			if ((r=chkerr()) < 1)
				return r;

#if 0//DEBUG
			fprintf(stderr,"DEBUG %s:%d field type %s:\n",__FUNCTION__,__LINE__,token_type_t_str(tt));
			debug_dump_declarator("  ",declor);
#endif

			count++;
			if (tq_peek().type == token_type_t::comma) {
				tq_discard();
				continue;
			}

			break;
		} while(1);

		if (tq_peek().type == token_type_t::semicolon) {
			tq_discard();
			return 1;
		}

		return errno_return(EINVAL);
	}

	int cc_state_t::external_declaration(void) {
		declaration_t declion;
		int r;

#if 0//DEBUG
		fprintf(stderr,"%s(line %d) begin parsing\n",__FUNCTION__,__LINE__);
#endif

		if (tq_peek().type == token_type_t::semicolon) {
			tq_discard();
			return 1;
		}
		if ((r=chkerr()) < 1)
			return r;
		if ((r=declaration_parse(declion)) < 1)
			return r;

		return 1;
	}

	int cc_state_t::translation_unit(void) {
		const token_t &t = tq_peek();
		int r;

		if (t.type == token_type_t::none || t.type == token_type_t::eof)
			return err; /* 0 or negative */

		if ((r=external_declaration()) < 1)
			return r;

		return 1;
	}

	int CCstep(cc_state_t &cc,rbuf &buf,source_file_object &sfo) {
		int r;

		cc.buf = &buf;
		cc.sfo = &sfo;

		if (cc.err == 0) {
			if ((r=cc.translation_unit()) < 1)
				return r;
		}

		if (cc.err < 0)
			return r;

		return 1;
	}

	template <class T> void typ_delete(T *p) {
		if (p) delete p;
		p = NULL;
	}

}

enum test_mode_t {
	TEST_NONE=0,
	TEST_SFO,      /* source file object */
	TEST_RBF,      /* rbuf read buffer, manual fill */
	TEST_RBFGC,    /* rbuf read buffer, getc() */
	TEST_RBFGCNU,  /* rbuf read buffer, getcnu() (unicode getc) */
	TEST_LGTOK,    /* lgtok lowest level general tokenizer */
	TEST_PPTOK,    /* pptok preprocessor token processing and macro expansion */
	TEST_LCTOK     /* lctok low level C token processing below compiler */
};

static std::vector<std::string>		main_input_files;
static enum test_mode_t			test_mode = TEST_NONE;

static void help(void) {
	fprintf(stderr,"cimcc [options] [input file [...]]\n");
	fprintf(stderr,"  --test <none|sfo|rbf|rbfgc|rbfgcnu|lgtok|pptok|lctok>         Test mode\n");
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
				else if (!strcmp(a,"lctok"))
					test_mode = TEST_LCTOK;
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
		std::unique_ptr<CCMiniC::source_file_object> sfo;

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
			sfo.reset(new CCMiniC::source_fd(0/*STDIN, takes ownership*/,"<stdin>"));
			assert(sfo->iface == CCMiniC::source_file_object::IF_FD);
		}
		else {
			int fd = open((*mifi).c_str(),O_RDONLY|O_BINARY);
			if (fd < 0) {
				fprintf(stderr,"Cannot open '%s', '%s'\n",(*mifi).c_str(),strerror(errno));
				return 1;
			}
			sfo.reset(new CCMiniC::source_fd(fd/*takes ownership*/,*mifi));
			assert(sfo->iface == CCMiniC::source_file_object::IF_FD);
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
			CCMiniC::rbuf rb;
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
			CCMiniC::rbuf rb;
			assert(rb.allocate());

			do {
				CCMiniC::unicode_char_t c = CCMiniC::getc(rb,*sfo);
				if (c == CCMiniC::unicode_eof) {
					assert(rb.data_avail() == 0);
					break;
				}
				else if (c == CCMiniC::unicode_invalid) {
					if (write(1/*STDOUT*/,"<INVALID>",9) != 9) return -1;
				}
				else {
					const std::string s = CCMiniC::utf8_to_str(c);
					if (write(1/*STDOUT*/,s.data(),s.size()) != ssize_t(s.size())) return -1;
				}
			} while (1);
		}
		else if (test_mode == TEST_RBFGCNU) {
			CCMiniC::rbuf rb;
			assert(rb.allocate());

			do {
				CCMiniC::unicode_char_t c = CCMiniC::getcnu(rb,*sfo);
				if (c == CCMiniC::unicode_eof) {
					assert(rb.data_avail() == 0);
					break;
				}
				else if (c == CCMiniC::unicode_invalid) {
					abort(); /* should not happen! */
				}
				else {
					assert(c >= CCMiniC::unicode_char_t(0l) && c <= CCMiniC::unicode_char_t(0xFFul));
					unsigned char cc = (unsigned char)c;
					if (write(1/*STDOUT*/,&cc,1) != ssize_t(1)) return -1;
				}
			} while (1);
		}
		else if (test_mode == TEST_LGTOK) {
			CCMiniC::lgtok_state_t lst;
			CCMiniC::token_t tok;
			CCMiniC::rbuf rb;
			int r;

			assert(rb.allocate());
			rb.set_source_file(CCMiniC::alloc_source_file(sfo->getname()));
			while ((r=CCMiniC::lgtok(lst,rb,*sfo,tok)) > 0) {
				printf("Token:");
				if (tok.pos.row > 0) printf(" pos:row=%u,col=%u,ofs=%u",tok.pos.row,tok.pos.col,tok.pos.ofs);
				if (tok.source_file < CCMiniC::source_files.size()) printf(" src='%s'",CCMiniC::source_files[tok.source_file].path.c_str());
				printf(" %s\n",tok.to_str().c_str());
			}

			if (r < 0) {
				fprintf(stderr,"Read error from %s, error %d\n",sfo->getname(),(int)r);
				return -1;
			}
		}
		else if (test_mode == TEST_PPTOK) {
			CCMiniC::lgtok_state_t lst;
			CCMiniC::pptok_state_t pst;
			CCMiniC::token_t tok;
			CCMiniC::rbuf rb;
			int r;

			assert(rb.allocate());
			rb.set_source_file(CCMiniC::alloc_source_file(sfo->getname()));
			while ((r=CCMiniC::pptok(pst,lst,rb,*sfo,tok)) > 0) {
				printf("Token:");
				if (tok.pos.row > 0) printf(" pos:row=%u,col=%u,ofs=%u",tok.pos.row,tok.pos.col,tok.pos.ofs);
				if (tok.source_file < CCMiniC::source_files.size()) printf(" src='%s'",CCMiniC::source_files[tok.source_file].path.c_str());
				printf(" %s\n",tok.to_str().c_str());
			}

			if (r < 0) {
				fprintf(stderr,"Read error from %s, error %d\n",sfo->getname(),(int)r);
				return -1;
			}
		}
		else if (test_mode == TEST_LCTOK) {
			CCMiniC::lgtok_state_t lst;
			CCMiniC::pptok_state_t pst;
			CCMiniC::token_t tok;
			CCMiniC::rbuf rb;
			int r;

			assert(rb.allocate());
			rb.set_source_file(CCMiniC::alloc_source_file(sfo->getname()));
			while ((r=CCMiniC::lctok(pst,lst,rb,*sfo,tok)) > 0) {
				printf("Token:");
				if (tok.pos.row > 0) printf(" pos:row=%u,col=%u,ofs=%u",tok.pos.row,tok.pos.col,tok.pos.ofs);
				if (tok.source_file < CCMiniC::source_files.size()) printf(" src='%s'",CCMiniC::source_files[tok.source_file].path.c_str());
				printf(" %s\n",tok.to_str().c_str());
			}

			if (r < 0) {
				fprintf(stderr,"Read error from %s, error %d\n",sfo->getname(),(int)r);
				return -1;
			}
		}
		else {
			/* normal compilation */
			CCMiniC::cc_state_t ccst;
			CCMiniC::rbuf rb;
			int r;

			assert(rb.allocate());
			rb.set_source_file(CCMiniC::alloc_source_file(sfo->getname()));
			while ((r=CCMiniC::CCstep(ccst,rb,*sfo)) > 0);

			ccst.debug_dump_scope_table("");
			ccst.debug_dump_symbol_table("");

			if (r < 0) {
				fprintf(stderr,"Read error from %s, error %d\n",sfo->getname(),(int)r);
				return -1;
			}
		}

		assert(sfo.get() != NULL);
	}

	CCMiniC::source_file_refcount_check();
	CCMiniC::identifier.refcount_check();
	CCMiniC::csliteral.refcount_check();
	CCMiniC::ast_node.refcount_check();

	if (test_mode == TEST_SFO || test_mode == TEST_LGTOK || test_mode == TEST_RBF ||
		test_mode == TEST_RBFGC || test_mode == TEST_RBFGCNU || test_mode == TEST_LGTOK ||
		test_mode == TEST_PPTOK || test_mode == TEST_LGTOK) return 0;

	return 0;
}

