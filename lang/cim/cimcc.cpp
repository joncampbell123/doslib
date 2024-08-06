
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <fcntl.h>

#include <type_traits>
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
	struct token_identifier_t {
		char*					name;
		size_t					length;

		void init(void) {
			name = NULL;
			length = 0;
		}

		void on_post_move(void) {
			/* this struct has already been copied, this is the old copy, clear pointers to prevent double free() */
			name = NULL;
			length = 0;
		}

		void on_pre_move(void) { /* our var is about to be replaced by a new var on std::move */
			length = 0;
			if (name) delete[] name;
			name = NULL;
		}

		void on_delete(void) {
			length = 0;
			if (name) delete[] name;
			name = NULL;
		}

		bool strcmp(const std::string &s) {
			if ((name == NULL || length == 0) && s.empty())
				return true;
			else if (memcmp(name,s.c_str(),length) == 0)
				return true;
			else
				return false;
		}
	};

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

		void on_delete(void) {
			length = 0;
			if (data) free(data);
			data = NULL;
		}
	};

	struct ast_node_t;

	struct ilc_cls_t {
private:
		static constexpr unsigned int c_int =         (1u <<  0u);
		static constexpr unsigned int c_float =       (1u <<  1u);
		static constexpr unsigned int c_other =       (1u <<  2u);
		static constexpr unsigned int i_bool =        (1u <<  3u);
		static constexpr unsigned int i_char =        (1u <<  4u);
		static constexpr unsigned int i_int =         (1u <<  5u);
		static constexpr unsigned int i_short =       (1u <<  6u);
		static constexpr unsigned int i_long =        (1u <<  7u);
		static constexpr unsigned int i_llong =       (1u <<  8u);
public:
		static constexpr unsigned int i_signed =      (1u <<  9u);
		static constexpr unsigned int i_unsigned =    (1u << 10u);
private:
		static constexpr unsigned int f_double =      (1u << 11u);
		static constexpr unsigned int f_float =       (1u << 12u);
public:
		static constexpr unsigned int g_const =       (1u << 13u);
		static constexpr unsigned int g_constexpr =   (1u << 14u);
		static constexpr unsigned int g_compileexpr = (1u << 15u);
		static constexpr unsigned int g_volatile =    (1u << 16u);
		static constexpr unsigned int g_static =      (1u << 17u);
private:
		static constexpr unsigned int p_near =        (1u << 18u);
		static constexpr unsigned int p_far =         (1u << 19u);
		static constexpr unsigned int p_huge =        (1u << 20u);

		static constexpr unsigned int c_valid =       (1u << 31u); /* this has been evaluated */

		/* do not leave these bits set after parsing */
		static constexpr unsigned int f_non_public =
			c_int|c_float|c_other|
			i_bool|i_char|i_int|i_short|i_long|i_llong|
			f_float|f_double|
			p_near|p_far|p_huge;
public:

		enum class c_t {
			ct_none=0,
			ct_int,
			ct_float,
			ct_other
		};

		enum class t_t {
			t_none=0,
			t_bool,
			t_char,
			t_int,
			t_short,
			t_long,
			t_llong,
			t_float,
			t_double,
			t_longdouble
		};

		enum class p_t {
			p_none=0,
			p_near,
			p_far,
			p_huge
		};

		unsigned int cls;
		c_t cls_c;
		t_t cls_t;
		p_t cls_p;

		void init(void);
		bool parse_ident_node(ast_node_t* &chk,bool idlist=false);
		bool parse_ident(ast_node_t* &chk);
		bool parse_idlist(ast_node_t* &chk);
		bool parse_idlist_final(void);
	};

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

		private:

		void common_delete(void) {
			switch (type) {
				case token_type_t::identifier:
					v.identifier.on_delete();
					break;
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
				case token_type_t::identifier:
					v.identifier.on_pre_move();
					break;
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
				case token_type_t::identifier:
					x.v.identifier.on_post_move();
					break;
				case token_type_t::characterliteral:
				case token_type_t::stringliteral:
					x.v.chrstrlit.on_post_move();
					break;
				default:
					break;
			}
		}
	};

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

		maxval
	};

	struct ast_node_t {
		struct ast_node_t*		next = NULL;
		struct ast_node_t*		child = NULL;
		ast_node_op_t			op = ast_node_op_t::none;
		struct token_t			tv;

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
		bool reduce(void);
		bool compile(void);
		void free_ast(void);
		void whitespace(void);
		void gtok(token_t &t);
		void gtok_number(token_t &t);
		int64_t getb_hex_cpp23(void);
		bool reduce(ast_node_t* &root);
		int64_t getb_octal_cpp23(void);
		void gtok_identifier(token_t &t);
		void gtok_prep_number_proc(void);
		int64_t getb_hex(unsigned int mc);
		int64_t getb_octal(unsigned int mc);
		bool expression(ast_node_t* &pchnode);
		bool if_statement(ast_node_t* &apnode);
		void skip_numeric_digit_separator(void);
		bool unary_expression(ast_node_t* &pchnode);
		bool shift_expression(ast_node_t* &pchnode);
		bool if_statement_parse(ast_node_t* &apnode);
		bool postfix_expression(ast_node_t* &pchnode);
		bool primary_expression(ast_node_t* &pchnode);
		bool additive_expression(ast_node_t* &pchnode);
		bool equality_expression(ast_node_t* &pchnode);
		bool cpp_scope_expression(ast_node_t* &pchnode);
		bool binary_or_expression(ast_node_t* &pchnode);
		bool relational_expression(ast_node_t* &pchnode);
		bool binary_xor_expression(ast_node_t* &pchnode);
		bool binary_and_expression(ast_node_t* &pchnode);
		bool logical_or_expression(ast_node_t* &pchnode);
		bool assignment_expression(ast_node_t* &pchnode);
		bool let_datatype_expression(ast_node_t* &tnode);
		bool logical_and_expression(ast_node_t* &pchnode);
		bool conditional_expression(ast_node_t* &pchnode);
		bool argument_expression_list(ast_node_t* &pchnode);
		bool multiplicative_expression(ast_node_t* &pchnode);
		bool fn_argument_expression_list(ast_node_t* &pchnode);
		bool statement(ast_node_t* &rnode,ast_node_t* &apnode);
		bool argument_expression_funccall(ast_node_t* &pchnode);
		bool let_expression(ast_node_t* &inode,ast_node_t* &enode);
		bool statement_does_not_need_semicolon(ast_node_t* apnode);
		void error_msg(const std::string msg,const position_t &pos);
		int64_t getb_with_escape(token_charstrliteral_t::strtype_t typ);
		bool fn_argument_expression(ast_node_t* &inode,ast_node_t* &enode);
		bool split_identifiers_expression(ast_node_t* &tnode,ast_node_t* &inode);
		unsigned int type_and_identifiers_expression(ast_node_t* &inode,unsigned int flags=0,ast_node_t*** inode_next=NULL);
		void gtok_chrstr_H_literal(const char qu,token_t &t,unsigned int flags=0,token_charstrliteral_t::strtype_t strtype = token_charstrliteral_t::strtype_t::T_BYTE);
		void gtok_chrstr_literal(const char qu,token_t &t,token_charstrliteral_t::strtype_t strtype = token_charstrliteral_t::strtype_t::T_BYTE);
		bool fn_expression(ast_node_t* &inode,ast_node_t* &alnode,ast_node_t* &bnode,unsigned int flags=0);
		bool gtok_check_ahead_H_identifier(const std::vector<uint8_t> &identifier,const char qu);
		int64_t getb_csc(token_charstrliteral_t::strtype_t typ);

		/* type and identifiers flags */
		static constexpr unsigned int TYPE_AND_IDENT_FL_ALLOW_FN = (1u << 0u);

		/* type and identifiers return value */
		static constexpr unsigned int TYPE_AND_IDENT_RT_FOUND = (1u << 0u);
		static constexpr unsigned int TYPE_AND_IDENT_RT_FN = (1u << 1u);

		/* fn_expression() flags */
		static constexpr unsigned int FN_EXPR_LET_EXPRESSION = (1u << 0u);
		static constexpr unsigned int FN_EXPR_ANONYMOUS = (1u << 1u);

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

		const token_t &tok_bufget(void) {
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

	static bool is_reserved_identifier(const token_type_t t);
	void token_to_string(std::string &s,const token_t &t);

	bool compiler::primary_expression(ast_node_t* &pchnode) {
		/* the bufpeek/get functions return a stock empty token if we read beyond available tokens */
		token_t &t = tok_bufpeek();

		if (t.type == token_type_t::intval || t.type == token_type_t::floatval || t.type == token_type_t::characterliteral) {
			assert(pchnode == NULL);
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::constant;
			pchnode->tv = std::move(t);
			tok_bufdiscard();
			return true;
		}
		else if (t.type == token_type_t::r_true || t.type == token_type_t::r_false) {
			assert(pchnode == NULL);
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::constant;
			pchnode->tv = std::move(t);
			pchnode->tv.type = token_type_t::intval;
			pchnode->tv.v.intval.initbool(t.type == token_type_t::r_true);
			tok_bufdiscard();
			return true;
		}
		else if (tok_bufpeek(0).type == token_type_t::poundsign && tok_bufpeek(1).type == token_type_t::identifier) {
			if (tok_bufpeek(1).v.identifier.strcmp("define")) {
				pchnode = new ast_node_t;
				pchnode->op = ast_node_op_t::r_pound_define;
				pchnode->tv = std::move(tok_bufpeek(0));
				tok_bufdiscard(2);
				return true;
			}
			else if (tok_bufpeek(1).v.identifier.strcmp("undef")) {
				pchnode = new ast_node_t;
				pchnode->op = ast_node_op_t::r_pound_undef;
				pchnode->tv = std::move(tok_bufpeek(0));
				tok_bufdiscard(2);
				return true;
			}
			else if (tok_bufpeek(1).v.identifier.strcmp("defined")) {
				pchnode = new ast_node_t;
				pchnode->op = ast_node_op_t::r_pound_defined;
				pchnode->tv = std::move(tok_bufpeek(0));
				tok_bufdiscard(2);
				return true;
			}
			else if (tok_bufpeek(1).v.identifier.strcmp("defval")) {
				pchnode = new ast_node_t;
				pchnode->op = ast_node_op_t::r_pound_defval;
				pchnode->tv = std::move(tok_bufpeek(0));
				tok_bufdiscard(2);
				return true;
			}
			else if (tok_bufpeek(1).v.identifier.strcmp("warning")) {
				pchnode = new ast_node_t;
				pchnode->op = ast_node_op_t::r_pound_warning;
				pchnode->tv = std::move(tok_bufpeek(0));
				tok_bufdiscard(2);
				return true;
			}
			else if (tok_bufpeek(1).v.identifier.strcmp("error")) {
				pchnode = new ast_node_t;
				pchnode->op = ast_node_op_t::r_pound_error;
				pchnode->tv = std::move(tok_bufpeek(0));
				tok_bufdiscard(2);
				return true;
			}
			else if (tok_bufpeek(1).v.identifier.strcmp("type")) {
				pchnode = new ast_node_t;
				pchnode->op = ast_node_op_t::r_pound_type;
				pchnode->tv = std::move(tok_bufpeek(0));
				tok_bufdiscard(2);
				return true;
			}
			else if (tok_bufpeek(1).v.identifier.strcmp("include")) {
				pchnode = new ast_node_t;
				pchnode->op = ast_node_op_t::r_pound_include;
				pchnode->tv = std::move(tok_bufpeek(0));
				tok_bufdiscard(2);
				return true;
			}
			else if (tok_bufpeek(1).v.identifier.strcmp("deftype")) {
				pchnode = new ast_node_t;
				pchnode->op = ast_node_op_t::r_pound_deftype;
				pchnode->tv = std::move(tok_bufpeek(0));
				tok_bufdiscard(2);

				if (tok_bufpeek().type != token_type_t::openparen) return false;
				tok_bufdiscard();

				if (!expression(pchnode->child))
					return false;

				if (tok_bufpeek().type != token_type_t::closeparen) return false;
				tok_bufdiscard();

				return true;
			}
			else {
				return false;
			}
		}
		else if (t.type == token_type_t::stringliteral) {
			assert(pchnode == NULL);
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::constant;
			pchnode->tv = std::move(t);
			tok_bufdiscard();

			/* build the AST nodes to ensure the string is concatenated left to right, i.e.
			 * "a" "b" "c" "d" becomes strcat(strcat(strcat("a" "b") "c") "d") not
			 * strcat(strcat(strcat("c" "d") "b") "a") */
			while (tok_bufpeek().type == token_type_t::stringliteral) {
				ast_node_t *sav_p = pchnode;
				pchnode = new ast_node_t;
				pchnode->op = ast_node_op_t::strcat;
				pchnode->child = sav_p;
				sav_p->next = new ast_node_t;
				sav_p->next->op = ast_node_op_t::constant;
				sav_p->next->tv = std::move(tok_bufpeek());
				tok_bufdiscard();
			}

			return true;
		}
		else if (t.type == token_type_t::r_const) {
			assert(pchnode == NULL);
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::r_const;
			pchnode->tv = std::move(t);
			tok_bufdiscard();

			return true;
		}
		else if (t.type == token_type_t::r_constexpr) {
			assert(pchnode == NULL);
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::r_constexpr;
			pchnode->tv = std::move(t);
			tok_bufdiscard();

			return true;
		}
		else if (t.type == token_type_t::r_compileexpr) {
			assert(pchnode == NULL);
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::r_compileexpr;
			pchnode->tv = std::move(t);
			tok_bufdiscard();

			return true;
		}
		else if (t.type == token_type_t::r_sizeof) {
			assert(pchnode == NULL);
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::r_sizeof;
			pchnode->tv = std::move(t);
			tok_bufdiscard();

			return true;
		}
		else if (t.type == token_type_t::r_offsetof) {
			assert(pchnode == NULL);
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::r_offsetof;
			pchnode->tv = std::move(t);
			tok_bufdiscard();

			return true;
		}
		else if (t.type == token_type_t::r_static_assert) {
			assert(pchnode == NULL);
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::r_static_assert;
			pchnode->tv = std::move(t);
			tok_bufdiscard();

			return true;
		}
		else if (t.type == token_type_t::r_size_t) {
			assert(pchnode == NULL);
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::r_size_t;
			pchnode->tv = std::move(t);
			tok_bufdiscard();

			return true;
		}
		else if (t.type == token_type_t::r_ssize_t) {
			assert(pchnode == NULL);
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::r_ssize_t;
			pchnode->tv = std::move(t);
			tok_bufdiscard();

			return true;
		}
		else if (t.type == token_type_t::r_static) {
			assert(pchnode == NULL);
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::r_static;
			pchnode->tv = std::move(t);
			tok_bufdiscard();

			return true;
		}
		else if (t.type == token_type_t::r_extern) {
			assert(pchnode == NULL);
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::r_extern;
			pchnode->tv = std::move(t);
			tok_bufdiscard();

			return true;
		}
		else if (t.type == token_type_t::r_auto) {
			assert(pchnode == NULL);
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::r_auto;
			pchnode->tv = std::move(t);
			tok_bufdiscard();

			return true;
		}
		else if (t.type == token_type_t::r_signed) {
			assert(pchnode == NULL);
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::r_signed;
			pchnode->tv = std::move(t);
			tok_bufdiscard();

			return true;
		}
		else if (t.type == token_type_t::r_unsigned) {
			assert(pchnode == NULL);
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::r_unsigned;
			pchnode->tv = std::move(t);
			tok_bufdiscard();

			return true;
		}
		else if (t.type == token_type_t::r_long) {
			assert(pchnode == NULL);
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::r_long;
			pchnode->tv = std::move(t);
			tok_bufdiscard();

			return true;
		}
		else if (t.type == token_type_t::r_short) {
			assert(pchnode == NULL);
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::r_short;
			pchnode->tv = std::move(t);
			tok_bufdiscard();

			return true;
		}
		else if (t.type == token_type_t::r_int) {
			assert(pchnode == NULL);
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::r_int;
			pchnode->tv = std::move(t);
			tok_bufdiscard();

			return true;
		}
		else if (t.type == token_type_t::r_bool) {
			assert(pchnode == NULL);
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::r_bool;
			pchnode->tv = std::move(t);
			tok_bufdiscard();

			return true;
		}
		else if (t.type == token_type_t::r_near) {
			assert(pchnode == NULL);
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::r_near;
			pchnode->tv = std::move(t);
			tok_bufdiscard();

			return true;
		}
		else if (t.type == token_type_t::r_far) {
			assert(pchnode == NULL);
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::r_far;
			pchnode->tv = std::move(t);
			tok_bufdiscard();

			return true;
		}
		else if (t.type == token_type_t::r_huge) {
			assert(pchnode == NULL);
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::r_huge;
			pchnode->tv = std::move(t);
			tok_bufdiscard();

			return true;
		}
		else if (t.type == token_type_t::r_float) {
			assert(pchnode == NULL);
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::r_float;
			pchnode->tv = std::move(t);
			tok_bufdiscard();

			return true;
		}
		else if (t.type == token_type_t::r_double) {
			assert(pchnode == NULL);
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::r_double;
			pchnode->tv = std::move(t);
			tok_bufdiscard();

			return true;
		}
		else if (t.type == token_type_t::r_this) {
			assert(pchnode == NULL);
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::r_this;
			pchnode->tv = std::move(t);
			tok_bufdiscard();

			return true;
		}
		else if (t.type == token_type_t::r_void) {
			assert(pchnode == NULL);
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::r_void;
			pchnode->tv = std::move(t);
			tok_bufdiscard();

			return true;
		}
		else if (t.type == token_type_t::r_char) {
			assert(pchnode == NULL);
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::r_char;
			pchnode->tv = std::move(t);
			tok_bufdiscard();

			return true;
		}
		else if (t.type == token_type_t::ellipsis) {
			assert(pchnode == NULL);
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::ellipsis;
			pchnode->tv = std::move(t);
			tok_bufdiscard();

			return true;
		}
		else if (t.type == token_type_t::r_volatile) {
			assert(pchnode == NULL);
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::r_volatile;
			pchnode->tv = std::move(t);
			tok_bufdiscard();

			return true;
		}
		else if (t.type == token_type_t::identifier) {
			assert(pchnode == NULL);
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::identifier;
			pchnode->tv = std::move(t);
			tok_bufdiscard();

			return true;
		}
		else if (t.type == token_type_t::r_typeof) {
			assert(pchnode == NULL);
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::r_typeof;
			pchnode->tv = std::move(t);
			tok_bufdiscard();

			if (tok_bufpeek().type != token_type_t::openparen) return false;
			tok_bufdiscard();

			if (!expression(pchnode->child))
				return false;

			if (tok_bufpeek().type != token_type_t::closeparen) return false;
			tok_bufdiscard();

			return true;
		}
		else if (t.type == token_type_t::dblleftsquarebracket) {
			assert(pchnode == NULL);
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::i_attributes;
			pchnode->tv = std::move(t);
			tok_bufdiscard();

			if (tok_bufpeek().type != token_type_t::dblrightsquarebracket) {
				ast_node_t **n = &(pchnode->child);

				/* using namespace: */
				if (tok_bufpeek(0).type == token_type_t::r_using && tok_bufpeek(1).type == token_type_t::identifier) {
					ast_node_t **sn;

					(*n) = new ast_node_t;
					(*n)->op = ast_node_op_t::r_using;
					(*n)->tv = std::move(tok_bufpeek());
					tok_bufdiscard();
					sn = &((*n)->child);
					n = &((*n)->next);

					(*sn) = new ast_node_t;
					(*sn)->op = ast_node_op_t::r_namespace;
					sn = &((*sn)->next);

					if (!cpp_scope_expression(*sn))
						return false;

					if (tok_bufpeek().type != token_type_t::colon) return false;
					tok_bufdiscard();
				}

				if (!assignment_expression(*n))
					return false;

				n = &((*n)->next);
				while (tok_bufpeek().type == token_type_t::comma) {
					tok_bufdiscard();

					if (!assignment_expression(*n))
						return false;

					n = &((*n)->next);
				}
			}

			if (tok_bufpeek().type != token_type_t::dblrightsquarebracket) return false;
			tok_bufdiscard();

			return true;
		}
		else if (t.type == token_type_t::openparen) {
			tok_bufdiscard(); /* eat it */

			/* [subexpression]
			 *  \
			 *   +-- [expression] */

			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::subexpression;
			if (!expression(pchnode->child))
				return false;

			{
				token_t &t = tok_bufpeek();
				if (t.type == token_type_t::closeparen)
					tok_bufdiscard(); /* eat it */
				else
					return false;
			}

			/* this might not be an expression, it might be a typecast.
			 * it depends on what follows after the closing parenthesis.
			 *
			 * (type)(expression)
			 * (type){compound literal}
			 * (type)identifier including (type)functioncall()
			 *
			 * Don't allow anything beyond that. If you want to typecast the negation of a variable you have to wrap it in parenthesis. */
			{
				token_t &nt = tok_bufpeek();

				if (	nt.type == token_type_t::openparen || nt.type == token_type_t::opencurly ||
					nt.type == token_type_t::identifier || is_reserved_identifier(nt.type) ||
					nt.type == token_type_t::intval || nt.type == token_type_t::floatval ||
					nt.type == token_type_t::characterliteral || nt.type == token_type_t::stringliteral) {
					pchnode->op = ast_node_op_t::typecast;
					if (!unary_expression(pchnode->child->next))
						return false;
				}
			}

			return true;
		}
		else if (t.type == token_type_t::opencurly) {
			tok_bufdiscard(); /* eat it */

			/* [scope]
			 *  \
			 *   +-- [expression]
			 */

			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::i_array;

			if (tok_bufpeek().type == token_type_t::closecurly) {
				/* well then it's a nothing */
			}
			else {
				if (!assignment_expression(pchnode->child))
					return false;

				ast_node_t *n = pchnode->child;
				while (tok_bufpeek().type == token_type_t::comma) {
					tok_bufdiscard();

					if (!assignment_expression(n->next))
						return false;

					n = n->next;
				}

				{
					token_t &t = tok_bufpeek();
					if (t.type == token_type_t::closecurly)
						tok_bufdiscard(); /* eat it */
					else
						return false;
				}
			}

			return true;
		}
		else if (t.type == token_type_t::r_fn) { /* anonymous function */
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::r_fn;
			pchnode->tv = std::move(tok_bufpeek());
			tok_bufdiscard();

			ast_node_t **n = &(pchnode->child);
			ast_node_t *i=NULL,*a=NULL,*b=NULL;

			/* first the type specification */
			{
				ast_node_t *t=NULL;

				if (!let_datatype_expression(t)) return false;
				assert(t != NULL);

				(*n) = new ast_node_t;
				(*n)->op = ast_node_op_t::i_datatype;
				(*n)->child = t;
				n = &((*n)->next);
			}

			if (!fn_expression(i,a,b,FN_EXPR_ANONYMOUS))
				return false;
			if (i == NULL)
				return false;

			if (i) { *n = i; n = &((*n)->next); while (*n) n = &((*n)->next); }
			if (a) { *n = a; n = &((*n)->next); while (*n) n = &((*n)->next); }
			if (b) { *n = b; n = &((*n)->next); while (*n) n = &((*n)->next); }

			return true;
		}

		return false;
	}

	bool compiler::fn_argument_expression_list(ast_node_t* &pchnode) {
		/* [argument] -> [argument] -> ...
		 *  \             \
		 *   +--- [expr]   +--- [expr] */
#define NLEX assignment_expression
		pchnode = new ast_node_t;
		pchnode->op = ast_node_op_t::argument;
		{
			ast_node_t **n = &(pchnode->child);
			ast_node_t *i=NULL,*e=NULL;

			assert(*n == NULL);
			if (!fn_argument_expression(i,e))
				return false;

			assert(*n == NULL);
			if (i) { *n = i; while (*n) n = &((*n)->next); }
			assert(*n == NULL);
			if (e) { *n = e; while (*n) n = &((*n)->next); }
		}

		ast_node_t *nb = pchnode;
		while (tok_bufpeek().type == token_type_t::comma) { /* , comma operator */
			tok_bufdiscard(); /* eat it */

			nb->next = new ast_node_t; nb = nb->next;
			nb->op = ast_node_op_t::argument;
			{
				ast_node_t **n = &(nb->child);
				ast_node_t *i=NULL,*e=NULL;

				assert(*n == NULL);
				if (!fn_argument_expression(i,e))
					return false;

				assert(*n == NULL);
				if (i) { *n = i; while (*n) n = &((*n)->next); }
				assert(*n == NULL);
				if (e) { *n = e; while (*n) n = &((*n)->next); }
			}
		}
#undef NLEX
		return true;
	}

	bool compiler::argument_expression_funccall(ast_node_t* &pchnode) {
#define NLEX assignment_expression
		if (tok_bufpeek(0).type == token_type_t::identifier && tok_bufpeek(1).type == token_type_t::colon) {
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::argument;
			pchnode->child = new ast_node_t;
			pchnode->child->op = ast_node_op_t::named_parameter;
			if (!primary_expression(pchnode->child->child))
				return false;

			if (tok_bufpeek().type != token_type_t::colon) return false;
			tok_bufdiscard();

			if (!NLEX(pchnode->child->next))
				return false;
		}
		else {
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::argument;
			if (!NLEX(pchnode->child))
				return false;
		}
#undef NLEX
		return true;
	}

	bool compiler::argument_expression_list(ast_node_t* &pchnode) {
		/* [argument] -> [argument] -> ...
		 *  \             \
		 *   +--- [expr]   +--- [expr] */
#define NLEX argument_expression_funccall
		if (!NLEX(pchnode))
			return false;

		ast_node_t *nb = pchnode;
		while (tok_bufpeek().type == token_type_t::comma) { /* , comma operator */
			tok_bufdiscard(); /* eat it */

			if (!NLEX(nb->next)) return false;
			nb = nb->next;
		}
#undef NLEX
		return true;
	}

	/* [https://en.cppreference.com/w/cpp/language/operator_precedence] level 1 */
	bool compiler::cpp_scope_expression(ast_node_t* &pchnode) {
#define NLEX primary_expression
		if (!NLEX(pchnode))
			return false;

		if (pchnode->op == ast_node_op_t::identifier) {
			while (tok_bufpeek().type == token_type_t::coloncolon) { /* :: but only identifiers */
				tok_bufdiscard(); /* eat it */

				/* [::]
				 *  \
				 *   +-- [left expr] -> [right expr] */

				ast_node_t *sav_p = pchnode;
				pchnode = new ast_node_t;
				pchnode->op = ast_node_op_t::scopeoperator;
				pchnode->child = sav_p;

				/* must be an identifier */
				if (tok_bufpeek().type == token_type_t::identifier) {
					if (!NLEX(sav_p->next))
						return false;
				}
				else {
					return false;
				}
			}
		}
#undef NLEX
		return true;
	}

	static bool is_reserved_identifier(const token_type_t t) {
		switch (t) {
			case token_type_t::r_const:
			case token_type_t::r_constexpr:
			case token_type_t::r_compileexpr:
			case token_type_t::r_static:
			case token_type_t::r_extern:
			case token_type_t::r_auto:
			case token_type_t::r_signed:
			case token_type_t::r_unsigned:
			case token_type_t::r_int:
			case token_type_t::r_long:
			case token_type_t::r_short:
			case token_type_t::r_bool:
			case token_type_t::r_near:
			case token_type_t::r_far:
			case token_type_t::r_huge:
			case token_type_t::r_float:
			case token_type_t::r_double:
			case token_type_t::r_void:
			case token_type_t::r_volatile:
			case token_type_t::r_char:
			case token_type_t::r_sizeof:
			case token_type_t::r_size_t:
			case token_type_t::r_ssize_t:
			case token_type_t::r_offsetof:
			case token_type_t::r_static_assert:
			case token_type_t::r_namespace:
			case token_type_t::r_typeof:
			case token_type_t::r_this:
				return true;
			default:
				break;
		}

		return false;
	}

	bool compiler::let_datatype_expression(ast_node_t* &tnode) {
#define NLEX cpp_scope_expression
		tnode = new ast_node_t;
		tnode->op = ast_node_op_t::identifier_list;
		tnode->tv.position = tok_bufpeek().position;

		ast_node_t **n = &(tnode->child);

		if (is_reserved_identifier(tok_bufpeek().type) || tok_bufpeek().type == token_type_t::dblleftsquarebracket || tok_bufpeek().type == token_type_t::poundsign) {
			while (is_reserved_identifier(tok_bufpeek().type) || tok_bufpeek().type == token_type_t::dblleftsquarebracket || tok_bufpeek().type == token_type_t::poundsign) {
				if (!NLEX(*n)) return false;
				n = &((*n)->next);
			}
		}
		else if (tok_bufpeek().type == token_type_t::identifier) {
			if (!NLEX(*n)) return false;
			n = &((*n)->next);
		}
		else {
			return false;
		}
#undef NLEX
		assert(tnode->child != NULL);
		return true;
	}

	bool compiler::split_identifiers_expression(ast_node_t* &tnode,ast_node_t* &inode) {
		tnode = new ast_node_t;
		tnode->op = ast_node_op_t::identifier_list;
		tnode->tv.position = tok_bufpeek().position;

		ast_node_t **n = &(tnode->child);
		int count = 0;

		/* int -> tnode (null) inode "int"
		 * long int -> tnode list("long") inode "int"
		 * int* -> tnode list("int") inode (null) and caller is expected to handle the * or & tokens
		 * signed long int -> tnode list("signed","long") inode "int"
		 * signed long int* -> tnode list("signed","long","int") inode (null) */
#define NLEX cpp_scope_expression
		while (is_reserved_identifier(tok_bufpeek().type) || tok_bufpeek().type == token_type_t::poundsign) {
			if (!NLEX(*n)) return false;
			n = &((*n)->next);
			count++;
		}

		if (tok_bufpeek().type == token_type_t::identifier) {
			if (!NLEX(inode)) return false;
			count++;

			if (count < 2 && tok_bufpeek().type == token_type_t::identifier) {
				if (inode) { (*n) = inode; inode = NULL; n = &((*n)->next); }
				if (!NLEX(inode)) return false;
				count++;
			}

			/* sorry, no more arbitrary sequence of identifiers, you're only allowed up to two */
			if (tok_bufpeek().type == token_type_t::identifier) return false;

			if (tok_bufpeek().type == token_type_t::star || tok_bufpeek().type == token_type_t::ampersand || tok_bufpeek().type == token_type_t::ampersandampersand) {
				if (inode) { (*n) = inode; inode = NULL; n = &((*n)->next); }
			}
		}

		if (tnode->child == NULL) {
			delete tnode;
			tnode = NULL;
		}

		/* if only a type, no identifier, inode == NULL and tnode != NULL which is OK */
		if (tnode == NULL && inode == NULL)
			return false;
#undef NLEX
		return true;
	}

	/* tnode = type node list
	 * inode = identifier node
	 *
	 * after_comma == true:
	 *   char x -> tnode identifier_list("char") inode "x"
	 *
	 * after_comma == false
	 *   x -> tnode (null) inode "x"
	 *
	 * tnode will never contain deref * or addrof & because those are specific to the identifier.
	 * char *x -> tnode identifier_list("char") inode deref(x)
	 *
	 * signed char *x,y,**z should be handled as:
	 *   tnode identifier_list("signed","char") inode deref("x") after_comma = false
	 *   tnode (null) inode "y" after_comma = true
	 *   tnode (null) inode deref(deref("z")) after_comma = true
	 *
	 * Use after_comma == false for first/only let declaration/definition, after_comma == true
	 * after any comma in a compound let */
	unsigned int compiler::type_and_identifiers_expression(ast_node_t* &inode,unsigned int flags,ast_node_t*** inode_next) {
		unsigned int retv = TYPE_AND_IDENT_RT_FOUND;

#define NLEX cpp_scope_expression
		if (inode == NULL) {
			ast_node_t **d = &inode;

			while (1) {
				if (tok_bufpeek().type == token_type_t::star) {
					tok_bufdiscard();
					assert(*d == NULL);
					(*d) = new ast_node_t;
					(*d)->op = ast_node_op_t::dereference;
					d = &((*d)->child);
				}
				else if (tok_bufpeek().type == token_type_t::ampersand) {
					tok_bufdiscard();
					assert(*d == NULL);
					(*d) = new ast_node_t;
					(*d)->op = ast_node_op_t::addressof;
					d = &((*d)->child);
				}
				else if (tok_bufpeek().type == token_type_t::ampersandampersand) {
					tok_bufdiscard();

					assert(*d == NULL);
					(*d) = new ast_node_t;
					(*d)->op = ast_node_op_t::addressof;
					d = &((*d)->child);

					assert(*d == NULL);
					(*d) = new ast_node_t;
					(*d)->op = ast_node_op_t::addressof;
					d = &((*d)->child);
				}
				else if (retv & TYPE_AND_IDENT_RT_FN) {
					/* once "fn" is encountered stop on anything other than * and & */
					break;
				}
				else if (tok_bufpeek().type == token_type_t::dblleftsquarebracket) {
					if (!primary_expression(*d)) return false;
					d = &((*d)->next);
					assert(*d == NULL);
				}
				else if (tok_bufpeek().type == token_type_t::identifier || is_reserved_identifier(tok_bufpeek().type) || tok_bufpeek().type == token_type_t::poundsign) {
					ast_node_t *t=NULL,*i=NULL;
					if (!split_identifiers_expression(t,i))
						return false;

					assert(t != NULL || i != NULL);
					if (t) { *d = t; d = &((*d)->next); }
					if (i) { *d = i; d = &((*d)->next); }
				}
				else if (tok_bufpeek().type == token_type_t::r_fn && (flags & TYPE_AND_IDENT_FL_ALLOW_FN)) {
					tok_bufdiscard();
					assert(*d == NULL);
					(*d) = new ast_node_t;
					(*d)->op = ast_node_op_t::r_fn;
					d = &((*d)->child);
					retv |= TYPE_AND_IDENT_RT_FN;
				}
				else {
					break;
				}
			}

			if (inode_next)
				*inode_next = d;
		}

		if (inode == NULL)
			return 0;
#undef NLEX
		return retv;
	}

	/* [https://en.cppreference.com/w/c/language/operator_precedence] level 1 */
	/* [https://en.cppreference.com/w/cpp/language/operator_precedence] level 2 */
	bool compiler::postfix_expression(ast_node_t* &pchnode) {
#define NLEX cpp_scope_expression
		if (!NLEX(pchnode))
			return false;

		if (	pchnode->op == ast_node_op_t::r_const || pchnode->op == ast_node_op_t::r_signed || pchnode->op == ast_node_op_t::r_long ||
			pchnode->op == ast_node_op_t::r_int || pchnode->op == ast_node_op_t::r_float || pchnode->op == ast_node_op_t::r_void ||
			pchnode->op == ast_node_op_t::r_char || pchnode->op == ast_node_op_t::r_volatile || pchnode->op == ast_node_op_t::scopeoperator ||
			pchnode->op == ast_node_op_t::r_bool || pchnode->op == ast_node_op_t::r_near || pchnode->op == ast_node_op_t::r_far ||
			pchnode->op == ast_node_op_t::r_huge || pchnode->op == ast_node_op_t::r_pound_type || pchnode->op == ast_node_op_t::r_short ||
			pchnode->op == ast_node_op_t::r_double || pchnode->op == ast_node_op_t::r_unsigned || pchnode->op == ast_node_op_t::r_near ||
			pchnode->op == ast_node_op_t::r_far ||pchnode->op == ast_node_op_t::r_huge) {
			/* Allow multiple consecutive reserved identifiers. Do not allow multiple consecutive arbitrary identifiers.
			 *
			 * This is necessary in order for later parsing to handle a statement like:
			 *
			 * "signed long int variable1 = 45l;"
			 *
			 * We also would like to support:
			 *
			 * "namespaced::type = 45l;"
			 *
			 */
			/* NTS: ->next is used to chain this identifier to another operand i.e. + or -, use ->child */
			if (tok_bufpeek().type == token_type_t::identifier || tok_bufpeek().type == token_type_t::poundsign || is_reserved_identifier(tok_bufpeek().type)) {
				ast_node_t *sav_p = pchnode;
				pchnode = new ast_node_t;
				pchnode->op = ast_node_op_t::identifier_list;
				pchnode->tv.position = sav_p->tv.position;
				pchnode->child = sav_p;

				ast_node_t *nn = sav_p;

				while (is_reserved_identifier(tok_bufpeek().type) || tok_bufpeek().type == token_type_t::poundsign) {
					if (!NLEX(nn->next))
						return false;
					nn = nn->next;
				}

				if (tok_bufpeek().type == token_type_t::identifier) {
					if (!NLEX(nn->next))
						return false;
					nn = nn->next;
				}
			}
		}

		/* This is written differently, because we need it built "inside out" */
		while (1) {
			if (tok_bufpeek().type == token_type_t::plusplus) { /* ++ */
				tok_bufdiscard(); /* eat it */

				/* [++]
				 *  \
				 *   +-- [expression] */

				ast_node_t *sav_p = pchnode;
				pchnode = new ast_node_t;
				pchnode->op = ast_node_op_t::postincrement;
				pchnode->child = sav_p;
			}
			else if (tok_bufpeek().type == token_type_t::minusminus) { /* -- */
				tok_bufdiscard(); /* eat it */

				/* [--]
				 *  \
				 *   +-- [expression] */

				ast_node_t *sav_p = pchnode;
				pchnode = new ast_node_t;
				pchnode->op = ast_node_op_t::postdecrement;
				pchnode->child = sav_p;
			}
			else if (tok_bufpeek().type == token_type_t::period) { /* . member */
				tok_bufdiscard(); /* eat it */

				/* [.]
				 *  \
				 *   +-- [left expr] -> [right expr] */

				ast_node_t *sav_p = pchnode;
				pchnode = new ast_node_t;
				pchnode->op = ast_node_op_t::structaccess;
				pchnode->child = sav_p;
				if (!NLEX(sav_p->next)) // TODO: should be IDENTIFIER only
					return false;
			}
			else if (tok_bufpeek().type == token_type_t::pointerarrow) { /* -> member */
				tok_bufdiscard(); /* eat it */

				/* [->]
				 *  \
				 *   +-- [left expr] -> [right expr] */

				ast_node_t *sav_p = pchnode;
				pchnode = new ast_node_t;
				pchnode->op = ast_node_op_t::structptraccess;
				pchnode->child = sav_p;
				if (!NLEX(sav_p->next)) // TODO: should be IDENTIFIER only
					return false;
			}
			else if (tok_bufpeek().type == token_type_t::leftsquarebracket) { /* [expr] array subscript */
				tok_bufdiscard(); /* eat it */

				/* [arraysubscript]
				 *  \
				 *   +-- [left expr] -> [subscript expr] */

				ast_node_t *sav_p = pchnode;
				pchnode = new ast_node_t;
				pchnode->op = ast_node_op_t::arraysubscript;
				pchnode->child = sav_p;
				if (!expression(sav_p->next))
					return false;

				{
					token_t &t = tok_bufpeek();
					if (t.type == token_type_t::rightsquarebracket)
						tok_bufdiscard(); /* eat it */
					else
						return false;
				}
			}
			else if (tok_bufpeek().type == token_type_t::openparen) { /* (expr) function call */
				tok_bufdiscard(); /* eat it */

				/* [function]
				 *  \
				 *   +-- [left expr] -> [argument] -> [argument] -> [argument] ... */

				ast_node_t *sav_p = pchnode;
				pchnode = new ast_node_t;
				pchnode->op = ast_node_op_t::functioncall;
				pchnode->child = sav_p;

				if (tok_bufpeek().type != token_type_t::closeparen) {
					if (!argument_expression_list(sav_p->next))
						return false;

					while (sav_p->next != NULL)
						sav_p = sav_p->next;
				}

				{
					token_t &t = tok_bufpeek();
					if (t.type == token_type_t::closeparen)
						tok_bufdiscard(); /* eat it */
					else
						return false;
				}

				/* if the child node is an identifier list, not an identifier,
				 * then someone is trying to make a C style definition/declaration
				 * and we're mistaking it as a function call */
				assert(pchnode->child != NULL);
				if (pchnode->child->op == ast_node_op_t::identifier_list)
					return false;
			}
			else {
				break;
			}
		}
#undef NLEX
		return true;
	}

	/* [https://en.cppreference.com/w/c/language/operator_precedence] level 2 */
	/* [https://en.cppreference.com/w/cpp/language/operator_precedence] level 3 */
	bool compiler::unary_expression(ast_node_t* &pchnode) {
#define NLEX postfix_expression
		/* the bufpeek/get functions return a stock empty token if we read beyond available tokens */
		{
			token_t &t = tok_bufpeek();

			switch (t.type) {
				case token_type_t::ampersandampersand: /* '&&' in this way must be handled as '& &' */
					/* [&]
					 *  \
					 *   +--- [&]
					 *         \
					 *          +--- expression */
					assert(pchnode == NULL);
					pchnode = new ast_node_t;
					pchnode->op = ast_node_op_t::addressof;
					pchnode->child = new ast_node_t;
					pchnode->child->op = ast_node_op_t::addressof;
					pchnode->tv = std::move(tok_bufpeek(0)); tok_bufdiscard(); /* eat it */
					return unary_expression(pchnode->child->child);

				case token_type_t::ampersand:
					/* [&]
					 *  \
					 *   +--- expression */
					assert(pchnode == NULL);
					pchnode = new ast_node_t;
					pchnode->op = ast_node_op_t::addressof;
					pchnode->tv = std::move(tok_bufpeek(0)); tok_bufdiscard(); /* eat it */
					return unary_expression(pchnode->child);

				case token_type_t::star:
					/* [*]
					 *  \
					 *   +--- expression */
					assert(pchnode == NULL);
					pchnode = new ast_node_t;
					pchnode->op = ast_node_op_t::dereference;
					pchnode->tv = std::move(tok_bufpeek(0)); tok_bufdiscard(); /* eat it */
					return unary_expression(pchnode->child);

				case token_type_t::minusminus:
					/* [--]
					 *  \
					 *   +--- expression */
					assert(pchnode == NULL);
					pchnode = new ast_node_t;
					pchnode->op = ast_node_op_t::predecrement;
					pchnode->tv = std::move(tok_bufpeek(0)); tok_bufdiscard(); /* eat it */
					return unary_expression(pchnode->child);

				case token_type_t::plusplus:
					/* [++]
					 *  \
					 *   +--- expression */
					assert(pchnode == NULL);
					pchnode = new ast_node_t;
					pchnode->op = ast_node_op_t::preincrement;
					pchnode->tv = std::move(tok_bufpeek(0)); tok_bufdiscard(); /* eat it */
					return unary_expression(pchnode->child);

				case token_type_t::minus:
					/* [-]
					 *  \
					 *   +--- expression */
					assert(pchnode == NULL);
					pchnode = new ast_node_t;
					pchnode->op = ast_node_op_t::negate;
					pchnode->tv = std::move(tok_bufpeek(0)); tok_bufdiscard(); /* eat it */
					return unary_expression(pchnode->child);

				case token_type_t::plus:
					/* [+]
					 *  \
					 *   +--- expression */
					assert(pchnode == NULL);
					pchnode = new ast_node_t;
					pchnode->op = ast_node_op_t::unaryplus;
					pchnode->tv = std::move(tok_bufpeek(0)); tok_bufdiscard(); /* eat it */
					return unary_expression(pchnode->child);

				case token_type_t::exclamation:
					/* [!]
					 *  \
					 *   +--- expression */
					assert(pchnode == NULL);
					pchnode = new ast_node_t;
					pchnode->op = ast_node_op_t::logicalnot;
					pchnode->tv = std::move(tok_bufpeek(0)); tok_bufdiscard(); /* eat it */
					return unary_expression(pchnode->child);

				case token_type_t::tilde:
					/* [~]
					 *  \
					 *   +--- expression */
					assert(pchnode == NULL);
					pchnode = new ast_node_t;
					pchnode->op = ast_node_op_t::binarynot;
					pchnode->tv = std::move(tok_bufpeek(0)); tok_bufdiscard(); /* eat it */
					return unary_expression(pchnode->child);

				default:
					return NLEX(pchnode);
			}
		}
#undef NLEX
		return true;
	}

	/* [https://en.cppreference.com/w/c/language/operator_precedence] level 3 */
	/* [https://en.cppreference.com/w/cpp/language/operator_precedence] level 5 */
	bool compiler::multiplicative_expression(ast_node_t* &pchnode) {
#define NLEX unary_expression
		if (!NLEX(pchnode))
			return false;

		/* This is written differently, because we need it built "inside out" */
		while (1) {
			if (tok_bufpeek().type == token_type_t::star) { /* * operator */
				/* [*]
				 *  \
				 *   +-- [left expr] -> [right expr] */

				ast_node_t *sav_p = pchnode;
				pchnode = new ast_node_t;
				pchnode->op = ast_node_op_t::multiply;
				pchnode->tv = std::move(tok_bufpeek(0)); tok_bufdiscard(); /* eat it */
				pchnode->child = sav_p;
				if (!NLEX(sav_p->next))
					return false;
			}
			else if (tok_bufpeek().type == token_type_t::slash) { /* / operator */
				/* [/]
				 *  \
				 *   +-- [left expr] -> [right expr] */

				ast_node_t *sav_p = pchnode;
				pchnode = new ast_node_t;
				pchnode->op = ast_node_op_t::divide;
				pchnode->tv = std::move(tok_bufpeek(0)); tok_bufdiscard(); /* eat it */
				pchnode->child = sav_p;
				if (!NLEX(sav_p->next))
					return false;
			}
			else if (tok_bufpeek().type == token_type_t::percent) { /* % operator */
				/* [%]
				 *  \
				 *   +-- [left expr] -> [right expr] */

				ast_node_t *sav_p = pchnode;
				pchnode = new ast_node_t;
				pchnode->op = ast_node_op_t::modulo;
				pchnode->tv = std::move(tok_bufpeek(0)); tok_bufdiscard(); /* eat it */
				pchnode->child = sav_p;
				if (!NLEX(sav_p->next))
					return false;
			}
			else {
				break;
			}
		}
#undef NLEX
		return true;
	}

	/* [https://en.cppreference.com/w/c/language/operator_precedence] level 4 */
	/* [https://en.cppreference.com/w/cpp/language/operator_precedence] level 6 */
	bool compiler::additive_expression(ast_node_t* &pchnode) {
#define NLEX multiplicative_expression
		if (!NLEX(pchnode))
			return false;

		/* This is written differently, because we need it built "inside out" for addition and subtraction
		 * to be done in the correct order, else subtraction won't work properly i.e. 5-6-3 needs to be
		 * built as ((5-6)-3) not (5-(6-3)). */
		while (1) {
			if (tok_bufpeek().type == token_type_t::plus) { /* + operator */
				/* [+]
				 *  \
				 *   +-- [left expr] -> [right expr] */

				ast_node_t *sav_p = pchnode;
				pchnode = new ast_node_t;
				pchnode->op = ast_node_op_t::add;
				pchnode->tv = std::move(tok_bufpeek(0)); tok_bufdiscard(); /* eat it */
				pchnode->child = sav_p;
				if (!NLEX(sav_p->next))
					return false;
			}
			else if (tok_bufpeek().type == token_type_t::minus) { /* - operator */
				/* [-]
				 *  \
				 *   +-- [left expr] -> [right expr] */

				ast_node_t *sav_p = pchnode;
				pchnode = new ast_node_t;
				pchnode->op = ast_node_op_t::subtract;
				pchnode->tv = std::move(tok_bufpeek(0)); tok_bufdiscard(); /* eat it */
				pchnode->child = sav_p;
				if (!NLEX(sav_p->next))
					return false;
			}
			else {
				break;
			}
		}
#undef NLEX
		return true;
	}

	/* [https://en.cppreference.com/w/c/language/operator_precedence] level 5 */
	/* [https://en.cppreference.com/w/cpp/language/operator_precedence] level 7 */
	bool compiler::shift_expression(ast_node_t* &pchnode) {
#define NLEX additive_expression
		if (!NLEX(pchnode))
			return false;

		while (1) {
			if (tok_bufpeek().type == token_type_t::leftleftangle) { /* << operator */
				tok_bufdiscard(); /* eat it */

				/* [<<]
				 *  \
				 *   +-- [left expr] -> [right expr] */

				ast_node_t *sav_p = pchnode;
				pchnode = new ast_node_t;
				pchnode->op = ast_node_op_t::leftshift;
				pchnode->child = sav_p;
				if (!NLEX(sav_p->next))
					return false;
			}
			else if (tok_bufpeek().type == token_type_t::rightrightangle) { /* >> operator */
				tok_bufdiscard(); /* eat it */

				/* [>>]
				 *  \
				 *   +-- [left expr] -> [right expr] */

				ast_node_t *sav_p = pchnode;
				pchnode = new ast_node_t;
				pchnode->op = ast_node_op_t::rightshift;
				pchnode->child = sav_p;
				if (!NLEX(sav_p->next))
					return false;
			}
			else {
				break;
			}
		}
#undef NLEX
		return true;
	}

	/* [https://en.cppreference.com/w/c/language/operator_precedence] level 6 */
	/* [https://en.cppreference.com/w/cpp/language/operator_precedence] level 9 */
	bool compiler::relational_expression(ast_node_t* &pchnode) {
#define NLEX shift_expression
		if (!NLEX(pchnode))
			return false;

		while (1) {
			if (tok_bufpeek().type == token_type_t::lessthan) { /* < operator */
				tok_bufdiscard(); /* eat it */

				/* [<]
				 *  \
				 *   +-- [left expr] -> [right expr] */

				ast_node_t *sav_p = pchnode;
				pchnode = new ast_node_t;
				pchnode->op = ast_node_op_t::lessthan;
				pchnode->child = sav_p;
				if (!NLEX(sav_p->next))
					return false;
			}
			else if (tok_bufpeek().type == token_type_t::greaterthan) { /* > operator */
				tok_bufdiscard(); /* eat it */

				/* [>]
				 *  \
				 *   +-- [left expr] -> [right expr] */

				ast_node_t *sav_p = pchnode;
				pchnode = new ast_node_t;
				pchnode->op = ast_node_op_t::greaterthan;
				pchnode->child = sav_p;
				if (!NLEX(sav_p->next))
					return false;
			}
			else if (tok_bufpeek().type == token_type_t::lessthanorequal) { /* <= operator */
				tok_bufdiscard(); /* eat it */

				/* [>]
				 *  \
				 *   +-- [left expr] -> [right expr] */

				ast_node_t *sav_p = pchnode;
				pchnode = new ast_node_t;
				pchnode->op = ast_node_op_t::lessthanorequal;
				pchnode->child = sav_p;
				if (!NLEX(sav_p->next))
					return false;
			}
			else if (tok_bufpeek().type == token_type_t::greaterthanorequal) { /* >= operator */
				tok_bufdiscard(); /* eat it */

				/* [>]
				 *  \
				 *   +-- [left expr] -> [right expr] */

				ast_node_t *sav_p = pchnode;
				pchnode = new ast_node_t;
				pchnode->op = ast_node_op_t::greaterthanorequal;
				pchnode->child = sav_p;
				if (!NLEX(sav_p->next))
					return false;
			}
			else {
				break;
			}
		}
#undef NLEX
		return true;
	}

	/* [https://en.cppreference.com/w/c/language/operator_precedence] level 7 */
	/* [https://en.cppreference.com/w/cpp/language/operator_precedence] level 10 */
	bool compiler::equality_expression(ast_node_t* &pchnode) {
#define NLEX relational_expression
		if (!NLEX(pchnode))
			return false;

		while (1) {
			if (tok_bufpeek().type == token_type_t::equalequal) { /* == operator */
				tok_bufdiscard(); /* eat it */

				/* [==]
				 *  \
				 *   +-- [left expr] -> [right expr] */

				ast_node_t *sav_p = pchnode;
				pchnode = new ast_node_t;
				pchnode->op = ast_node_op_t::equals;
				pchnode->child = sav_p;
				if (!NLEX(sav_p->next))
					return false;
			}
			else if (tok_bufpeek().type == token_type_t::exclamationequal) { /* != operator */
				tok_bufdiscard(); /* eat it */

				/* [!=]
				 *  \
				 *   +-- [left expr] -> [right expr] */

				ast_node_t *sav_p = pchnode;
				pchnode = new ast_node_t;
				pchnode->op = ast_node_op_t::notequals;
				pchnode->child = sav_p;
				if (!NLEX(sav_p->next))
					return false;
			}
			else {
				break;
			}
		}
#undef NLEX
		return true;
	}

	/* [https://en.cppreference.com/w/c/language/operator_precedence] level 8 */
	/* [https://en.cppreference.com/w/cpp/language/operator_precedence] level 11 */
	bool compiler::binary_and_expression(ast_node_t* &pchnode) {
#define NLEX equality_expression
		if (!NLEX(pchnode))
			return false;

		while (tok_bufpeek().type == token_type_t::ampersand) { /* & operator */
			tok_bufdiscard(); /* eat it */

			/* [&]
			 *  \
			 *   +-- [left expr] -> [right expr] */

			ast_node_t *sav_p = pchnode;
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::binary_and;
			pchnode->child = sav_p;
			if (!NLEX(sav_p->next))
				return false;
		}
#undef NLEX
		return true;
	}

	/* [https://en.cppreference.com/w/c/language/operator_precedence] level 9 */
	/* [https://en.cppreference.com/w/cpp/language/operator_precedence] level 12 */
	bool compiler::binary_xor_expression(ast_node_t* &pchnode) {
#define NLEX binary_and_expression
		if (!NLEX(pchnode))
			return false;

		while (tok_bufpeek().type == token_type_t::caret) { /* ^ operator */
			tok_bufdiscard(); /* eat it */

			/* [^]
			 *  \
			 *   +-- [left expr] -> [right expr] */

			ast_node_t *sav_p = pchnode;
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::binary_xor;
			pchnode->child = sav_p;
			if (!NLEX(sav_p->next))
				return false;
		}
#undef NLEX
		return true;
	}

	/* [https://en.cppreference.com/w/c/language/operator_precedence] level 10 */
	/* [https://en.cppreference.com/w/cpp/language/operator_precedence] level 13 */
	bool compiler::binary_or_expression(ast_node_t* &pchnode) {
#define NLEX binary_xor_expression
		if (!NLEX(pchnode))
			return false;

		while (tok_bufpeek().type == token_type_t::pipe) { /* | operator */
			tok_bufdiscard(); /* eat it */

			/* [|]
			 *  \
			 *   +-- [left expr] -> [right expr] */

			ast_node_t *sav_p = pchnode;
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::binary_or;
			pchnode->child = sav_p;
			if (!NLEX(sav_p->next))
				return false;
		}
#undef NLEX
		return true;
	}

	/* [https://en.cppreference.com/w/c/language/operator_precedence] level 11 */
	/* [https://en.cppreference.com/w/cpp/language/operator_precedence] level 14 */
	bool compiler::logical_and_expression(ast_node_t* &pchnode) {
#define NLEX binary_or_expression
		if (!NLEX(pchnode))
			return false;

		while (tok_bufpeek().type == token_type_t::ampersandampersand) { /* && operator */
			tok_bufdiscard(); /* eat it */

			/* [&&]
			 *  \
			 *   +-- [left expr] -> [right expr] */

			ast_node_t *sav_p = pchnode;
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::logical_and;
			pchnode->child = sav_p;
			if (!NLEX(sav_p->next))
				return false;
		}
#undef NLEX
		return true;
	}

	/* [https://en.cppreference.com/w/c/language/operator_precedence] level 12 */
	/* [https://en.cppreference.com/w/cpp/language/operator_precedence] level 15 */
	bool compiler::logical_or_expression(ast_node_t* &pchnode) {
#define NLEX logical_and_expression
		if (!NLEX(pchnode))
			return false;

		while (tok_bufpeek().type == token_type_t::pipepipe) { /* || operator */
			tok_bufdiscard(); /* eat it */

			/* [||]
			 *  \
			 *   +-- [left expr] -> [right expr] */

			ast_node_t *sav_p = pchnode;
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::logical_or;
			pchnode->child = sav_p;
			if (!NLEX(sav_p->next))
				return false;
		}
#undef NLEX
		return true;
	}

	/* [https://en.cppreference.com/w/c/language/operator_precedence] level 13 */
	/* [https://en.cppreference.com/w/cpp/language/operator_precedence] level 16 */
	bool compiler::conditional_expression(ast_node_t* &pchnode) {
#define NLEX logical_or_expression
		if (!NLEX(pchnode))
			return false;

		if (tok_bufpeek().type == token_type_t::question) {
			tok_bufdiscard(); /* eat it */

			/* [?]
			 *  \
			 *   +-- [left expr] -> [middle expr] -> [right expr] */

			ast_node_t *sav_p = pchnode;
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::ternary;
			pchnode->child = sav_p;
			if (!expression(sav_p->next))
				return false;

			{
				token_t &t = tok_bufpeek();
				if (t.type == token_type_t::colon)
					tok_bufdiscard(); /* eat it */
				else
					return false;
			}

			if (!conditional_expression(sav_p->next->next))
				return false;
		}
#undef NLEX
		return true;
	}

	/* [https://en.cppreference.com/w/c/language/operator_precedence] level 14 */
	/* [https://en.cppreference.com/w/cpp/language/operator_precedence] level 16 */
	bool compiler::assignment_expression(ast_node_t* &pchnode) {
#define NLEX conditional_expression
		if (!NLEX(pchnode))
			return false;

		/* assignment expressions are parsed right to left, therefore something like a = b = c = d
		 * needs to be handled like a = (b = (c = d)) */
		if (tok_bufpeek().type == token_type_t::equal) {
			/* [=]
			 *  \
			 *   +-- [left expr] -> [right expr] */

			ast_node_t *sav_p = pchnode;
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::assign;
			pchnode->tv = std::move(tok_bufpeek(0)); tok_bufdiscard(); /* eat it */
			pchnode->child = sav_p;
			return assignment_expression(sav_p->next);
		}
		else if (tok_bufpeek().type == token_type_t::plusequal) {
			/* [+=]
			 *  \
			 *   +-- [left expr] -> [right expr] */

			ast_node_t *sav_p = pchnode;
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::assignadd;
			pchnode->tv = std::move(tok_bufpeek(0)); tok_bufdiscard(); /* eat it */
			pchnode->child = sav_p;
			return assignment_expression(sav_p->next);
		}
		else if (tok_bufpeek().type == token_type_t::minusequal) {
			/* [-=]
			 *  \
			 *   +-- [left expr] -> [right expr] */

			ast_node_t *sav_p = pchnode;
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::assignsubtract;
			pchnode->tv = std::move(tok_bufpeek(0)); tok_bufdiscard(); /* eat it */
			pchnode->child = sav_p;
			return assignment_expression(sav_p->next);
		}
		else if (tok_bufpeek().type == token_type_t::starequal) {
			/* [*=]
			 *  \
			 *   +-- [left expr] -> [right expr] */

			ast_node_t *sav_p = pchnode;
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::assignmultiply;
			pchnode->tv = std::move(tok_bufpeek(0)); tok_bufdiscard(); /* eat it */
			pchnode->child = sav_p;
			return assignment_expression(sav_p->next);
		}
		else if (tok_bufpeek().type == token_type_t::slashequal) {
			/* [/=]
			 *  \
			 *   +-- [left expr] -> [right expr] */

			ast_node_t *sav_p = pchnode;
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::assigndivide;
			pchnode->tv = std::move(tok_bufpeek(0)); tok_bufdiscard(); /* eat it */
			pchnode->child = sav_p;
			return assignment_expression(sav_p->next);
		}
		else if (tok_bufpeek().type == token_type_t::percentequal) {
			/* [%=]
			 *  \
			 *   +-- [left expr] -> [right expr] */

			ast_node_t *sav_p = pchnode;
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::assignmodulo;
			pchnode->tv = std::move(tok_bufpeek(0)); tok_bufdiscard(); /* eat it */
			pchnode->child = sav_p;
			return assignment_expression(sav_p->next);
		}
		else if (tok_bufpeek().type == token_type_t::ampersandequal) {
			/* [&=]
			 *  \
			 *   +-- [left expr] -> [right expr] */

			ast_node_t *sav_p = pchnode;
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::assignand;
			pchnode->tv = std::move(tok_bufpeek(0)); tok_bufdiscard(); /* eat it */
			pchnode->child = sav_p;
			return assignment_expression(sav_p->next);
		}
		else if (tok_bufpeek().type == token_type_t::caretequal) {
			/* [^=]
			 *  \
			 *   +-- [left expr] -> [right expr] */

			ast_node_t *sav_p = pchnode;
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::assignxor;
			pchnode->tv = std::move(tok_bufpeek(0)); tok_bufdiscard(); /* eat it */
			pchnode->child = sav_p;
			return assignment_expression(sav_p->next);
		}
		else if (tok_bufpeek().type == token_type_t::pipeequal) {
			/* [|=]
			 *  \
			 *   +-- [left expr] -> [right expr] */

			ast_node_t *sav_p = pchnode;
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::assignor;
			pchnode->tv = std::move(tok_bufpeek(0)); tok_bufdiscard(); /* eat it */
			pchnode->child = sav_p;
			return assignment_expression(sav_p->next);
		}
		else if (tok_bufpeek().type == token_type_t::leftleftangleequal) { /* <<= operator */
			/* [<<=]
			 *  \
			 *   +-- [left expr] -> [right expr] */

			ast_node_t *sav_p = pchnode;
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::assignleftshift;
			pchnode->tv = std::move(tok_bufpeek(0)); tok_bufdiscard(); /* eat it */
			pchnode->child = sav_p;
			return assignment_expression(sav_p->next);
		}
		else if (tok_bufpeek().type == token_type_t::rightrightangleequal) { /* >>= operator */
			/* [>>=]
			 *  \
			 *   +-- [left expr] -> [right expr] */

			ast_node_t *sav_p = pchnode;
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::assignrightshift;
			pchnode->tv = std::move(tok_bufpeek(0)); tok_bufdiscard(); /* eat it */
			pchnode->child = sav_p;
			return assignment_expression(sav_p->next);
		}
#undef NLEX
		return true;
	}

	/* [https://en.cppreference.com/w/c/language/operator_precedence] level 15 */
	/* [https://en.cppreference.com/w/cpp/language/operator_precedence] level 17 */
	bool compiler::expression(ast_node_t* &pchnode) {
#define NLEX assignment_expression
		if (!NLEX(pchnode))
			return false;

		while (tok_bufpeek().type == token_type_t::comma) { /* , comma operator */
			/* [,]
			 *  \
			 *   +-- [left expr] -> [right expr] */

			ast_node_t *sav_p = pchnode;
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::comma;
			pchnode->tv = std::move(tok_bufpeek(0)); tok_bufdiscard(); /* eat it */
			pchnode->child = sav_p;
			if (!NLEX(sav_p->next))
				return false;
		}
#undef NLEX
		return true;
	}

	bool compiler::fn_expression(ast_node_t* &inode,ast_node_t* &alnode,ast_node_t* &bnode,unsigned int flags) {
		if (flags & FN_EXPR_ANONYMOUS) {
			ast_node_t** inode_next = &inode;

			if (tok_bufpeek().type != token_type_t::openparen) {
				if (!type_and_identifiers_expression(inode,0,&inode_next))
					return false;
			}

			(*inode_next) = new ast_node_t;
			(*inode_next)->op = ast_node_op_t::i_anonymous;
			inode_next = &((*inode_next)->next);
		}
		else {
			if (!type_and_identifiers_expression(inode))
				return false;
		}

		if (tok_bufpeek().type == token_type_t::openparen) {
			tok_bufdiscard();

			if (tok_bufpeek().type != token_type_t::closeparen) {
				if (!fn_argument_expression_list(alnode))
					return false;
			}

			if (tok_bufpeek().type == token_type_t::closeparen) {
				tok_bufdiscard();
			}
			else {
				return false;
			}
		}

		if (flags & FN_EXPR_LET_EXPRESSION) {
			/* within let, allow fn func1(...) = { function body }
			 * any other assignment expression is left for the calling let_expression() function to handle */
			if (tok_bufpeek(0).type == token_type_t::equal && tok_bufpeek(1).type == token_type_t::opencurly) {
				tok_bufdiscard(); /* eat the equals sign, leave the curly */

				ast_node_t *n = NULL;
				if (!statement(bnode,n))
					return false;
			}
		}
		else {
			if (tok_bufpeek().type == token_type_t::opencurly) {
				ast_node_t *n = NULL;
				if (!statement(bnode,n))
					return false;
			}
		}

		/* parsing stops here if function body */
		if (bnode != NULL)
			return true;

		/* parsing stops here if let expression */
		if (flags & FN_EXPR_LET_EXPRESSION)
			return true;

		if (tok_bufpeek().type == token_type_t::semicolon) {
			/* ok */
		}
		else if (tok_bufpeek().type == token_type_t::comma) {
			/* ok */
		}
		else {
			return false;
		}
#undef NLEX
		return true;
	}

	bool compiler::fn_argument_expression(ast_node_t* &inode,ast_node_t* &enode) {
		/* "..." serves as a way to make variadic functions, just as in C, and is also the last argument in the function because any
		 * amount of anything can exist past the last parameter.
		 *
		 * "*" serves as a way to specify that past this point, parameters must be referenced by name, not position, just as in Python */
		if (tok_bufpeek().type == token_type_t::ellipsis) {
			inode = new ast_node_t;
			inode->op = ast_node_op_t::ellipsis;
			inode->tv = std::move(tok_bufpeek(0));
			tok_bufdiscard();
			return true;
		}
		else if (tok_bufpeek().type == token_type_t::star) {
			inode = new ast_node_t;
			inode->op = ast_node_op_t::named_arg_required_boundary;
			inode->tv = std::move(tok_bufpeek(0));
			tok_bufdiscard();
			return true;
		}

		return let_expression(inode,enode);
	}

	bool compiler::let_expression(ast_node_t* &inode,ast_node_t* &enode) {
		ast_node_t** inode_next = &inode;
		unsigned int tair;

		if ((tair=type_and_identifiers_expression(inode,TYPE_AND_IDENT_FL_ALLOW_FN,&inode_next)) == 0)
			return false;

		if (tair & TYPE_AND_IDENT_RT_FN) {
			/* If type and identifiers returned because it found "fn" then the ident node so far
			 * is just the return type deref down to the "fn" node, so run fn_expression() to get
			 * the real identifier and deref and parameter list. */
			ast_node_t *i=NULL,*a=NULL,*b=NULL;

			if (!fn_expression(i,a,b,FN_EXPR_LET_EXPRESSION))
				return false;
			if (i == NULL)
				return false;

			if (i) { *inode_next = i; inode_next = &((*inode_next)->next); while (*inode_next) inode_next = &((*inode_next)->next); }
			if (a) { *inode_next = a; inode_next = &((*inode_next)->next); while (*inode_next) inode_next = &((*inode_next)->next); }
			if (b) { *inode_next = b; inode_next = &((*inode_next)->next); while (*inode_next) inode_next = &((*inode_next)->next); }
		}

		/* allow [expression] if an array of the type is desired */
		if (tok_bufpeek().type == token_type_t::leftsquarebracket) {
			tok_bufdiscard(); /* eat it */

			ast_node_t *a = new ast_node_t;
			a->op = ast_node_op_t::arraysubscript;

			/* allow [] i.e. for function parameters */
			if (tok_bufpeek().type != token_type_t::rightsquarebracket) {
				if (!expression(a->child))
					return false;
			}

			{
				token_t &t = tok_bufpeek();
				if (t.type == token_type_t::rightsquarebracket)
					tok_bufdiscard(); /* eat it */
				else
					return false;
			}

			if (a) { *inode_next = a; inode_next = &((*inode_next)->next); while (*inode_next) inode_next = &((*inode_next)->next); }
		}

		if (tok_bufpeek().type == token_type_t::semicolon || tok_bufpeek().type == token_type_t::comma || tok_bufpeek().type == token_type_t::closeparen) {
			/* ok */
		}
		else if (tok_bufpeek().type == token_type_t::equal) {
			tok_bufdiscard(); /* eat it */

			enode = new ast_node_t;
			enode->op = ast_node_op_t::assign;
			if (!assignment_expression(enode->child))
				return false;
		}
		else {
			return false;
		}

		return true;
	}

	bool compiler::statement_does_not_need_semicolon(ast_node_t* apnode) {
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

	bool compiler::if_statement(ast_node_t* &apnode) {
		apnode = new ast_node_t;
		apnode->op = ast_node_op_t::r_if;
		apnode->tv = std::move(tok_bufpeek()); //assumed to be the "if" token!
		tok_bufdiscard();

		return if_statement_parse(apnode->child);
	}

	bool compiler::if_statement_parse(ast_node_t* &apnode) {
		if (tok_bufpeek().type != token_type_t::openparen)
			return false;
		tok_bufdiscard();

		if (!expression(apnode))
			return false;

		if (tok_bufpeek().type != token_type_t::closeparen)
			return false;
		tok_bufdiscard();

		ast_node_t *n = apnode->next;
		if (!statement(apnode->next,n))
			return false;
		assert(n->next == NULL);

		if (tok_bufpeek().type == token_type_t::r_else) {
			n->next = new ast_node_t;
			n->next->op = ast_node_op_t::r_else;
			n->next->tv = std::move(tok_bufpeek());
			tok_bufdiscard();
			n = n->next;

			ast_node_t *n2 = n->child;
			if (!statement(n->child,n2))
				return false;
		}

		return true;
	}

	bool compiler::statement(ast_node_t* &rnode,ast_node_t* &apnode) {
		/* [statement] -> [statement] -> ...
		 *   \                \
		 *    +-- expression   +-- expression */

		tok_buf_refill();
		if (!tok_buf_empty()) {
			/* allow empty statements (a lone ';'), that's OK */
			if (tok_bufpeek().type == token_type_t::semicolon) {
				tok_bufdiscard(); /* eat the ; */
			}
			else {
				if (apnode) {
					apnode->next = new ast_node_t;
					apnode = apnode->next;
				}
				else {
					rnode = apnode = new ast_node_t;
				}
				apnode->op = ast_node_op_t::statement;
				apnode->tv.position = tok_bufpeek(0).position;

				while (tok_bufpeek(0).type == token_type_t::identifier && tok_bufpeek(1).type == token_type_t::colon) { /* label: ... */
					apnode->op = ast_node_op_t::label;
					apnode->tv.position = tok_bufpeek(0).position;
					apnode->next = new ast_node_t;
					apnode = apnode->next;
					apnode->op = ast_node_op_t::statement;
					tok_bufdiscard(2);
				}

				if (tok_bufpeek(0).type == token_type_t::poundsign) {
					if (tok_bufpeek(1).type == token_type_t::r_if) {
						apnode->child = new ast_node_t;
						apnode->child->op = ast_node_op_t::r_pound_if;
						apnode->child->tv = std::move(tok_bufpeek(0));
						tok_bufdiscard(2);

						if (!if_statement_parse(apnode->child->child))
							return false;

						if (tok_bufpeek().type == token_type_t::semicolon)
							tok_bufdiscard();

						return true;
					}
				}

				/* allow { compound statements } in a { scope } */
				if (tok_bufpeek().type == token_type_t::opencurly) {
					tok_bufdiscard(); /* eat the { */

					/* [scope]
					 *  \
					 *   +-- [statement] -> [statement] ... */
					apnode->op = ast_node_op_t::scope;

					ast_node_t *bc = NULL;

					while (1) {
						if (tok_bufpeek().type == token_type_t::closecurly) {
							tok_bufdiscard();
							break;
						}

						if (!statement(apnode->child,bc))
							return false;
					}

					/* does not need semicolon */
				}
				else if (tok_bufpeek().type == token_type_t::r_namespace) {
					ast_node_t **n = &(apnode->child);

					assert(*n == NULL);
					(*n) = new ast_node_t;
					(*n)->op = ast_node_op_t::r_namespace;
					(*n)->tv = std::move(tok_bufpeek());
					tok_bufdiscard(); /* eat the namespace */
					n = &((*n)->child);

					if (tok_bufpeek().type == token_type_t::identifier) {
						(*n) = new ast_node_t;
						(*n)->op = ast_node_op_t::identifier;
						(*n)->tv = std::move(tok_bufpeek());
						tok_bufdiscard();
						n = &((*n)->next);
					}
					else {
						(*n) = new ast_node_t;
						(*n)->op = ast_node_op_t::i_anonymous;
						n = &((*n)->next);
					}

					if (tok_bufpeek().type == token_type_t::semicolon) {
						return true;
					}
					else if (tok_bufpeek().type == token_type_t::opencurly) {
						ast_node_t *bc = NULL;
						return statement(*n,bc);
					}
					else {
						return false;
					}
				}
				else if (tok_bufpeek().type == token_type_t::r_return) {
					assert(apnode->child == NULL);
					apnode->child = new ast_node_t;
					apnode->child->op = ast_node_op_t::r_return;
					apnode->child->tv = std::move(tok_bufpeek());
					tok_bufdiscard();

					if (tok_bufpeek().type != token_type_t::semicolon) {
						if (!expression(apnode->child->child))
							return false;
					}

					{
						token_t &t = tok_bufpeek();
						if (t.type == token_type_t::semicolon || t.type == token_type_t::eof)
							tok_bufdiscard(); /* eat the EOF or semicolon */
						else
							return false;
					}
				}
				else if (tok_bufpeek().type == token_type_t::r_break) {
					assert(apnode->child == NULL);
					apnode->child = new ast_node_t;
					apnode->child->op = ast_node_op_t::r_break;
					apnode->child->tv = std::move(tok_bufpeek());
					tok_bufdiscard();

					{
						token_t &t = tok_bufpeek();
						if (t.type == token_type_t::semicolon || t.type == token_type_t::eof)
							tok_bufdiscard(); /* eat the EOF or semicolon */
						else
							return false;
					}
				}
				else if (tok_bufpeek().type == token_type_t::r_continue) {
					assert(apnode->child == NULL);
					apnode->child = new ast_node_t;
					apnode->child->op = ast_node_op_t::r_continue;
					apnode->child->tv = std::move(tok_bufpeek());
					tok_bufdiscard();

					{
						token_t &t = tok_bufpeek();
						if (t.type == token_type_t::semicolon || t.type == token_type_t::eof)
							tok_bufdiscard(); /* eat the EOF or semicolon */
						else
							return false;
					}
				}
				else if (tok_bufpeek().type == token_type_t::r_struct || tok_bufpeek().type == token_type_t::r_union) {
					assert(apnode->child == NULL);
					apnode->child = new ast_node_t;
					if (tok_bufpeek().type == token_type_t::r_union)
						apnode->child->op = ast_node_op_t::r_union;
					else
						apnode->child->op = ast_node_op_t::r_struct;
					apnode->child->tv = std::move(tok_bufpeek());
					tok_bufdiscard();

					ast_node_t **n = &(apnode->child->child);

					{
						ast_node_t **sn = NULL;

						while (1) {
							if (is_reserved_identifier(tok_bufpeek().type) || tok_bufpeek().type == token_type_t::dblleftsquarebracket || tok_bufpeek().type == token_type_t::poundsign) {
								if (sn == NULL) {
									(*n) = new ast_node_t;
									(*n)->op = ast_node_op_t::identifier_list;
									sn = &((*n)->child);
									n = &((*n)->next);
								}

								if (!primary_expression(*sn)) return false;
								sn = &((*sn)->next);
								assert(*sn == NULL);
							}
							else {
								break;
							}
						}
					}

					if (tok_bufpeek().type == token_type_t::identifier) {
						if (!cpp_scope_expression(*n)) return false;
						n = &((*n)->next);
						assert(*n == NULL);
					}
					else {
						(*n) = new ast_node_t;
						(*n)->op = ast_node_op_t::i_anonymous;
						n = &((*n)->next);
						assert(*n == NULL);
					}

					if (tok_bufpeek().type != token_type_t::opencurly) return false;

					{
						ast_node_t *bc = NULL;
						if (!statement(*n,bc)) return false;
						n = &((*n)->next);
						assert(*n == NULL);
					}

					if (tok_bufpeek().type == token_type_t::identifier) {
						/* then the first variable */
						{
							(*n) = new ast_node_t;
							(*n)->op = ast_node_op_t::r_let;

							ast_node_t *i=NULL,*e=NULL;
							ast_node_t **sn = &((*n)->child);
							n = &((*n)->next);

							if (!let_expression(i,e))
								return false;
							if (i == NULL)
								return false;
							if (e != NULL) /* no assignment allowed */
								return false;

							if (i) { *sn = i; sn = &((*sn)->next); while (*sn) sn = &((*sn)->next); }
						}

						while (tok_bufpeek().type == token_type_t::comma) {
							(*n) = new ast_node_t;
							(*n)->op = ast_node_op_t::r_let;

							ast_node_t *i=NULL,*e=NULL;
							ast_node_t **sn = &((*n)->child);
							n = &((*n)->next);

							tok_bufdiscard(); /* eat comma */

							if (!let_expression(i,e))
								return false;
							if (i == NULL)
								return false;
							if (e != NULL) /* no assignment allowed */
								return false;

							if (i) { *sn = i; sn = &((*sn)->next); while (*sn) sn = &((*sn)->next); }
						}
					}

					{
						token_t &t = tok_bufpeek();
						if (t.type == token_type_t::semicolon || t.type == token_type_t::eof)
							tok_bufdiscard(); /* eat the EOF or semicolon */
						else
							return false;
					}
				}
				else if (tok_bufpeek().type == token_type_t::r_fn) {
					assert(apnode->child == NULL);
					apnode->child = new ast_node_t;
					apnode->child->op = ast_node_op_t::r_fn;
					apnode->child->tv = std::move(tok_bufpeek());
					tok_bufdiscard();

					ast_node_t **n = &(apnode->child->child);
					ast_node_t *i=NULL,*a=NULL,*b=NULL;

					/* first the type specification */
					{
						ast_node_t *t=NULL;

						if (!let_datatype_expression(t)) return false;
						assert(t != NULL);

						(*n) = new ast_node_t;
						(*n)->op = ast_node_op_t::i_datatype;
						(*n)->child = t;
						n = &((*n)->next);
					}

					if (!fn_expression(i,a,b))
						return false;
					if (i == NULL)
						return false;

					if (i) { *n = i; n = &((*n)->next); while (*n) n = &((*n)->next); }
					if (a) { *n = a; n = &((*n)->next); while (*n) n = &((*n)->next); }
					if (b) { *n = b; n = &((*n)->next); while (*n) n = &((*n)->next); }

					if (b != NULL && b->op == ast_node_op_t::scope) {
						/* if parsing a { scope } a semicolon is not required after the closing curly brace */
					}
					else {
						token_t &t = tok_bufpeek();
						if (t.type == token_type_t::semicolon || t.type == token_type_t::eof)
							tok_bufdiscard(); /* eat the EOF or semicolon */
						else
							return false;
					}
				}
				else if (tok_bufpeek(0).type == token_type_t::r_using && tok_bufpeek(1).type == token_type_t::r_namespace) {
					ast_node_t **n = &(apnode->child);

					assert(*n == NULL);

					(*n) = new ast_node_t;
					(*n)->op = ast_node_op_t::r_using;
					tok_bufdiscard(); /* eat using */
					n = &((*n)->child);

					(*n) = new ast_node_t;
					(*n)->op = ast_node_op_t::r_namespace;
					tok_bufdiscard(); /* eat namespace */
					n = &((*n)->next);

					if (tok_bufpeek().type == token_type_t::identifier) {
						(*n) = new ast_node_t;
						(*n)->op = ast_node_op_t::identifier;
						(*n)->tv = std::move(tok_bufpeek());
						tok_bufdiscard();
						n = &((*n)->next);
					}
					else {
						(*n) = new ast_node_t;
						(*n)->op = ast_node_op_t::i_anonymous;
						n = &((*n)->next);
					}

					if (tok_bufpeek().type == token_type_t::semicolon) {
						return true;
					}
					else {
						return false;
					}
				}
				else if (tok_bufpeek().type == token_type_t::r_typedef) {
					assert(apnode->child == NULL);
					apnode->child = new ast_node_t;
					apnode->child->op = ast_node_op_t::r_typedef;
					apnode->child->tv = std::move(tok_bufpeek());
					tok_bufdiscard();

					ast_node_t **n = &(apnode->child->child);

					/* first the type specification */
					{
						ast_node_t *t=NULL;

						if (!let_datatype_expression(t)) return false;
						assert(t != NULL);

						(*n) = new ast_node_t;
						(*n)->op = ast_node_op_t::i_datatype;
						(*n)->child = t;
						n = &((*n)->next);
					}

					/* then the first variable */
					{
						(*n) = new ast_node_t;
						(*n)->op = ast_node_op_t::r_let;

						ast_node_t *i=NULL,*e=NULL;
						ast_node_t **sn = &((*n)->child);
						n = &((*n)->next);

						if (!let_expression(i,e))
							return false;
						if (i == NULL)
							return false;
						if (e != NULL) /* no assignment allowed */
							return false;

						if (i) { *sn = i; sn = &((*sn)->next); while (*sn) sn = &((*sn)->next); }
					}

					while (tok_bufpeek().type == token_type_t::comma) {
						(*n) = new ast_node_t;
						(*n)->op = ast_node_op_t::r_let;

						ast_node_t *i=NULL,*e=NULL;
						ast_node_t **sn = &((*n)->child);
						n = &((*n)->next);

						tok_bufdiscard(); /* eat comma */

						if (!let_expression(i,e))
							return false;
						if (i == NULL)
							return false;
						if (e != NULL) /* no assignment allowed */
							return false;

						if (i) { *sn = i; sn = &((*sn)->next); while (*sn) sn = &((*sn)->next); }
					}

					{
						token_t &t = tok_bufpeek();
						if (t.type == token_type_t::semicolon || t.type == token_type_t::eof)
							tok_bufdiscard(); /* eat the EOF or semicolon */
						else
							return false;
					}
				}
				else if (tok_bufpeek().type == token_type_t::r_asm || tok_bufpeek().type == token_type_t::r__asm || tok_bufpeek().type == token_type_t::r___asm || tok_bufpeek().type == token_type_t::r___asm__) {
					assert(apnode->child == NULL);
					apnode->child = new ast_node_t;
					apnode->child->op = ast_node_op_t::r_asm;
					apnode->child->tv = std::move(tok_bufpeek());
					tok_bufdiscard();

					ast_node_t **n = &(apnode->child->child);

					while (1) {
						if (tok_bufpeek().type == token_type_t::r_volatile) {
							(*n) = new ast_node_t;
							(*n)->op = ast_node_op_t::r_volatile;
							(*n)->tv = std::move(tok_bufpeek());
							tok_bufdiscard();
							n = &((*n)->next);
						}
						else if (tok_bufpeek().type == token_type_t::dblleftsquarebracket) {
							if (!primary_expression(*n)) return false;
							n = &((*n)->next);
							assert(*n == NULL);
						}
						else {
							break;
						}
					}

					if (tok_bufpeek().type != token_type_t::openparen) return false;
					tok_bufdiscard();

					if (tok_bufpeek().type != token_type_t::closeparen) {
						if (!argument_expression_list(*n))
							return false;

						while ((*n) != NULL)
							n = &((*n)->next);
					}

					if (tok_bufpeek().type != token_type_t::closeparen) return false;
					tok_bufdiscard();

					{
						token_t &t = tok_bufpeek();
						if (t.type == token_type_t::semicolon || t.type == token_type_t::eof)
							tok_bufdiscard(); /* eat the EOF or semicolon */
						else
							return false;
					}
				}
				else if (tok_bufpeek().type == token_type_t::r_let) {
					assert(apnode->child == NULL);
					apnode->child = new ast_node_t;
					apnode->child->op = ast_node_op_t::i_compound_let;
					apnode->child->tv = std::move(tok_bufpeek());
					tok_bufdiscard();

					ast_node_t **n = &(apnode->child->child);

					/* first the type specification */
					{
						ast_node_t *t=NULL;

						if (!let_datatype_expression(t)) return false;
						assert(t != NULL);

						(*n) = new ast_node_t;
						(*n)->op = ast_node_op_t::i_datatype;
						(*n)->child = t;
						n = &((*n)->next);
					}

					/* then the first variable */
					{
						(*n) = new ast_node_t;
						(*n)->op = ast_node_op_t::r_let;

						ast_node_t *i=NULL,*e=NULL;
						ast_node_t **sn = &((*n)->child);
						n = &((*n)->next);

						if (!let_expression(i,e))
							return false;
						if (i == NULL)
							return false;

						if (i) { *sn = i; sn = &((*sn)->next); while (*sn) sn = &((*sn)->next); }
						if (e) { *sn = e; sn = &((*sn)->next); while (*sn) sn = &((*sn)->next); }
					}

					while (tok_bufpeek().type == token_type_t::comma) {
						(*n) = new ast_node_t;
						(*n)->op = ast_node_op_t::r_let;

						ast_node_t *i=NULL,*e=NULL;
						ast_node_t **sn = &((*n)->child);
						n = &((*n)->next);

						tok_bufdiscard(); /* eat comma */

						if (!let_expression(i,e))
							return false;
						if (i == NULL)
							return false;

						if (i) { *sn = i; sn = &((*sn)->next); while (*sn) sn = &((*sn)->next); }
						if (e) { *sn = e; sn = &((*sn)->next); while (*sn) sn = &((*sn)->next); }
					}

					{
						token_t &t = tok_bufpeek();
						if (t.type == token_type_t::semicolon || t.type == token_type_t::eof)
							tok_bufdiscard(); /* eat the EOF or semicolon */
						else
							return false;
					}
				}
				else if (tok_bufpeek().type == token_type_t::r_for) {
					assert(apnode->child == NULL);
					apnode->child = new ast_node_t;
					apnode->child->op = ast_node_op_t::r_for;
					apnode->child->tv = std::move(tok_bufpeek());
					tok_bufdiscard();

					ast_node_t *nn = apnode->child;

					if (tok_bufpeek().type != token_type_t::openparen)
						return false;
					tok_bufdiscard();

					if (tok_bufpeek().type == token_type_t::semicolon) {
						nn->next = new ast_node_t;
						nn->next->op = ast_node_op_t::none;
					}
					else {
						if (!expression(nn->next))
							return false;
					}
					nn = nn->next;

					if (tok_bufpeek().type != token_type_t::semicolon)
						return false;
					tok_bufdiscard();

					if (tok_bufpeek().type == token_type_t::semicolon) {
						nn->next = new ast_node_t;
						nn->next->op = ast_node_op_t::none;
					}
					else {
						if (!expression(nn->next))
							return false;
					}
					nn = nn->next;

					if (tok_bufpeek().type != token_type_t::semicolon)
						return false;
					tok_bufdiscard();

					if (tok_bufpeek().type == token_type_t::closeparen) {
						nn->next = new ast_node_t;
						nn->next->op = ast_node_op_t::none;
					}
					else {
						if (!expression(nn->next))
							return false;
					}
					nn = nn->next;

					if (tok_bufpeek().type != token_type_t::closeparen)
						return false;
					tok_bufdiscard();

					ast_node_t *n = nn->next;
					if (!statement(nn->next,n))
						return false;
				}
				else if (tok_bufpeek().type == token_type_t::r_do) {
					assert(apnode->child == NULL);
					apnode->child = new ast_node_t;
					apnode->child->op = ast_node_op_t::r_do;
					apnode->child->tv = std::move(tok_bufpeek());
					tok_bufdiscard();

					/* apparently do getch(); while (kbhit()); without the curly braces is perfectly fine.
					 * means the same thing as do { getch(); } while (kbhit()); */
					ast_node_t *n = apnode->child->child;
					if (!statement(apnode->child->child,n))
						return false;

					if (tok_bufpeek().type != token_type_t::r_while)
						return false;
					tok_bufdiscard();

					if (tok_bufpeek().type != token_type_t::openparen)
						return false;
					tok_bufdiscard();

					if (!expression(apnode->child->child->next))
						return false;

					if (tok_bufpeek().type != token_type_t::closeparen)
						return false;
					tok_bufdiscard();
				}
				else if (tok_bufpeek().type == token_type_t::r_while) {
					assert(apnode->child == NULL);
					apnode->child = new ast_node_t;
					apnode->child->op = ast_node_op_t::r_while;
					apnode->child->tv = std::move(tok_bufpeek());
					tok_bufdiscard();

					if (tok_bufpeek().type != token_type_t::openparen)
						return false;
					tok_bufdiscard();

					if (!expression(apnode->child->child))
						return false;

					if (tok_bufpeek().type != token_type_t::closeparen)
						return false;
					tok_bufdiscard();

					ast_node_t *n = apnode->child->child->next;
					if (!statement(apnode->child->child->next,n))
						return false;
				}
				else if (tok_bufpeek().type == token_type_t::r_switch) {
					assert(apnode->child == NULL);
					apnode->child = new ast_node_t;
					apnode->child->op = ast_node_op_t::r_switch;
					apnode->child->tv = std::move(tok_bufpeek());
					tok_bufdiscard();

					if (tok_bufpeek().type != token_type_t::openparen)
						return false;
					tok_bufdiscard();

					if (!expression(apnode->child->child))
						return false;

					if (tok_bufpeek().type != token_type_t::closeparen)
						return false;
					tok_bufdiscard();

					/* NTS: C apparently only requires a statement, which doesn't really make sense without { compount statement } */
					/*      We require a compound statementok_bufpeek(). What would do do with a single statement? A default case? */
					if (tok_bufpeek().type != token_type_t::opencurly)
						return false;
					tok_bufdiscard();

					/* parse statements, look for case IDENTIFIER: and make those another node.
					 * ignore statements prior to the first case based on the fact that you can
					 * put statements before any case statement but GCC will warn about ignored
					 * statements. */
					ast_node_t *nl = apnode->child->child,*casefirst = NULL,*casenode = NULL;
					while (1) {
						if (tok_bufpeek().type == token_type_t::eof)
							return false;
						if (tok_bufpeek().type == token_type_t::closecurly)
							break;

						while (1) {
							if (tok_bufpeek().type == token_type_t::r_case) { /* case conditionalexpression : statement */
								nl->next = new ast_node_t;
								nl->next->op = ast_node_op_t::r_case;
								nl->next->tv = std::move(tok_bufpeek());
								tok_bufdiscard();
								nl = nl->next;

								if (!conditional_expression(nl->child))
									return false;

								if (tok_bufpeek().type != token_type_t::colon)
									return false;
								tok_bufdiscard();

								casefirst = casenode = nl->child;
							}
							else if (tok_bufpeek().type == token_type_t::r_default) { /* default : statement */
								nl->next = new ast_node_t;
								nl->next->op = ast_node_op_t::r_case;
								nl->next->tv = std::move(tok_bufpeek());
								tok_bufdiscard();
								nl = nl->next;

								nl->child = new ast_node_t;
								nl->child->op = ast_node_op_t::r_default;

								if (tok_bufpeek().type != token_type_t::colon)
									return false;
								tok_bufdiscard();

								casefirst = casenode = nl->child;
							}
							else {
								break;
							}
						}

						if (casefirst != NULL) {
							if (!statement(casefirst,casenode))
								return false;
						}
						else {
							ast_node_t *stmt=NULL,*junk=NULL;
							if (!statement(stmt,junk))
								return false;

							stmt->free_nodes();
							delete stmt;
							stmt = NULL;
						}
					}

					if (tok_bufpeek().type != token_type_t::closecurly)
						return false;
					tok_bufdiscard();
				}
				else if (tok_bufpeek().type == token_type_t::r_if) {
					assert(apnode->child == NULL);
					if (!if_statement(apnode->child))
						return false;
				}
				else if (tok_bufpeek().type == token_type_t::r_goto) {
					assert(apnode->child == NULL);
					apnode->child = new ast_node_t;
					apnode->child->op = ast_node_op_t::r_goto;
					apnode->child->tv = std::move(tok_bufpeek());
					tok_bufdiscard();

					/* must be an identifier */
					if (tok_bufpeek().type != token_type_t::identifier)
						return false;

					apnode->child->child = new ast_node_t;
					apnode->child->child->op = ast_node_op_t::identifier;
					apnode->child->child->tv = std::move(tok_bufpeek());
					tok_bufdiscard();

					{
						token_t &t = tok_bufpeek();
						if (t.type == token_type_t::semicolon || t.type == token_type_t::eof)
							tok_bufdiscard(); /* eat the EOF or semicolon */
						else
							return false;
					}
				}
				else {
					if (!expression(apnode->child))
						return false;

					if (statement_does_not_need_semicolon(apnode->child)) {
						/* if parsing a { scope } a semicolon is not required after the closing curly brace */
					}
					else {
						token_t &t = tok_bufpeek();
						if (t.type == token_type_t::semicolon || t.type == token_type_t::eof)
							tok_bufdiscard(); /* eat the EOF or semicolon */
						else
							return false;
					}
				}
			}
		}

		return true;
	}

	void compiler::free_ast(void) {
		root_node->free_nodes();
		delete root_node;
		root_node = NULL;
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

	static void const_cvt_int(ast_node_t &r,const bool do_unsigned=false) {
		if (r.op == ast_node_op_t::constant) {
			if (r.tv.type == token_type_t::floatval) {
				const long double v = r.tv.v.floatval.val;
				r.tv.type = token_type_t::intval;
				if (do_unsigned) {
					r.tv.v.intval.initu();
					r.tv.v.intval.v.u = (unsigned long long)v;
				}
				else {
					r.tv.v.intval.init();
					r.tv.v.intval.v.v = (signed long long)v;
				}
			}
		}
	}

	static void const_cvt_float(ast_node_t &r) {
		if (r.op == ast_node_op_t::constant) {
			if (r.tv.type == token_type_t::intval) {
				if (r.tv.v.intval.flags & token_intval_t::FL_SIGNED) {
					const signed long long v = r.tv.v.intval.v.v;
					r.tv.type = token_type_t::floatval;
					r.tv.v.floatval.init();
					r.tv.v.floatval.val = (long double)v;
				}
				else {
					const unsigned long long v = r.tv.v.intval.v.u;
					r.tv.type = token_type_t::floatval;
					r.tv.v.floatval.init();
					r.tv.v.floatval.val = (long double)v;
				}
			}
		}
	}

	static void const_intval_cvt_signed(ast_node_t &r) { /* must already be intval */
		assert(r.op == ast_node_op_t::constant);
		assert(r.tv.type == token_type_t::intval);

		if (!(r.tv.v.intval.flags & token_intval_t::FL_SIGNED)) {
			r.tv.v.intval.flags |= token_intval_t::FL_SIGNED;
		}
	}

	static void const_intval_cvt_unsigned(ast_node_t &r) { /* must already be intval */
		assert(r.op == ast_node_op_t::constant);
		assert(r.tv.type == token_type_t::intval);

		if (r.tv.v.intval.flags & token_intval_t::FL_SIGNED) {
			r.tv.v.intval.flags &= ~token_intval_t::FL_SIGNED;
		}
	}

	static void const_intval_cvt_from_boolean(ast_node_t &r) { /* must already be intval */
		assert(r.op == ast_node_op_t::constant);
		assert(r.tv.type == token_type_t::intval);

		if (r.tv.v.intval.itype == token_intval_t::T_BOOL)
			r.tv.v.intval.itype = token_intval_t::T_UNSPEC;
	}

	static void const_cvt_both_prep(ast_node_t &a,ast_node_t &b) {
		assert(a.op == ast_node_op_t::constant && b.op == ast_node_op_t::constant);

		/* if either value is float, convert to float */
		if (a.tv.type == token_type_t::floatval || b.tv.type == token_type_t::floatval) {
			const_cvt_float(a);
			const_cvt_float(b);
		}

		if (a.tv.type == token_type_t::intval && b.tv.type == token_type_t::intval) {
			/* if either is not boolean, then neither is boolean */
			if (!(a.tv.v.intval.itype == token_intval_t::T_BOOL && b.tv.v.intval.itype == token_intval_t::T_BOOL)) {
				const_intval_cvt_from_boolean(a);
				const_intval_cvt_from_boolean(b);
			}

			/* if either is signed, convert to signed */
			if ((a.tv.v.intval.flags & token_intval_t::FL_SIGNED) || (b.tv.v.intval.flags & token_intval_t::FL_SIGNED)) {
				const_intval_cvt_signed(a);
				const_intval_cvt_signed(b);
			}

			assert((a.tv.v.intval.flags&token_intval_t::FL_SIGNED) == (b.tv.v.intval.flags&token_intval_t::FL_SIGNED));
		}

		assert(a.tv.type == b.tv.type);
	}

	//////////////

	/* for integer types only */
	template <typename T> static bool reduce_make_boolean_intval(const T a) {
		return a != T(0);
	}

	/* for float types only */
	template <typename T> static bool reduce_make_boolean_floatval(const T a) {
		return a != T(0);
	}

	//////////////

	/* for integer types only */
	template <typename T> static bool reduce_lognot_intval(bool &r,const T a) {
		r = !reduce_make_boolean_intval(a);
		return true;
	}

	/* for float types only */
	template <typename T> static bool reduce_lognot_floatval(bool &r,const T a) {
		r = !reduce_make_boolean_floatval(a);
		return true;
	}

	//////////////

	/* for integer types only */
	template <typename T> static bool reduce_shl_rangecheck(const T b) {
		const size_t max_bits = sizeof(token_intval_t::v.u) * size_t(8u) - size_t(std::is_signed<T>::value ? 1 : 0);
		return (typename std::make_unsigned<T>::type)(b) <= max_bits;
	}

	//////////////

	/* for integer types only */
	template <typename T> static bool reduce_shr_rangecheck(const T b) {
		const size_t max_bits = sizeof(token_intval_t::v.u) * size_t(8u) - size_t(std::is_signed<T>::value ? 1 : 0);
		return (typename std::make_unsigned<T>::type)(b) <= max_bits;
	}

	//////////////

	/* for integer types only */
	template <typename T> static bool reduce_shl_intval(T &r,const T a,const T b) {
		r = a << b;
		return true;
	}

	//////////////

	/* for integer types only */
	template <typename T> static bool reduce_shr_intval(T &r,const T a,const T b) {
		/* expected for signed int: sign bit used to fill MSB for shift so that
		 * -4 (0xFFFC) >> 1 == -2 (0xFFFE)
		 * signed behavior should not follow unsigned behavior
		 * -4 (0xFFFC) >> 1 == 32766 (0x7FFE) */
		r = a >> b;
		return true;
	}

	//////////////

	/* for integer types only */
	template <typename T> static bool reduce_binand_intval(T &r,const T a,const T b) {
		r = a & b;
		return true;
	}

	//////////////

	/* for integer types only */
	template <typename T> static bool reduce_binxor_intval(T &r,const T a,const T b) {
		r = a ^ b;
		return true;
	}

	//////////////

	/* for integer types only */
	template <typename T> static bool reduce_binor_intval(T &r,const T a,const T b) {
		r = a | b;
		return true;
	}

	//////////////

	/* for integer types only */
	template <typename T> static bool reduce_logor_intval(bool &r,const T a,const T b) {
		r = reduce_make_boolean_intval(a) || reduce_make_boolean_intval(b);
		return true;
	}

	/* for float types only */
	template <typename T> static bool reduce_logor_floatval(bool &r,const T a,const T b) {
		r = reduce_make_boolean_intval(a) || reduce_make_boolean_intval(b);
		return true;
	}

	//////////////

	/* for integer types only */
	template <typename T> static bool reduce_gt_intval(bool &r,const T a,const T b) {
		r = a > b;
		return true;
	}

	/* for float types only */
	template <typename T> static bool reduce_gt_floatval(bool &r,const T a,const T b) {
		r = a > b;
		return true;
	}

	//////////////

	/* for integer types only */
	template <typename T> static bool reduce_gte_intval(bool &r,const T a,const T b) {
		r = a >= b;
		return true;
	}

	/* for float types only */
	template <typename T> static bool reduce_gte_floatval(bool &r,const T a,const T b) {
		r = a >= b;
		return true;
	}

	//////////////

	/* for integer types only */
	template <typename T> static bool reduce_lt_intval(bool &r,const T a,const T b) {
		r = a < b;
		return true;
	}

	/* for float types only */
	template <typename T> static bool reduce_lt_floatval(bool &r,const T a,const T b) {
		r = a < b;
		return true;
	}

	//////////////

	/* for integer types only */
	template <typename T> static bool reduce_lte_intval(bool &r,const T a,const T b) {
		r = a <= b;
		return true;
	}

	/* for float types only */
	template <typename T> static bool reduce_lte_floatval(bool &r,const T a,const T b) {
		r = a <= b;
		return true;
	}

	//////////////

	/* for integer types only */
	template <typename T> static bool reduce_equ_intval(bool &r,const T a,const T b) {
		r = a == b;
		return true;
	}

	/* for float types only */
	template <typename T> static bool reduce_equ_floatval(bool &r,const T a,const T b) {
		r = a == b;
		return true;
	}

	//////////////

	/* for integer types only */
	template <typename T> static bool reduce_nequ_intval(bool &r,const T a,const T b) {
		r = a != b;
		return true;
	}

	/* for float types only */
	template <typename T> static bool reduce_nequ_floatval(bool &r,const T a,const T b) {
		r = a != b;
		return true;
	}

	//////////////

	/* for integer types only */
	template <typename T> static bool reduce_logand_intval(bool &r,const T a,const T b) {
		r = reduce_make_boolean_intval(a) && reduce_make_boolean_intval(b);
		return true;
	}

	/* for float types only */
	template <typename T> static bool reduce_logand_floatval(bool &r,const T a,const T b) {
		r = reduce_make_boolean_intval(a) && reduce_make_boolean_intval(b);
		return true;
	}

	//////////////

	/* for integer types only */
	template <typename T> static bool reduce_binnot_intval(T &r,const T a) {
		r = ~a;
		return true;
	}

	//////////////

	/* for integer types only */
	template <typename T> static bool reduce_neg_intval(T &r,const T a) {
		r = -a;
		return true;
	}

	/* for float types only */
	template <typename T> static bool reduce_neg_floatval(T &r,const T a) {
		r = -a;
		return true;
	}

	//////////////

	/* for integer types only */
	template <typename T> static bool reduce_div_intval(T &r,const T a,const T b) {
		if (b == T(0)) return false;
		r = a / b;
		return true;
	}

	/* for float types only */
	template <typename T> static bool reduce_div_floatval(T &r,const T a,const T b) {
		if (b == T(0)) return false;
		r = a / b;
		return true;
	}

	//////////////

	/* for integer types only */
	template <typename T> static bool reduce_mod_intval(T &r,const T a,const T b) {
		if (b == T(0)) return false;
		r = a % b;
		return true;
	}

	//////////////

	/* for integer types only */
	template <typename T> static bool reduce_sub_intval(T &r,const T a,const T b) {
		r = a - b;
		return true;
	}

	/* for float types only */
	template <typename T> static bool reduce_sub_floatval(T &r,const T a,const T b) {
		r = a - b;
		return true;
	}

	//////////////

	/* for integer types only */
	template <typename T> static bool reduce_add_intval(T &r,const T a,const T b) {
		r = a + b;
		return true;
	}

	/* for float types only */
	template <typename T> static bool reduce_add_floatval(T &r,const T a,const T b) {
		r = a + b;
		return true;
	}

	//////////////

	/* for integer types only */
	template <typename T> static bool reduce_mul_intval(T &r,const T a,const T b) {
		r = a * b;
		return true;
	}

	/* for float types only */
	template <typename T> static bool reduce_mul_floatval(T &r,const T a,const T b) {
		r = a * b;
		return true;
	}

	//////////////

	static void const_cvt_bool(ast_node_t &r) {
		if (r.op == ast_node_op_t::constant) {
			if (r.tv.type == token_type_t::floatval) {
				const long double v = r.tv.v.floatval.val;
				r.tv.type = token_type_t::intval;
				r.tv.v.intval.init();
				r.tv.v.intval.itype = token_intval_t::T_BOOL;
				r.tv.v.intval.v.v = reduce_make_boolean_intval(v);
			}
			else if (r.tv.type == token_type_t::intval) {
				const signed long v = r.tv.v.intval.v.v;
				r.tv.v.intval.itype = token_intval_t::T_BOOL;
				r.tv.v.intval.v.v = reduce_make_boolean_intval(v);
			}
		}
	}

	/* [r]
	 *  \- [a]
	 *
	 * become
	 *
	 * [a] */
	static void reduce_move_up_replace_single(ast_node_t* &r,ast_node_t* &a) {
		assert(r != NULL);
		assert(a != NULL);
		assert(a->child == NULL);
		assert(a->next == NULL);
		assert(r->child == a);

		r->op = a->op;
		r->tv = std::move(a->tv);
		r->free_children(); // invalidates a
		a = NULL;
	}

	static void reduce_move_b_to_a(ast_node_t* &r,ast_node_t* &a,ast_node_t* &b) {
		assert(r->child == a);
		assert(a->next == b);
		r->child = b;
		a->next = NULL;
		a->free_nodes();
		delete a;
		a = b;
		b = NULL;
	}

	/* [r]
	 *  \- [a]
	 *
	 * become
	 *
	 * [a]
	 *
	 * [a] may have children */
	static void reduce_move_up_replace_single_with_child(ast_node_t* &r,ast_node_t* &a) {
		assert(r != NULL);
		assert(a != NULL);
		assert(a->next == NULL);
		assert(r->child == a);

		r->op = a->op;
		r->tv = std::move(a->tv);
		r->child = a->child; /* r->child == a until this line */
		a->child = NULL;

		a->free_nodes();
		delete a;
		a = NULL;
	}

	/* [r]
	 *  \- [a] -> [b]
	 *
	 * become
	 *
	 * [v ? a : b]
	 *
	 * [a] and [b] may have children */
	static void reduce_move_up_replace_single_ternary(ast_node_t* &r,ast_node_t* &a,ast_node_t* &t,ast_node_t* &f,const bool v) {
		assert(r != NULL);
		assert(t != NULL);
		assert(f != NULL);
		assert(r->child == a); r->child = NULL;
		assert(a->next == t); a->next = NULL;
		assert(t->next == f); t->next = NULL;
		assert(f->next == NULL);

		ast_node_t* const chose = v?t:f;

		r->op = chose->op;
		r->tv = std::move(chose->tv);
		r->child = chose->child;
		chose->child = NULL;

		a->free_nodes(); delete a; a = NULL;
		t->free_nodes(); delete t; t = NULL;
		f->free_nodes(); delete f; f = NULL;
	}

	/* [r]
	 *  \- [a] -> [b]
	 *
	 * become
	 *
	 * [a] */
	static void reduce_move_up_replace_single(ast_node_t* &r,ast_node_t* &a,ast_node_t* &b) {
		assert(r != NULL);
		assert(a != NULL);
		assert(b != NULL);
		assert(a->child == NULL);
		assert(a->next == b);
		assert(b->child == NULL);
		assert(b->next == NULL);
		assert(r->child == a);

		r->op = a->op;
		r->tv = std::move(a->tv);
		r->free_children(); // invalidates a and b
		a = NULL;
		b = NULL;
	}

	static void reduce_check_op(ast_node_t* r,const ast_node_op_t sop) {
		assert(r != NULL);
		assert(r->op == sop);
	}

	static constexpr unsigned int RGP_ALLOW_CHILD = (1u << 0u);
	static constexpr unsigned int RGP_ALLOW_NEXT = (1u << 1u);
	static constexpr unsigned int RGP_OPTIONAL_PARAMS = (1u << 2u);

	/* [c]
	 *  \- [a] ...
	 *
	 * n = [a] */
	static bool reduce_get_param_common(ast_node_t* &n,ast_node_t* &c,const unsigned int fl=0) {
		if (!c) {
			if (fl & RGP_OPTIONAL_PARAMS) { n = NULL; return true; }
			else return false;
		}

		n = c;
		if (n->child && !(fl & RGP_ALLOW_CHILD)) return false;
		if (n->next && !(fl & RGP_ALLOW_NEXT)) return false;

		return true;
	}

	/* [r]
	 *  \- [a]
	 *
	 * a = [a] */
	static bool reduce_get_one_param(ast_node_t* &r,ast_node_t* &a,const unsigned int fl=0) {
		if (!reduce_get_param_common(/*fill in*/a,/*from*/r->child,fl)) return false;
		return true;
	}

	/* [r]
	 *  \- [a] -> [b]
	 *
	 * a = [a]
	 * b = [b] */
	static bool reduce_get_two_params(ast_node_t* &r,ast_node_t* &a,ast_node_t* &b,const unsigned int fl=0) {
		if (!reduce_get_param_common(/*fill in*/a,/*from*/r->child,fl | RGP_ALLOW_NEXT)) return false;
		if (!reduce_get_param_common(/*fill in*/b,/*from*/a->next,fl)) return false;
		return true;
	}

	/* [r]
	 *  \- [a] -> [b] -> [c]
	 *
	 * a = [a]
	 * b = [b]
	 * c = [c] */
	static bool reduce_get_three_params(ast_node_t* &r,ast_node_t* &a,ast_node_t* &b,ast_node_t* &c,const unsigned int fl=0) {
		if (!reduce_get_param_common(/*fill in*/a,/*from*/r->child,fl | RGP_ALLOW_NEXT)) return false;
		if (!reduce_get_param_common(/*fill in*/b,/*from*/a->next,fl | RGP_ALLOW_NEXT)) return false;
		if (!reduce_get_param_common(/*fill in*/c,/*from*/b->next,fl)) return false;
		return true;
	}

	static bool reduce_subexpr(ast_node_t* &r) { /* ast_node_op_t::subexpression */
		/* [subexpression]
		 *   \- [a]
		 *
		 * become
		 *
		 * [a]
		 *
		 * but if there are more nodes
		 *
		 * [subexpression]
		 *   \- [a] -> [b] -> [c] 
		 *
		 * do not move up
		 *
		 * children in [a] are permitted */
		reduce_check_op(r,ast_node_op_t::subexpression);

		ast_node_t *a=NULL;
		if (!reduce_get_one_param(r,a,RGP_ALLOW_CHILD)) return true;
		reduce_move_up_replace_single_with_child(r,a);

		return true;
	}

	static bool reduce_binnot(ast_node_t* &r) { /* ast_node_op_t::binarynot */
		/* [binarynot]
		 *   \- [a]
		 *
		 * become
		 *
		 * [~a] */
		reduce_check_op(r,ast_node_op_t::binarynot);

		ast_node_t *a=NULL;
		if (!reduce_get_one_param(r,a)) return true;

		if (a->op == ast_node_op_t::constant) {
			if (a->tv.type == token_type_t::intval) {
				const_intval_cvt_from_boolean(*a); /* convert boolean to int */

				if (a->tv.v.intval.flags & token_intval_t::FL_SIGNED) {
					signed long long result;
					if (!reduce_binnot_intval(result,a->tv.v.intval.v.v)) return false;
					reduce_move_up_replace_single(r,a);
					r->tv.v.intval.v.v = result;
				}
				else {
					unsigned long long result;
					if (!reduce_binnot_intval(result,a->tv.v.intval.v.u)) return false;
					reduce_move_up_replace_single(r,a);
					r->tv.v.intval.v.u = result;
				}
			}
			else if (a->tv.type == token_type_t::floatval) {
				/* you can't do that to a floating point value! */
				return false;
			}
		}

		return true;
	}

	static bool reduce_lognot(ast_node_t* &r) { /* ast_node_op_t::logicalnot */
		/* [binarynot]
		 *   \- [a]
		 *
		 * become
		 *
		 * [!a] */
		reduce_check_op(r,ast_node_op_t::logicalnot);

		ast_node_t *a=NULL;
		if (!reduce_get_one_param(r,a)) return true;

		if (a->op == ast_node_op_t::constant) {
			if (a->tv.type == token_type_t::intval) {
				if (a->tv.v.intval.flags & token_intval_t::FL_SIGNED) {
					bool result;
					if (!reduce_lognot_intval(result,a->tv.v.intval.v.v)) return false;
					a->tv.v.intval.flags &= ~token_intval_t::FL_SIGNED;
					reduce_move_up_replace_single(r,a);
					r->tv.v.intval.initbool(result);
				}
				else {
					bool result;
					if (!reduce_lognot_intval(result,a->tv.v.intval.v.u)) return false;
					reduce_move_up_replace_single(r,a);
					r->tv.v.intval.initbool(result);
				}
			}
			else if (a->tv.type == token_type_t::floatval) {
				bool result;
				if (!reduce_lognot_floatval(result,a->tv.v.floatval.val)) return false;
				reduce_move_up_replace_single(r,a);
				r->tv.type = token_type_t::intval;
				r->tv.v.intval.initbool(result);
			}
		}

		return true;
	}

	static bool reduce_uplus(ast_node_t* &r) { /* ast_node_op_t::unaryplus */
		/* [unaryplus]
		 *   \- [a]
		 *
		 * become
		 *
		 * [a] */
		reduce_check_op(r,ast_node_op_t::unaryplus);

		ast_node_t *a=NULL;
		if (!reduce_get_one_param(r,a)) return true;
		if (a->tv.type == token_type_t::intval) const_intval_cvt_from_boolean(*a); /* convert boolean to int */
		reduce_move_up_replace_single(r,a);

		return true;
	}

	static bool reduce_neg(ast_node_t* &r) { /* ast_node_op_t::negate */
		/* [negate]
		 *   \- [a]
		 *
		 * become
		 *
		 * [-a] */
		reduce_check_op(r,ast_node_op_t::negate);

		ast_node_t *a=NULL;
		if (!reduce_get_one_param(r,a)) return true;

		if (a->op == ast_node_op_t::constant) {
			if (a->tv.type == token_type_t::intval) {
				const_intval_cvt_from_boolean(*a); /* convert boolean to int */
				const_intval_cvt_signed(*a); /* result is always signed */

				{
					signed long long result;
					if (!reduce_neg_intval(result,a->tv.v.intval.v.v)) return false;
					reduce_move_up_replace_single(r,a);
					r->tv.v.intval.v.v = result;
				}
			}
			else if (a->tv.type == token_type_t::floatval) {
				long double result;
				if (!reduce_neg_floatval(result,a->tv.v.floatval.val)) return false;
				reduce_move_up_replace_single(r,a);
				r->tv.v.floatval.val = result;
			}
		}

		return true;
	}

	static bool reduce_comma(ast_node_t* &r) { /* ast_node_op_t::comma */
		/* [comma]
		 *   \- [a] -> [b]
		 *
		 * become
		 *
		 * [b] */
		reduce_check_op(r,ast_node_op_t::comma);

		ast_node_t *a=NULL,*b=NULL;
		if (!reduce_get_two_params(r,a,b)) return true;

		if (a->op == ast_node_op_t::constant && b->op == ast_node_op_t::constant) {
			reduce_move_b_to_a(r,a,b);
			reduce_move_up_replace_single(r,a);
		}

		return true;
	}

	static bool reduce_ternary(ast_node_t* &r) { /* ast_node_op_t::ternary */
		/* [comma]
		 *   \- [a] -> [b] -> [c]
		 *
		 * become
		 *
		 * [a ? b : c] */
		reduce_check_op(r,ast_node_op_t::ternary);

		ast_node_t *a=NULL,*b=NULL,*c=NULL;
		if (!reduce_get_three_params(r,a,b,c,RGP_ALLOW_CHILD)) return true;

		if (a->op == ast_node_op_t::constant) {
			bool result;

			if (a->tv.type == token_type_t::intval) {
				if (a->tv.v.intval.flags & token_intval_t::FL_SIGNED)
					result = reduce_make_boolean_intval(a->tv.v.intval.v.v);
				else
					result = reduce_make_boolean_intval(a->tv.v.intval.v.u);
			}
			else if (a->tv.type == token_type_t::floatval) {
				result = reduce_make_boolean_floatval(a->tv.v.floatval.val);
			}
			else {
				return true;
			}

			reduce_move_up_replace_single_ternary(r,a,b,c,result);
		}

		return true;
	}

	static bool reduce_shl(ast_node_t* &r) { /* ast_node_op_t::leftshift */
		/* [leftshift]
		 *   \- [a] -> [b]
		 *
		 * become
		 *
		 * [a << b] */
		reduce_check_op(r,ast_node_op_t::leftshift);

		ast_node_t *a=NULL,*b=NULL;
		if (!reduce_get_two_params(r,a,b)) return true;

		if (a->op == ast_node_op_t::constant && b->op == ast_node_op_t::constant) {
			const_cvt_both_prep(*a,*b);

			if (a->tv.type == b->tv.type) {
				if (a->tv.type == token_type_t::intval) {
					const_intval_cvt_from_boolean(*a); /* convert boolean to int */
					const_intval_cvt_from_boolean(*b); /* convert boolean to int */
					if (a->tv.v.intval.flags & token_intval_t::FL_SIGNED) {
						signed long long result;
						if (!reduce_shl_rangecheck(b->tv.v.intval.v.v)) return true;
						if (!reduce_shl_intval(result,a->tv.v.intval.v.v,b->tv.v.intval.v.v)) return false;
						reduce_move_up_replace_single(r,a,b);
						r->tv.v.intval.v.v = result;
					}
					else {
						unsigned long long result;
						if (!reduce_shl_rangecheck(b->tv.v.intval.v.v)) return true;
						if (!reduce_shl_intval(result,a->tv.v.intval.v.u,b->tv.v.intval.v.u)) return false;
						reduce_move_up_replace_single(r,a,b);
						r->tv.v.intval.v.u = result;
					}
				}
				else if (a->tv.type == token_type_t::floatval) {
					/* you can't do that to a floating point value! */
					return false;
				}
			}
		}

		return true;
	}

	static bool reduce_shr(ast_node_t* &r) { /* ast_node_op_t::rightshift */
		/* [rightshift]
		 *   \- [a] -> [b]
		 *
		 * become
		 *
		 * [a >> b] */
		reduce_check_op(r,ast_node_op_t::rightshift);

		ast_node_t *a=NULL,*b=NULL;
		if (!reduce_get_two_params(r,a,b)) return true;

		if (a->op == ast_node_op_t::constant && b->op == ast_node_op_t::constant) {
			const_cvt_both_prep(*a,*b);

			if (a->tv.type == b->tv.type) {
				if (a->tv.type == token_type_t::intval) {
					const_intval_cvt_from_boolean(*a); /* convert boolean to int */
					const_intval_cvt_from_boolean(*b); /* convert boolean to int */
					if (a->tv.v.intval.flags & token_intval_t::FL_SIGNED) {
						signed long long result;
						if (!reduce_shr_rangecheck(b->tv.v.intval.v.v)) return true;
						if (!reduce_shr_intval(result,a->tv.v.intval.v.v,b->tv.v.intval.v.v)) return false;
						reduce_move_up_replace_single(r,a,b);
						r->tv.v.intval.v.v = result;
					}
					else {
						unsigned long long result;
						if (!reduce_shr_rangecheck(b->tv.v.intval.v.v)) return true;
						if (!reduce_shr_intval(result,a->tv.v.intval.v.u,b->tv.v.intval.v.u)) return false;
						reduce_move_up_replace_single(r,a,b);
						r->tv.v.intval.v.u = result;
					}
				}
				else if (a->tv.type == token_type_t::floatval) {
					/* you can't do that to a floating point value! */
					return false;
				}
			}
		}

		return true;
	}

	static bool reduce_binand(ast_node_t* &r) { /* ast_node_op_t::binary_and */
		/* [binary_and]
		 *   \- [a] -> [b]
		 *
		 * become
		 *
		 * [a & b] */
		reduce_check_op(r,ast_node_op_t::binary_and);

		ast_node_t *a=NULL,*b=NULL;
		if (!reduce_get_two_params(r,a,b)) return true;

		if (a->op == ast_node_op_t::constant && b->op == ast_node_op_t::constant) {
			const_cvt_both_prep(*a,*b);

			if (a->tv.type == b->tv.type) {
				if (a->tv.type == token_type_t::intval) {
					const_intval_cvt_from_boolean(*a); /* convert boolean to int */
					const_intval_cvt_from_boolean(*b); /* convert boolean to int */
					if (a->tv.v.intval.flags & token_intval_t::FL_SIGNED) {
						signed long long result;
						if (!reduce_binand_intval(result,a->tv.v.intval.v.v,b->tv.v.intval.v.v)) return false;
						reduce_move_up_replace_single(r,a,b);
						r->tv.v.intval.v.v = result;
					}
					else {
						unsigned long long result;
						if (!reduce_binand_intval(result,a->tv.v.intval.v.u,b->tv.v.intval.v.u)) return false;
						reduce_move_up_replace_single(r,a,b);
						r->tv.v.intval.v.u = result;
					}
				}
				else if (a->tv.type == token_type_t::floatval) {
					/* you can't do that to a floating point value! */
					return false;
				}
			}
		}

		return true;
	}

	static bool reduce_binxor(ast_node_t* &r) { /* ast_node_op_t::binary_xor */
		/* [binary_xor]
		 *   \- [a] -> [b]
		 *
		 * become
		 *
		 * [a ^ b] */
		reduce_check_op(r,ast_node_op_t::binary_xor);

		ast_node_t *a=NULL,*b=NULL;
		if (!reduce_get_two_params(r,a,b)) return true;

		if (a->op == ast_node_op_t::constant && b->op == ast_node_op_t::constant) {
			const_cvt_both_prep(*a,*b);

			if (a->tv.type == b->tv.type) {
				if (a->tv.type == token_type_t::intval) {
					const_intval_cvt_from_boolean(*a); /* convert boolean to int */
					const_intval_cvt_from_boolean(*b); /* convert boolean to int */
					if (a->tv.v.intval.flags & token_intval_t::FL_SIGNED) {
						signed long long result;
						if (!reduce_binxor_intval(result,a->tv.v.intval.v.v,b->tv.v.intval.v.v)) return false;
						reduce_move_up_replace_single(r,a,b);
						r->tv.v.intval.v.v = result;
					}
					else {
						unsigned long long result;
						if (!reduce_binxor_intval(result,a->tv.v.intval.v.u,b->tv.v.intval.v.u)) return false;
						reduce_move_up_replace_single(r,a,b);
						r->tv.v.intval.v.u = result;
					}
				}
				else if (a->tv.type == token_type_t::floatval) {
					/* you can't do that to a floating point value! */
					return false;
				}
			}
		}

		return true;
	}

	static bool reduce_binor(ast_node_t* &r) { /* ast_node_op_t::binary_or */
		/* [binary_or]
		 *   \- [a] -> [b]
		 *
		 * become
		 *
		 * [a | b] */
		reduce_check_op(r,ast_node_op_t::binary_or);

		ast_node_t *a=NULL,*b=NULL;
		if (!reduce_get_two_params(r,a,b)) return true;

		if (a->op == ast_node_op_t::constant && b->op == ast_node_op_t::constant) {
			const_cvt_both_prep(*a,*b);

			if (a->tv.type == b->tv.type) {
				if (a->tv.type == token_type_t::intval) {
					const_intval_cvt_from_boolean(*a); /* convert boolean to int */
					const_intval_cvt_from_boolean(*b); /* convert boolean to int */
					if (a->tv.v.intval.flags & token_intval_t::FL_SIGNED) {
						signed long long result;
						if (!reduce_binor_intval(result,a->tv.v.intval.v.v,b->tv.v.intval.v.v)) return false;
						reduce_move_up_replace_single(r,a,b);
						r->tv.v.intval.v.v = result;
					}
					else {
						unsigned long long result;
						if (!reduce_binor_intval(result,a->tv.v.intval.v.u,b->tv.v.intval.v.u)) return false;
						reduce_move_up_replace_single(r,a,b);
						r->tv.v.intval.v.u = result;
					}
				}
				else if (a->tv.type == token_type_t::floatval) {
					/* you can't do that to a floating point value! */
					return false;
				}
			}
		}

		return true;
	}

	static bool reduce_logor(ast_node_t* &r) { /* ast_node_op_t::logical_or */
		/* [logical_or]
		 *   \- [a] -> [b]
		 *
		 * become
		 *
		 * [a || b] */
		reduce_check_op(r,ast_node_op_t::logical_or);

		ast_node_t *a=NULL,*b=NULL;
		if (!reduce_get_two_params(r,a,b)) return true;

		if (a->op == ast_node_op_t::constant && b->op == ast_node_op_t::constant) {
			const_cvt_both_prep(*a,*b);

			if (a->tv.type == b->tv.type) {
				if (a->tv.type == token_type_t::intval) {
					if (a->tv.v.intval.flags & token_intval_t::FL_SIGNED) {
						bool result;
						if (!reduce_logor_intval(result,a->tv.v.intval.v.v,b->tv.v.intval.v.v)) return false;
						a->tv.v.intval.flags &= ~token_intval_t::FL_SIGNED;
						reduce_move_up_replace_single(r,a,b);
						r->tv.v.intval.initbool(result);
					}
					else {
						bool result;
						if (!reduce_logor_intval(result,a->tv.v.intval.v.u,b->tv.v.intval.v.u)) return false;
						reduce_move_up_replace_single(r,a,b);
						r->tv.v.intval.initbool(result);
					}
				}
				else if (a->tv.type == token_type_t::floatval) {
					bool result;
					if (!reduce_logor_floatval(result,a->tv.v.floatval.val,b->tv.v.floatval.val)) return false;
					reduce_move_up_replace_single(r,a,b);
					r->tv.type = token_type_t::intval;
					r->tv.v.intval.initbool(result);
				}
			}
		}

		return true;
	}

	static bool reduce_gt(ast_node_t* &r) { /* ast_node_op_t::greaterthan */
		/* [greaterthan]
		 *   \- [a] -> [b]
		 *
		 * become
		 *
		 * [a > b] */
		reduce_check_op(r,ast_node_op_t::greaterthan);

		ast_node_t *a=NULL,*b=NULL;
		if (!reduce_get_two_params(r,a,b)) return true;

		if (a->op == ast_node_op_t::constant && b->op == ast_node_op_t::constant) {
			const_cvt_both_prep(*a,*b);

			if (a->tv.type == b->tv.type) {
				if (a->tv.type == token_type_t::intval) {
					if (a->tv.v.intval.flags & token_intval_t::FL_SIGNED) {
						bool result;
						if (!reduce_gt_intval(result,a->tv.v.intval.v.v,b->tv.v.intval.v.v)) return false;
						a->tv.v.intval.flags &= ~token_intval_t::FL_SIGNED;
						reduce_move_up_replace_single(r,a,b);
						r->tv.v.intval.initbool(result);
					}
					else {
						bool result;
						if (!reduce_gt_intval(result,a->tv.v.intval.v.u,b->tv.v.intval.v.u)) return false;
						reduce_move_up_replace_single(r,a,b);
						r->tv.v.intval.initbool(result);
					}
				}
				else if (a->tv.type == token_type_t::floatval) {
					bool result;
					if (!reduce_gt_floatval(result,a->tv.v.floatval.val,b->tv.v.floatval.val)) return false;
					reduce_move_up_replace_single(r,a,b);
					r->tv.type = token_type_t::intval;
					r->tv.v.intval.initbool(result);
				}
			}
		}

		return true;
	}

	static bool reduce_gte(ast_node_t* &r) { /* ast_node_op_t::greaterthanorequal */
		/* [greaterthanorequal]
		 *   \- [a] -> [b]
		 *
		 * become
		 *
		 * [a >= b] */
		reduce_check_op(r,ast_node_op_t::greaterthanorequal);

		ast_node_t *a=NULL,*b=NULL;
		if (!reduce_get_two_params(r,a,b)) return true;

		if (a->op == ast_node_op_t::constant && b->op == ast_node_op_t::constant) {
			const_cvt_both_prep(*a,*b);

			if (a->tv.type == b->tv.type) {
				if (a->tv.type == token_type_t::intval) {
					if (a->tv.v.intval.flags & token_intval_t::FL_SIGNED) {
						bool result;
						if (!reduce_gte_intval(result,a->tv.v.intval.v.v,b->tv.v.intval.v.v)) return false;
						a->tv.v.intval.flags &= ~token_intval_t::FL_SIGNED;
						reduce_move_up_replace_single(r,a,b);
						r->tv.v.intval.initbool(result);
					}
					else {
						bool result;
						if (!reduce_gte_intval(result,a->tv.v.intval.v.u,b->tv.v.intval.v.u)) return false;
						reduce_move_up_replace_single(r,a,b);
						r->tv.v.intval.initbool(result);
					}
				}
				else if (a->tv.type == token_type_t::floatval) {
					bool result;
					if (!reduce_gte_floatval(result,a->tv.v.floatval.val,b->tv.v.floatval.val)) return false;
					reduce_move_up_replace_single(r,a,b);
					r->tv.type = token_type_t::intval;
					r->tv.v.intval.initbool(result);
				}
			}
		}

		return true;
	}

	static bool reduce_lt(ast_node_t* &r) { /* ast_node_op_t::lessthan */
		/* [lessthan]
		 *   \- [a] -> [b]
		 *
		 * become
		 *
		 * [a < b] */
		reduce_check_op(r,ast_node_op_t::lessthan);

		ast_node_t *a=NULL,*b=NULL;
		if (!reduce_get_two_params(r,a,b)) return true;

		if (a->op == ast_node_op_t::constant && b->op == ast_node_op_t::constant) {
			const_cvt_both_prep(*a,*b);

			if (a->tv.type == b->tv.type) {
				if (a->tv.type == token_type_t::intval) {
					if (a->tv.v.intval.flags & token_intval_t::FL_SIGNED) {
						bool result;
						if (!reduce_lt_intval(result,a->tv.v.intval.v.v,b->tv.v.intval.v.v)) return false;
						a->tv.v.intval.flags &= ~token_intval_t::FL_SIGNED;
						reduce_move_up_replace_single(r,a,b);
						r->tv.v.intval.initbool(result);
					}
					else {
						bool result;
						if (!reduce_lt_intval(result,a->tv.v.intval.v.u,b->tv.v.intval.v.u)) return false;
						reduce_move_up_replace_single(r,a,b);
						r->tv.v.intval.initbool(result);
					}
				}
				else if (a->tv.type == token_type_t::floatval) {
					bool result;
					if (!reduce_lt_floatval(result,a->tv.v.floatval.val,b->tv.v.floatval.val)) return false;
					reduce_move_up_replace_single(r,a,b);
					r->tv.type = token_type_t::intval;
					r->tv.v.intval.initbool(result);
				}
			}
		}

		return true;
	}

	static bool reduce_lte(ast_node_t* &r) { /* ast_node_op_t::lessthanorequal */
		/* [lessthanorequal]
		 *   \- [a] -> [b]
		 *
		 * become
		 *
		 * [a <= b] */
		reduce_check_op(r,ast_node_op_t::lessthanorequal);

		ast_node_t *a=NULL,*b=NULL;
		if (!reduce_get_two_params(r,a,b)) return true;

		if (a->op == ast_node_op_t::constant && b->op == ast_node_op_t::constant) {
			const_cvt_both_prep(*a,*b);

			if (a->tv.type == b->tv.type) {
				if (a->tv.type == token_type_t::intval) {
					if (a->tv.v.intval.flags & token_intval_t::FL_SIGNED) {
						bool result;
						if (!reduce_lte_intval(result,a->tv.v.intval.v.v,b->tv.v.intval.v.v)) return false;
						a->tv.v.intval.flags &= ~token_intval_t::FL_SIGNED;
						reduce_move_up_replace_single(r,a,b);
						r->tv.v.intval.initbool(result);
					}
					else {
						bool result;
						if (!reduce_lte_intval(result,a->tv.v.intval.v.u,b->tv.v.intval.v.u)) return false;
						reduce_move_up_replace_single(r,a,b);
						r->tv.v.intval.initbool(result);
					}
				}
				else if (a->tv.type == token_type_t::floatval) {
					bool result;
					if (!reduce_lte_floatval(result,a->tv.v.floatval.val,b->tv.v.floatval.val)) return false;
					reduce_move_up_replace_single(r,a,b);
					r->tv.type = token_type_t::intval;
					r->tv.v.intval.initbool(result);
				}
			}
		}

		return true;
	}

	static bool reduce_equ(ast_node_t* &r) { /* ast_node_op_t::equals */
		/* [equals]
		 *   \- [a] -> [b]
		 *
		 * become
		 *
		 * [a == b] */
		reduce_check_op(r,ast_node_op_t::equals);

		ast_node_t *a=NULL,*b=NULL;
		if (!reduce_get_two_params(r,a,b)) return true;

		if (a->op == ast_node_op_t::constant && b->op == ast_node_op_t::constant) {
			const_cvt_both_prep(*a,*b);

			if (a->tv.type == b->tv.type) {
				if (a->tv.type == token_type_t::intval) {
					if (a->tv.v.intval.flags & token_intval_t::FL_SIGNED) {
						bool result;
						if (!reduce_equ_intval(result,a->tv.v.intval.v.v,b->tv.v.intval.v.v)) return false;
						a->tv.v.intval.flags &= ~token_intval_t::FL_SIGNED;
						reduce_move_up_replace_single(r,a,b);
						r->tv.v.intval.initbool(result);
					}
					else {
						bool result;
						if (!reduce_equ_intval(result,a->tv.v.intval.v.u,b->tv.v.intval.v.u)) return false;
						reduce_move_up_replace_single(r,a,b);
						r->tv.v.intval.initbool(result);
					}
				}
				else if (a->tv.type == token_type_t::floatval) {
					bool result;
					if (!reduce_equ_floatval(result,a->tv.v.floatval.val,b->tv.v.floatval.val)) return false;
					reduce_move_up_replace_single(r,a,b);
					r->tv.type = token_type_t::intval;
					r->tv.v.intval.initbool(result);
				}
			}
		}

		return true;
	}

	static bool reduce_nequ(ast_node_t* &r) { /* ast_node_op_t::notequals */
		/* [notequals]
		 *   \- [a] -> [b]
		 *
		 * become
		 *
		 * [a != b] */
		reduce_check_op(r,ast_node_op_t::notequals);

		ast_node_t *a=NULL,*b=NULL;
		if (!reduce_get_two_params(r,a,b)) return true;

		if (a->op == ast_node_op_t::constant && b->op == ast_node_op_t::constant) {
			const_cvt_both_prep(*a,*b);

			if (a->tv.type == b->tv.type) {
				if (a->tv.type == token_type_t::intval) {
					if (a->tv.v.intval.flags & token_intval_t::FL_SIGNED) {
						bool result;
						if (!reduce_nequ_intval(result,a->tv.v.intval.v.v,b->tv.v.intval.v.v)) return false;
						a->tv.v.intval.flags &= ~token_intval_t::FL_SIGNED;
						reduce_move_up_replace_single(r,a,b);
						r->tv.v.intval.initbool(result);
					}
					else {
						bool result;
						if (!reduce_nequ_intval(result,a->tv.v.intval.v.u,b->tv.v.intval.v.u)) return false;
						reduce_move_up_replace_single(r,a,b);
						r->tv.v.intval.initbool(result);
					}
				}
				else if (a->tv.type == token_type_t::floatval) {
					bool result;
					if (!reduce_nequ_floatval(result,a->tv.v.floatval.val,b->tv.v.floatval.val)) return false;
					reduce_move_up_replace_single(r,a,b);
					r->tv.type = token_type_t::intval;
					r->tv.v.intval.initbool(result);
				}
			}
		}

		return true;
	}

	static bool reduce_logand(ast_node_t* &r) { /* ast_node_op_t::logical_and */
		/* [logical_and]
		 *   \- [a] -> [b]
		 *
		 * become
		 *
		 * [a && b] */
		reduce_check_op(r,ast_node_op_t::logical_and);

		ast_node_t *a=NULL,*b=NULL;
		if (!reduce_get_two_params(r,a,b)) return true;

		if (a->op == ast_node_op_t::constant && b->op == ast_node_op_t::constant) {
			const_cvt_both_prep(*a,*b);

			if (a->tv.type == b->tv.type) {
				if (a->tv.type == token_type_t::intval) {
					if (a->tv.v.intval.flags & token_intval_t::FL_SIGNED) {
						bool result;
						if (!reduce_logand_intval(result,a->tv.v.intval.v.v,b->tv.v.intval.v.v)) return false;
						a->tv.v.intval.flags &= ~token_intval_t::FL_SIGNED;
						reduce_move_up_replace_single(r,a,b);
						r->tv.v.intval.initbool(result);
					}
					else {
						bool result;
						if (!reduce_logand_intval(result,a->tv.v.intval.v.u,b->tv.v.intval.v.u)) return false;
						reduce_move_up_replace_single(r,a,b);
						r->tv.v.intval.initbool(result);
					}
				}
				else if (a->tv.type == token_type_t::floatval) {
					bool result;
					if (!reduce_logand_floatval(result,a->tv.v.floatval.val,b->tv.v.floatval.val)) return false;
					reduce_move_up_replace_single(r,a,b);
					r->tv.type = token_type_t::intval;
					r->tv.v.intval.initbool(result);
				}
			}
		}

		return true;
	}

	static bool reduce_div(ast_node_t* &r) { /* ast_node_op_t::divide */
		/* [div]
		 *   \- [a] -> [b]
		 *
		 * become
		 *
		 * [a / b] */
		reduce_check_op(r,ast_node_op_t::divide);

		ast_node_t *a=NULL,*b=NULL;
		if (!reduce_get_two_params(r,a,b)) return true;

		if (a->op == ast_node_op_t::constant && b->op == ast_node_op_t::constant) {
			const_cvt_both_prep(*a,*b);

			if (a->tv.type == b->tv.type) {
				if (a->tv.type == token_type_t::intval) {
					const_intval_cvt_from_boolean(*a); /* convert boolean to int */
					const_intval_cvt_from_boolean(*b); /* convert boolean to int */
					if (a->tv.v.intval.flags & token_intval_t::FL_SIGNED) {
						signed long long result;
						if (!reduce_div_intval(result,a->tv.v.intval.v.v,b->tv.v.intval.v.v)) return false;
						reduce_move_up_replace_single(r,a,b);
						r->tv.v.intval.v.v = result;
					}
					else {
						unsigned long long result;
						if (!reduce_div_intval(result,a->tv.v.intval.v.u,b->tv.v.intval.v.u)) return false;
						reduce_move_up_replace_single(r,a,b);
						r->tv.v.intval.v.u = result;
					}
				}
				else if (a->tv.type == token_type_t::floatval) {
					long double result;
					if (!reduce_div_floatval(result,a->tv.v.floatval.val,b->tv.v.floatval.val)) return false;
					reduce_move_up_replace_single(r,a,b);
					r->tv.v.floatval.val = result;
				}
			}
		}

		return true;
	}

	static bool reduce_mod(ast_node_t* &r) { /* ast_node_op_t::modulo */
		/* [modulo]
		 *   \- [a] -> [b]
		 *
		 * become
		 *
		 * [a % b] */
		reduce_check_op(r,ast_node_op_t::modulo);

		ast_node_t *a=NULL,*b=NULL;
		if (!reduce_get_two_params(r,a,b)) return true;

		if (a->op == ast_node_op_t::constant && b->op == ast_node_op_t::constant) {
			const_cvt_both_prep(*a,*b);

			if (a->tv.type == b->tv.type) {
				if (a->tv.type == token_type_t::intval) {
					const_intval_cvt_from_boolean(*a); /* convert boolean to int */
					const_intval_cvt_from_boolean(*b); /* convert boolean to int */
					if (a->tv.v.intval.flags & token_intval_t::FL_SIGNED) {
						signed long long result;
						if (!reduce_mod_intval(result,a->tv.v.intval.v.v,b->tv.v.intval.v.v)) return false;
						reduce_move_up_replace_single(r,a,b);
						r->tv.v.intval.v.v = result;
					}
					else {
						unsigned long long result;
						if (!reduce_mod_intval(result,a->tv.v.intval.v.u,b->tv.v.intval.v.u)) return false;
						reduce_move_up_replace_single(r,a,b);
						r->tv.v.intval.v.u = result;
					}
				}
				else if (a->tv.type == token_type_t::floatval) {
					/* that's what fmod() is for! */
					return false;
				}
			}
		}

		return true;
	}

	static bool reduce_sub(ast_node_t* &r) { /* ast_node_op_t::subtract */
		/* [subtract]
		 *   \- [a] -> [b]
		 *
		 * become
		 *
		 * [a - b] */
		reduce_check_op(r,ast_node_op_t::subtract);

		ast_node_t *a=NULL,*b=NULL;
		if (!reduce_get_two_params(r,a,b)) return true;

		if (a->op == ast_node_op_t::constant && b->op == ast_node_op_t::constant) {
			const_cvt_both_prep(*a,*b);

			if (a->tv.type == b->tv.type) {
				if (a->tv.type == token_type_t::intval) {
					const_intval_cvt_from_boolean(*a); /* convert boolean to int */
					const_intval_cvt_from_boolean(*b); /* convert boolean to int */
					if (a->tv.v.intval.flags & token_intval_t::FL_SIGNED) {
						signed long long result;
						if (!reduce_sub_intval(result,a->tv.v.intval.v.v,b->tv.v.intval.v.v)) return false;
						reduce_move_up_replace_single(r,a,b);
						r->tv.v.intval.v.v = result;
					}
					else {
						unsigned long long result;
						if (!reduce_sub_intval(result,a->tv.v.intval.v.u,b->tv.v.intval.v.u)) return false;
						reduce_move_up_replace_single(r,a,b);
						r->tv.v.intval.v.u = result;
					}
				}
				else if (a->tv.type == token_type_t::floatval) {
					long double result;
					if (!reduce_sub_floatval(result,a->tv.v.floatval.val,b->tv.v.floatval.val)) return false;
					reduce_move_up_replace_single(r,a,b);
					r->tv.v.floatval.val = result;
				}
			}
		}

		return true;
	}

	static bool reduce_mul(ast_node_t* &r) { /* ast_node_op_t::multiply */
		/* [multiply]
		 *   \- [a] -> [b]
		 *
		 * become
		 *
		 * [a * b] */
		reduce_check_op(r,ast_node_op_t::multiply);

		ast_node_t *a=NULL,*b=NULL;
		if (!reduce_get_two_params(r,a,b)) return true;

		if (a->op == ast_node_op_t::constant && b->op == ast_node_op_t::constant) {
			const_cvt_both_prep(*a,*b);

			if (a->tv.type == b->tv.type) {
				if (a->tv.type == token_type_t::intval) {
					const_intval_cvt_from_boolean(*a); /* convert boolean to int */
					const_intval_cvt_from_boolean(*b); /* convert boolean to int */
					if (a->tv.v.intval.flags & token_intval_t::FL_SIGNED) {
						signed long long result;
						if (!reduce_mul_intval(result,a->tv.v.intval.v.v,b->tv.v.intval.v.v)) return false;
						reduce_move_up_replace_single(r,a,b);
						r->tv.v.intval.v.v = result;
					}
					else {
						unsigned long long result;
						if (!reduce_mul_intval(result,a->tv.v.intval.v.u,b->tv.v.intval.v.u)) return false;
						reduce_move_up_replace_single(r,a,b);
						r->tv.v.intval.v.u = result;
					}
				}
				else if (a->tv.type == token_type_t::floatval) {
					long double result;
					if (!reduce_mul_floatval(result,a->tv.v.floatval.val,b->tv.v.floatval.val)) return false;
					reduce_move_up_replace_single(r,a,b);
					r->tv.v.floatval.val = result;
				}
			}
		}

		return true;
	}

	static bool reduce_add(ast_node_t* &r) { /* ast_node_op_t::add */
		/* [add]
		 *   \- [a] -> [b]
		 *
		 * become
		 *
		 * [a + b] */
		reduce_check_op(r,ast_node_op_t::add);

		ast_node_t *a=NULL,*b=NULL;
		if (!reduce_get_two_params(r,a,b)) return true;

		if (a->op == ast_node_op_t::constant && b->op == ast_node_op_t::constant) {
			const_cvt_both_prep(*a,*b);

			if (a->tv.type == b->tv.type) {
				if (a->tv.type == token_type_t::intval) {
					const_intval_cvt_from_boolean(*a); /* convert boolean to int */
					const_intval_cvt_from_boolean(*b); /* convert boolean to int */
					if (a->tv.v.intval.flags & token_intval_t::FL_SIGNED) {
						signed long long result;
						if (!reduce_add_intval(result,a->tv.v.intval.v.v,b->tv.v.intval.v.v)) return false;
						reduce_move_up_replace_single(r,a,b);
						r->tv.v.intval.v.v = result;
					}
					else {
						unsigned long long result;
						if (!reduce_add_intval(result,a->tv.v.intval.v.u,b->tv.v.intval.v.u)) return false;
						reduce_move_up_replace_single(r,a,b);
						r->tv.v.intval.v.u = result;
					}
				}
				else if (a->tv.type == token_type_t::floatval) {
					long double result;
					if (!reduce_add_floatval(result,a->tv.v.floatval.val,b->tv.v.floatval.val)) return false;
					reduce_move_up_replace_single(r,a,b);
					r->tv.v.floatval.val = result;
				}
			}
		}

		return true;
	}

	void ilc_cls_t::init(void) {
		cls = 0;
		cls_c = c_t::ct_none;
		cls_t = t_t::t_none;
		cls_p = p_t::p_none;
	}

	bool ilc_cls_t::parse_ident_node(ast_node_t* &chk,bool idlist) {
		switch (chk->op) {
			case ast_node_op_t::r_float:
				cls |= ilc_cls_t::c_float | ilc_cls_t::f_float;
				chk = chk->next;
				break;
			case ast_node_op_t::r_double:
				cls |= ilc_cls_t::c_float | ilc_cls_t::f_double;
				chk = chk->next;
				break;
			case ast_node_op_t::r_bool:
				cls |= ilc_cls_t::c_int | ilc_cls_t::i_bool;
				chk = chk->next;
				break;
			case ast_node_op_t::r_char:
				cls |= ilc_cls_t::c_int | ilc_cls_t::i_char;
				chk = chk->next;
				break;
			case ast_node_op_t::r_int:
				cls |= ilc_cls_t::c_int | ilc_cls_t::i_int;
				chk = chk->next;
				break;
			case ast_node_op_t::r_short:
				cls |= ilc_cls_t::c_int | ilc_cls_t::i_short;
				chk = chk->next;
				break;
			case ast_node_op_t::r_long: {
				const unsigned int p_cls = cls;

				cls |= ilc_cls_t::i_long; /* could be "long" as in int, or "long double" */
				chk = chk->next;

				if (idlist && chk && chk->op == ast_node_op_t::r_long) { /* long long */
					cls = p_cls | ilc_cls_t::i_llong;
					chk = chk->next;
				}
				break; }
			case ast_node_op_t::r_signed:
				cls |= ilc_cls_t::c_int | ilc_cls_t::i_signed;
				chk = chk->next;
				break;
			case ast_node_op_t::r_unsigned:
				cls |= ilc_cls_t::c_int | ilc_cls_t::i_unsigned;
				chk = chk->next;
				break;
			case ast_node_op_t::r_const:
				cls |= ilc_cls_t::g_const;
				chk = chk->next;
				break;
			case ast_node_op_t::r_constexpr:
				cls |= ilc_cls_t::g_constexpr;
				chk = chk->next;
				break;
			case ast_node_op_t::r_compileexpr:
				cls |= ilc_cls_t::g_compileexpr;
				chk = chk->next;
				break;
			case ast_node_op_t::r_volatile:
				cls |= ilc_cls_t::g_volatile;
				chk = chk->next;
				break;
			case ast_node_op_t::r_static:
				cls |= ilc_cls_t::g_static;
				chk = chk->next;
				break;
			case ast_node_op_t::r_near:
				cls |= ilc_cls_t::p_near;
				chk = chk->next;
				break;
			case ast_node_op_t::r_far:
				cls |= ilc_cls_t::p_far;
				chk = chk->next;
				break;
			case ast_node_op_t::r_huge:
				cls |= ilc_cls_t::p_huge;
				chk = chk->next;
				break;
			case ast_node_op_t::i_attributes:
				chk = chk->next;
				break;
			default:
				/* anything else: stop on the token, mark it other, leave "chk" at the other token */
				cls |= ilc_cls_t::c_other;
				if (idlist) return false;
		}

		return true;
	}

	bool ilc_cls_t::parse_ident(ast_node_t* &chk) {
		if (!parse_ident_node(chk,/*idlist*/false))
			return false;

		if (!parse_idlist_final())
			return false;

		return true;
	}

	bool ilc_cls_t::parse_idlist(ast_node_t* &chk) {
		while (chk != NULL) {
			if (!parse_ident_node(chk,/*idlist*/true)) /* will advance chk=chk->next */
				break;
		}

		if (!parse_idlist_final())
			return false;

		if ((cls_c == c_t::ct_other) != (chk != NULL))
			return false;

		return true;
	}

	bool ilc_cls_t::parse_idlist_final(void) {
		cls |= ilc_cls_t::c_valid;

		/* "long" does not set ilc_cls_t::c_int to allow "long double".
		 * if "long" was sspecified and neither ilc_cls_t::c_int|ilc_cls_t::c_float then assume ilc_cls_t::c_int */
		if ((cls & (ilc_cls_t::c_int|ilc_cls_t::c_float)) == 0 && (cls & ilc_cls_t::i_long))
			cls |= ilc_cls_t::c_int;

		/* you can declare an int, a float, or an other, but not any combination of them */
		switch (cls & (ilc_cls_t::c_float|ilc_cls_t::c_int|ilc_cls_t::c_other)) {
			case 0: /* none of them */
				break; /* OK */
			case ilc_cls_t::c_other:
				cls_c = c_t::ct_other;
				break; /* OK */
			case ilc_cls_t::c_float:
				if (cls & ilc_cls_t::i_llong)
					return false; /* no such thing, "long long" float */

				cls_c = c_t::ct_float;
				if ((cls & (ilc_cls_t::f_double|ilc_cls_t::i_long)) == (ilc_cls_t::f_double|ilc_cls_t::i_long))
					cls_t = t_t::t_longdouble;
				else if (cls & ilc_cls_t::f_double)
					cls_t = t_t::t_double;
				else if (cls & ilc_cls_t::f_float)
					cls_t = t_t::t_float;
				break; /* OK */
			case ilc_cls_t::c_int:
				cls_c = c_t::ct_int;
				if ((cls & (ilc_cls_t::i_signed|ilc_cls_t::i_unsigned)) == (ilc_cls_t::i_signed|ilc_cls_t::i_unsigned))
					return false; /* you cannot declare something unsigned AND signed at the same time */
				if ((cls & (ilc_cls_t::i_long|ilc_cls_t::i_llong)) == (ilc_cls_t::i_long|ilc_cls_t::i_llong))
					return false; /* you cannot declare something long AND long long at the same time */

				if (cls & ilc_cls_t::i_llong)
					cls_t = t_t::t_llong;
				else if (cls & ilc_cls_t::i_long)
					cls_t = t_t::t_long;
				else if (cls & ilc_cls_t::i_int)
					cls_t = t_t::t_int;
				else if (cls & ilc_cls_t::i_short)
					cls_t = t_t::t_short;
				else if (cls & ilc_cls_t::i_char)
					cls_t = t_t::t_char;
				else if (cls & ilc_cls_t::i_bool)
					cls_t = t_t::t_bool;
				break; /* OK */
			default:
				return false; /* NO! */
		}

		/* you can declare something near, far, or huge, but not any combination of them */
		switch (cls & (ilc_cls_t::p_near|ilc_cls_t::p_far|ilc_cls_t::p_huge)) {
			case 0: /* none of them */
				break; /* OK */
			case ilc_cls_t::p_huge:
				cls_p = p_t::p_huge;
				break; /* OK */
			case ilc_cls_t::p_near:
				cls_p = p_t::p_near;
				break; /* OK */
			case ilc_cls_t::p_far:
				cls_p = p_t::p_far;
				break; /* OK */
			default:
				return false; /* NO! */
		}

		/* clear some flags that others should not use */
		cls &= ~ilc_cls_t::f_non_public;
		return true;
	}

	static bool reduce_typecast_const_doit(ast_node_t* &r,ast_node_t* &a,ast_node_t* &b,ilc_cls_t &ilc) {
		if (ilc.cls_c == ilc_cls_t::c_t::ct_other) {
			/* do nothing */
		}
		else if (ilc.cls_c == ilc_cls_t::c_t::ct_float) {
			const_cvt_float(*b);

			switch (ilc.cls_t) {
				case ilc_cls_t::t_t::t_float:
					b->tv.v.floatval.ftype = token_floatval_t::T_LONGDOUBLE;
					break;
				case ilc_cls_t::t_t::t_double:
					b->tv.v.floatval.ftype = token_floatval_t::T_DOUBLE;
					break;
				case ilc_cls_t::t_t::t_longdouble:
					b->tv.v.floatval.ftype = token_floatval_t::T_FLOAT;
					break;
				default:
					break;
			}

			reduce_move_b_to_a(r,a,b);
			reduce_move_up_replace_single(r,a);
		}
		else if (ilc.cls_c == ilc_cls_t::c_t::ct_int) {
			if (ilc.cls_t == ilc_cls_t::t_t::t_bool) {
				const_cvt_bool(*b);
				b->tv.v.intval.itype = token_intval_t::T_BOOL;
			}
			else {
				const_cvt_int(*b,(ilc.cls&ilc_cls_t::i_unsigned)?true:false);

				if (ilc.cls & ilc_cls_t::i_signed)
					const_intval_cvt_signed(*b);
				else if (ilc.cls & ilc_cls_t::i_unsigned)
					const_intval_cvt_unsigned(*b);

				switch (ilc.cls_t) {
					case ilc_cls_t::t_t::t_llong:
						b->tv.v.intval.itype = token_intval_t::T_LONGLONG;
						break;
					case ilc_cls_t::t_t::t_long:
						b->tv.v.intval.itype = token_intval_t::T_LONG;
						break;
					case ilc_cls_t::t_t::t_int:
						b->tv.v.intval.itype = token_intval_t::T_INT;
						break;
					case ilc_cls_t::t_t::t_short:
						b->tv.v.intval.itype = token_intval_t::T_SHORT;
						break;
					case ilc_cls_t::t_t::t_char:
						b->tv.v.intval.itype = token_intval_t::T_CHAR;
						break;
					default:
						break;
				}
			}

			reduce_move_b_to_a(r,a,b);
			reduce_move_up_replace_single(r,a);
		}
		else if (ilc.cls_c == ilc_cls_t::c_t::ct_none) {
			reduce_move_b_to_a(r,a,b);
			reduce_move_up_replace_single(r,a);
		}

		return true;
	}

	static bool reduce_typeid_to_typeclsif(ast_node_t* &a) {
		assert(a != NULL);

		/* if possible, convert to typeclsif */
		if (a->op == ast_node_op_t::identifier_list) {
			ast_node_t *chk = a->child;
			ilc_cls_t ilc;

			ilc.init();
			if (!ilc.parse_idlist(chk))
				return false;

			/* free all child nodes, or at least up to chk, except
			 * for i_attribute nodes which must be preserved  */
			{
				ast_node_t **p = &(a->child); /* node to modify ->next to remove node */

				while ((*p) && (chk == NULL || (*p) != chk)) {
					if ((*p)->op == ast_node_op_t::i_attributes) {
						p = &((*p)->next);
					}
					else {
						ast_node_t *d = (*p);
						(*p) = (*p)->next;
						d->free_children();
						delete d;
					}
				}
			}
			a->op = ast_node_op_t::r_typeclsif;
			a->tv.type = token_type_t::r_typeclsif;
			a->tv.v.typecls = std::move(ilc);
		}

		return true;
	}

	static bool reduce_typeid_to_typeclsif(ast_node_t* &r,ast_node_t* &a) {
		assert(r != NULL);
		assert(a != NULL);
		assert(r->child == a);

		/* if possible, convert to typeclsif */
		if (a->op == ast_node_op_t::identifier_list) {
			if (!reduce_typeid_to_typeclsif(a))
				return false;
		}
		else if (
			a->op == ast_node_op_t::r_float ||
			a->op == ast_node_op_t::r_double ||
			a->op == ast_node_op_t::r_bool ||
			a->op == ast_node_op_t::r_char ||
			a->op == ast_node_op_t::r_int ||
			a->op == ast_node_op_t::r_short ||
			a->op == ast_node_op_t::r_long ||
			a->op == ast_node_op_t::r_signed ||
			a->op == ast_node_op_t::r_unsigned ||
			a->op == ast_node_op_t::r_const ||
			a->op == ast_node_op_t::r_constexpr ||
			a->op == ast_node_op_t::r_compileexpr ||
			a->op == ast_node_op_t::r_volatile ||
			a->op == ast_node_op_t::r_near ||
			a->op == ast_node_op_t::r_far ||
			a->op == ast_node_op_t::r_huge ||
			a->op == ast_node_op_t::identifier) {
			ast_node_t *chk = a;
			ilc_cls_t ilc;

			ilc.init();
			if (!ilc.parse_ident(chk))
				return false;

			if (chk == a) {

				/* [r]
				 *  \- [old a] -> ...
				 *
				 * to
				 *
				 * [r]
				 *  \- [new a] -> ...
				 *      \- [old a] */

				ast_node_t *pa = a;

				a = new ast_node_t;
				a->tv.position = pa->tv.position;

				assert(r->child == pa);
				r->child = a;
				a->child = pa;

				a->next = pa->next;
				pa->next = NULL;
			}

			a->op = ast_node_op_t::r_typeclsif;
			a->tv.type = token_type_t::r_typeclsif;
			a->tv.v.typecls = std::move(ilc);
		}

		return true;
	}

	static bool reduce_typecast(ast_node_t* &r) { /* ast_node_op_t::typecast */
		/* [typecast]
		 *   \- [a] -> [b]
		 *
		 * become
		 *
		 * (a)b */
		reduce_check_op(r,ast_node_op_t::typecast);

		ast_node_t *a=NULL,*b=NULL;
		if (!reduce_get_two_params(r,a,b,RGP_ALLOW_CHILD)) return true;

		if (!reduce_typeid_to_typeclsif(r,a)) return false;

#if 0//DEBUG
		ast_node_t *p_a = a;
		ast_node_t *p_b = b;

		if (!reduce_get_two_params(r,a,b,RGP_ALLOW_CHILD)) return true;

		assert(a == p_a);
		assert(b == p_b);
#endif

		if (a->op == ast_node_op_t::r_typeclsif) {
			if (b->op == ast_node_op_t::constant) {
				if (!reduce_typecast_const_doit(r,a,b,a->tv.v.typecls))
					return false;
			}
		}

		return true;
	}

	static bool reduce_compound_let(ast_node_t* &r) { /* ast_node_op_t::i_compound_let */
		/* [compound let]
		 *   \- [datatype] -> [let] -> ...
		 *                     \- [identifier] -> ...
		 *
		 * our job is to convert the datatype to a typeclsif */
		reduce_check_op(r,ast_node_op_t::i_compound_let);

		ast_node_t *dt=NULL,*tmp=r->child;

		if (!tmp) return true;
		dt = tmp; tmp = tmp->next;

		if (dt->op == ast_node_op_t::i_datatype) {
			if (!reduce_typeid_to_typeclsif(dt->child))
				return false;
		}

		return true;
	}

	static bool reduce_r_fn(ast_node_t* &r) { /* ast_node_op_t::r_fn */
		/* [r_fn]
		 *   \- [datatype] -> [identifier] -> ...
		 *
		 * our job is to convert the datatype to a typeclsif */
		reduce_check_op(r,ast_node_op_t::r_fn);

		ast_node_t *dt=NULL,*tmp=r->child;

		if (!tmp) return true;
		dt = tmp; tmp = tmp->next;

		if (dt->op == ast_node_op_t::i_datatype) {
			if (!reduce_typeid_to_typeclsif(dt->child))
				return false;
		}

		if (tmp && (tmp->op == ast_node_op_t::identifier || tmp->op == ast_node_op_t::i_anonymous))
			tmp = tmp->next;

		while (tmp && tmp->op == ast_node_op_t::argument) {
			if (!reduce_typeid_to_typeclsif(tmp->child))
				return false;

			tmp = tmp->next;
		}

		return true;
	}

	static bool reduce_fncall(ast_node_t* &r) { /* ast_node_op_t::functioncall */
		/* [functioncall]
		 *   \- [identifier] -> .... (args)
		 *
		 * become, if basic type, same as a typecast
		 *
		 * [ident] -> [arg] ...
		 *
		 * ident(arg) */
		reduce_check_op(r,ast_node_op_t::functioncall);

		ast_node_t *id=NULL,*a1=NULL,*tmp=r->child;

		if (!tmp) return true;
		id = tmp; tmp = tmp->next;

		if (tmp && tmp->op == ast_node_op_t::argument) {
			a1 = tmp->child;
			tmp = tmp->next;
		}

		if (	id->op == ast_node_op_t::r_bool ||
			id->op == ast_node_op_t::r_char ||
			id->op == ast_node_op_t::r_int ||
			id->op == ast_node_op_t::r_short ||
			id->op == ast_node_op_t::r_long ||
			id->op == ast_node_op_t::r_float ||
			id->op == ast_node_op_t::r_double ||
			id->op == ast_node_op_t::r_signed ||
			id->op == ast_node_op_t::r_unsigned) {
			if (a1 != NULL) {
				if (a1->op == ast_node_op_t::constant && tmp == NULL) { /* constant and only one param */
					/* convert the function call into a typecast and run typecast reduce */
					r->op = ast_node_op_t::typecast;
					assert(r->child != NULL);
					assert(r->child == id);
					assert(r->child->next != NULL);
					assert(r->child->next->child == a1);
					/* lift a1 up one level */
					delete r->child->next;
					r->child->next = a1;
					return reduce_typecast(r);
				}
			}
		}

		return true;
	}

	bool compiler::reduce(ast_node_t* &root) {
		if (root == NULL)
			return true;

		for (ast_node_t *n=root;n;n=n->next) {
			if (!reduce(n->child))
				return false;

			switch (n->op) {
				case ast_node_op_t::multiply:		if (!reduce_mul(n)) return false; break;
				case ast_node_op_t::divide:		if (!reduce_div(n)) return false; break;
				case ast_node_op_t::modulo:		if (!reduce_mod(n)) return false; break;
				case ast_node_op_t::add:		if (!reduce_add(n)) return false; break;
				case ast_node_op_t::subtract:		if (!reduce_sub(n)) return false; break;
				case ast_node_op_t::negate:		if (!reduce_neg(n)) return false; break;
				case ast_node_op_t::unaryplus:		if (!reduce_uplus(n)) return false; break;
				case ast_node_op_t::subexpression:	if (!reduce_subexpr(n)) return false; break;
				case ast_node_op_t::binarynot:		if (!reduce_binnot(n)) return false; break;
				case ast_node_op_t::logicalnot:		if (!reduce_lognot(n)) return false; break;
				case ast_node_op_t::logical_or:		if (!reduce_logor(n)) return false; break;
				case ast_node_op_t::logical_and:	if (!reduce_logand(n)) return false; break;
				case ast_node_op_t::comma:		if (!reduce_comma(n)) return false; break;
				case ast_node_op_t::ternary:		if (!reduce_ternary(n)) return false; break;
				case ast_node_op_t::binary_xor:		if (!reduce_binxor(n)) return false; break;
				case ast_node_op_t::binary_and:		if (!reduce_binand(n)) return false; break;
				case ast_node_op_t::binary_or:		if (!reduce_binor(n)) return false; break;
				case ast_node_op_t::notequals:		if (!reduce_nequ(n)) return false; break;
				case ast_node_op_t::leftshift:		if (!reduce_shl(n)) return false; break;
				case ast_node_op_t::rightshift:		if (!reduce_shr(n)) return false; break;
				case ast_node_op_t::equals:		if (!reduce_equ(n)) return false; break;
				case ast_node_op_t::lessthanorequal:	if (!reduce_lte(n)) return false; break;
				case ast_node_op_t::greaterthanorequal:	if (!reduce_gte(n)) return false; break;
				case ast_node_op_t::lessthan:		if (!reduce_lt(n)) return false; break;
				case ast_node_op_t::greaterthan:	if (!reduce_gt(n)) return false; break;
				case ast_node_op_t::typecast:		if (!reduce_typecast(n)) return false; break;
				case ast_node_op_t::functioncall:	if (!reduce_fncall(n)) return false; break;
				case ast_node_op_t::i_compound_let:	if (!reduce_compound_let(n)) return false; break;
				case ast_node_op_t::r_fn:		if (!reduce_r_fn(n)) return false; break;
				default: break;
			};
		}

		return true;
	}

	bool compiler::reduce(void) {
		if (!reduce(root_node))
			return false;

		return true;
	}

	bool compiler::compile(void) {
		ast_node_t *apnode = NULL,*rnode = NULL;
		bool ok = true;
		token_t tok;

		if (!pb.is_alloc()) {
			if (!pb.alloc(4096))
				return false;
		}

		tok_buf_clear();
		if (tok_snamelist) {
			token_source_name_t t;

			t.name = pb_ctx.source_name;
			tok_pos.source = (*tok_snamelist).size();
			(*tok_snamelist).push_back(std::move(t));
		}
		tok_pos.first_line();
		tok_buf_refill();
		while (!tok_buf_empty()) {
			if (!statement(rnode,apnode)) {
				error_msg("Syntax error",tok_bufpeek(0).position);
				ok = false;
				break;
			}
		}

		if (root_node) {
			ast_node_t *r = root_node;
			while (r->next != NULL) r = r->next;
			r->next = rnode;
		}
		else {
			root_node = rnode;
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
			t.v.identifier.length = identlen;
			assert(t.v.identifier.length != 0);
			t.v.identifier.name = new char[t.v.identifier.length+1]; /* string + NUL */
			memcpy(t.v.identifier.name,start,t.v.identifier.length);
			t.v.identifier.name[t.v.identifier.length] = 0;
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
					case ilc_cls_t::c_t::ct_int: s += " c=int"; break;
					case ilc_cls_t::c_t::ct_float: s += " c=float"; break;
					case ilc_cls_t::c_t::ct_other: s += " c=other"; break;
					default: break;
				}

				switch (t.v.typecls.cls_t) {
					case ilc_cls_t::t_t::t_bool: s += " t=bool"; break;
					case ilc_cls_t::t_t::t_char: s += " t=char"; break;
					case ilc_cls_t::t_t::t_int: s += " t=int"; break;
					case ilc_cls_t::t_t::t_short: s += " t=short"; break;
					case ilc_cls_t::t_t::t_long: s += " t=long"; break;
					case ilc_cls_t::t_t::t_llong: s += " t=llong"; break;
					case ilc_cls_t::t_t::t_float: s += " t=float"; break;
					case ilc_cls_t::t_t::t_double: s += " t=double"; break;
					case ilc_cls_t::t_t::t_longdouble: s += " t=longdouble"; break;
					default: break;
				}

				switch (t.v.typecls.cls_p) {
					case ilc_cls_t::p_t::p_near: s += " p=near"; break;
					case ilc_cls_t::p_t::p_far: s += " p=far"; break;
					case ilc_cls_t::p_t::p_huge: s += " p=huge"; break;
					default: break;
				}

				snprintf(buf,sizeof(buf)," cls=0x%x",t.v.typecls.cls); s += buf;
				if (t.v.typecls.cls & ilc_cls_t::i_signed) s += " signed";
				if (t.v.typecls.cls & ilc_cls_t::i_unsigned) s += " unsigned";
				if (t.v.typecls.cls & ilc_cls_t::g_const) s += " const";
				if (t.v.typecls.cls & ilc_cls_t::g_constexpr) s += " constexpr";
				if (t.v.typecls.cls & ilc_cls_t::g_compileexpr) s += " compileexpr";
				if (t.v.typecls.cls & ilc_cls_t::g_volatile) s += " volatile";
				if (t.v.typecls.cls & ilc_cls_t::g_static) s += " static";

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
				s = "<identifier: \"";
				if (t.v.identifier.name) s += std::string(t.v.identifier.name,t.v.identifier.length);
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
		if (!cc.reduce()) {
			fprintf(stderr,"Failed to compile+reduce\n");
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

