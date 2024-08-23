
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <fcntl.h>

#include <unordered_map>
#include <type_traits>
#include <stdexcept>
#include <vector>
#include <string>
#include <limits>
#include <new>

namespace CIMCC {

	/////////

	typedef int (*refill_function_t)(unsigned char *at,size_t how_much,void *context,size_t context_size);

	static int refill_null(unsigned char *,size_t,void*,size_t) { return 0; }

	/////////

	bool is_decimal_digit(const int c,const int base=10) {
		return (c >= '0' && c <= ('0'+base-1));
	}

	bool is_hexadecimal_digit(const int c) {
		return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
	}

	unsigned char p_decimal_digit(const char c) { /* NTS: Use alternate function for hexadecimal */
		return c - '0';
	}

	unsigned char p_hexadecimal_digit(const char c) {
		if (c >= 'a' && c <= 'f')
			return c + 10 - 'a';
		else if (c >= 'A' && c <= 'F')
			return c + 10 - 'A';

		return c - '0';
	}

	bool is_whitespace(const int c) {
		return (c == ' ' || c == '\t' || c == '\n' || c == '\r');
	}

	bool is_identifier_first_char(const char c) {
		return (c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'));
	}

	bool is_identifier_char(const char c) {
		return (c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'));
	}

	/////////

	/* H prefix flags */
	enum {
		HFLAG_CR_NEWLINES = (1u << 0u),
		HFLAG_LF_NEWLINES = (1u << 1u)
	};

	/////////

	struct token_t;

	struct token_source_name_t {
		std::string		name;
	};

	typedef std::vector<token_source_name_t> token_source_name_list_t;

	struct position_t {
		static constexpr uint16_t nosource = 0xFFFFu;

		void first_line(void) {
			pchar = 0;
			line = 1;
			col = 1;
		}

		void next_line(void) {
			line++;
			col = 1;
		}

		int			line = -1;
		int			col = -1;
		uint16_t		source = nosource;
		uint32_t		pchar = 0;
	};

	struct context_t {
		std::string		source_name;
		unsigned char*		ptr = NULL;
		size_t			ptr_size = 0;
		bool			ownership = false;

		bool alloc(size_t sz) {
			if (ptr != NULL)
				return false;
			if (sz == 0 || sz > 65536)
				return false;

			ptr = new(std::nothrow) unsigned char[sz];
			if (ptr == NULL)
				return false;

			ownership = true;
			ptr_size = sz;
			return true;
		}

		void free(void) {
			if (ptr && ownership) delete[] ptr;
			ownership = false;
			ptr_size = 0;
			ptr = NULL;
		}

		context_t() { }

		~context_t() { free(); }
	};

	struct parse_buffer {
		unsigned char*		buffer = NULL; /* base of the allocated buffer */
		unsigned char*		read = NULL; /* read position */
		unsigned char*		end = NULL; /* last valid byte (and where more is loaded) */
		size_t			buffer_size = 0; /* allocated buffer size */
		bool			ownership = false;

		bool is_alloc(void) const {
			return buffer != NULL;
		}

		inline unsigned char* fence(void) const {
			return buffer + buffer_size;
		}

		bool alloc(size_t sz=0) {
			/* No, we're not going to allow changing the buffer size on the fly. Free it first. */
			if (buffer != NULL) return false;

			/* Be reasonable, don't ask for large buffers and don't make it too small */
			if (sz < 2048 || sz > 8192) sz = 4096;

			/* Allocate the buffer, return false if it fails.
			 * Use std::nothrow to avoid having to deal with C++ exceptions. */
			/* Allocating the buffer resets all pointers. */
			buffer = new(std::nothrow) unsigned char[sz];
			if (buffer == NULL) return false;
			read = end = buffer;
			buffer_size = sz;
			ownership = true;
			return true;
		}

		void free(void) {
			if (buffer && ownership) delete[] buffer;
			buffer = read = end = NULL;
			ownership = false;
			buffer_size = 0;
		}

		bool sanity_check(void) const {
			return buffer <= read && read <= end && end <= fence();
		}

		bool eof(void) const {
			return read >= end;
		}

		void flush(size_t keep=0/*how much to keep prior to read ptr*/) {
			if (buffer) {
				assert(sanity_check());

				/* it is hard to flush if the caller asks to keep too much */
				if (keep > (buffer_size/4u))
					keep = (buffer_size/4u);

				if (keep > size_t(read-buffer))
					keep = size_t(read-buffer);

				size_t howmuch = size_t(end-read);
				if (howmuch != 0 && read != (buffer + keep)) {
					assert((buffer + keep + howmuch) <= fence());
					assert((read + howmuch) <= fence());
					assert((buffer + keep) < read); /* already checked read != (buffer + keep) */
					memmove(buffer + keep,read,howmuch);
				}
				read = buffer + keep;
				end = read + howmuch;

				assert(sanity_check());
			}
		}

		void lazy_flush(size_t keep=0) {
			if (size_t(fence() - end) < (buffer_size / 2u))
				flush(keep);
		}

		void refill(refill_function_t f,context_t &c) {
			/* caller must lazy flush to make room */
			if (f && end < fence()) {
				int got = f(end,size_t(fence() - end),c.ptr,c.ptr_size);
				if (got > 0) {
					end += size_t(got);
					assert(sanity_check());
				}
			}
		}

		parse_buffer() { }

		~parse_buffer() { free(); }
	};

	/////////

	enum class token_type_t {
		eof=-1,
		none=0,
		intval,
		floatval,
		comma,
		semicolon,
		minus,
		plus,
		exclamation,
		tilde,
		star,
		slash,
		percent,
		equal,
		plusequal,
		minusequal,
		openparen,
		closeparen,
		starequal,
		slashequal,
		percentequal,
		colon,
		question,
		coloncolon,
		pipe,
		pipepipe,
		ampersand,
		ampersandampersand,
		caret,
		ampersandequal,
		caretequal,
		pipeequal,
		equalequal,
		exclamationequal,
		greaterthan,
		lessthan,
		greaterthanorequal,
		lessthanorequal,
		leftleftangle,
		rightrightangle,
		leftleftangleequal,
		rightrightangleequal,
		plusplus,
		minusminus,
		period,
		pointerarrow,
		leftsquarebracket,
		rightsquarebracket,
		identifier,
		characterliteral,
		stringliteral,
		opencurly,
		closecurly,
		r_return, /* reserved word "return" */
		r_goto, /* reserved word "goto" */
		r_if, /* reserved word "if" */
		r_else, /* reserved word "else" */
		r_break,
		r_continue,
		r_switch,
		r_case,
		r_default,
		r_while,
		r_do,
		r_for,
		r_let,
		r_fn,
		r_const,
		r_constexpr,
		r_compileexpr,
		r_static,
		r_extern,
		r_auto,
		r_signed,
		r_unsigned,
		r_long,
		r_short,
		r_int,
		r_float,
		r_double,
		r_void,
		r_volatile,
		r_char,
		ellipsis,
		r_sizeof,
		r_size_t,
		r_ssize_t,
		r_offsetof,
		r_static_assert,
		r_typedef,
		r_using,
		r_namespace,
		dblleftsquarebracket,
		dblrightsquarebracket,
		r_typeof,
		r_bool,
		r_true,
		r_false,
		r_near,
		r_far,
		r_huge,
		r_asm, // asm
		r__asm, // _asm
		r___asm, // __asm
		r___asm__, // __asm__
		r_this,
		r_struct,
		r_union,
		poundsign,
		r_typeclsif,

		maxval
	};

	/* do not define constructor or destructor because this will be used in a union */
	struct token_intval_t {
		static constexpr unsigned int FL_SIGNED = (1u << 0u);

		enum {
			T_UNSPEC=0,
			T_CHAR=1,
			T_SHORT=2,
			T_INT=3,
			T_LONG=4,
			T_LONGLONG=5,
			T_BOOL=6
		};

		unsigned char				flags;
		unsigned char				itype;
		union {
			unsigned long long		u;
			signed long long		v;
		} v;

		inline void init_common(void) {
			itype = T_UNSPEC;
			v.u = 0;
		}

		void init(void) {
			flags = FL_SIGNED;
			init_common();
		}

		void initu(void) {
			flags = 0;
			init_common();
		}

		void initbool(void) {
			v.u = 0;
			flags = 0;
			itype = T_BOOL;
		}

		void initbool(const bool b) {
			flags = 0;
			itype = T_BOOL;
			v.u = b ? 1 : 0;
		}
	};

	/* do not define constructor or destructor because this will be used in a union */
	struct token_floatval_t {
		enum {
			T_UNSPEC=0,
			T_HALF=1,
			T_FLOAT=2,
			T_DOUBLE=3,
			T_LONGDOUBLE=4
		};

		unsigned char				ftype;
		long double				val;

		void init(void) {
			ftype = T_UNSPEC;
			val = std::numeric_limits<long double>::quiet_NaN();
		}
	};

	/* do not define constructor or destructor because this will be used in a union */
	struct token_identifier_value_t {
		std::string				name;

		token_identifier_value_t() { }
		token_identifier_value_t(const token_identifier_value_t &) = delete;
		token_identifier_value_t(token_identifier_value_t &&x) { common_move(x); }
		token_identifier_value_t &operator=(const token_identifier_value_t &) = delete;
		token_identifier_value_t &operator=(token_identifier_value_t &&x) { common_move(x); return *this; }

		~token_identifier_value_t() {
			name.clear();
		}

		void set(size_t len,const char *s) {
			assert(s != NULL);
			assert(len != 0);
			name = std::move(std::string(s,len));
		}

		token_identifier_value_t(size_t len,const char *s) { set(len,s); }

		void common_move(token_identifier_value_t &x) {
			name = std::move(x.name);
		}
	};

	typedef size_t token_identifier_t;
	static constexpr token_identifier_t token_identifier_none = ~size_t(0);

	/* TODO: Obviously, this can be optimized! */
	std::vector<token_identifier_value_t> token_identifier_list;
	std::unordered_map< std::hash<std::string>::result_type, token_identifier_t > token_identifier_list_lookup;

	token_identifier_t token_identifier_value_lookup(const std::string &s) {
		auto i = token_identifier_list_lookup.find(std::hash<std::string>()(s));
		if (i != token_identifier_list_lookup.end()) return i->second;
		return token_identifier_none;
	}

	token_identifier_t token_identifier_value_lookup(size_t len,const char *s) {
		return token_identifier_value_lookup(std::string(s,len));
	}

	token_identifier_t token_identifier_value_alloc(size_t len,const char *s) {
		token_identifier_t id;

		id = token_identifier_value_lookup(len,s);
		if (id == token_identifier_none) {
			id = token_identifier_list.size();
			token_identifier_list.emplace_back(len,s);
			token_identifier_list_lookup[std::hash<std::string>()(token_identifier_list[id].name)] = id;
		}

		return id;
	}

	token_identifier_value_t &token_identifier_value_get(const token_identifier_t x) {
		assert(x < token_identifier_list.size());
		return token_identifier_list[x];
	}

	/* do not define constructor or destructor because this will be used in a union */
	struct token_charstrliteral_t {
		enum class strtype_t {
			T_UNSPEC=0,
			T_BYTE,
			T_UTF8,
			T_UTF16,
			T_UTF32,
			T_WIDE /* this is only used internally, never in an ast node */
		};

		void*					data;
		size_t					length; /* in bytes, even for UTF/WIDE */
		strtype_t				type;

		void init(void) {
			data = NULL;
			length = 0;
			type = strtype_t::T_UNSPEC;
		}

		void on_post_move(void) {
			/* this struct has already been copied, this is the old copy, clear pointers to prevent double free() */
			data = NULL;
			length = 0;
		}

		void on_pre_move(void) { /* our var is about to be replaced by a new var on std::move */
			length = 0;
			if (data) free(data);
			data = NULL;
		}

		void clear_data(void) {
			length = 0;
			if (data) free(data);
			data = NULL;
		}

		void on_delete(void) {
			clear_data();
		}
	};

	struct ast_node_t;

	struct ilc_cls_t {
public:
		static constexpr unsigned int f_signed =      (1u <<  0u);
		static constexpr unsigned int f_unsigned =    (1u <<  1u);
		static constexpr unsigned int f_const =       (1u <<  2u);
		static constexpr unsigned int f_constexpr =   (1u <<  3u);
		static constexpr unsigned int f_compileexpr = (1u <<  4u);
		static constexpr unsigned int f_volatile =    (1u <<  5u);
private:
		static constexpr unsigned int f_bool =        (1u <<  6u);
		static constexpr unsigned int f_char =        (1u <<  7u);
		static constexpr unsigned int f_int =         (1u <<  8u);
		static constexpr unsigned int f_short =       (1u <<  9u);
		static constexpr unsigned int f_long =        (1u << 10u);
		static constexpr unsigned int f_llong =       (1u << 11u);
		static constexpr unsigned int f_void =        (1u << 12u);
		static constexpr unsigned int f_double =      (1u << 13u);
		static constexpr unsigned int f_float =       (1u << 14u);
		static constexpr unsigned int f_static =      (1u << 15u);
		static constexpr unsigned int f_extern =      (1u << 16u);
		static constexpr unsigned int f_near =        (1u << 17u);
		static constexpr unsigned int f_far =         (1u << 18u);
		static constexpr unsigned int f_huge =        (1u << 19u);

		/* do not leave these bits set after parsing */
		static constexpr unsigned int f_non_public =
			f_bool|f_char|f_int|f_short|f_long|f_llong|f_void|f_float|f_double|
			f_static|f_extern|
			f_near|f_far|f_huge;
		static constexpr unsigned int f_builtin_types =
			f_bool|f_char|f_int|f_short|f_long|f_llong|f_void|f_double|f_float;
		static constexpr unsigned int f_signage =
			f_signed|f_unsigned;
		static constexpr unsigned int f_storage =
			f_static|f_extern;
		static constexpr unsigned int f_pointer_type =
			f_near|f_far|f_huge;

public:
		enum {
			STORAGE=0,
			CV,
			TYPE,
			PTYPE
		};

		enum class c_t {
			_none=0,
			_int,
			_float,
			_void,
			_other
		};

		enum class t_t {
			_none=0,
			_bool,
			_char,
			_int,
			_short,
			_long,
			_llong,
			_float,
			_double,
			_longdouble
		};

		enum class p_t {
			_none=0,
			_near,
			_far,
			_huge
		};

		enum class s_t {
			_none=0,
			_static,
			_extern
		};

		token_identifier_t other_id;
		token_type_t other_t;
		unsigned int cls;
		c_t cls_c;
		t_t cls_t;
		p_t cls_p;
		s_t cls_s;

		void init(void);
		bool finalize(void);
		bool empty(void) const;
		bool parse_builtin_type_token(const unsigned int parse_state,const token_t &t);
		bool parse_builtin_type_token(const unsigned int parse_state,const token_t &t0,const token_t &t1);
	};

	bool ilc_cls_t::empty(void) const {
		return cls == 0 && cls_c == c_t::_none && cls_t == t_t::_none && cls_p == p_t::_none && cls_s == s_t::_none &&
			other_id == token_identifier_none && other_t == token_type_t::none;
	}

	void ilc_cls_t::init(void) {
		cls = 0u;
		cls_c = c_t::_none;
		cls_t = t_t::_none;
		cls_p = p_t::_none;
		cls_s = s_t::_none;
		other_t = token_type_t::none;
		other_id = token_identifier_none;
	}

	bool ilc_cls_t::finalize(void) {
/* -------------------- type ------------------- */
		{
			const unsigned int f = cls & f_builtin_types;

			if (f == 0)                        { cls_c = c_t::_none;  cls_t = t_t::_none; }
			else if (f == f_bool)              { cls_c = c_t::_int;   cls_t = t_t::_bool; }
			else if (f == f_char)              { cls_c = c_t::_int;   cls_t = t_t::_char; }
			else if (f == f_int)               { cls_c = c_t::_int;   cls_t = t_t::_int; }
			else if (f == f_short)             { cls_c = c_t::_int;   cls_t = t_t::_short; }
			else if (f == f_long)              { cls_c = c_t::_int;   cls_t = t_t::_long; }
			else if (f == (f_int|f_long))      { cls_c = c_t::_int;   cls_t = t_t::_long; }
			else if (f == f_llong)             { cls_c = c_t::_int;   cls_t = t_t::_llong; }
			else if (f == (f_int|f_llong))     { cls_c = c_t::_int;   cls_t = t_t::_llong; }
			else if (f == (f_long|f_double))   { cls_c = c_t::_float; cls_t = t_t::_longdouble; }
			else if (f == f_double)            { cls_c = c_t::_float; cls_t = t_t::_double; }
			else if (f == f_float)             { cls_c = c_t::_float; cls_t = t_t::_float; }
			else if (f == f_void)              { cls_c = c_t::_void;  cls_t = t_t::_none; }
			else                               return false;
		}
/* -------------------- signed/unsigned -------------------- */
		{
			const unsigned int f = cls & f_signage;

			/* you cannot declare something signed AND unsigned at the same time! */
			if (f == (f_signed|f_unsigned)) return false;
		}
/* -------------------- storage ------------------- */
		{
			const unsigned int f = cls & f_storage;

			if (f == 0)                        { cls_s = s_t::_none; }
			else if (f == f_static)            { cls_s = s_t::_static; }
			else if (f == f_extern)            { cls_s = s_t::_extern; }
			else                               return false;
		}
/* -------------------- pointer type ------------------- */
		{
			const unsigned int f = cls & f_pointer_type;

			if (f == 0)                        { cls_p = p_t::_none; }
			else if (f == f_near)              { cls_p = p_t::_near; }
			else if (f == f_far)               { cls_p = p_t::_far; }
			else if (f == f_huge)              { cls_p = p_t::_huge; }
			else                               return false;
		}

		cls &= ~f_non_public;

		if (cls_c == c_t::_none) {
			if (other_t != token_type_t::none || other_id != token_identifier_none)
				cls_c = c_t::_other;
		}

		if (cls_c == c_t::_other) {
			if (cls & (f_signed|f_unsigned)) return false;
		}

		return true;
	}

	typedef ilc_cls_t token_typeclsif_t;

	struct token_t {
		token_type_t		type = token_type_t::none;
		position_t		position;

		union token_value_t {
			token_intval_t		intval;		// type == intval
			token_floatval_t	floatval;	// type == floatval
			token_identifier_t	identifier;	// type == identifier
			token_charstrliteral_t	chrstrlit;	// type == characterliteral, type == stringliteral
			token_typeclsif_t	typecls;	// type == typeclsif
		} v;

		token_t() { }
		~token_t() { common_delete(); }
		token_t(const token_t &x) = delete;
		token_t &operator=(const token_t &x) = delete;
		token_t(token_t &&x) { common_move(x); }
		token_t &operator=(token_t &&x) { common_move(x); return *this; }

		void make_bool_val(const bool b) {
			common_delete();
			type = token_type_t::intval;
			v.intval.initbool(b);
		}

		private:

		void common_delete(void) {
			switch (type) {
				case token_type_t::characterliteral:
				case token_type_t::stringliteral:
					v.chrstrlit.on_delete();
					break;
				default:
					break;
			}
		}

		void common_move(token_t &x) {
			switch (type) {
				case token_type_t::characterliteral:
				case token_type_t::stringliteral:
					v.chrstrlit.on_pre_move();
					break;
				default:
					break;
			}

			position = x.position;
			type = x.type;
			v = x.v;

			switch (type) {
				case token_type_t::characterliteral:
				case token_type_t::stringliteral:
					x.v.chrstrlit.on_post_move();
					break;
				default:
					break;
			}
		}
	};

	bool ilc_cls_t::parse_builtin_type_token(const unsigned int parse_state,const token_t &t0,const token_t &t1) {
#define TC(tt1,tt2,f) if (t0.type == token_type_t::tt1 && t1.type == token_type_t::tt2) { if (cls & f) return false; cls |= f; return true; }
		switch (parse_state) {
			case TYPE:
				TC(r_long,r_long,f_llong);
			default:
				break;
		}
#undef TC
		return false;
	}

	bool ilc_cls_t::parse_builtin_type_token(const unsigned int parse_state,const token_t &t) {
#define TC(st,t,f) case token_type_t::t: if (parse_state == st) { if (cls & f) return false; cls |= f; return true; } break
		switch (t.type) {
			TC(TYPE,      r_signed,         f_signed);
			TC(TYPE,      r_unsigned,       f_unsigned);
			TC(CV,        r_const,          f_const);
			TC(CV,        r_constexpr,      f_constexpr);
			TC(CV,        r_compileexpr,    f_compileexpr);
			TC(CV,        r_volatile,       f_volatile);
			TC(TYPE,      r_bool,           f_bool);
			TC(TYPE,      r_char,           f_char);
			TC(TYPE,      r_int,            f_int);
			TC(TYPE,      r_short,          f_short);
			TC(TYPE,      r_long,           f_long);
			TC(TYPE,      r_void,           f_void);
			TC(TYPE,      r_double,         f_double);
			TC(TYPE,      r_float,          f_float);
			TC(STORAGE,   r_static,         f_static);
			TC(STORAGE,   r_extern,         f_extern);
			TC(PTYPE,     r_near,           f_near);
			TC(PTYPE,     r_far,            f_far);
			TC(PTYPE,     r_huge,           f_huge);
			default:                        break;
		}
#undef TC

		return false;
	}

	/////////

	static void check(const bool r) {
		if (!r) throw std::runtime_error("Check failed");
	}

	/////////

	enum class ast_node_op_t {
		none=0,
		statement,
		constant,
		comma,
		negate,
		unaryplus,
		logicalnot,
		binarynot,
		add,
		subtract,
		multiply,
		divide,
		modulo,
		assign,
		assignadd,
		assignsubtract,
		assignmultiply,
		assigndivide,
		assignmodulo,
		assignand,
		assignxor,
		assignor,
		ternary,
		logical_or,
		logical_and,
		binary_or,
		binary_xor,
		binary_and,
		equals,
		notequals,
		lessthan,
		greaterthan,
		lessthanorequal,
		greaterthanorequal,
		leftshift,
		rightshift,
		assignleftshift,
		assignrightshift,
		preincrement,
		predecrement,
		dereference,
		addressof,
		postincrement,
		postdecrement,
		structaccess,
		structptraccess,
		arraysubscript,
		subexpression,
		functioncall,
		argument,
		identifier,
		identifier_list,
		strcat,
		scope,
		r_return,
		r_goto,
		r_if,
		r_else,
		r_break,
		r_continue,
		r_switch,
		r_case,
		r_default,
		r_while,
		r_do,
		r_for,
		r_let,
		r_fn,
		r_const,
		r_constexpr,
		r_compileexpr,
		r_static,
		r_extern,
		r_auto,
		r_signed,
		r_unsigned,
		r_long,
		r_short,
		r_int,
		r_float,
		r_double,
		r_void,
		r_volatile,
		r_char,
		ellipsis,
		i_compound_let,
		i_datatype,
		i_anonymous,
		typecast,
		scopeoperator,
		named_arg_required_boundary,
		named_parameter,
		label,
		r_sizeof,
		r_size_t,
		r_ssize_t,
		r_offsetof,
		r_static_assert,
		r_typedef,
		r_using,
		r_namespace,
		r_typeof,
		i_array,
		i_attributes,
		r_bool,
		r_near,
		r_far,
		r_huge,
		r_asm,
		r_this,
		r_struct,
		r_union,
		r_pound_if,
		r_pound_define,
		r_pound_undef,
		r_pound_defined,
		r_pound_defval,
		r_pound_warning,
		r_pound_error,
		r_pound_deftype,
		r_pound_type,
		r_pound_include,
		r_typeclsif,
		i_as,

		maxval
	};

	struct ast_node_t {
		struct ast_node_t*		next = NULL;
		struct ast_node_t*		child = NULL;
		ast_node_op_t			op = ast_node_op_t::none;
		struct token_t			tv;

		struct cursor {
			cursor() { }
			cursor(ast_node_t* &n) { c = &n; }

			ast_node_t* &operator* () { return *c; }
			const ast_node_t* operator* () const { return (const ast_node_t*)(*c); }
			ast_node_t** operator=(ast_node_t **nc) { return (c = nc); }

			cursor &set_and_then_to_next(ast_node_t *s) {
				ast_node_t::set(*c,s);
				to_next_until_last();
				return *this;
			}

			cursor &to_next(void) {
				assert(c != NULL);
				assert((*c) != NULL);
				c = &((*c)->next);
				return *this;
			}

			cursor &to_next_until_last(void) {
				assert(c != NULL);
				while (*c) c = &((*c)->next);
				return *this;
			}

			cursor &to_child(void) {
				assert(c != NULL);
				assert((*c) != NULL);
				c = &((*c)->child);
				return *this;
			}

			ast_node_t* &ref_next(void) {
				assert(c != NULL);
				assert((*c) != NULL);
				return (*c)->next;
			}

			ast_node_t* &ref_child(void) {
				assert(c != NULL);
				assert((*c) != NULL);
				return (*c)->child;
			}

			ast_node_t**		c = NULL;
		};

		ast_node_t() {
		}

		ast_node_t(const ast_node_op_t init_op) : op(init_op) {
		}

		ast_node_t(const ast_node_op_t init_op,token_t &init_tv) : op(init_op) {
			tv = std::move(init_tv);
		}

		static void set(ast_node_t* &d,ast_node_t *s) {
			assert(d == NULL);
			assert(s != NULL);
			d = s;
		}

		static void goto_next(ast_node_t* &n) {
			assert(n != NULL);
			assert(n->next != NULL);
			n = n->next;
		}

		static ast_node_t *mk_constant(token_t &t) {
			return new ast_node_t(ast_node_op_t::constant, t);
		}

		static ast_node_t *mk_bool_constant(token_t &t,const bool b) {
			ast_node_t *r = new ast_node_t(ast_node_op_t::constant, t);
			r->tv.make_bool_val(b);
			return r;
		}

		/* [parent]
		 *
		 * to
		 *
		 * [new parent]
		 *  \- [old parent] */
		static ast_node_t *parent_to_child_with_new_parent(ast_node_t* &cur_p,ast_node_t* const new_p) {
			assert(cur_p != NULL);
			assert(new_p != NULL);
			new_p->set_child(cur_p);
			return (cur_p = new_p);
		}

		void set_next(ast_node_t* const n) {
			set(next,n);
		}

		void set_child(ast_node_t* const c) {
			set(child,c);
		}

		bool unlink_child(void) {
			if (child) {
				struct ast_node_t *dm = child;
				child = child->child;
				delete dm;
				return true;
			}
			return false;
		}

		bool unlink_next(void) {
			if (next) {
				struct ast_node_t *dm = next;
				next = next->next;
				delete dm;
				return true;
			}

			return false;
		}

		void free_children(void) {
			while (child) {
				child->free_next();
				unlink_child();
			}
		}

		void free_next(void) {
			while (next) {
				next->free_children();
				unlink_next();
			}
		}

		void free_nodes(void) {
			free_next();
			free_children();
		}
	};

	/////////

	struct compiler {
		compiler() {
		}

		~compiler() {
		}

		void set_source_cb(refill_function_t f) {
			pb_refill = f;
		}

		void set_source_name(const char *x) {
			pb_ctx.source_name = x;
		}

		void set_source_name(const std::string &x) {
			pb_ctx.source_name = x;
		}

		void set_source_ctx(void *ptr=NULL,size_t sz=0) {
			pb_ctx.free();
			pb_ctx.ptr_size = sz;
			pb_ctx.ownership = false;
			pb_ctx.ptr = (unsigned char*)ptr;
		}

		bool alloc_source_ctx(size_t sz) {
			return pb_ctx.alloc(sz);
		}

		void free_source_ctx(void) {
			pb_ctx.free();
		}

		unsigned char *source_ctx(void) {
			return pb_ctx.ptr;
		}

		size_t source_ctx_size(void) const {
			return pb_ctx.ptr_size;
		}

		void update_pos(const uint32_t c) {
			if (c == '\n')
				tok_pos.next_line();
			else if (c != '\r')
				tok_pos.col++;

			tok_pos.pchar = c;
		}

		char peekb(const size_t ahead=0) {
			if ((pb.read+ahead) >= pb.end) refill();
			if ((pb.read+ahead) < pb.end) return *(pb.read);
			return 0;
		}

		static constexpr unsigned int getb_flag_noposupd = (1u << 0u);

		void skipb(const unsigned int flags=0) {
			if (pb.read == pb.end) refill();
			if (pb.read < pb.end) { if (!(flags & getb_flag_noposupd)) update_pos(*pb.read); pb.read++; }
		}

		char getb(const unsigned int flags=0) {
			const char r = peekb();
			skipb(flags);
			return r;
		}

		uint32_t getb_utf8(void) {
			uint32_t ub = (uint32_t)((unsigned char)getb(getb_flag_noposupd));

			if (ub >= 0xFE) {
				update_pos(ub);
				return 0; // invalid
			}
			else if (ub >= 0xC0) {
				/* (2) 110xxxxx 10xxxxxx */
				/* (3) 1110xxxx 10xxxxxx 10xxxxxx */
				/* (4) 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx */
				/* (5) 111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx */
				/* (6) 1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx */
				unsigned char c;
				size_t len = 2;
				{
					uint8_t tmp = (uint8_t)ub;
					while (tmp >= 0xE0u) { len++; tmp = (tmp << 1u) & 0xFFu; } /* 8-bit shift out top bits until value is 110xxxxxx to determine length */
				}

				ub &= (1u << (7u - len)) - 1u;
				len--;

				while (len > 0 && ((c=peekb()) & 0xC0) == 0x80) { /* 10xxxxxx */
					ub = (ub << 6u) + (c & 0x3Fu);
					skipb();
					len--;
				}

				if (len > 0) {
					update_pos(ub);
					return 0; // incomplete
				}
			}
			else if (ub >= 0x80) {
				update_pos(ub);
				return 0; // we're in the middle of a char
			}

			update_pos(ub);
			return ub;
		}

		void refill(void);
		bool compile(void);
		void free_ast(void);
		void whitespace(void);
		void gtok(token_t &t);
		void gtok_number(token_t &t);
		int64_t getb_hex_cpp23(void);
		int64_t getb_octal_cpp23(void);
		void gtok_identifier(token_t &t);
		void gtok_prep_number_proc(void);
		int64_t getb_hex(unsigned int mc);
		ast_node_t *string_literals(void);
		int64_t getb_octal(unsigned int mc);
		bool statement(ast_node_t::cursor &nc);
		bool expression(ast_node_t::cursor &nc);
		void skip_numeric_digit_separator(void);
		bool subexpression(ast_node_t::cursor &nc);
		bool type_deref_list(ast_node_t::cursor &p_nc);
		bool statement_semicolon_check(ast_node_t::cursor &s_nc);
		bool typecast(ast_node_t::cursor &p_nc,const ast_node_op_t op);
		bool type_list(ast_node_t::cursor &p_nc,const ast_node_op_t op);
		bool next_token_is_type(void);

		bool primary_expression(ast_node_t::cursor &nc);

		bool cpp_scope_expression(ast_node_t::cursor &nc);

		bool functioncall_operator(ast_node_t::cursor &nc);

		bool array_operator(ast_node_t::cursor &nc);

		bool postfix_expression(ast_node_t::cursor &nc);
		bool postfix_expression_common(ast_node_t::cursor &nc,const ast_node_op_t anot,bool second);

		bool unary_expression(ast_node_t::cursor &nc);
		bool unary_expression_common(ast_node_t::cursor &nc,const ast_node_op_t anot);

		bool additive_expression(ast_node_t::cursor &nc);
		bool additive_expression_common(ast_node_t::cursor &nc,const ast_node_op_t anot);

		bool shift_expression(ast_node_t::cursor &nc);
		bool shift_expression_common(ast_node_t::cursor &nc,const ast_node_op_t anot);

		bool relational_expression(ast_node_t::cursor &nc);
		bool relational_expression_common(ast_node_t::cursor &nc,const ast_node_op_t anot);

		bool equality_expression(ast_node_t::cursor &nc);
		bool equality_expression_common(ast_node_t::cursor &nc,const ast_node_op_t anot);

		bool binary_and_expression(ast_node_t::cursor &nc);

		bool binary_xor_expression(ast_node_t::cursor &nc);

		bool binary_or_expression(ast_node_t::cursor &nc);

		bool logical_and_expression(ast_node_t::cursor &nc);

		bool logical_or_expression(ast_node_t::cursor &nc);

		bool conditional_expression(ast_node_t::cursor &nc);

		bool assignment_expression(ast_node_t::cursor &nc);
		bool assignment_expression_common(ast_node_t::cursor &nc,const ast_node_op_t anot);

		bool multiplicative_expression(ast_node_t::cursor &nc);
		bool multiplicative_expression_common(ast_node_t::cursor &nc,const ast_node_op_t anot);

		bool statement(ast_node_t* &rnode,ast_node_t* &apnode);
		void error_msg(const std::string msg,const position_t &pos);
		int64_t getb_with_escape(token_charstrliteral_t::strtype_t typ);
		bool statement_does_not_need_semicolon(ast_node_t* const apnode);
		void gtok_chrstr_H_literal(const char qu,token_t &t,unsigned int flags=0,token_charstrliteral_t::strtype_t strtype = token_charstrliteral_t::strtype_t::T_BYTE);
		void gtok_chrstr_literal(const char qu,token_t &t,token_charstrliteral_t::strtype_t strtype = token_charstrliteral_t::strtype_t::T_BYTE);
		bool gtok_check_ahead_H_identifier(const std::vector<uint8_t> &identifier,const char qu);
		int64_t getb_csc(token_charstrliteral_t::strtype_t typ);

		/* type and identifiers flags */
		static constexpr unsigned int TYPE_AND_IDENT_FL_ALLOW_FN = (1u << 0u);

		/* type and identifiers return value */
		static constexpr unsigned int TYPE_AND_IDENT_RT_FOUND = (1u << 0u);
		static constexpr unsigned int TYPE_AND_IDENT_RT_FN = (1u << 1u);
		static constexpr unsigned int TYPE_AND_IDENT_RT_REF = (1u << 2u); /* * or & was involved */

		ast_node_t*		root_node = NULL;

		static constexpr size_t tok_buf_size = 32;
		static constexpr size_t tok_buf_threshhold = 16;
		token_t			tok_buf[tok_buf_size];
		size_t			tok_buf_i = 0,tok_buf_o = 0;
		token_t			tok_empty;

		/* position tracking */
		position_t			tok_pos;
		token_source_name_list_t*	tok_snamelist = NULL;

		void set_source_name_list(token_source_name_list_t *l) {
			tok_snamelist = l;
		}

		void tok_buf_clear(void) {
			tok_buf_i = tok_buf_o = 0;
		}

		bool tok_buf_empty(void) const {
			return tok_buf_o == tok_buf_i;
		}

		size_t tok_buf_avail(void) const {
			return tok_buf_o - tok_buf_i;
		}

		size_t tok_buf_canadd(void) const {
			return tok_buf_size - tok_buf_o;
		}

		bool tok_buf_sanity_check(void) const {
			return tok_buf_i <= tok_buf_o && tok_buf_i <= tok_buf_size && tok_buf_o <= tok_buf_size;
		}

		void tok_buf_flush(void) {
			if (tok_buf_i != 0u) {
				assert(tok_buf_sanity_check());
				const size_t rem = tok_buf_avail();
				assert((tok_buf_i+rem) <= tok_buf_size);
				for (size_t i=0;i < rem;i++) tok_buf[i] = std::move(tok_buf[i+tok_buf_i]);
				tok_buf_o = rem;
				tok_buf_i = 0;
				assert(tok_buf_avail() == rem);
				assert(tok_buf_sanity_check());
			}
		}

		void tok_buf_lazy_flush(void) {
			if (tok_buf_i >= tok_buf_threshhold) tok_buf_flush();
		}

		void tok_buf_refill(void) {
			token_t t;

			tok_buf_lazy_flush();

			size_t todo = tok_buf_canadd();
			while (todo-- != 0u) {
				gtok(t);
				if (t.type == token_type_t::eof) break;
				tok_buf[tok_buf_o++] = std::move(t);
			}

			assert(tok_buf_sanity_check());
		}

		token_t &tok_bufpeek(const size_t p=0) {
			if ((tok_buf_i+p) >= tok_buf_o) tok_buf_refill();
			if ((tok_buf_i+p) < tok_buf_o) return tok_buf[tok_buf_i+p];
			return tok_empty;
		}

		void tok_bufdiscard(size_t count=1) {
			while (count-- != 0u) {
				if (tok_buf_i < tok_buf_o) tok_buf_i++;
				else break;
			}
		}

		token_t &tok_bufget(void) {
			/* WARNING: Return reference is valid UNTIL the next call to this function or any other tok_buf*() function.
			 *          If you need to keep the value past that point declare a token_t and std::move() this reference to it. */
			tok_buf_refill();
			if (tok_buf_i < tok_buf_o) return tok_buf[tok_buf_i++];
			return tok_empty;
		}

		private:
		parse_buffer		pb;
		context_t		pb_ctx;
		refill_function_t	pb_refill = refill_null;
	};

	void compiler::refill(void) {
		assert(pb.sanity_check());
		pb.lazy_flush();
		pb.refill(pb_refill,pb_ctx);
		assert(pb.sanity_check());
	}

	void token_to_string(std::string &s,const token_t &t);

	ast_node_t *compiler::string_literals(void) {
		assert(tok_bufpeek().type == token_type_t::stringliteral);

		ast_node_t *r = NULL;
		ast_node_t::cursor nc(r);

		/* remember the string type, we're not going to allow string concatenation of different types */
		const token_charstrliteral_t::strtype_t stype = tok_bufpeek().v.chrstrlit.type;

		ast_node_t::set(*nc, ast_node_t::mk_constant(tok_bufget()));

		if (tok_bufpeek().type == token_type_t::stringliteral) {
			ast_node_t::cursor c_nc = nc;
			c_nc.to_next();

			ast_node_t::parent_to_child_with_new_parent(*nc,/*new parent*/new ast_node_t(ast_node_op_t::strcat));

			while (tok_bufpeek().type == token_type_t::stringliteral) {
				if (tok_bufpeek().v.chrstrlit.type != stype) {
					error_msg("Cannot string concat dissimilar string types",tok_bufpeek().position);
					break; /* if the string type differs, stop */
				}

				ast_node_t::set(*c_nc, ast_node_t::mk_constant(tok_bufget()));
				c_nc.to_next();
			}
		}

		return r;
	}

	static const unsigned char type_list_ps_order[] = {
		ilc_cls_t::STORAGE,
		ilc_cls_t::CV,
		ilc_cls_t::TYPE,
		ilc_cls_t::PTYPE,
	};

	static const unsigned char type_deref_list_ps_order[] = {
		ilc_cls_t::CV,
		ilc_cls_t::PTYPE
	};

	bool compiler::type_list(ast_node_t::cursor &p_nc,const ast_node_op_t op) {
		ast_node_t::cursor nc = p_nc;
		ilc_cls_t ilc; ilc.init();

		for (size_t pi=0;pi < sizeof(type_list_ps_order)/sizeof(type_list_ps_order[0]);pi++) {
			while (1) {
				if (tok_bufpeek(1).type == token_type_t::openparen) break; /* we're looking for int, long, etc. not int(), long() */
				if (ilc.parse_builtin_type_token(type_list_ps_order[pi],tok_bufpeek(0),tok_bufpeek(1))) tok_bufdiscard(2);
				else if (ilc.parse_builtin_type_token(type_list_ps_order[pi],tok_bufpeek())) tok_bufdiscard();
				else break;
			}
		}

		if (ilc.cls_c == ilc_cls_t::c_t::_none) {
			switch (tok_bufpeek().type) {
				case token_type_t::r_auto:
				case token_type_t::r_size_t:
				case token_type_t::r_ssize_t:
					ilc.other_t = tok_bufpeek().type; tok_bufdiscard(); break;
				default:
					break;
			}
		}

		if (ilc.cls != 0 || ilc.cls_c != ilc_cls_t::c_t::_none || ilc.other_t != token_type_t::none || ilc.other_id != token_identifier_none) {
			ast_node_t::set(*nc, new ast_node_t(op));
			(*nc)->tv.type = token_type_t::r_typeclsif;
			if (!ilc.finalize()) return false; /* returns false if invalid combination */
			(*nc)->tv.v.typecls = std::move(ilc);
			nc.to_child();
		}

		return true;
	}

	bool compiler::type_deref_list(ast_node_t::cursor &p_nc) {
		ast_node_t::cursor nc = p_nc;

		while (1) {
			switch (tok_bufpeek().type) {
				case token_type_t::star:
					assert((*nc)->tv.type == token_type_t::r_typeclsif);
					ast_node_t::parent_to_child_with_new_parent(*nc,/*new parent*/new ast_node_t(ast_node_op_t::dereference, tok_bufget()));
					(*nc)->tv.type = token_type_t::r_typeclsif;
					(*nc)->tv.v.typecls.init();
					break;
				case token_type_t::ampersand:
					assert((*nc)->tv.type == token_type_t::r_typeclsif);
					ast_node_t::parent_to_child_with_new_parent(*nc,/*new parent*/new ast_node_t(ast_node_op_t::addressof, tok_bufget()));
					(*nc)->tv.type = token_type_t::r_typeclsif;
					(*nc)->tv.v.typecls.init();
					break;
				case token_type_t::ampersandampersand:
					assert((*nc)->tv.type == token_type_t::r_typeclsif);

					ast_node_t::parent_to_child_with_new_parent(*nc,/*new parent*/new ast_node_t(ast_node_op_t::addressof));
					(*nc)->tv.type = token_type_t::r_typeclsif;
					(*nc)->tv.v.typecls.init();

					ast_node_t::parent_to_child_with_new_parent(*nc,/*new parent*/new ast_node_t(ast_node_op_t::addressof, tok_bufget()));
					(*nc)->tv.type = token_type_t::r_typeclsif;
					(*nc)->tv.v.typecls.init();
					break;
				case token_type_t::r_const:
				case token_type_t::r_volatile:
				case token_type_t::r_huge:
				case token_type_t::r_near:
				case token_type_t::r_far:
					assert((*nc)->tv.type == token_type_t::r_typeclsif);
					if (!(*nc)->tv.v.typecls.empty()) return false;
					for (size_t pi=0;pi < sizeof(type_deref_list_ps_order)/sizeof(type_deref_list_ps_order[0]);pi++) {
						while (1) {
							if (tok_bufpeek(1).type == token_type_t::openparen) break; /* we're looking for int, long, etc. not int(), long() */
							if ((*nc)->tv.v.typecls.parse_builtin_type_token(type_deref_list_ps_order[pi],tok_bufpeek(0),tok_bufpeek(1))) tok_bufdiscard(2);
							else if ((*nc)->tv.v.typecls.parse_builtin_type_token(type_deref_list_ps_order[pi],tok_bufpeek())) tok_bufdiscard();
							else break;
						}
					}
					if (!(*nc)->tv.v.typecls.finalize()) return false; /* returns false if invalid combination */
					break;
				default: goto done_parsing;
			}
		}
done_parsing:
		return true;
	}

	bool compiler::typecast(ast_node_t::cursor &nc,const ast_node_op_t op) {
		if (!type_list(nc,op)) return false;

		if (*nc) {
			ast_node_t::cursor sub_nc = nc; sub_nc.to_child();
			ast_node_t::set(*sub_nc, new ast_node_t(ast_node_op_t::i_as));
			(*sub_nc)->tv.type = token_type_t::r_typeclsif;
			(*sub_nc)->tv.v.typecls.init();
			if (!type_deref_list(sub_nc)) return false;
		}

		return true;
	}

	bool compiler::subexpression(ast_node_t::cursor &nc) {
		assert(tok_bufpeek().type == token_type_t::openparen);

		{
			ast_node_t *chk = NULL;
			ast_node_t::cursor chk_nc(chk);
			token_t tok = std::move(tok_bufget());

			if (!typecast(chk_nc,ast_node_op_t::typecast)) return false;

			if (*chk_nc) { /* typecast() filled in the node */
				ast_node_t::set(*nc, *chk_nc);
			}
			else {
				if (!expression(chk_nc)) return false;
				ast_node_t::set(*nc, new ast_node_t(ast_node_op_t::subexpression,tok)); (*nc)->set_child(*chk_nc);
			}
		}

		if (tok_bufpeek().type != token_type_t::closeparen) return false;
		tok_bufdiscard();

		return true;
	}

	bool compiler::primary_expression(ast_node_t::cursor &nc) {
		/* the bufpeek/get functions return a stock empty token if we read beyond available tokens */
		switch (tok_bufpeek().type) {
			case token_type_t::intval:
			case token_type_t::floatval:
			case token_type_t::characterliteral:
				ast_node_t::set(*nc, ast_node_t::mk_constant(tok_bufget())); return true;
			case token_type_t::stringliteral:
				ast_node_t::set(*nc, string_literals()); return true;
			case token_type_t::r_true:                ast_node_t::set(*nc, ast_node_t::mk_bool_constant(tok_bufget(),true)); return true;
			case token_type_t::r_false:               ast_node_t::set(*nc, ast_node_t::mk_bool_constant(tok_bufget(),false)); return true;
			case token_type_t::r_const:               ast_node_t::set(*nc, new ast_node_t(ast_node_op_t::r_const, tok_bufget())); return true;
			case token_type_t::r_constexpr:           ast_node_t::set(*nc, new ast_node_t(ast_node_op_t::r_constexpr, tok_bufget())); return true;
			case token_type_t::r_compileexpr:         ast_node_t::set(*nc, new ast_node_t(ast_node_op_t::r_compileexpr, tok_bufget())); return true;
			case token_type_t::r_sizeof:              ast_node_t::set(*nc, new ast_node_t(ast_node_op_t::r_sizeof, tok_bufget())); return true;
			case token_type_t::r_offsetof:            ast_node_t::set(*nc, new ast_node_t(ast_node_op_t::r_offsetof, tok_bufget())); return true;
			case token_type_t::r_static_assert:       ast_node_t::set(*nc, new ast_node_t(ast_node_op_t::r_static_assert, tok_bufget())); return true;
			case token_type_t::r_size_t:              ast_node_t::set(*nc, new ast_node_t(ast_node_op_t::r_size_t, tok_bufget())); return true;
			case token_type_t::r_ssize_t:             ast_node_t::set(*nc, new ast_node_t(ast_node_op_t::r_ssize_t, tok_bufget())); return true;
			case token_type_t::r_static:              ast_node_t::set(*nc, new ast_node_t(ast_node_op_t::r_static, tok_bufget())); return true;
			case token_type_t::r_extern:              ast_node_t::set(*nc, new ast_node_t(ast_node_op_t::r_extern, tok_bufget())); return true;
			case token_type_t::r_auto:                ast_node_t::set(*nc, new ast_node_t(ast_node_op_t::r_auto, tok_bufget())); return true;
			case token_type_t::r_signed:              ast_node_t::set(*nc, new ast_node_t(ast_node_op_t::r_signed, tok_bufget())); return true;
			case token_type_t::r_unsigned:            ast_node_t::set(*nc, new ast_node_t(ast_node_op_t::r_unsigned, tok_bufget())); return true;
			case token_type_t::r_long:                ast_node_t::set(*nc, new ast_node_t(ast_node_op_t::r_long, tok_bufget())); return true;
			case token_type_t::r_short:               ast_node_t::set(*nc, new ast_node_t(ast_node_op_t::r_short, tok_bufget())); return true;
			case token_type_t::r_int:                 ast_node_t::set(*nc, new ast_node_t(ast_node_op_t::r_int, tok_bufget())); return true;
			case token_type_t::r_bool:                ast_node_t::set(*nc, new ast_node_t(ast_node_op_t::r_bool, tok_bufget())); return true;
			case token_type_t::r_near:                ast_node_t::set(*nc, new ast_node_t(ast_node_op_t::r_near, tok_bufget())); return true;
			case token_type_t::r_far:                 ast_node_t::set(*nc, new ast_node_t(ast_node_op_t::r_far, tok_bufget())); return true;
			case token_type_t::r_huge:                ast_node_t::set(*nc, new ast_node_t(ast_node_op_t::r_huge, tok_bufget())); return true;
			case token_type_t::r_float:               ast_node_t::set(*nc, new ast_node_t(ast_node_op_t::r_float, tok_bufget())); return true;
			case token_type_t::r_double:              ast_node_t::set(*nc, new ast_node_t(ast_node_op_t::r_double, tok_bufget())); return true;
			case token_type_t::r_this:                ast_node_t::set(*nc, new ast_node_t(ast_node_op_t::r_this, tok_bufget())); return true;
			case token_type_t::r_void:                ast_node_t::set(*nc, new ast_node_t(ast_node_op_t::r_void, tok_bufget())); return true;
			case token_type_t::r_char:                ast_node_t::set(*nc, new ast_node_t(ast_node_op_t::r_char, tok_bufget())); return true;
			case token_type_t::ellipsis:              ast_node_t::set(*nc, new ast_node_t(ast_node_op_t::ellipsis, tok_bufget())); return true;
			case token_type_t::r_volatile:            ast_node_t::set(*nc, new ast_node_t(ast_node_op_t::r_volatile, tok_bufget())); return true;
			case token_type_t::identifier:            ast_node_t::set(*nc, new ast_node_t(ast_node_op_t::identifier, tok_bufget())); return true;
			case token_type_t::openparen:             return subexpression(nc);

			default: break;
		}

		return false;
	}

	/* [https://en.cppreference.com/w/cpp/language/operator_precedence] level 1 */
	bool compiler::cpp_scope_expression(ast_node_t::cursor &nc) {
#define NLEX primary_expression
		if (!NLEX(nc)) return false;

		if ((*nc)->op == ast_node_op_t::identifier) {
			while (tok_bufpeek().type == token_type_t::coloncolon) { /* :: but only identifiers */
				tok_bufdiscard(); /* eat it */

				/* [::]
				 *  \
				 *   +-- [left expr] -> [right expr] */

				ast_node_t::cursor cur_nc = nc; cur_nc.to_next();
				ast_node_t::parent_to_child_with_new_parent(*nc,/*new parent*/new ast_node_t(ast_node_op_t::scopeoperator));

				/* must be an identifier */
				if (tok_bufpeek().type == token_type_t::identifier) {
					if (!NLEX(cur_nc)) return false;
				}
				else {
					return false;
				}
			}
		}
#undef NLEX
		return true;
	}

#define NLEX cpp_scope_expression
	bool compiler::postfix_expression_common(ast_node_t::cursor &nc,const ast_node_op_t anot,bool second) {
		if ((*nc)->op == ast_node_op_t::identifier_list) return false;
		ast_node_t::cursor cur_nc = nc; cur_nc.to_next(); /* save cursor */
		ast_node_t::parent_to_child_with_new_parent(*nc,/*new parent*/new ast_node_t(anot,tok_bufget())); /* make child of comma */

		if (second) {
			if (!NLEX(cur_nc))
				return false;
		}

		return true;
	}

	bool compiler::functioncall_operator(ast_node_t::cursor &nc) {
		assert(tok_bufpeek().type == token_type_t::openparen);
		ast_node_t::cursor cur_nc = nc; cur_nc.to_next(); /* save cursor */
		ast_node_t::parent_to_child_with_new_parent(*nc,/*new parent*/new ast_node_t(ast_node_op_t::functioncall,tok_bufget())); /* make child of comma */

		do {
			if (tok_bufpeek().type == token_type_t::closeparen) break;

			ast_node_t::set(*cur_nc, new ast_node_t(ast_node_op_t::argument));
			ast_node_t::cursor e_nc = cur_nc; cur_nc.to_next(); e_nc.to_child();

			/* name parameter support */
			if (tok_bufpeek(0).type == token_type_t::identifier && tok_bufpeek(1).type == token_type_t::colon) {
				ast_node_t::set(*e_nc, new ast_node_t(ast_node_op_t::named_parameter));
				ast_node_t::cursor ei_nc = e_nc; ei_nc.to_child(); e_nc.to_next();

				if (!cpp_scope_expression(ei_nc))
					return false;

				tok_bufdiscard();
			}

			if (!assignment_expression(e_nc))
				return false;

			if (tok_bufpeek().type == token_type_t::closeparen) {
				break;
			}
			else if (tok_bufpeek().type == token_type_t::comma) {
				tok_bufdiscard();
			}
			else {
				return false;
			}
		} while (1);

		if (tok_bufpeek().type != token_type_t::closeparen) return false;
		tok_bufdiscard();

		return true;
	}

	bool compiler::array_operator(ast_node_t::cursor &nc) {
		assert(tok_bufpeek().type == token_type_t::leftsquarebracket);
		ast_node_t::cursor cur_nc = nc; cur_nc.to_next(); /* save cursor */
		ast_node_t::parent_to_child_with_new_parent(*nc,/*new parent*/new ast_node_t(ast_node_op_t::arraysubscript,tok_bufget())); /* make child of comma */

		if (!expression(cur_nc))
			return false;

		if (tok_bufpeek().type != token_type_t::rightsquarebracket) return false;
		tok_bufdiscard();

		return true;
	}

	/* [https://en.cppreference.com/w/c/language/operator_precedence] level 1 */
	/* [https://en.cppreference.com/w/cpp/language/operator_precedence] level 2 */
	bool compiler::postfix_expression(ast_node_t::cursor &nc) {
		if (!NLEX(nc)) return false;

		while (1) {
			switch (tok_bufpeek().type) {
				case token_type_t::plusplus:               if (!postfix_expression_common(nc,ast_node_op_t::postincrement,false)) return false; break;
				case token_type_t::minusminus:             if (!postfix_expression_common(nc,ast_node_op_t::postdecrement,false)) return false; break;
				case token_type_t::period:                 if (!postfix_expression_common(nc,ast_node_op_t::structaccess,true)) return false; break;
				case token_type_t::pointerarrow:           if (!postfix_expression_common(nc,ast_node_op_t::structptraccess,true)) return false; break;
				case token_type_t::leftsquarebracket:      if (!array_operator(nc)) return false; break;
				case token_type_t::openparen:
					if ((*nc)->op == ast_node_op_t::typecast) goto done_parsing; /* you cannot function call a typecast */
					return functioncall_operator(nc);
				default:                                   goto done_parsing;
			}
		}
done_parsing:
		return true;
	}
#undef NLEX

#define NLEX postfix_expression
	bool compiler::unary_expression_common(ast_node_t::cursor &nc,const ast_node_op_t anot) {
		ast_node_t::set(*nc, new ast_node_t(anot,tok_bufget()));
		ast_node_t::cursor cur_nc = nc; cur_nc.to_child();
		return unary_expression(cur_nc);
	}

	/* [https://en.cppreference.com/w/c/language/operator_precedence] level 2 */
	/* [https://en.cppreference.com/w/cpp/language/operator_precedence] level 3 */
	bool compiler::unary_expression(ast_node_t::cursor &nc) {
		while (1) {
			switch (tok_bufpeek().type) {
				case token_type_t::ampersandampersand: {
					ast_node_t::set(*nc, new ast_node_t(ast_node_op_t::addressof,tok_bufget()));
					ast_node_t::cursor cur_nc = nc; cur_nc.to_child();
					ast_node_t::set(*cur_nc, new ast_node_t(ast_node_op_t::addressof));
					cur_nc.to_child();
					return unary_expression(cur_nc); }

				case token_type_t::ampersand:              return unary_expression_common(nc,ast_node_op_t::addressof);
				case token_type_t::star:                   return unary_expression_common(nc,ast_node_op_t::dereference);
				case token_type_t::minusminus:             return unary_expression_common(nc,ast_node_op_t::predecrement);
				case token_type_t::plusplus:               return unary_expression_common(nc,ast_node_op_t::preincrement);
				case token_type_t::minus:                  return unary_expression_common(nc,ast_node_op_t::negate);
				case token_type_t::plus:                   return unary_expression_common(nc,ast_node_op_t::unaryplus);
				case token_type_t::exclamation:            return unary_expression_common(nc,ast_node_op_t::logicalnot);
				case token_type_t::tilde:                  return unary_expression_common(nc,ast_node_op_t::binarynot);

				default:
					if (!NLEX(nc)) return false;
					if ((*nc)->op == ast_node_op_t::typecast) {
						ast_node_t::cursor cur_nc = nc; cur_nc.to_child().to_next_until_last();
						return unary_expression(cur_nc);
					}
					return true;
			}
		}

		return true;
	}
#undef NLEX

#define NLEX unary_expression
	bool compiler::multiplicative_expression_common(ast_node_t::cursor &nc,const ast_node_op_t anot) {
		if ((*nc)->op == ast_node_op_t::identifier_list) return false;
		ast_node_t::cursor cur_nc = nc; cur_nc.to_next(); /* save cursor */
		ast_node_t::parent_to_child_with_new_parent(*nc,/*new parent*/new ast_node_t(anot,tok_bufget())); /* make child of comma */
		return NLEX(cur_nc);
	}

	/* [https://en.cppreference.com/w/c/language/operator_precedence] level 3 */
	/* [https://en.cppreference.com/w/cpp/language/operator_precedence] level 5 */
	bool compiler::multiplicative_expression(ast_node_t::cursor &nc) {
		if (!NLEX(nc))
			return false;

		/* left operator right
		 *
		 * [operator]
		 *  \
		 *   +-- [left expr] -> [right expr] */

		while (1) {
			switch (tok_bufpeek().type) {
				case token_type_t::star:                   if (!multiplicative_expression_common(nc,ast_node_op_t::multiply)) return false; break;
				case token_type_t::slash:                  if (!multiplicative_expression_common(nc,ast_node_op_t::divide)) return false; break;
				case token_type_t::percent:                if (!multiplicative_expression_common(nc,ast_node_op_t::modulo)) return false; break;
				default:                                   goto done_parsing;
			}
		}
done_parsing:
		return true;
	}
#undef NLEX

#define NLEX multiplicative_expression
	bool compiler::additive_expression_common(ast_node_t::cursor &nc,const ast_node_op_t anot) {
		if ((*nc)->op == ast_node_op_t::identifier_list) return false;
		ast_node_t::cursor cur_nc = nc; cur_nc.to_next(); /* save cursor */
		ast_node_t::parent_to_child_with_new_parent(*nc,/*new parent*/new ast_node_t(anot,tok_bufget())); /* make child of comma */
		return NLEX(cur_nc);
	}

	/* [https://en.cppreference.com/w/c/language/operator_precedence] level 4 */
	/* [https://en.cppreference.com/w/cpp/language/operator_precedence] level 6 */
	bool compiler::additive_expression(ast_node_t::cursor &nc) {
		if (!NLEX(nc)) return false;

		/* left operator right
		 *
		 * [operator]
		 *  \
		 *   +-- [left expr] -> [right expr] */

		while (1) {
			switch (tok_bufpeek().type) {
				case token_type_t::plus:                   if (!additive_expression_common(nc,ast_node_op_t::add)) return false; break;
				case token_type_t::minus:                  if (!additive_expression_common(nc,ast_node_op_t::subtract)) return false; break;
				default:                                   goto done_parsing;
			}
		}
done_parsing:
		return true;
	}
#undef NLEX

#define NLEX additive_expression
	bool compiler::shift_expression_common(ast_node_t::cursor &nc,const ast_node_op_t anot) {
		if ((*nc)->op == ast_node_op_t::identifier_list) return false;
		ast_node_t::cursor cur_nc = nc; cur_nc.to_next(); /* save cursor */
		ast_node_t::parent_to_child_with_new_parent(*nc,/*new parent*/new ast_node_t(anot,tok_bufget())); /* make child of comma */
		return NLEX(cur_nc);
	}

	/* [https://en.cppreference.com/w/c/language/operator_precedence] level 5 */
	/* [https://en.cppreference.com/w/cpp/language/operator_precedence] level 7 */
	bool compiler::shift_expression(ast_node_t::cursor &nc) {
		if (!NLEX(nc)) return false;

		/* left operator right
		 *
		 * [operator]
		 *  \
		 *   +-- [left expr] -> [right expr] */

		while (1) {
			switch (tok_bufpeek().type) {
				case token_type_t::leftleftangle:          if (!shift_expression_common(nc,ast_node_op_t::leftshift)) return false; break;
				case token_type_t::rightrightangle:        if (!shift_expression_common(nc,ast_node_op_t::rightshift)) return false; break;
				default:                                   goto done_parsing;
			}
		}
done_parsing:
		return true;
	}
#undef NLEX

#define NLEX shift_expression
	bool compiler::relational_expression_common(ast_node_t::cursor &nc,const ast_node_op_t anot) {
		if ((*nc)->op == ast_node_op_t::identifier_list) return false;
		ast_node_t::cursor cur_nc = nc; cur_nc.to_next(); /* save cursor */
		ast_node_t::parent_to_child_with_new_parent(*nc,/*new parent*/new ast_node_t(anot,tok_bufget())); /* make child of comma */
		return NLEX(cur_nc);
	}

	/* [https://en.cppreference.com/w/c/language/operator_precedence] level 6 */
	/* [https://en.cppreference.com/w/cpp/language/operator_precedence] level 9 */
	bool compiler::relational_expression(ast_node_t::cursor &nc) {
		if (!NLEX(nc)) return false;

		/* left operator right
		 *
		 * [operator]
		 *  \
		 *   +-- [left expr] -> [right expr] */

		while (1) {
			switch (tok_bufpeek().type) {
				case token_type_t::lessthan:               if (!relational_expression_common(nc,ast_node_op_t::lessthan)) return false; break;
				case token_type_t::greaterthan:            if (!relational_expression_common(nc,ast_node_op_t::greaterthan)) return false; break;
				case token_type_t::lessthanorequal:        if (!relational_expression_common(nc,ast_node_op_t::lessthanorequal)) return false; break;
				case token_type_t::greaterthanorequal:     if (!relational_expression_common(nc,ast_node_op_t::greaterthanorequal)) return false; break;
				default:                                   goto done_parsing;
			}
		}
done_parsing:
		return true;
	}
#undef NLEX

	//////////////////////////////////////

#define NLEX relational_expression
	bool compiler::equality_expression_common(ast_node_t::cursor &nc,const ast_node_op_t anot) {
		if ((*nc)->op == ast_node_op_t::identifier_list) return false;
		ast_node_t::cursor cur_nc = nc; cur_nc.to_next(); /* save cursor */
		ast_node_t::parent_to_child_with_new_parent(*nc,/*new parent*/new ast_node_t(anot,tok_bufget())); /* make child of comma */
		return NLEX(cur_nc);
	}

	/* [https://en.cppreference.com/w/c/language/operator_precedence] level 7 */
	/* [https://en.cppreference.com/w/cpp/language/operator_precedence] level 10 */
	bool compiler::equality_expression(ast_node_t::cursor &nc) {
		if (!NLEX(nc)) return false;

		/* left operator right
		 *
		 * [operator]
		 *  \
		 *   +-- [left expr] -> [right expr] */

		while (1) {
			switch (tok_bufpeek().type) {
				case token_type_t::equalequal:             if (!equality_expression_common(nc,ast_node_op_t::equals)) return false; break;
				case token_type_t::exclamationequal:       if (!equality_expression_common(nc,ast_node_op_t::notequals)) return false; break;
				default:                                   goto done_parsing;
			}
		}
done_parsing:
		return true;
	}
#undef NLEX

	//////////////////////////////////////

	/* [https://en.cppreference.com/w/c/language/operator_precedence] level 8 */
	/* [https://en.cppreference.com/w/cpp/language/operator_precedence] level 11 */
	bool compiler::binary_and_expression(ast_node_t::cursor &nc) {
#define NLEX equality_expression
		if (!NLEX(nc)) return false;

		/* [&]
		 *  \
		 *   +-- [left expr] -> [right expr] */

		while (tok_bufpeek().type == token_type_t::ampersand) { /* & operator */
			ast_node_t::cursor cur_nc = nc; cur_nc.to_next(); /* save cursor */
			ast_node_t::parent_to_child_with_new_parent(*nc,/*new parent*/new ast_node_t(ast_node_op_t::binary_and,tok_bufget())); /* make child of comma */
			if (!NLEX(cur_nc)) return false; /* read new one */
		}
#undef NLEX
		return true;
	}

	/* [https://en.cppreference.com/w/c/language/operator_precedence] level 9 */
	/* [https://en.cppreference.com/w/cpp/language/operator_precedence] level 12 */
	bool compiler::binary_xor_expression(ast_node_t::cursor &nc) {
#define NLEX binary_and_expression
		if (!NLEX(nc))
			return false;

		/* [^]
		 *  \
		 *   +-- [left expr] -> [right expr] */

		while (tok_bufpeek().type == token_type_t::caret) { /* ^ operator */
			ast_node_t::cursor cur_nc = nc; cur_nc.to_next(); /* save cursor */
			ast_node_t::parent_to_child_with_new_parent(*nc,/*new parent*/new ast_node_t(ast_node_op_t::binary_xor,tok_bufget())); /* make child of comma */
			if (!NLEX(cur_nc)) return false; /* read new one */
		}
#undef NLEX
		return true;
	}

	/* [https://en.cppreference.com/w/c/language/operator_precedence] level 10 */
	/* [https://en.cppreference.com/w/cpp/language/operator_precedence] level 13 */
	bool compiler::binary_or_expression(ast_node_t::cursor &nc) {
#define NLEX binary_xor_expression
		if (!NLEX(nc)) return false;

		/* [|]
		 *  \
		 *   +-- [left expr] -> [right expr] */

		while (tok_bufpeek().type == token_type_t::pipe) { /* | operator */
			ast_node_t::cursor cur_nc = nc; cur_nc.to_next(); /* save cursor */
			ast_node_t::parent_to_child_with_new_parent(*nc,/*new parent*/new ast_node_t(ast_node_op_t::binary_or,tok_bufget())); /* make child of comma */
			if (!NLEX(cur_nc)) return false; /* read new one */
		}
#undef NLEX
		return true;
	}

	/* [https://en.cppreference.com/w/c/language/operator_precedence] level 11 */
	/* [https://en.cppreference.com/w/cpp/language/operator_precedence] level 14 */
	bool compiler::logical_and_expression(ast_node_t::cursor &nc) {
#define NLEX binary_or_expression
		if (!NLEX(nc)) return false;

		/* [&&]
		 *  \
		 *   +-- [left expr] -> [right expr] */

		while (tok_bufpeek().type == token_type_t::ampersandampersand) { /* && operator */
			ast_node_t::cursor cur_nc = nc; cur_nc.to_next(); /* save cursor */
			ast_node_t::parent_to_child_with_new_parent(*nc,/*new parent*/new ast_node_t(ast_node_op_t::logical_and,tok_bufget())); /* make child of comma */
			if (!NLEX(cur_nc)) return false; /* read new one */
		}
#undef NLEX
		return true;
	}

	/* [https://en.cppreference.com/w/c/language/operator_precedence] level 12 */
	/* [https://en.cppreference.com/w/cpp/language/operator_precedence] level 15 */
	bool compiler::logical_or_expression(ast_node_t::cursor &nc) {
#define NLEX logical_and_expression
		if (!NLEX(nc)) return false;

		/* [||]
		 *  \
		 *   +-- [left expr] -> [right expr] */

		while (tok_bufpeek().type == token_type_t::pipepipe) { /* || operator */
			ast_node_t::cursor cur_nc = nc; cur_nc.to_next(); /* save cursor */
			ast_node_t::parent_to_child_with_new_parent(*nc,/*new parent*/new ast_node_t(ast_node_op_t::logical_or,tok_bufget())); /* make child of comma */
			if (!NLEX(cur_nc)) return false; /* read new one */
		}
#undef NLEX
		return true;
	}

	/* [https://en.cppreference.com/w/c/language/operator_precedence] level 13 */
	/* [https://en.cppreference.com/w/cpp/language/operator_precedence] level 16 */
	bool compiler::conditional_expression(ast_node_t::cursor &nc) {
#define NLEX logical_or_expression
		if (!NLEX(nc)) return false;

		/* left ? middle : right
		 *
		 * [?]
		 *  \
		 *   +-- [left expr] -> [middle expr] -> [right expr] */

		if (tok_bufpeek().type == token_type_t::question) {
			tok_bufdiscard();

			ast_node_t::cursor cur_nc = nc; cur_nc.to_next(); /* save cursor */
			ast_node_t::parent_to_child_with_new_parent(*nc,/*new parent*/new ast_node_t(ast_node_op_t::ternary)); /* make child of comma */

			if (!expression(cur_nc)) return false;
			cur_nc.to_next();

			if (tok_bufpeek().type != token_type_t::colon) return false;
			tok_bufdiscard();

			if (!conditional_expression(cur_nc)) return false;
			cur_nc.to_next();
		}
#undef NLEX
		return true;
	}

	//////////////////////////////////////

#define NLEX conditional_expression
	bool compiler::assignment_expression_common(ast_node_t::cursor &nc,const ast_node_op_t anot) {
		if ((*nc)->op == ast_node_op_t::identifier_list) return false;
		ast_node_t::cursor cur_nc = nc; cur_nc.to_next(); /* save cursor */
		ast_node_t::parent_to_child_with_new_parent(*nc,/*new parent*/new ast_node_t(anot,tok_bufget())); /* make child of comma */
		return assignment_expression(cur_nc);
	}

	/* [https://en.cppreference.com/w/c/language/operator_precedence] level 14 */
	/* [https://en.cppreference.com/w/cpp/language/operator_precedence] level 16 */
	bool compiler::assignment_expression(ast_node_t::cursor &nc) {
		if (!NLEX(nc)) return false;

		/* left operator right
		 *
		 * [operator]
		 *  \
		 *   +-- [left expr] -> [right expr] */

		switch (tok_bufpeek().type) {
			case token_type_t::equal:                return assignment_expression_common(nc,ast_node_op_t::assign);
			case token_type_t::plusequal:            return assignment_expression_common(nc,ast_node_op_t::assignadd);
			case token_type_t::minusequal:           return assignment_expression_common(nc,ast_node_op_t::assignsubtract);
			case token_type_t::starequal:            return assignment_expression_common(nc,ast_node_op_t::assignmultiply);
			case token_type_t::slashequal:           return assignment_expression_common(nc,ast_node_op_t::assigndivide);
			case token_type_t::percentequal:         return assignment_expression_common(nc,ast_node_op_t::assignmodulo);
			case token_type_t::ampersandequal:       return assignment_expression_common(nc,ast_node_op_t::assignand);
			case token_type_t::caretequal:           return assignment_expression_common(nc,ast_node_op_t::assignxor);
			case token_type_t::pipeequal:            return assignment_expression_common(nc,ast_node_op_t::assignor);
			case token_type_t::leftleftangleequal:   return assignment_expression_common(nc,ast_node_op_t::assignleftshift);
			case token_type_t::rightrightangleequal: return assignment_expression_common(nc,ast_node_op_t::assignrightshift);
			default:                                 break;
		};
		return true;
	}
#undef NLEX

	//////////////////////////////////////

	/* [https://en.cppreference.com/w/c/language/operator_precedence] level 15 */
	/* [https://en.cppreference.com/w/cpp/language/operator_precedence] level 17 */
	bool compiler::expression(ast_node_t::cursor &nc) {
#define NLEX assignment_expression
		if (!NLEX(nc)) return false;

		/* [,]
		 *  \
		 *   +-- [left expr] -> [right expr] */

		while (tok_bufpeek().type == token_type_t::comma) { /* , comma operator */
			ast_node_t::cursor cur_nc = nc; cur_nc.to_next(); /* save cursor */
			ast_node_t::parent_to_child_with_new_parent(*nc,/*new parent*/new ast_node_t(ast_node_op_t::comma,tok_bufget())); /* make child of comma */
			if (!NLEX(cur_nc)) return false; /* read new one */
		}
#undef NLEX
		return true;
	}

	bool compiler::statement_does_not_need_semicolon(ast_node_t* const apnode) {
		/* { scope } */
		if (apnode->op == ast_node_op_t::scope)
			return true;

		/* function() { scope }
		 *
		 * but not function(), that still requires semicolon */
		if (apnode->op == ast_node_op_t::functioncall) {
			ast_node_t* s = apnode->child;
			if (s && (s->op == ast_node_op_t::identifier || s->op == ast_node_op_t::identifier_list)) s=s->next;
			while (s && s->op == ast_node_op_t::argument) s=s->next;
			if (s && s->op == ast_node_op_t::scope)
				return true;
		}

		/* if (...) statement, parsing has already consumed semicolon */
		if (apnode->op == ast_node_op_t::r_if)
			return true;

		/* switch { ... } handling has it's own curly brace enforcement, no need for semicolon */
		/* so does while () { ... } */
		if (apnode->op == ast_node_op_t::r_switch || apnode->op == ast_node_op_t::r_while)
			return true;

		return false;
	}

	bool compiler::statement(ast_node_t::cursor &nc) {
		tok_buf_refill();
		if (tok_buf_empty()) return true;

		/* a statement of ; is fine */
		if (tok_bufpeek().type == token_type_t::semicolon) {
			tok_bufdiscard();
			return true;
		}

		ast_node_t::set(*nc, new ast_node_t(ast_node_op_t::statement));
		(*nc)->tv.position = tok_bufpeek(0).position;

		ast_node_t::cursor s_nc(nc);
		s_nc.to_child();
		nc.to_next();

		/* allow { scope } */
		if (tok_bufpeek().type == token_type_t::opencurly) {
			tok_bufdiscard();

			ast_node_t::set(*s_nc, new ast_node_t(ast_node_op_t::scope)); s_nc.to_child();

			while (1) {
				if (tok_bufpeek().type == token_type_t::closecurly) {
					tok_bufdiscard();
					break;
				}

				if (!statement(s_nc)) return false;
			}

			return true;
		}

		if (tok_bufpeek().type == token_type_t::r_return) {
			tok_bufdiscard();

			ast_node_t::set(*s_nc, new ast_node_t(ast_node_op_t::r_return)); s_nc.to_child();

			if (!expression(s_nc))
				return false;

			return statement_semicolon_check(s_nc);
		}

		/* look for "int" because it may be "int x;" or "int func1()" or something of that kind.
		 * this also includes "static" "const" "extern" and so on. typecast() will return without
		 * filling in *s_nc if nothing there of that type. */
		if (!type_list(s_nc,ast_node_op_t::i_compound_let))
			return false;

		if (*s_nc) { /* filled in if types were seen */
			ast_node_t::cursor l_nc = s_nc; l_nc.to_child();
			unsigned int count = 0; /* function body allowed only if the first and only provided */

			while (1) {
				ast_node_t::set(*l_nc, new ast_node_t(ast_node_op_t::r_let));

				ast_node_t::cursor sl_nc = l_nc; sl_nc.to_child();

				ast_node_t::set(*sl_nc, new ast_node_t(ast_node_op_t::i_as));
				(*sl_nc)->tv.type = token_type_t::r_typeclsif;
				(*sl_nc)->tv.v.typecls.init();
				if (!type_deref_list(sl_nc)) return false;
				sl_nc.to_next();

				if (!cpp_scope_expression(sl_nc)) return false;
				sl_nc.to_next();

				if (tok_bufpeek().type == token_type_t::openparen) {
					(*l_nc)->op = ast_node_op_t::r_fn;
					tok_bufdiscard();

					bool allow_named_boundary = true;

					if (tok_bufpeek().type != token_type_t::closeparen) {
						do {
							if (tok_bufpeek().type == token_type_t::ellipsis) {
								ast_node_t::set(*sl_nc, new ast_node_t(ast_node_op_t::argument));
								(*sl_nc)->set_child(new ast_node_t(ast_node_op_t::ellipsis, tok_bufget()));
								sl_nc.to_next();

								/* must be last param */
								if (tok_bufpeek().type != token_type_t::closeparen) return false;
								break;
							}
							else if (tok_bufpeek().type == token_type_t::star) {
								if (!allow_named_boundary) return false;

								/* beyond this point you must name the parameter by prefixing each argument
								 * when calling the function with the name: of the parameter. based on how
								 * named parameters are done in Python. You can only put this ONCE in the
								 * argument list! */
								(*sl_nc) = new ast_node_t(ast_node_op_t::named_arg_required_boundary, tok_bufget());
								allow_named_boundary = false;
								sl_nc.to_next();
							}
							else {
								if (!type_list(sl_nc,ast_node_op_t::argument))
									return false;
								if (*sl_nc == NULL) /* type required */
									return false;

								ast_node_t::cursor a_nc = sl_nc; sl_nc.to_next(); a_nc.to_child();

								(*a_nc) = new ast_node_t(ast_node_op_t::i_as);
								(*a_nc)->tv.type = token_type_t::r_typeclsif;
								(*a_nc)->tv.v.typecls.init();
								if (!type_deref_list(a_nc)) return false;
								a_nc.to_next();

								if (!cpp_scope_expression(a_nc)) return false;
								a_nc.to_next();
							}

							if (tok_bufpeek().type == token_type_t::closeparen) {
								break;
							}
							else if (tok_bufpeek().type == token_type_t::comma) {
								tok_bufdiscard();
								continue;
							}
							else {
								return false;
							}
						} while (1);
					}

					if (tok_bufpeek().type != token_type_t::closeparen) return false;
					tok_bufdiscard();
				}

				/* allow { scope } to follow to give a function it's body but only the FIRST definition,
				 * after which this loop must terminate. You cannot provide a function body and then define
				 * more variables. You cannot define variables then a function with body.
				 * C/C++ doesn't let you do that and neither will we. */
				if (count == 0 && (*l_nc)->op == ast_node_op_t::r_fn && tok_bufpeek().type == token_type_t::opencurly) {
					if (!statement(sl_nc)) return false;
					return true;
				}

				/* if an equals sign follows, the var/function has an initialized value */
				if (tok_bufpeek().type == token_type_t::equal) {
					tok_bufdiscard();

					(*sl_nc) = new ast_node_t(ast_node_op_t::assign);
					ast_node_t::cursor e_nc = sl_nc; sl_nc.to_next(); e_nc.to_child();

					if (!assignment_expression(e_nc)) return false;
				}

				l_nc.to_next();
				count++;

				if (tok_bufpeek().type == token_type_t::semicolon) {
					break;
				}
				else if (tok_bufpeek().type == token_type_t::comma) {
					tok_bufdiscard();
				}
				else {
					return false;
				}
			}

			return statement_semicolon_check(s_nc);
		}

		if (!expression(s_nc))
			return false;

		return statement_semicolon_check(s_nc);
	}

	bool compiler::statement_semicolon_check(ast_node_t::cursor &s_nc) {
		if (statement_does_not_need_semicolon(*s_nc)) {
			/* if parsing a { scope } a semicolon is not required after the closing curly brace */
		}
		else {
			token_t &t = tok_bufpeek();
			if (t.type == token_type_t::semicolon)
				tok_bufdiscard(); /* eat the semicolon */
			else
				return false;
		}

		/* if it's just an identifier list it's probably gibberish or
		 * accidental use of C/C++ syntax to declare a variable i.e. "int x" */
		if ((*s_nc)->op == ast_node_op_t::identifier_list)
			return false;

		return true;
	}

	void compiler::free_ast(void) {
		if (root_node) {
			root_node->free_nodes();
			delete root_node;
			root_node = NULL;
		}
	}

	const std::string &toksnames_getname(CIMCC::token_source_name_list_t *toksnames,const size_t ent);

	void compiler::error_msg(const std::string msg,const position_t &pos) {
		fprintf(stderr,"Error: ");
		{
			unsigned char count = 0;
			if (pos.source != pos.nosource) { if (count++ != 0) fprintf(stderr,", "); fprintf(stderr,"in \"%s\"",toksnames_getname(tok_snamelist,pos.source).c_str()); }
			if (pos.line >= 0) { if (count++ != 0) fprintf(stderr,", "); fprintf(stderr,"line %d",pos.line); }
			if (pos.col >= 0) { if (count++ != 0) fprintf(stderr,", "); fprintf(stderr,"col %d",pos.col); }
		}
		fprintf(stderr,": %s\n",msg.c_str());
	}

	//////////////

	bool compiler::compile(void) {
		ast_node_t::cursor nc(root_node); nc.to_next_until_last();
		bool ok = true;
		token_t tok;

		if (!pb.is_alloc()) {
			if (!pb.alloc(4096))
				return false;
		}

		if (tok_snamelist) {
			token_source_name_t t;

			t.name = pb_ctx.source_name;
			tok_pos.source = (*tok_snamelist).size();
			(*tok_snamelist).push_back(std::move(t));
		}

		tok_buf_clear();
		tok_pos.first_line();
		tok_buf_refill();
		while (!tok_buf_empty()) {
			if (!statement(nc)) {
				error_msg("Syntax error, parser at",tok_bufpeek(0).position);
				ok = false;
				break;
			}
		}

		return ok;
	}

	void compiler::whitespace(void) {
		do {
			const char c = peekb(); /* returns 0 on EOF */
			if (is_whitespace(c)) skipb();
			else break;
		} while (1);
	}

	void vec_encode_utf16surrogate(std::vector<uint16_t> &v,uint32_t c) {
		/* surrogate encoding */
		c -= 0x10000u;
		v.push_back((uint16_t)(0xD800u + (c >> 10u)));
		v.push_back((uint16_t)(0xDC00u + (c & 0x3FFu)));
	}

	void vec_encode_utf8(std::vector<uint8_t> &v,uint32_t c) {
		if (c >= 0x4000000u) {
			v.push_back((uint8_t)(((c >> 30u) & 0x01u) + 0xFC));
			v.push_back((uint8_t)(((c >> 24u) & 0x3Fu) + 0x80));
			v.push_back((uint8_t)(((c >> 18u) & 0x3Fu) + 0x80));
			v.push_back((uint8_t)(((c >> 12u) & 0x3Fu) + 0x80));
			v.push_back((uint8_t)(((c >>  6u) & 0x3Fu) + 0x80));
			v.push_back((uint8_t)(( c         & 0x3Fu) + 0x80));
		}
		else if (c >= 0x200000u) {
			v.push_back((uint8_t)(((c >> 24u) & 0x03u) + 0xF8));
			v.push_back((uint8_t)(((c >> 18u) & 0x3Fu) + 0x80));
			v.push_back((uint8_t)(((c >> 12u) & 0x3Fu) + 0x80));
			v.push_back((uint8_t)(((c >>  6u) & 0x3Fu) + 0x80));
			v.push_back((uint8_t)(( c         & 0x3Fu) + 0x80));
		}
		else if (c >= 0x10000u) {
			v.push_back((uint8_t)(((c >> 18u) & 0x07u) + 0xF0));
			v.push_back((uint8_t)(((c >> 12u) & 0x3Fu) + 0x80));
			v.push_back((uint8_t)(((c >>  6u) & 0x3Fu) + 0x80));
			v.push_back((uint8_t)(( c         & 0x3Fu) + 0x80));
		}
		else if (c >= 0x800u) {
			v.push_back((uint8_t)(((c >> 12u) & 0x0Fu) + 0xE0));
			v.push_back((uint8_t)(((c >>  6u) & 0x3Fu) + 0x80));
			v.push_back((uint8_t)(( c         & 0x3Fu) + 0x80));
		}
		else if (c >= 0x80u) {
			v.push_back((uint8_t)(((c >>  6u) & 0x1Fu) + 0xC0));
			v.push_back((uint8_t)(( c         & 0x3Fu) + 0x80));
		}
		else {
			v.push_back((uint8_t)c);
		}
	}

	bool compiler::gtok_check_ahead_H_identifier(const std::vector<uint8_t> &identifier,const char qu) {
		gtok_prep_number_proc(); /* happens to do what we need: lazy flush and refill the buffer */

		unsigned char *scan = pb.read;

		/* This function must be called after reading and consuming the '\n' character.
		 * The next byte to read must be just after newline. */
		for (auto i=identifier.begin();i!=identifier.end();i++) {
			if (scan >= pb.end) return false;
			if (*(scan++) != *i) return false;
		}

		/* The next char must be the single or double quote char */
		if (scan >= pb.end) return false;
		if (*(scan++) != qu) return false;

		/* Consume the identifier and quote mark we just checked for the caller */
		pb.read = scan;

		return true;
	}

	void compiler::gtok_chrstr_H_literal(const char qu,token_t &t,unsigned int flags,token_charstrliteral_t::strtype_t strtype) {
		int64_t c;

		/* H"<<IDENTIFIER \n
		 * zero or more lines of string constant
		 * IDENTIFIER" */

		if (qu == '\"')
			t.type = token_type_t::stringliteral;
		else
			t.type = token_type_t::characterliteral;

		/* the next two chars must be "<<" */
		if (getb() != '<') return;
		if (getb() != '<') return;

		/* and then the next chars must be an identifier, no space between << and identifier,
		 * and only identifier valid characters. */
		std::vector<uint8_t> identifier;

		if (!is_identifier_first_char(peekb())) return;
		identifier.push_back(getb());

		while (is_identifier_char(peekb())) identifier.push_back(getb());

		/* white space is allowed, but then must be followed by a newline */
		/* NTS: \r is seen as a whitespace so for DOS or Windows \r\n line breaks this will stop at the \n */
		while (peekb() != '\n' && is_whitespace(peekb())) skipb();

		/* newline, or it's not valid! */
		if (peekb() != '\n') return;
		skipb();

		/* T_WIDE is an alias for T_UTF16, we'll add a compiler option where it can be an alias of T_UTF32 instead later on */
		if (strtype == token_charstrliteral_t::strtype_t::T_WIDE)
			strtype = token_charstrliteral_t::strtype_t::T_UTF16;

		if (strtype == token_charstrliteral_t::strtype_t::T_UTF16) {
			std::vector<uint16_t> tmp;

			while (1) {
				if (pb.eof()) break;

				c = getb_csc(strtype);
				if (c < 0) break;

				if (c == '\n') {
					/* check ahead: are the characters after the newline
					 * the exact identifier followed by a single or double quote? */
					if (gtok_check_ahead_H_identifier(identifier,qu)) {
						/* this function will have already set the read position to the end of it */
						break;
					}

					if (flags & HFLAG_CR_NEWLINES)
						tmp.push_back((uint16_t)('\r'));
					if (flags & HFLAG_LF_NEWLINES)
						tmp.push_back((uint16_t)('\n'));
				}
				else if (c >= 0x110000u)
					break;
				else if (c >= 0x10000u)
					vec_encode_utf16surrogate(tmp,(uint16_t)c);
				else
					tmp.push_back((uint16_t)c);
			}

			t.v.chrstrlit.type = strtype;
			t.v.chrstrlit.length = tmp.size() * sizeof(uint16_t);
			if (t.v.chrstrlit.length != 0) {
				assert(t.v.chrstrlit.length == (tmp.size() * sizeof(uint16_t)));
				t.v.chrstrlit.data = malloc(t.v.chrstrlit.length);
				memcpy(t.v.chrstrlit.data,tmp.data(),t.v.chrstrlit.length);
			}
			else {
				t.v.chrstrlit.data = NULL;
			}
		}
		else if (strtype == token_charstrliteral_t::strtype_t::T_UTF32) {
			std::vector<uint32_t> tmp;

			while (1) {
				if (pb.eof()) break;

				c = getb_csc(strtype);
				if (c < 0) break;

				if (c == '\n') {
					/* check ahead: are the characters after the newline
					 * the exact identifier followed by a single or double quote? */
					if (gtok_check_ahead_H_identifier(identifier,qu)) {
						/* this function will have already set the read position to the end of it */
						break;
					}

					if (flags & HFLAG_CR_NEWLINES)
						tmp.push_back((uint16_t)('\r'));
					if (flags & HFLAG_LF_NEWLINES)
						tmp.push_back((uint16_t)('\n'));
				}
				else {
					tmp.push_back((uint32_t)c);
				}
			}

			t.v.chrstrlit.type = strtype;
			t.v.chrstrlit.length = tmp.size() * sizeof(uint32_t);
			if (t.v.chrstrlit.length != 0) {
				assert(t.v.chrstrlit.length == (tmp.size() * sizeof(uint32_t)));
				t.v.chrstrlit.data = malloc(t.v.chrstrlit.length);
				memcpy(t.v.chrstrlit.data,tmp.data(),t.v.chrstrlit.length);
			}
			else {
				t.v.chrstrlit.data = NULL;
			}
		}
		else {
			std::vector<uint8_t> tmp;

			while (1) {
				if (pb.eof()) break;

				c = getb_csc(strtype);
				if (c < 0) break;

				if (c == '\n') {
					/* check ahead: are the characters after the newline
					 * the exact identifier followed by a single or double quote? */
					if (gtok_check_ahead_H_identifier(identifier,qu)) {
						/* this function will have already set the read position to the end of it */
						break;
					}

					if (flags & HFLAG_CR_NEWLINES)
						tmp.push_back((uint16_t)('\r'));
					if (flags & HFLAG_LF_NEWLINES)
						tmp.push_back((uint16_t)('\n'));
				}
				else if (strtype == token_charstrliteral_t::strtype_t::T_UTF8) {
					/* The u8'...' constant is supposed to be a char8_t byte array of UTF-8 where
					 * every character is within normal ASCII range. Screw that, encode it as UTF-8. */
					if (c >= 0x80000000u)
						break;
					else
						vec_encode_utf8(tmp,(uint8_t)c);
				}
				else {
					if (c > 0xFFu) break;
					tmp.push_back((uint8_t)c);
				}
			}

			t.v.chrstrlit.type = strtype;
			t.v.chrstrlit.length = tmp.size();
			if (t.v.chrstrlit.length != 0) {
				assert(t.v.chrstrlit.length == tmp.size());
				t.v.chrstrlit.data = malloc(t.v.chrstrlit.length);
				memcpy(t.v.chrstrlit.data,tmp.data(),t.v.chrstrlit.length);
			}
			else {
				t.v.chrstrlit.data = NULL;
			}
		}
	}

	void compiler::gtok_chrstr_literal(const char qu,token_t &t,token_charstrliteral_t::strtype_t strtype) {
		int64_t c;

		if (qu == '\"')
			t.type = token_type_t::stringliteral;
		else
			t.type = token_type_t::characterliteral;

		/* T_WIDE is an alias for T_UTF16, we'll add a compiler option where it can be an alias of T_UTF32 instead later on */
		if (strtype == token_charstrliteral_t::strtype_t::T_WIDE)
			strtype = token_charstrliteral_t::strtype_t::T_UTF16;

		if (strtype == token_charstrliteral_t::strtype_t::T_UTF16) {
			std::vector<uint16_t> tmp;

			while (1) {
				if (pb.eof()) break;
				if (peekb() == qu) {
					skipb();
					break;
				}

				c = getb_with_escape(strtype);
				if (c < 0) break;

				if (c >= 0x110000u)
					break;
				else if (c >= 0x10000u)
					vec_encode_utf16surrogate(tmp,(uint32_t)c);
				else
					tmp.push_back((uint16_t)c);
			}

			t.v.chrstrlit.type = strtype;
			t.v.chrstrlit.length = tmp.size() * sizeof(uint16_t);
			if (t.v.chrstrlit.length != 0) {
				assert(t.v.chrstrlit.length == (tmp.size() * sizeof(uint16_t)));
				t.v.chrstrlit.data = malloc(t.v.chrstrlit.length);
				memcpy(t.v.chrstrlit.data,tmp.data(),t.v.chrstrlit.length);
			}
			else {
				t.v.chrstrlit.data = NULL;
			}
		}
		else if (strtype == token_charstrliteral_t::strtype_t::T_UTF32) {
			std::vector<uint32_t> tmp;

			while (1) {
				if (pb.eof()) break;
				if (peekb() == qu) {
					skipb();
					break;
				}

				c = getb_with_escape(strtype);
				if (c < 0) break;
				tmp.push_back((uint32_t)c);
			}

			t.v.chrstrlit.type = strtype;
			t.v.chrstrlit.length = tmp.size() * sizeof(uint32_t);
			if (t.v.chrstrlit.length != 0) {
				assert(t.v.chrstrlit.length == (tmp.size() * sizeof(uint32_t)));
				t.v.chrstrlit.data = malloc(t.v.chrstrlit.length);
				memcpy(t.v.chrstrlit.data,tmp.data(),t.v.chrstrlit.length);
			}
			else {
				t.v.chrstrlit.data = NULL;
			}
		}
		else {
			std::vector<uint8_t> tmp;

			while (1) {
				if (pb.eof()) break;
				if (peekb() == qu) {
					skipb();
					break;
				}

				c = getb_with_escape(strtype);
				if (c < 0) break;

				if (strtype == token_charstrliteral_t::strtype_t::T_UTF8) {
					/* The u8'...' constant is supposed to be a char8_t byte array of UTF-8 where
					 * every character is within normal ASCII range. Screw that, encode it as UTF-8. */
					if (c >= 0x80000000u)
						break;
					else
						vec_encode_utf8(tmp,(uint32_t)c);
				}
				else {
					if (c > 0xFFu) break;
					tmp.push_back((uint8_t)c);
				}
			}

			t.v.chrstrlit.type = strtype;
			t.v.chrstrlit.length = tmp.size();
			if (t.v.chrstrlit.length != 0) {
				assert(t.v.chrstrlit.length == tmp.size());
				t.v.chrstrlit.data = malloc(t.v.chrstrlit.length);
				memcpy(t.v.chrstrlit.data,tmp.data(),t.v.chrstrlit.length);
			}
			else {
				t.v.chrstrlit.data = NULL;
			}
		}
	}

	void compiler::gtok_identifier(token_t &t) {
		gtok_prep_number_proc();
		assert(pb.read < pb.end); /* wait, peekb() returned a digit we didn't getb()?? */

		/* do not flush, do not call getb() or flush() or lazy_flush() in this block */
		unsigned char *start = pb.read;

		while (pb.read < pb.end && is_identifier_char(*(pb.read))) pb.read++;
		tok_pos.col += (int)(pb.read - start);

		assert(start != pb.read);
		const size_t identlen = size_t(pb.read - start);

		unsigned char *end = pb.read;

		/* But wait---It might not be an identifier, it might be a char or string literal with L, u, U, etc. at the start! */
		while (pb.read < pb.end && is_whitespace(*pb.read)) {
			update_pos(*(pb.read));
			pb.read++;
		}

		if (pb.read < pb.end) {
			if (*pb.read == '\'' || *pb.read == '\"') { /* i.e. L'W' or L"Hello" */
				token_charstrliteral_t::strtype_t st = token_charstrliteral_t::strtype_t::T_BYTE;
				unsigned char hflags = HFLAG_LF_NEWLINES; /* default 0x0a (LF) newlines */
				unsigned char *scan = start;
				unsigned char special = 0;

				if ((scan+2) <= end && !memcmp(scan,"u8",2)) {
					/* "u8 has type char and is equal to the code point as long as it's one byte" (paraphrased) */
					/* Pfffffttt, ha! If u8 is limited to one byte then why even have it? */
					st = token_charstrliteral_t::strtype_t::T_UTF8;
					tok_pos.col += 2;
					scan += 2;
				}
				else if ((scan+1) <= end && *scan == 'L') {
					st = token_charstrliteral_t::strtype_t::T_WIDE;
					tok_pos.col += 1;
					scan += 1;
				}
				else if ((scan+1) <= end && *scan == 'u') {
					st = token_charstrliteral_t::strtype_t::T_UTF16;
					tok_pos.col += 1;
					scan += 1;
				}
				else if ((scan+1) <= end && *scan == 'U') {
					st = token_charstrliteral_t::strtype_t::T_UTF32;
					tok_pos.col += 1;
					scan += 1;
				}

				/* NTS: C++23 defines a "raw string literal" R syntax which looks absolutely awful and I refuse to implement it.
				 *
				 * Instead, we support a cleaner way to enter whole strings with minimal escaping that already exists. It's used
				 * by the GNU bash shell called "here documents". We support it like this with the H (for "here") prefix for our variation of it:
				 *
				 * string x = u8H"<<EOF
				 * String constant without escaping of any kind.
				 * Can contain newlines if it spans multiple lines.
				 * It stops the instant you specify the same identifier at the start of a line that you used to start it (in this case EOF).
				 * EOF";
				 *
				 * There you go. And you don't need this pattern matching on both sides crap that C++23 R syntax uses. Yechh. */
				if ((scan+1) <= end && *scan == 'H') {
					tok_pos.col += 1;
					special = 'H';
					scan += 1;

					/* allow 'D' to specify that newlines should be encoded \r\n instead of \n i.e. MS-DOS and Windows */
					if ((scan+1) <= end && *scan == 'D') {
						hflags &= ~(HFLAG_CR_NEWLINES | HFLAG_LF_NEWLINES);
						hflags = HFLAG_CR_NEWLINES | HFLAG_LF_NEWLINES;
						tok_pos.col += 1;
						scan += 1;
					}
					/* allow 'M' to specify that newlines should be encoded \r instead of \n i.e. classic Macintosh OS */
					else if ((scan+1) <= end && *scan == 'M') {
						hflags &= ~(HFLAG_CR_NEWLINES | HFLAG_LF_NEWLINES);
						hflags = HFLAG_CR_NEWLINES;
						tok_pos.col += 1;
						scan += 1;
					}
					/* allow 'U' to specify that newlines should be encoded \n (which is default) i.e. Unix/Linux */
					else if ((scan+1) <= end && *scan == 'U') {
						hflags &= ~(HFLAG_CR_NEWLINES | HFLAG_LF_NEWLINES);
						hflags = HFLAG_LF_NEWLINES;
						tok_pos.col += 1;
						scan += 1;
					}
				}

				if (scan == end) {
					tok_pos.col += 1; /* pb.read++ */
					if (special == 'H')
						gtok_chrstr_H_literal(*pb.read++,t,hflags,st);
					else
						gtok_chrstr_literal(*pb.read++,t,st);

					return;
				}
			}
		}

		/* check for reserved words */
		if (identlen == 6 && !memcmp(start,"return",6)) {
			t.type = token_type_t::r_return;
		}
		else if (identlen == 4 && !memcmp(start,"goto",4)) {
			t.type = token_type_t::r_goto;
		}
		else if (identlen == 4 && !memcmp(start,"else",4)) {
			t.type = token_type_t::r_else;
		}
		else if (identlen == 2 && !memcmp(start,"if",2)) {
			t.type = token_type_t::r_if;
		}
		else if (identlen == 8 && !memcmp(start,"continue",8)) {
			t.type = token_type_t::r_continue;
		}
		else if (identlen == 5 && !memcmp(start,"break",5)) {
			t.type = token_type_t::r_break;
		}
		else if (identlen == 6 && !memcmp(start,"switch",6)) {
			t.type = token_type_t::r_switch;
		}
		else if (identlen == 4 && !memcmp(start,"case",4)) {
			t.type = token_type_t::r_case;
		}
		else if (identlen == 7 && !memcmp(start,"default",7)) {
			t.type = token_type_t::r_default;
		}
		else if (identlen == 5 && !memcmp(start,"while",5)) {
			t.type = token_type_t::r_while;
		}
		else if (identlen == 2 && !memcmp(start,"do",2)) {
			t.type = token_type_t::r_do;
		}
		else if (identlen == 3 && !memcmp(start,"for",3)) {
			t.type = token_type_t::r_for;
		}
		else if (identlen == 3 && !memcmp(start,"let",3)) {
			t.type = token_type_t::r_let;
		}
		else if (identlen == 2 && !memcmp(start,"fn",2)) {
			t.type = token_type_t::r_fn;
		}
		else if (identlen == 5 && !memcmp(start,"const",5)) {
			t.type = token_type_t::r_const;
		}
		else if (identlen == 6 && !memcmp(start,"static",6)) {
			t.type = token_type_t::r_static;
		}
		else if (identlen == 6 && !memcmp(start,"extern",6)) {
			t.type = token_type_t::r_extern;
		}
		else if (identlen == 4 && !memcmp(start,"auto",4)) {
			t.type = token_type_t::r_auto;
		}
		else if (identlen == 6 && !memcmp(start,"signed",6)) {
			t.type = token_type_t::r_signed;
		}
		else if (identlen == 3 && !memcmp(start,"int",3)) {
			t.type = token_type_t::r_int;
		}
		else if (identlen == 4 && !memcmp(start,"bool",4)) {
			t.type = token_type_t::r_bool;
		}
		else if (identlen == 4 && !memcmp(start,"true",4)) {
			t.type = token_type_t::r_true;
		}
		else if (identlen == 5 && !memcmp(start,"false",5)) {
			t.type = token_type_t::r_false;
		}
		else if (identlen == 4 && !memcmp(start,"near",4)) {
			t.type = token_type_t::r_near;
		}
		else if (identlen == 3 && !memcmp(start,"far",3)) {
			t.type = token_type_t::r_far;
		}
		else if (identlen == 4 && !memcmp(start,"huge",4)) {
			t.type = token_type_t::r_huge;
		}
		else if (identlen == 3 && !memcmp(start,"asm",3)) {
			t.type = token_type_t::r_asm;
		}
		else if (identlen == 4 && !memcmp(start,"this",4)) {
			t.type = token_type_t::r_this;
		}
		else if (identlen == 4 && !memcmp(start,"_asm",4)) {
			t.type = token_type_t::r__asm;
		}
		else if (identlen == 5 && !memcmp(start,"__asm",5)) {
			t.type = token_type_t::r___asm;
		}
		else if (identlen == 7 && !memcmp(start,"__asm__",7)) {
			t.type = token_type_t::r___asm__;
		}
		else if (identlen == 5 && !memcmp(start,"union",5)) {
			t.type = token_type_t::r_union;
		}
		else if (identlen == 6 && !memcmp(start,"struct",6)) {
			t.type = token_type_t::r_struct;
		}
		else if (identlen == 5 && !memcmp(start,"float",5)) {
			t.type = token_type_t::r_float;
		}
		else if (identlen == 6 && !memcmp(start,"double",6)) {
			t.type = token_type_t::r_double;
		}
		else if (identlen == 4 && !memcmp(start,"void",4)) {
			t.type = token_type_t::r_void;
		}
		else if (identlen == 4 && !memcmp(start,"char",4)) {
			t.type = token_type_t::r_char;
		}
		else if (identlen == 8 && !memcmp(start,"volatile",8)) {
			t.type = token_type_t::r_volatile;
		}
		else if (identlen == 8 && !memcmp(start,"unsigned",8)) {
			t.type = token_type_t::r_unsigned;
		}
		else if (identlen == 4 && !memcmp(start,"long",4)) {
			t.type = token_type_t::r_long;
		}
		else if (identlen == 5 && !memcmp(start,"short",5)) {
			t.type = token_type_t::r_short;
		}
		else if (identlen == 9 && !memcmp(start,"constexpr",9)) {
			t.type = token_type_t::r_constexpr;
		}
		else if (identlen == 11 && !memcmp(start,"compileexpr",11)) {
			t.type = token_type_t::r_compileexpr;
		}
		else if (identlen == 9 && !memcmp(start,"namespace",9)) {
			t.type = token_type_t::r_namespace;
		}
		else if (identlen == 5 && !memcmp(start,"using",5)) {
			t.type = token_type_t::r_using;
		}
		else if (identlen == 6 && !memcmp(start,"typeof",6)) {
			t.type = token_type_t::r_typeof;
		}
		else if (identlen == 7 && !memcmp(start,"typedef",7)) {
			t.type = token_type_t::r_typedef;
		}
		else if (identlen == 6 && !memcmp(start,"sizeof",6)) {
			t.type = token_type_t::r_sizeof;
		}
		else if (identlen == 6 && !memcmp(start,"size_t",6)) {
			t.type = token_type_t::r_size_t;
		}
		else if (identlen == 7 && !memcmp(start,"ssize_t",7)) {
			t.type = token_type_t::r_ssize_t;
		}
		else if (identlen == 8 && !memcmp(start,"offsetof",8)) {
			t.type = token_type_t::r_offsetof;
		}
		else if (identlen == 13 && !memcmp(start,"static_assert",13)) {
			t.type = token_type_t::r_static_assert;
		}
		else {
			/* OK, it's an identifier */
			t.type = token_type_t::identifier;
			t.v.identifier = token_identifier_value_alloc(identlen,(const char*)start);
		}
	}

	void compiler::skip_numeric_digit_separator(void) {
		/* Skip C++14 digit separator */
		if (peekb() == '\'') skipb();
	}

	int64_t compiler::getb_octal(unsigned int max_digits) {
		int64_t s = 0;

		for (size_t i=0;i < max_digits && is_decimal_digit(peekb(),8);i++)
			s = (s * 8ull) + p_decimal_digit(getb());

		return s;
	}

	int64_t compiler::getb_hex(unsigned int max_digits) {
		int64_t s = 0;

		for (size_t i=0;i < max_digits && is_hexadecimal_digit(peekb());i++)
			s = (s * 16ull) + p_hexadecimal_digit(getb());

		return s;
	}

	int64_t compiler::getb_csc(token_charstrliteral_t::strtype_t typ) {
		/* This is going to be compiled on DOS or Windows or asked to compile source code edited on DOS or Windows someday,
		 * might as well deal with the whole \r\n vs \n line break issue here. */
		if (peekb() == '\r') skipb(); /* eat the \r, assume \n follows */

		/* if the type is any kind of unicode (not byte), then read the char as UTF-8 and return the full code */
		if (typ > token_charstrliteral_t::strtype_t::T_BYTE)
			return getb_utf8();

		/* unescaped */
		return getb();
	}

	int64_t compiler::getb_hex_cpp23(void) {
		/* caller already consumed '{' */
		const int64_t v = getb_hex(256);
		if (peekb() == '}') {
			skipb();
			return v;
		}

		return -1ll;
	}

	int64_t compiler::getb_octal_cpp23(void) {
		/* caller already consumed '{' */
		const int64_t v = getb_octal(256);
		if (peekb() == '}') {
			skipb();
			return v;
		}

		return -1ll;
	}

	int64_t compiler::getb_with_escape(token_charstrliteral_t::strtype_t typ) {
		if (peekb() == '\\') {
			skipb();

			switch (peekb()) {
				case '\'': case '\"': case '?': case '\\': return getb();
				case 'a': skipb(); return 0x07;
				case 'b': skipb(); return 0x08;
				case 'f': skipb(); return 0x0C;
				case 'n': skipb(); return 0x0A;
				case 'r': skipb(); return 0x0D;
				case 't': skipb(); return 0x09;
				case 'v': skipb(); return 0x0B;
				case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7':
					return getb_octal(3);
				case 'o': skipb(); /* C++23 o{n...} */
					if (peekb() == '{') { skipb(); /* eat '{' */ return getb_octal_cpp23(); }
					return -1ll;;
				case 'u': skipb();
					if (peekb() == '{') { skipb(); /* eat '{' */ return getb_hex_cpp23(); }
					return getb_hex(4);
				case 'U': skipb();
					if (peekb() == '{') { skipb(); /* eat '{' */ return getb_hex_cpp23(); }
					return getb_hex(8);
				case 'x': skipb();
					if (peekb() == '{') { skipb(); /* eat '{' */ return getb_hex_cpp23(); }
					switch (typ) {
						case token_charstrliteral_t::strtype_t::T_BYTE: return getb_hex(2);
						case token_charstrliteral_t::strtype_t::T_UTF16: return getb_hex(4);
						case token_charstrliteral_t::strtype_t::T_UTF32: return getb_hex(8);
						default: return getb_hex(256);
					}
					break;
				default: return -1ll;
			}
		}

		return getb_csc(typ);
	}

	void compiler::gtok_number(token_t &t) {
		std::string tmp;

		unsigned char suffix = 0;
		unsigned char base = 10;
		bool is_float = false;
		char c;

		static constexpr unsigned S_LONG=(1u << 0u);
		static constexpr unsigned S_LONGLONG=(1u << 1u);
		static constexpr unsigned S_UNSIGNED=(1u << 2u);
		static constexpr unsigned S_FLOAT=(1u << 3u);
		static constexpr unsigned S_DOUBLE=(1u << 4u);

		if (peekb() == '0') {
			skipb();
			if (peekb() == 'x') { /* hexadecimal */
				skipb();
				base = 16;
				skip_numeric_digit_separator();
				while (is_hexadecimal_digit(c=peekb())) { tmp += c; skipb(); skip_numeric_digit_separator(); }
			}
			else if (peekb() == 'b') { /* binary */
				skipb();
				base = 2;
				skip_numeric_digit_separator();
				while (is_decimal_digit(c=peekb(),2)) { tmp += c; skipb(); skip_numeric_digit_separator(); }
			}
			else if (is_decimal_digit(peekb(),8)) { /* octal, 2nd digit (do not advance read ptr) */
				base = 8;
				skip_numeric_digit_separator();
				while (is_decimal_digit(c=peekb(),8)) { tmp += c; skipb(); skip_numeric_digit_separator(); }
			}
			else {
				/* it's just zero and a different token afterwards */
				tmp += '0';
			}
		}
		else { /* decimal */
			skip_numeric_digit_separator();
			while (is_decimal_digit(c=peekb())) { tmp += c; skipb(); skip_numeric_digit_separator(); }
		}

		/* it might be a floating point constant if it has a decimal point */
		if (peekb() == '.' && base == 10) {
			skipb(); tmp += '.';
			skip_numeric_digit_separator();
			while (is_decimal_digit(c=peekb())) { tmp += c; skipb(); skip_numeric_digit_separator(); }
			is_float = true;
		}

		/* it might be a float with exponent field */
		c=peekb(); if ((c == 'e' || c == 'E') && base == 10) {
			/* e4, e-4, e+4 */
			tmp += c; skipb(); c=peekb();
			if (c == '-' || c == '+') { tmp += c; skipb(); }
			skip_numeric_digit_separator();
			while (is_decimal_digit(c=peekb())) { tmp += c; skipb(); skip_numeric_digit_separator(); }
			is_float = true;
		}

		/* check suffixes */
		while (1) {
			c=peekb();
			if (c == 'l' || c == 'L') {
				suffix |= S_LONG;
				skipb(); c=peekb();

				if (c == 'l' || c == 'L') {
					suffix |= S_LONGLONG;
					skipb();
				}
			}
			else if (c == 'u' || c == 'U') {
				suffix |= S_UNSIGNED;
				skipb();
			}
			else if (c == 'f' || c == 'F') {
				suffix |= S_FLOAT;
				is_float = true;
				skipb();
			}
			else if (c == 'd' || c == 'D') {
				suffix |= S_DOUBLE;
				is_float = true;
				skipb();
			}
			else {
				break;
			}
		}

		/* DONE SCANNING. Parse number */
		if (is_float) {
			t.type = token_type_t::floatval;
			auto &nt = t.v.floatval;
			nt.init();

			/* strtold() should handle the decimal point and exponent for us */
			nt.val = strtold(tmp.c_str(),NULL);

			if (suffix & S_LONG) nt.ftype = token_floatval_t::T_LONGDOUBLE;
			else if (suffix & S_DOUBLE) nt.ftype = token_floatval_t::T_DOUBLE;
			else if (suffix & S_FLOAT) nt.ftype = token_floatval_t::T_FLOAT;
		}
		else {
			t.type = token_type_t::intval;
			auto &nt = t.v.intval;

			if (suffix & S_UNSIGNED) nt.initu();
			else nt.init();

			/* strtoull() should work fine */
			nt.v.u = strtoull(tmp.c_str(),NULL,base);

			if (suffix & S_LONGLONG) nt.itype = token_intval_t::T_LONGLONG;
			else if (suffix & S_LONG) nt.itype = token_intval_t::T_LONG;
		}
	}

	void compiler::gtok_prep_number_proc(void) {
		/* The buffer is 2048 to 8192 bytes,
		 * the lazy flush is half that size.
		 * Flush the buffer and refill to assure
		 * that the next 1024 bytes are in buffer,
		 * so we can parse it with char pointers.
		 * If your numbers are THAT long you probably
		 * aren't actually writing numbers.
		 * The only other way that less than 1024
		 * could be available is if we're reading
		 * near or at EOF.*/
		pb.lazy_flush();
		assert(size_t(pb.fence() - pb.read) >= (pb.buffer_size / 2u)); /* lazy_flush() did it's job? */
		refill();
	}

	void compiler::gtok(token_t &t) {
		char c;

		whitespace();
		t.position = tok_pos;
		if (pb.eof()) {
			t.type = token_type_t::eof;
			return;
		}

		c = peekb();
		switch (c) {
			case '0': case '1': case '2': case '3':
			case '4': case '5': case '6': case '7':
			case '8': case '9':
				gtok_number(t);
				break;
			case '.':
				t.type = token_type_t::period;
				skipb();
				if (peekb(0) == '.' && peekb(1) == '.') {
					t.type = token_type_t::ellipsis;
					skipb();
					skipb();
				}
				break;
			case ',':
				t.type = token_type_t::comma;
				skipb();
				break;
			case ';':
				t.type = token_type_t::semicolon;
				skipb();
				break;
			case '-':
				t.type = token_type_t::minus;
				skipb();
				if (peekb() == '=') { /* -= */
					t.type = token_type_t::minusequal;
					skipb();
				}
				else if (peekb() == '-') { /* -- */
					t.type = token_type_t::minusminus;
					skipb();
				}
				else if (peekb() == '>') { /* -> */
					t.type = token_type_t::pointerarrow;
					skipb();
				}
				break;
			case '+':
				t.type = token_type_t::plus;
				skipb();
				if (peekb() == '=') { /* += */
					t.type = token_type_t::plusequal;
					skipb();
				}
				else if (peekb() == '+') { /* ++ */
					t.type = token_type_t::plusplus;
					skipb();
				}
				break;
			case '!':
				t.type = token_type_t::exclamation;
				skipb();
				if (peekb() == '=') { /* != */
					t.type = token_type_t::exclamationequal;
					skipb();
				}
				break;
			case '~':
				t.type = token_type_t::tilde;
				skipb();
				break;
			case '*':
				t.type = token_type_t::star;
				skipb();
				if (peekb() == '=') { /* *= */
					t.type = token_type_t::starequal;
					skipb();
				}
				break;
			case '/':
				t.type = token_type_t::slash;
				skipb();
				if (peekb() == '=') { /* /= */
					t.type = token_type_t::slashequal;
					skipb();
				}
				else if (peekb() == '/') {
					/* C++ comment, which runs to end of line. Make no token for it. */
					skipb();
					while (1) {
						if (peekb() == '\n') {
							skipb();
							return gtok(t); /* and use recursion to try again */
						}
						else if (pb.eof()) {
							t.type = token_type_t::eof;
							break;
						}
						else {
							skipb();
						}
					}
				}
				else if (peekb() == '*') {
					/* C comment, which runs until the same sequence in reverse */
					skipb();

					int depth = 1;
					while (depth > 0) {
						if (pb.eof()) {
							t.type = token_type_t::eof;
							break;
						}

						if (peekb() == '*') {
							skipb();
							if (getb() == '/') depth--;
						}
						else if (peekb() == '/') {
							skipb();
							if (getb() == '*') depth++;
						}
						else {
							skipb();
						}
					}

					return gtok(t); /* and use recursion to try again */
				}
				break;
			case '%':
				t.type = token_type_t::percent;
				skipb();
				if (peekb() == '=') { /* /= */
					t.type = token_type_t::percentequal;
					skipb();
				}
				break;
			case '=':
				t.type = token_type_t::equal;
				skipb();
				if (peekb() == '=') { /* == */
					t.type = token_type_t::equalequal;
					skipb();
				}
				break;
			case '[':
				t.type = token_type_t::leftsquarebracket;
				skipb();
				if (peekb() == '[') { /* [[ */
					t.type = token_type_t::dblleftsquarebracket;
					skipb();
				}
				break;
			case ']':
				t.type = token_type_t::rightsquarebracket;
				skipb();
				if (peekb() == ']') { /* ]] */
					t.type = token_type_t::dblrightsquarebracket;
					skipb();
				}
				break;
			case '<':
				t.type = token_type_t::lessthan;
				skipb();
				if (peekb() == '=') { /* <= */
					t.type = token_type_t::lessthanorequal;
					skipb();
				}
				else if (peekb() == '<') { /* << */
					t.type = token_type_t::leftleftangle;
					skipb();
					if (peekb() == '=') { /* <<= */
						t.type = token_type_t::leftleftangleequal;
						skipb();
					}
				}
				break;
			case '>':
				t.type = token_type_t::greaterthan;
				skipb();
				if (peekb() == '=') { /* >= */
					t.type = token_type_t::greaterthanorequal;
					skipb();
				}
				else if (peekb() == '>') { /* >> */
					t.type = token_type_t::rightrightangle;
					skipb();
					if (peekb() == '=') { /* >>= */
						t.type = token_type_t::rightrightangleequal;
						skipb();
					}
				}
				break;
			case '(':
				t.type = token_type_t::openparen;
				skipb();
				break;
			case ')':
				t.type = token_type_t::closeparen;
				skipb();
				break;
			case '{':
				t.type = token_type_t::opencurly;
				skipb();
				break;
			case '}':
				t.type = token_type_t::closecurly;
				skipb();
				break;
			case '?':
				t.type = token_type_t::question;
				skipb();
				break;
			case '#':
				t.type = token_type_t::poundsign;
				skipb();
				break;
			case '^':
				t.type = token_type_t::caret;
				skipb();
				if (peekb() == '=') { /* ^= */
					t.type = token_type_t::caretequal;
					skipb();
				}
				break;
			case ':':
				t.type = token_type_t::colon;
				skipb();
				if (peekb() == ':') { /* :: */
					t.type = token_type_t::coloncolon;
					skipb();
				}
				break;
			case '|':
				t.type = token_type_t::pipe;
				skipb();
				if (peekb() == '|') { /* || */
					t.type = token_type_t::pipepipe;
					skipb();
				}
				else if (peekb() == '=') { /* |= */
					t.type = token_type_t::pipeequal;
					skipb();
				}
				break;
			case '&':
				t.type = token_type_t::ampersand;
				skipb();
				if (peekb() == '&') { /* && */
					t.type = token_type_t::ampersandampersand;
					skipb();
				}
				else if (peekb() == '=') { /* &= */
					t.type = token_type_t::ampersandequal;
					skipb();
				}
				break;
			case '\'':
			case '\"':
				skipb();
				gtok_chrstr_literal(c,t);
				break;
			default:
				if (is_identifier_first_char(c)) {
					gtok_identifier(t);
				}
				else {
					t.type = token_type_t::none;
					skipb();
				}
				break;
		}
	}

	void token_to_string(std::string &s,const token_t &t) {
		char buf[64];

		switch (t.type) {
			case token_type_t::eof:
				s = "<eof>";
				break;
			case token_type_t::none:
				s = "<none>";
				break;
			case token_type_t::intval:
				if (t.v.intval.flags & token_intval_t::FL_SIGNED)
					snprintf(buf,sizeof(buf),"%lld (0x%llx)",t.v.intval.v.v,t.v.intval.v.u);
				else
					snprintf(buf,sizeof(buf),"%llu (0x%llx)",t.v.intval.v.u,t.v.intval.v.u);

				s = "<intval ";
				s += buf;
				snprintf(buf,sizeof(buf)," f=0x%x",(unsigned int)t.v.intval.flags);
				s += buf;
				snprintf(buf,sizeof(buf)," t=%u",(unsigned int)t.v.intval.itype);
				s += buf;
				s += ">";
				break;
			case token_type_t::floatval:
				snprintf(buf,sizeof(buf),"%0.20f",(double)t.v.floatval.val);
				s = "<floatval ";
				s += buf;
				snprintf(buf,sizeof(buf)," t=%u",(unsigned int)t.v.floatval.ftype);
				s += buf;
				s += ">";
				break;
			case token_type_t::comma:
				s = "<comma>";
				break;
			case token_type_t::semicolon:
				s = "<semicolon>";
				break;
			case token_type_t::minus:
				s = "<minus>";
				break;
			case token_type_t::plus:
				s = "<plus>";
				break;
			case token_type_t::plusequal:
				s = "<+=>";
				break;
			case token_type_t::minusequal:
				s = "<-=>";
				break;
			case token_type_t::starequal:
				s = "<*=>";
				break;
			case token_type_t::slashequal:
				s = "</=>";
				break;
			case token_type_t::percentequal:
				s = "<%=>";
				break;
			case token_type_t::ampersandequal:
				s = "<&=>";
				break;
			case token_type_t::caretequal:
				s = "<^=>";
				break;
			case token_type_t::pipeequal:
				s = "<|=>";
				break;
			case token_type_t::exclamation:
				s = "<!>";
				break;
			case token_type_t::tilde:
				s = "<~>";
				break;
			case token_type_t::star:
				s = "<*>";
				break;
			case token_type_t::slash:
				s = "</>";
				break;
			case token_type_t::percent:
				s = "<%>";
				break;
			case token_type_t::openparen:
				s = "<openparen>";
				break;
			case token_type_t::closeparen:
				s = "<closeparen>";
				break;
			case token_type_t::opencurly:
				s = "<opencurly>";
				break;
			case token_type_t::closecurly:
				s = "<closecurly>";
				break;
			case token_type_t::colon:
				s = "<:>";
				break;
			case token_type_t::coloncolon:
				s = "<::>";
				break;
			case token_type_t::question:
				s = "<?>";
				break;
			case token_type_t::pipe:
				s = "<|>";
				break;
			case token_type_t::pipepipe:
				s = "<||>";
				break;
			case token_type_t::ampersand:
				s = "<&>";
				break;
			case token_type_t::ampersandampersand:
				s = "<&&>";
				break;
			case token_type_t::caret:
				s = "<^>";
				break;
			case token_type_t::poundsign:
				s = "<#>";
				break;
			case token_type_t::equal:
				s = "<=>";
				break;
			case token_type_t::equalequal:
				s = "<==>";
				break;
			case token_type_t::exclamationequal:
				s = "<!=>";
				break;
			case token_type_t::greaterthan:
				s = "<gt>";
				break;
			case token_type_t::lessthan:
				s = "<lt>";
				break;
			case token_type_t::greaterthanorequal:
				s = "<gteq>";
				break;
			case token_type_t::lessthanorequal:
				s = "<lteq>";
				break;
			case token_type_t::leftleftangle:
				s = "<llab>";
				break;
			case token_type_t::rightrightangle:
				s = "<rrab>";
				break;
			case token_type_t::leftleftangleequal:
				s = "<llabequ>";
				break;
			case token_type_t::rightrightangleequal:
				s = "<rrabequ>";
				break;
			case token_type_t::plusplus:
				s = "<++>";
				break;
			case token_type_t::minusminus:
				s = "<-->";
				break;
			case token_type_t::period:
				s = "<.>";
				break;
			case token_type_t::pointerarrow:
				s = "< ptr-> >";
				break;
			case token_type_t::leftsquarebracket:
				s = "<lsqrbrkt>";
				break;
			case token_type_t::rightsquarebracket:
				s = "<rsqrbrkt>";
				break;
			case token_type_t::dblleftsquarebracket:
				s = "<dbllsqrbrkt>";
				break;
			case token_type_t::dblrightsquarebracket:
				s = "<dblrsqrbrkt>";
				break;
			case token_type_t::r_return:
				s = "<r_return>";
				break;
			case token_type_t::r_goto:
				s = "<r_goto>";
				break;
			case token_type_t::r_else:
				s = "<r_else>";
				break;
			case token_type_t::r_if:
				s = "<r_if>";
				break;
			case token_type_t::r_break:
				s = "<r_break>";
				break;
			case token_type_t::r_continue:
				s = "<r_continue>";
				break;
			case token_type_t::r_case:
				s = "<r_case>";
				break;
			case token_type_t::r_switch:
				s = "<r_switch>";
				break;
			case token_type_t::r_while:
				s = "<r_while>";
				break;
			case token_type_t::r_do:
				s = "<r_do>";
				break;
			case token_type_t::r_for:
				s = "<r_for>";
				break;
			case token_type_t::r_let:
				s = "<r_let>";
				break;
			case token_type_t::r_fn:
				s = "<r_fn>";
				break;
			case token_type_t::r_const:
				s = "<r_const>";
				break;
			case token_type_t::r_extern:
				s = "<r_extern>";
				break;
			case token_type_t::r_auto:
				s = "<r_auto>";
				break;
			case token_type_t::r_int:
				s = "<r_int>";
				break;
			case token_type_t::r_bool:
				s = "<r_bool>";
				break;
			case token_type_t::r_true:
				s = "<r_true>";
				break;
			case token_type_t::r_false:
				s = "<r_false>";
				break;
			case token_type_t::r_near:
				s = "<r_near>";
				break;
			case token_type_t::r_far:
				s = "<r_far>";
				break;
			case token_type_t::r_huge:
				s = "<r_huge>";
				break;
			case token_type_t::r_asm:
				s = "<r_asm>";
				break;
			case token_type_t::r_this:
				s = "<r_this>";
				break;
			case token_type_t::r_union:
				s = "<r_union>";
				break;
			case token_type_t::r_struct:
				s = "<r_struct>";
				break;
			case token_type_t::r__asm:
				s = "<r__asm>";
				break;
			case token_type_t::r___asm:
				s = "<r___asm>";
				break;
			case token_type_t::r___asm__:
				s = "<r___asm__>";
				break;
			case token_type_t::r_float:
				s = "<r_float>";
				break;
			case token_type_t::r_double:
				s = "<r_double>";
				break;
			case token_type_t::r_void:
				s = "<r_void>";
				break;
			case token_type_t::r_char:
				s = "<r_char>";
				break;
			case token_type_t::r_volatile:
				s = "<r_volatile>";
				break;
			case token_type_t::r_signed:
				s = "<r_signed>";
				break;
			case token_type_t::r_unsigned:
				s = "<r_unsigned>";
				break;
			case token_type_t::r_long:
				s = "<r_long>";
				break;
			case token_type_t::r_short:
				s = "<r_short>";
				break;
			case token_type_t::r_static:
				s = "<r_static>";
				break;
			case token_type_t::r_constexpr:
				s = "<r_constexpr>";
				break;
			case token_type_t::r_compileexpr:
				s = "<r_compileexpr>";
				break;
			case token_type_t::r_namespace:
				s = "<r_namespace>";
				break;
			case token_type_t::r_using:
				s = "<r_using>";
				break;
			case token_type_t::r_typeof:
				s = "<r_typeof>";
				break;
			case token_type_t::r_typedef:
				s = "<r_typedef>";
				break;
			case token_type_t::r_typeclsif:
				s = "<r_typeclsif:";

				switch (t.v.typecls.cls_c) {
					case ilc_cls_t::c_t::_int: s += " c=int"; break;
					case ilc_cls_t::c_t::_float: s += " c=float"; break;
					case ilc_cls_t::c_t::_void: s += " c=void"; break;
					case ilc_cls_t::c_t::_other: s += " c=other"; break;
					default: break;
				}

				switch (t.v.typecls.cls_t) {
					case ilc_cls_t::t_t::_bool: s += " t=bool"; break;
					case ilc_cls_t::t_t::_char: s += " t=char"; break;
					case ilc_cls_t::t_t::_int: s += " t=int"; break;
					case ilc_cls_t::t_t::_short: s += " t=short"; break;
					case ilc_cls_t::t_t::_long: s += " t=long"; break;
					case ilc_cls_t::t_t::_llong: s += " t=llong"; break;
					case ilc_cls_t::t_t::_float: s += " t=float"; break;
					case ilc_cls_t::t_t::_double: s += " t=double"; break;
					case ilc_cls_t::t_t::_longdouble: s += " t=longdouble"; break;
					default: break;
				}

				switch (t.v.typecls.cls_p) {
					case ilc_cls_t::p_t::_near: s += " p=near"; break;
					case ilc_cls_t::p_t::_far: s += " p=far"; break;
					case ilc_cls_t::p_t::_huge: s += " p=huge"; break;
					default: break;
				}

				switch (t.v.typecls.cls_s) {
					case ilc_cls_t::s_t::_static: s += " s=static"; break;
					case ilc_cls_t::s_t::_extern: s += " s=extern"; break;
					default: break;
				}

				snprintf(buf,sizeof(buf)," cls=0x%x",t.v.typecls.cls); s += buf;
				if (t.v.typecls.cls & ilc_cls_t::f_signed) s += " signed";
				if (t.v.typecls.cls & ilc_cls_t::f_unsigned) s += " unsigned";
				if (t.v.typecls.cls & ilc_cls_t::f_const) s += " const";
				if (t.v.typecls.cls & ilc_cls_t::f_constexpr) s += " constexpr";
				if (t.v.typecls.cls & ilc_cls_t::f_compileexpr) s += " compileexpr";
				if (t.v.typecls.cls & ilc_cls_t::f_volatile) s += " volatile";

				if (t.v.typecls.other_id != token_identifier_none || t.v.typecls.other_t != token_type_t::none) {
					s += " other=";
					switch (t.v.typecls.other_t) {
						case token_type_t::r_auto: s += "auto"; break;
						case token_type_t::r_size_t: s += "size_t"; break;
						case token_type_t::r_ssize_t: s += "ssize_t"; break;

						case token_type_t::identifier:
						case token_type_t::none:
							s += "\"";
							s += token_identifier_value_get(t.v.typecls.other_id).name;
							s += "\"";
							break;

						default:
							s += "?";
							break;
					}
				}

				s += ">";
				break;
			case token_type_t::r_sizeof:
				s = "<r_sizeof>";
				break;
			case token_type_t::r_offsetof:
				s = "<r_offsetof>";
				break;
			case token_type_t::r_static_assert:
				s = "<r_static_assert>";
				break;
			case token_type_t::r_size_t:
				s = "<r_size_t>";
				break;
			case token_type_t::r_ssize_t:
				s = "<r_ssize_t>";
				break;
			case token_type_t::r_default:
				s = "<r_default>";
				break;
			case token_type_t::ellipsis:
				s = "<...>";
				break;
			case token_type_t::identifier:
				/* NTS: Everything is an identifier. The code handling the AST tree must make
				 *      sense of sizeof(), int, variable vs typedef, etc. on it's own */
				s = "<identifier: #";
				s += std::to_string(t.v.identifier);
				s += " \"";

				if (t.v.identifier != token_identifier_none)
					s += token_identifier_value_get(t.v.identifier).name;

				s += "\">";
				break;
			case token_type_t::characterliteral:
			case token_type_t::stringliteral:
				if (t.type == token_type_t::stringliteral)
					s = "<str-literal: ";
				else
					s = "<char-literal: ";

				snprintf(buf,sizeof(buf),"t=%u ",(unsigned int)t.v.chrstrlit.type);
				s += buf;
				/* the parsing code may initially use T_WIDE but will then change it to T_UTF16 or T_UTF32 */
				if (t.v.chrstrlit.type == token_charstrliteral_t::strtype_t::T_UTF32) {
					uint32_t *b = (uint32_t*)t.v.chrstrlit.data;

					s += "{";
					for (size_t i=0;i < (t.v.chrstrlit.length/sizeof(uint32_t));i++) {
						snprintf(buf,sizeof(buf)," 0x%x",b[i]);
						s += buf;
					}
					s += " }";

					s += " \"";
					for (size_t i=0;i < (t.v.chrstrlit.length/sizeof(uint32_t));i++) {
						if (b[i] >= 0x20) {
							std::vector<uint8_t> tmp;
							vec_encode_utf8(tmp,b[i]);
							for (auto i=tmp.begin();i!=tmp.end();i++) s += (char)(*i);
						}
						else if (b[i] == '\r') {
							s += "\\r";
						}
						else if (b[i] == '\n') {
							s += "\\n";
						}
						else if (b[i] == '\t') {
							s += "\\t";
						}
						else {
							s += ".";
						}
					}
					s += "\"";
				}
				else if (t.v.chrstrlit.type == token_charstrliteral_t::strtype_t::T_UTF16) { 
					uint16_t *b = (uint16_t*)t.v.chrstrlit.data;

					s += "{";
					for (size_t i=0;i < (t.v.chrstrlit.length/sizeof(uint16_t));i++) {
						snprintf(buf,sizeof(buf)," 0x%x",b[i]);
						s += buf;
					}
					s += " }";

					s += " \"";
					for (size_t i=0;i < (t.v.chrstrlit.length/sizeof(uint16_t));i++) {
						if ((i+2) <= (t.v.chrstrlit.length/sizeof(uint16_t)) && (b[i]&0xFC00u) == 0xD800u && (b[i+1]&0xFC00u) == 0xDC00u) {
							/* surrogage pair */
							std::vector<uint8_t> tmp;
							const uint32_t code = 0x10000 + ((b[i]&0x3FF) << 10u) + (b[i+1]&0x3FFu);
							vec_encode_utf8(tmp,code);
							for (auto i=tmp.begin();i!=tmp.end();i++) s += (char)(*i);
							i++; // we read two, for loop increments one
						}
						else if (b[i] >= 0x20) {
							std::vector<uint8_t> tmp;
							vec_encode_utf8(tmp,b[i]);
							for (auto i=tmp.begin();i!=tmp.end();i++) s += (char)(*i);
						}
						else if (b[i] == '\r') {
							s += "\\r";
						}
						else if (b[i] == '\n') {
							s += "\\n";
						}
						else if (b[i] == '\t') {
							s += "\\t";
						}
						else {
							s += ".";
						}
					}
					s += "\"";
				}
				else {
					unsigned char *b = (unsigned char*)t.v.chrstrlit.data;

					s += "{";
					for (size_t i=0;i < t.v.chrstrlit.length;i++) {
						snprintf(buf,sizeof(buf)," 0x%x",b[i]);
						s += buf;
					}
					s += " }";

					s += " \"";
					for (size_t i=0;i < t.v.chrstrlit.length;i++) {
						if (b[i] >= 0x20 && b[i] <= 0x7E) {
							s += (char)b[i];
						}
						else if (b[i] >= 0x80 && t.v.chrstrlit.type == token_charstrliteral_t::strtype_t::T_UTF8) {
							s += (char)b[i];
						}
						else if (b[i] == '\r') {
							s += "\\r";
						}
						else if (b[i] == '\n') {
							s += "\\n";
						}
						else if (b[i] == '\t') {
							s += "\\t";
						}
						else {
							s += ".";
						}
					}
					s += "\"";
				}
				s += ">";
				break;
			default:
				s = "?";
				break;
		}
	}

	static const std::string empty_const_string;

	const std::string &toksnames_getname(CIMCC::token_source_name_list_t *toksnames,const size_t ent) {
		if (toksnames) {
			if (ent < (*toksnames).size())
				return (*toksnames)[ent].name;
		}

		return empty_const_string;
	}

	void dump_ast_nodes(ast_node_t *parent,CIMCC::token_source_name_list_t *toksnames,const size_t depth=0) {
		std::string s;
		std::string indent;

		for (size_t i=0;i < depth;i++)
			indent += "..";

		if (depth == 0)
			fprintf(stderr,"AST TREE:\n");

		for (;parent;parent=parent->next) {
			const char *name;

			switch (parent->op) {
				case ast_node_op_t::none:
					name = "none";
					break;
				case ast_node_op_t::constant:
					name = "const";
					break;
				case ast_node_op_t::negate:
					name = "negate";
					break;
				case ast_node_op_t::unaryplus:
					name = "unaryplus";
					break;
				case ast_node_op_t::comma:
					name = "comma";
					break;
				case ast_node_op_t::statement:
					name = "statement";
					break;
				case ast_node_op_t::logicalnot:
					name = "logicalnot";
					break;
				case ast_node_op_t::binarynot:
					name = "binarynot";
					break;
				case ast_node_op_t::add:
					name = "add";
					break;
				case ast_node_op_t::subtract:
					name = "subtract";
					break;
				case ast_node_op_t::multiply:
					name = "multiply";
					break;
				case ast_node_op_t::divide:
					name = "divide";
					break;
				case ast_node_op_t::modulo:
					name = "modulo";
					break;
				case ast_node_op_t::assign:
					name = "assign";
					break;
				case ast_node_op_t::assignadd:
					name = "assignadd";
					break;
				case ast_node_op_t::assignsubtract:
					name = "assignsubtract";
					break;
				case ast_node_op_t::assignmultiply:
					name = "assignmultiply";
					break;
				case ast_node_op_t::assigndivide:
					name = "assigndivide";
					break;
				case ast_node_op_t::assignmodulo:
					name = "assignmodulo";
					break;
				case ast_node_op_t::assignand:
					name = "assignand";
					break;
				case ast_node_op_t::assignor:
					name = "assignor";
					break;
				case ast_node_op_t::assignxor:
					name = "assignxor";
					break;
				case ast_node_op_t::ternary:
					name = "ternary";
					break;
				case ast_node_op_t::logical_or:
					name = "logical_or";
					break;
				case ast_node_op_t::logical_and:
					name = "logical_and";
					break;
				case ast_node_op_t::binary_or:
					name = "binary_or";
					break;
				case ast_node_op_t::binary_xor:
					name = "binary_xor";
					break;
				case ast_node_op_t::binary_and:
					name = "binary_and";
					break;
				case ast_node_op_t::equals:
					name = "equals";
					break;
				case ast_node_op_t::notequals:
					name = "notequals";
					break;
				case ast_node_op_t::lessthan:
					name = "lessthan";
					break;
				case ast_node_op_t::greaterthan:
					name = "greaterthan";
					break;
				case ast_node_op_t::lessthanorequal:
					name = "lessthanorequal";
					break;
				case ast_node_op_t::greaterthanorequal:
					name = "greaterthanorequal";
					break;
				case ast_node_op_t::leftshift:
					name = "llshift";
					break;
				case ast_node_op_t::rightshift:
					name = "rrshift";
					break;
				case ast_node_op_t::assignleftshift:
					name = "assignllshift";
					break;
				case ast_node_op_t::assignrightshift:
					name = "assignrrshift";
					break;
				case ast_node_op_t::postdecrement:
					name = "decrement--";
					break;
				case ast_node_op_t::postincrement:
					name = "increment++";
					break;
				case ast_node_op_t::predecrement:
					name = "--decrement";
					break;
				case ast_node_op_t::preincrement:
					name = "++increment";
					break;
				case ast_node_op_t::dereference:
					name = "deref";
					break;
				case ast_node_op_t::addressof:
					name = "addrof";
					break;
				case ast_node_op_t::structaccess:
					name = "structaccess";
					break;
				case ast_node_op_t::structptraccess:
					name = "structptraccess";
					break;
				case ast_node_op_t::arraysubscript:
					name = "arraysubscript";
					break;
				case ast_node_op_t::subexpression:
					name = "subexpression";
					break;
				case ast_node_op_t::typecast:
					name = "typecast";
					break;
				case ast_node_op_t::functioncall:
					name = "functioncall";
					break;
				case ast_node_op_t::argument:
					name = "argument";
					break;
				case ast_node_op_t::identifier_list:
					name = "identifier_list";
					break;
				case ast_node_op_t::identifier:
					name = "identifier";
					break;
				case ast_node_op_t::strcat:
					name = "strcat";
					break;
				case ast_node_op_t::scope:
					name = "scope";
					break;
				case ast_node_op_t::r_return:
					name = "r_return";
					break;
				case ast_node_op_t::r_goto:
					name = "r_goto";
					break;
				case ast_node_op_t::r_else:
					name = "r_else";
					break;
				case ast_node_op_t::r_if:
					name = "r_if";
					break;
				case ast_node_op_t::r_pound_if:
					name = "r_pound_if";
					break;
				case ast_node_op_t::r_pound_define:
					name = "r_pound_define";
					break;
				case ast_node_op_t::r_pound_undef:
					name = "r_pound_undef";
					break;
				case ast_node_op_t::r_pound_defined:
					name = "r_pound_defined";
					break;
				case ast_node_op_t::r_pound_defval:
					name = "r_pound_defval";
					break;
				case ast_node_op_t::r_pound_warning:
					name = "r_pound_warning";
					break;
				case ast_node_op_t::r_pound_error:
					name = "r_pound_error";
					break;
				case ast_node_op_t::r_pound_deftype:
					name = "r_pound_deftype";
					break;
				case ast_node_op_t::r_pound_type:
					name = "r_pound_type";
					break;
				case ast_node_op_t::r_pound_include:
					name = "r_pound_include";
					break;
				case ast_node_op_t::label:
					name = "label";
					break;
				case ast_node_op_t::scopeoperator:
					name = "scopeoperator";
					break;
				case ast_node_op_t::r_break:
					name = "r_break";
					break;
				case ast_node_op_t::r_continue:
					name = "r_continue";
					break;
				case ast_node_op_t::r_switch:
					name = "r_switch";
					break;
				case ast_node_op_t::r_case:
					name = "r_case";
					break;
				case ast_node_op_t::r_default:
					name = "r_default";
					break;
				case ast_node_op_t::r_while:
					name = "r_while";
					break;
				case ast_node_op_t::r_do:
					name = "do";
					break;
				case ast_node_op_t::r_for:
					name = "for";
					break;
				case ast_node_op_t::r_let:
					name = "let";
					break;
				case ast_node_op_t::r_fn:
					name = "fn";
					break;
				case ast_node_op_t::r_const:
					name = "const";
					break;
				case ast_node_op_t::r_extern:
					name = "extern";
					break;
				case ast_node_op_t::r_auto:
					name = "auto";
					break;
				case ast_node_op_t::r_int:
					name = "int";
					break;
				case ast_node_op_t::r_bool:
					name = "bool";
					break;
				case ast_node_op_t::r_near:
					name = "near";
					break;
				case ast_node_op_t::r_far:
					name = "far";
					break;
				case ast_node_op_t::r_asm:
					name = "asm";
					break;
				case ast_node_op_t::r_this:
					name = "this";
					break;
				case ast_node_op_t::r_union:
					name = "union";
					break;
				case ast_node_op_t::r_struct:
					name = "struct";
					break;
				case ast_node_op_t::r_huge:
					name = "huge";
					break;
				case ast_node_op_t::r_float:
					name = "float";
					break;
				case ast_node_op_t::r_double:
					name = "double";
					break;
				case ast_node_op_t::r_void:
					name = "void";
					break;
				case ast_node_op_t::r_char:
					name = "char";
					break;
				case ast_node_op_t::ellipsis:
					name = "ellipsis";
					break;
				case ast_node_op_t::named_parameter:
					name = "named param";
					break;
				case ast_node_op_t::named_arg_required_boundary:
					name = "named arg req boundary";
					break;
				case ast_node_op_t::r_volatile:
					name = "volatile";
					break;
				case ast_node_op_t::r_signed:
					name = "signed";
					break;
				case ast_node_op_t::r_unsigned:
					name = "unsigned";
					break;
				case ast_node_op_t::r_long:
					name = "long";
					break;
				case ast_node_op_t::r_short:
					name = "short";
					break;
				case ast_node_op_t::r_static:
					name = "static";
					break;
				case ast_node_op_t::r_constexpr:
					name = "constexpr";
					break;
				case ast_node_op_t::r_compileexpr:
					name = "compileexpr";
					break;
				case ast_node_op_t::r_namespace:
					name = "namespace";
					break;
				case ast_node_op_t::r_using:
					name = "using";
					break;
				case ast_node_op_t::r_typeof:
					name = "typeof";
					break;
				case ast_node_op_t::r_typedef:
					name = "typedef";
					break;
				case ast_node_op_t::r_typeclsif:
					name = "typeclsif";
					break;
				case ast_node_op_t::r_sizeof:
					name = "sizeof";
					break;
				case ast_node_op_t::r_offsetof:
					name = "offsetof";
					break;
				case ast_node_op_t::r_static_assert:
					name = "static_assert";
					break;
				case ast_node_op_t::r_size_t:
					name = "size_t";
					break;
				case ast_node_op_t::r_ssize_t:
					name = "ssize_t";
					break;
				case ast_node_op_t::i_compound_let:
					name = "compound let";
					break;
				case ast_node_op_t::i_datatype:
					name = "datatype";
					break;
				case ast_node_op_t::i_anonymous:
					name = "anonymous";
					break;
				case ast_node_op_t::i_as:
					name = "as";
					break;
				case ast_node_op_t::i_array:
					name = "array";
					break;
				case ast_node_op_t::i_attributes:
					name = "attributes";
					break;
				default:
					name = "?";
					break;
			};

			token_to_string(s,parent->tv);
			fprintf(stderr,"..%s%s: %s",
				indent.c_str(),name,s.c_str());
			if (parent->tv.position.line >= 0)
				fprintf(stderr," line=%d",parent->tv.position.line);
			if (parent->tv.position.col >= 0)
				fprintf(stderr," col=%d",parent->tv.position.col);
			if (parent->tv.position.source != position_t::nosource)
				fprintf(stderr," source='%s'",toksnames_getname(toksnames,parent->tv.position.source).c_str());
			fprintf(stderr,"\n");
			dump_ast_nodes(parent->child,toksnames,depth+1u);
		}
	}

}

///////////////////////////////
struct src_ctx {
	int				fd = -1;

	void close(void) {
		if (fd >= 0) ::close(fd);
		fd = -1;
	}

	src_ctx() { }
	~src_ctx() { close(); }
};

static int refill_fdio(unsigned char *at,size_t sz,void *context,size_t context_size) {
	assert(context_size >= sizeof(src_ctx));
	assert(context != NULL);
	src_ctx *ctx = (src_ctx*)context;
	return read(ctx->fd,at,sz);
}

int main(int argc,char **argv) {
	if (argc < 2) {
		fprintf(stderr,"Specify file\n");
		return 1;
	}

	{
		CIMCC::token_source_name_list_t toksnames;
		CIMCC::compiler cc;
		src_ctx fdctx;

		fdctx.fd = open(argv[1],O_RDONLY);
		if (fdctx.fd < 0) {
			fprintf(stderr,"Failed to open file\n");
			return 1;
		}
		cc.set_source_name(argv[1]);
		cc.set_source_cb(refill_fdio);
		cc.set_source_ctx(&fdctx,sizeof(fdctx));
		cc.set_source_name_list(&toksnames);

		if (!cc.compile()) {
			fprintf(stderr,"Failed to compile\n");
			dump_ast_nodes(cc.root_node,&toksnames);
			cc.free_ast();
			return 1;
		}
		dump_ast_nodes(cc.root_node,&toksnames);
		cc.free_ast();

		/* fdctx destructor closes file descriptor */
	}

	return 0;
}

