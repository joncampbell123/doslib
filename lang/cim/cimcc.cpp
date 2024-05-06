
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <fcntl.h>

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
		return (c == ' ' || c == '\t' || c == '\n');
	}

	/////////

	struct token_t;

	struct context_t {
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
			T_LONGLONG=5
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

	struct token_t {
		token_type_t		type = token_type_t::none;

		union token_value_t {
			token_intval_t		intval;		// type == intval
			token_floatval_t	floatval;	// type == floatval
		} v;
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

		char peekb(void) {
			if (pb.read == pb.end) refill();
			if (pb.read < pb.end) return *(pb.read);
			return 0;
		}

		void skipb(void) {
			if (pb.read == pb.end) refill();
			if (pb.read < pb.end) pb.read++;
		}

		char getb(void) {
			const char r = peekb();
			skipb();
			return r;
		}

		void refill(void);
		bool compile(void);
		void free_ast(void);
		void whitespace(void);
		void gtok(token_t &t);
		void gtok_number(token_t &t);
		void gtok_prep_number_proc(void);
		bool expression(ast_node_t* &pchnode);
		bool unary_expression(ast_node_t* &pchnode);
		bool shift_expression(ast_node_t* &pchnode);
		bool postfix_expression(ast_node_t* &pchnode);
		bool primary_expression(ast_node_t* &pchnode);
		bool additive_expression(ast_node_t* &pchnode);
		bool equality_expression(ast_node_t* &pchnode);
		bool binary_or_expression(ast_node_t* &pchnode);
		bool relational_expression(ast_node_t* &pchnode);
		bool binary_xor_expression(ast_node_t* &pchnode);
		bool binary_and_expression(ast_node_t* &pchnode);
		bool logical_or_expression(ast_node_t* &pchnode);
		bool assignment_expression(ast_node_t* &pchnode);
		bool logical_and_expression(ast_node_t* &pchnode);
		bool conditional_expression(ast_node_t* &pchnode);
		bool argument_expression_list(ast_node_t* &pchnode);
		bool multiplicative_expression(ast_node_t* &pchnode);
		bool statement(ast_node_t* &rnode,ast_node_t* &apnode);

		ast_node_t*		root_node = NULL;

		static constexpr size_t tok_buf_size = 32;
		static constexpr size_t tok_buf_threshhold = 16;
		token_t			tok_buf[tok_buf_size];
		size_t			tok_buf_i = 0,tok_buf_o = 0;
		token_t			tok_empty;

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
				if (rem != 0) memmove(&tok_buf[0],&tok_buf[tok_buf_i],rem*sizeof(token_t));
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

	void token_to_string(std::string &s,const token_t &t);

	bool compiler::primary_expression(ast_node_t* &pchnode) {
		/* the bufpeek/get functions return a stock empty token if we read beyond available tokens */
		token_t &t = tok_bufpeek();

		if (t.type == token_type_t::intval || t.type == token_type_t::floatval) {
			assert(pchnode == NULL);
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::constant;
			pchnode->tv = std::move(t);
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

			return true;
		}

		return false;
	}

	bool compiler::argument_expression_list(ast_node_t* &pchnode) {
#define NLEX assignment_expression
		if (!NLEX(pchnode))
			return false;

		while (tok_bufpeek().type == token_type_t::comma) { /* , comma operator */
			tok_bufdiscard(); /* eat it */

			/* [,]
			 *  \
			 *   +-- [left expr] -> [right expr] */

			ast_node_t *sav_p = pchnode;
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::comma;
			pchnode->child = sav_p;
			if (!NLEX(sav_p->next))
				return false;
		}
#undef NLEX
		return true;
	}

	/* [https://en.cppreference.com/w/c/language/operator_precedence] level 1 */
	bool compiler::postfix_expression(ast_node_t* &pchnode) {
#define NLEX primary_expression
		if (!NLEX(pchnode))
			return false;

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

				/* [functioncall]
				 *  \
				 *   +-- [left expr] -> [argument expression list] */

				ast_node_t *sav_p = pchnode;
				pchnode = new ast_node_t;
				pchnode->op = ast_node_op_t::functioncall;
				pchnode->child = sav_p;

				if (tok_bufpeek().type != token_type_t::closeparen) {
					if (!argument_expression_list(sav_p->next))
						return false;
				}

				{
					token_t &t = tok_bufpeek();
					if (t.type == token_type_t::closeparen)
						tok_bufdiscard(); /* eat it */
					else
						return false;
				}
			}
			else {
				break;
			}
		}
#undef NLEX
		return true;
	}

	/* [https://en.cppreference.com/w/c/language/operator_precedence] level 2 */
	bool compiler::unary_expression(ast_node_t* &pchnode) {
#define NLEX postfix_expression
		/* the bufpeek/get functions return a stock empty token if we read beyond available tokens */
		{
			token_t &t = tok_bufpeek();

			switch (t.type) {
				case token_type_t::ampersand:
					tok_bufdiscard(); /* eat it */

					/* [&]
					 *  \
					 *   +--- expression */
					assert(pchnode == NULL);
					pchnode = new ast_node_t;
					pchnode->op = ast_node_op_t::addressof;
					return unary_expression(pchnode->child);

				case token_type_t::star:
					tok_bufdiscard(); /* eat it */

					/* [*]
					 *  \
					 *   +--- expression */
					assert(pchnode == NULL);
					pchnode = new ast_node_t;
					pchnode->op = ast_node_op_t::dereference;
					return unary_expression(pchnode->child);

				case token_type_t::minusminus:
					tok_bufdiscard(); /* eat it */

					/* [--]
					 *  \
					 *   +--- expression */
					assert(pchnode == NULL);
					pchnode = new ast_node_t;
					pchnode->op = ast_node_op_t::predecrement;
					return unary_expression(pchnode->child);

				case token_type_t::plusplus:
					tok_bufdiscard(); /* eat it */

					/* [++]
					 *  \
					 *   +--- expression */
					assert(pchnode == NULL);
					pchnode = new ast_node_t;
					pchnode->op = ast_node_op_t::preincrement;
					return unary_expression(pchnode->child);

				case token_type_t::minus:
					tok_bufdiscard(); /* eat it */

					/* [-]
					 *  \
					 *   +--- expression */
					assert(pchnode == NULL);
					pchnode = new ast_node_t;
					pchnode->op = ast_node_op_t::negate;
					return unary_expression(pchnode->child);

				case token_type_t::plus:
					tok_bufdiscard(); /* eat it */

					/* [+]
					 *  \
					 *   +--- expression */
					assert(pchnode == NULL);
					pchnode = new ast_node_t;
					pchnode->op = ast_node_op_t::unaryplus;
					return unary_expression(pchnode->child);

				case token_type_t::exclamation:
					tok_bufdiscard(); /* eat it */

					/* [!]
					 *  \
					 *   +--- expression */
					assert(pchnode == NULL);
					pchnode = new ast_node_t;
					pchnode->op = ast_node_op_t::logicalnot;
					return unary_expression(pchnode->child);

				case token_type_t::tilde:
					tok_bufdiscard(); /* eat it */

					/* [~]
					 *  \
					 *   +--- expression */
					assert(pchnode == NULL);
					pchnode = new ast_node_t;
					pchnode->op = ast_node_op_t::binarynot;
					return unary_expression(pchnode->child);

				default:
					return NLEX(pchnode);
			}
		}
#undef NLEX
		return true;
	}

	/* [https://en.cppreference.com/w/c/language/operator_precedence] level 3 */
	bool compiler::multiplicative_expression(ast_node_t* &pchnode) {
#define NLEX unary_expression
		if (!NLEX(pchnode))
			return false;

		/* This is written differently, because we need it built "inside out" */
		while (1) {
			if (tok_bufpeek().type == token_type_t::star) { /* * operator */
				tok_bufdiscard(); /* eat it */

				/* [*]
				 *  \
				 *   +-- [left expr] -> [right expr] */

				ast_node_t *sav_p = pchnode;
				pchnode = new ast_node_t;
				pchnode->op = ast_node_op_t::multiply;
				pchnode->child = sav_p;
				if (!NLEX(sav_p->next))
					return false;
			}
			else if (tok_bufpeek().type == token_type_t::slash) { /* / operator */
				tok_bufdiscard(); /* eat it */

				/* [/]
				 *  \
				 *   +-- [left expr] -> [right expr] */

				ast_node_t *sav_p = pchnode;
				pchnode = new ast_node_t;
				pchnode->op = ast_node_op_t::divide;
				pchnode->child = sav_p;
				if (!NLEX(sav_p->next))
					return false;
			}
			else if (tok_bufpeek().type == token_type_t::percent) { /* % operator */
				tok_bufdiscard(); /* eat it */

				/* [%]
				 *  \
				 *   +-- [left expr] -> [right expr] */

				ast_node_t *sav_p = pchnode;
				pchnode = new ast_node_t;
				pchnode->op = ast_node_op_t::modulo;
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
	bool compiler::additive_expression(ast_node_t* &pchnode) {
#define NLEX multiplicative_expression
		if (!NLEX(pchnode))
			return false;

		/* This is written differently, because we need it built "inside out" for addition and subtraction
		 * to be done in the correct order, else subtraction won't work properly i.e. 5-6-3 needs to be
		 * built as ((5-6)-3) not (5-(6-3)). */
		while (1) {
			if (tok_bufpeek().type == token_type_t::plus) { /* + operator */
				tok_bufdiscard(); /* eat it */

				/* [+]
				 *  \
				 *   +-- [left expr] -> [right expr] */

				ast_node_t *sav_p = pchnode;
				pchnode = new ast_node_t;
				pchnode->op = ast_node_op_t::add;
				pchnode->child = sav_p;
				if (!NLEX(sav_p->next))
					return false;
			}
			else if (tok_bufpeek().type == token_type_t::minus) { /* - operator */
				tok_bufdiscard(); /* eat it */

				/* [-]
				 *  \
				 *   +-- [left expr] -> [right expr] */

				ast_node_t *sav_p = pchnode;
				pchnode = new ast_node_t;
				pchnode->op = ast_node_op_t::subtract;
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
	bool compiler::assignment_expression(ast_node_t* &pchnode) {
#define NLEX conditional_expression
		if (!NLEX(pchnode))
			return false;

		/* assignment expressions are parsed right to left, therefore something like a = b = c = d
		 * needs to be handled like a = (b = (c = d)) */
		if (tok_bufpeek().type == token_type_t::equal) {
			tok_bufdiscard(); /* eat it */

			/* [=]
			 *  \
			 *   +-- [left expr] -> [right expr] */

			ast_node_t *sav_p = pchnode;
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::assign;
			pchnode->child = sav_p;
			return assignment_expression(sav_p->next);
		}
		else if (tok_bufpeek().type == token_type_t::plusequal) {
			tok_bufdiscard(); /* eat it */

			/* [+=]
			 *  \
			 *   +-- [left expr] -> [right expr] */

			ast_node_t *sav_p = pchnode;
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::assignadd;
			pchnode->child = sav_p;
			return assignment_expression(sav_p->next);
		}
		else if (tok_bufpeek().type == token_type_t::minusequal) {
			tok_bufdiscard(); /* eat it */

			/* [-=]
			 *  \
			 *   +-- [left expr] -> [right expr] */

			ast_node_t *sav_p = pchnode;
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::assignsubtract;
			pchnode->child = sav_p;
			return assignment_expression(sav_p->next);
		}
		else if (tok_bufpeek().type == token_type_t::starequal) {
			tok_bufdiscard(); /* eat it */

			/* [*=]
			 *  \
			 *   +-- [left expr] -> [right expr] */

			ast_node_t *sav_p = pchnode;
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::assignmultiply;
			pchnode->child = sav_p;
			return assignment_expression(sav_p->next);
		}
		else if (tok_bufpeek().type == token_type_t::slashequal) {
			tok_bufdiscard(); /* eat it */

			/* [/=]
			 *  \
			 *   +-- [left expr] -> [right expr] */

			ast_node_t *sav_p = pchnode;
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::assigndivide;
			pchnode->child = sav_p;
			return assignment_expression(sav_p->next);
		}
		else if (tok_bufpeek().type == token_type_t::percentequal) {
			tok_bufdiscard(); /* eat it */

			/* [%=]
			 *  \
			 *   +-- [left expr] -> [right expr] */

			ast_node_t *sav_p = pchnode;
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::assignmodulo;
			pchnode->child = sav_p;
			return assignment_expression(sav_p->next);
		}
		else if (tok_bufpeek().type == token_type_t::ampersandequal) {
			tok_bufdiscard(); /* eat it */

			/* [&=]
			 *  \
			 *   +-- [left expr] -> [right expr] */

			ast_node_t *sav_p = pchnode;
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::assignand;
			pchnode->child = sav_p;
			return assignment_expression(sav_p->next);
		}
		else if (tok_bufpeek().type == token_type_t::caretequal) {
			tok_bufdiscard(); /* eat it */

			/* [^=]
			 *  \
			 *   +-- [left expr] -> [right expr] */

			ast_node_t *sav_p = pchnode;
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::assignxor;
			pchnode->child = sav_p;
			return assignment_expression(sav_p->next);
		}
		else if (tok_bufpeek().type == token_type_t::pipeequal) {
			tok_bufdiscard(); /* eat it */

			/* [|=]
			 *  \
			 *   +-- [left expr] -> [right expr] */

			ast_node_t *sav_p = pchnode;
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::assignor;
			pchnode->child = sav_p;
			return assignment_expression(sav_p->next);
		}
		else if (tok_bufpeek().type == token_type_t::leftleftangleequal) { /* <<= operator */
			tok_bufdiscard(); /* eat it */

			/* [<<=]
			 *  \
			 *   +-- [left expr] -> [right expr] */

			ast_node_t *sav_p = pchnode;
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::assignleftshift;
			pchnode->child = sav_p;
			return assignment_expression(sav_p->next);
		}
		else if (tok_bufpeek().type == token_type_t::rightrightangleequal) { /* >>= operator */
			tok_bufdiscard(); /* eat it */

			/* [>>=]
			 *  \
			 *   +-- [left expr] -> [right expr] */

			ast_node_t *sav_p = pchnode;
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::assignrightshift;
			pchnode->child = sav_p;
			return assignment_expression(sav_p->next);
		}
#undef NLEX
		return true;
	}

	/* [https://en.cppreference.com/w/c/language/operator_precedence] level 15 */
	bool compiler::expression(ast_node_t* &pchnode) {
#define NLEX assignment_expression
		if (!NLEX(pchnode))
			return false;

		while (tok_bufpeek().type == token_type_t::comma) { /* , comma operator */
			tok_bufdiscard(); /* eat it */

			/* [,]
			 *  \
			 *   +-- [left expr] -> [right expr] */

			ast_node_t *sav_p = pchnode;
			pchnode = new ast_node_t;
			pchnode->op = ast_node_op_t::comma;
			pchnode->child = sav_p;
			if (!NLEX(sav_p->next))
				return false;
		}
#undef NLEX
		return true;
	}

	bool compiler::statement(ast_node_t* &rnode,ast_node_t* &apnode) {
		if (apnode) {
			apnode->next = new ast_node_t;
			apnode = apnode->next;
		}
		else {
			rnode = apnode = new ast_node_t;
		}
		apnode->op = ast_node_op_t::statement;

		/* [statement] -> [statement] -> ...
		 *   \                \
		 *    +-- expression   +-- expression */

		tok_buf_refill();
		if (!tok_buf_empty()) {
			if (!expression(apnode->child))
				return false;

			{
				token_t &t = tok_bufpeek();
				if (t.type == token_type_t::semicolon || t.type == token_type_t::eof)
					tok_bufdiscard(); /* eat the EOF or semicolon */
				else
					return false;
			}
		}

		return true;
	}

	void compiler::free_ast(void) {
		root_node->free_nodes();
		delete root_node;
		root_node = NULL;
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
		tok_buf_refill();
		while (!tok_buf_empty()) {
			if (!statement(rnode,apnode)) {
				fprintf(stderr,"Syntax error\n");
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

	void compiler::gtok_number(token_t &t) {
		gtok_prep_number_proc();
		assert(pb.read < pb.end); /* wait, peekb() returned a digit we didn't getb()?? */

		/* do not flush, do not call getb() or flush() or lazy_flush() in this block */
		unsigned char *start = pb.read;
		unsigned char base = 10;
		unsigned char suffix = 0;
		bool is_float = false;

		static constexpr unsigned S_LONG=(1u << 0u);
		static constexpr unsigned S_LONGLONG=(1u << 1u);
		static constexpr unsigned S_UNSIGNED=(1u << 2u);
		static constexpr unsigned S_FLOAT=(1u << 3u);
		static constexpr unsigned S_DOUBLE=(1u << 4u);

		if (pb.read < pb.end && *pb.read == '0') {
			pb.read++;

			if (pb.read < pb.end) {
				if (*pb.read == 'x') { /* hexadecimal */
					pb.read++;
					base = 16;
					while (pb.read < pb.end && is_hexadecimal_digit(*pb.read)) pb.read++;
				}
				else if (*pb.read == 'b') { /* binary */
					pb.read++;
					base = 2;
					while (pb.read < pb.end && is_decimal_digit(*pb.read,2)) pb.read++;
				}
				else if (is_decimal_digit(*pb.read,8)) { /* octal, 2nd digit (do not advance read ptr) */
					base = 8;
					while (pb.read < pb.end && is_decimal_digit(*pb.read,8)) pb.read++;
				}
				else {
					/* it's just zero and a different token afterwards */
				}
			}
		}
		else { /* decimal */
			while (pb.read < pb.end && is_decimal_digit(*pb.read)) pb.read++;
		}

		/* it might be a floating point constant if it has a decimal point */
		if (pb.read < pb.end && *pb.read == '.' && base == 10) {
			pb.read++;

			while (pb.read < pb.end && is_decimal_digit(*pb.read)) pb.read++;
			is_float = true;
		}

		/* it might be a float with exponent field */
		if (pb.read < pb.end && (*pb.read == 'e' || *pb.read == 'E') && base == 10) {
			pb.read++;

			/* e4, e-4, e+4 */
			if (pb.read < pb.end && (*pb.read == '-' || *pb.read == '+')) pb.read++;
			while (pb.read < pb.end && is_decimal_digit(*pb.read)) pb.read++;
			is_float = true;
		}

		/* check suffixes */
		while (1) {
			if (pb.read < pb.end && (*pb.read == 'l' || *pb.read == 'L')) {
				suffix |= S_LONG;
				pb.read++;

				if (pb.read < pb.end && (*pb.read == 'l' || *pb.read == 'L')) {
					suffix |= S_LONGLONG;
					pb.read++;
				}
			}
			else if (pb.read < pb.end && (*pb.read == 'u' || *pb.read == 'U')) {
				suffix |= S_UNSIGNED;
				pb.read++;
			}
			else if (pb.read < pb.end && (*pb.read == 'f' || *pb.read == 'F')) {
				suffix |= S_FLOAT;
				is_float = true;
				pb.read++;
			}
			else if (pb.read < pb.end && (*pb.read == 'd' || *pb.read == 'D')) {
				suffix |= S_DOUBLE;
				is_float = true;
				pb.read++;
			}
			else {
				break;
			}
		}

		assert(start != pb.read);

		/* DONE SCANNING. Parse number */
		if (is_float) {
			t.type = token_type_t::floatval;
			auto &nt = t.v.floatval;
			nt.init();

			/* strtold() should handle the decimal point and exponent for us */
			nt.val = strtold((char*)start,NULL);

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
			nt.v.u = strtoull((char*)start,NULL,base);

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
				break;
			case ']':
				t.type = token_type_t::rightsquarebracket;
				skipb();
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
			case '?':
				t.type = token_type_t::question;
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
			default:
				t.type = token_type_t::none;
				skipb();
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
				sprintf(buf,"%llu",t.v.intval.v.u);
				s = "<intval ";
				s += buf;
				sprintf(buf," f=0x%x",(unsigned int)t.v.intval.flags);
				s += buf;
				sprintf(buf," t=%u",(unsigned int)t.v.intval.itype);
				s += buf;
				s += ">";
				break;
			case token_type_t::floatval:
				sprintf(buf,"%0.20f",(double)t.v.floatval.val);
				s = "<floatval ";
				s += buf;
				sprintf(buf," t=%u",(unsigned int)t.v.floatval.ftype);
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
			default:
				s = "?";
				break;
		}
	}

	void dump_ast_nodes(ast_node_t *parent,const size_t depth=0) {
		std::string s;
		std::string indent;

		for (size_t i=0;i < depth;i++)
			indent += "  ";

		fprintf(stderr,"AST TREE: ");
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
					name = "decrement++";
					break;
				case ast_node_op_t::predecrement:
					name = "--decrement";
					break;
				case ast_node_op_t::preincrement:
					name = "++decrement";
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
				case ast_node_op_t::functioncall:
					name = "functioncall";
					break;
				default:
					name = "?";
					break;
			};

			token_to_string(s,parent->tv);
			fprintf(stderr,"  %s%s: %s\n",indent.c_str(),name,s.c_str());
			dump_ast_nodes(parent->child,depth+1u);
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
		CIMCC::compiler cc;
		src_ctx fdctx;

		fdctx.fd = open(argv[1],O_RDONLY);
		if (fdctx.fd < 0) {
			fprintf(stderr,"Failed to open file\n");
			return 1;
		}
		cc.set_source_cb(refill_fdio);
		cc.set_source_ctx(&fdctx,sizeof(fdctx));

		cc.compile();
		dump_ast_nodes(cc.root_node);
		cc.free_ast();

		/* fdctx destructor closes file descriptor */
	}

	return 0;
}

