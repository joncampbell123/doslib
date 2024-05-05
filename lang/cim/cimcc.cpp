
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
		void whitespace(void);
		void gtok(token_t &t);
		void decimal_constant(unsigned long long &v,unsigned char base,char first_char=0);
		void hexadecimal_constant(unsigned long long &v,char first_char=0);

		private:
		parse_buffer		pb;
		context_t		pb_ctx;
		refill_function_t	pb_refill = refill_null;
	};

	enum class token_type_t {
		eof=-1,
		none=0,
		intval=1,
		floatval=2,
		comma=3,
		semicolon
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
	};

	struct token_t {
		token_type_t		type = token_type_t::none;

		union token_value_t {
			token_intval_t		intval;		// type == intval
			token_floatval_t	floatval;	// type == floatval
		} v;

		token_t() { }
		~token_t() { }
	};

	void compiler::refill(void) {
		assert(pb.sanity_check());
		pb.lazy_flush();
		pb.refill(pb_refill,pb_ctx);
		assert(pb.sanity_check());
	}

	void token_to_string(std::string &s,token_t &t);

	bool compiler::compile(void) {
		token_t tok;

		if (!pb.is_alloc()) {
			if (!pb.alloc(4096))
				return false;
		}

		refill();
		while (!pb.eof()) {
			gtok(/*&*/tok);

			{
				std::string s;
				token_to_string(s,tok);
				fprintf(stderr,"%s\n",s.c_str());
			}
		}

		return true;
	}

	void compiler::whitespace(void) {
		do {
			const char c = peekb(); /* returns 0 on EOF */
			if (is_whitespace(c)) skipb();
			else break;
		} while (1);
	}

	void compiler::decimal_constant(unsigned long long &v,unsigned char base,char first_char) {
		const unsigned long long maxval = 0xFFFFFFFFFFFFFFFFULL;
		const unsigned long long willoverflow = maxval / (unsigned long long)base;
		char c;

		if (first_char > 0)
			v = p_decimal_digit(first_char); /* assume first_char is a digit because the caller already checked */
		else
			v = 0ull;

		while (is_decimal_digit(c=peekb(),base)) {
			skipb();
			if (v <= willoverflow) {
				v *= (unsigned long long)base;
				v += p_decimal_digit(c);
			}
			else {
				v = maxval;
			}
		}
	}

	void compiler::hexadecimal_constant(unsigned long long &v,char first_char) {
		const unsigned long long maxval = 0xFFFFFFFFFFFFFFFFULL;
		const unsigned long long willoverflow = maxval / 16ull;
		char c;

		if (first_char > 0)
			v = p_hexadecimal_digit(first_char); /* assume first_char is a digit because the caller already checked */
		else
			v = 0ull;

		while (is_hexadecimal_digit(c=peekb())) {
			skipb();
			if (v <= willoverflow) {
				v *= 16ull;
				v += p_hexadecimal_digit(c);
			}
			else {
				v = maxval;
			}
		}
	}

	void compiler::gtok(token_t &t) {
		char c;

		whitespace();
		if (pb.eof()) {
			t.type = token_type_t::eof;
			return;
		}

		c = getb();
		if (c == '0') {
			c = peekb();
			/* 0nnnn    octal
			 * 0bnnn    binary
			 * 0xnnn    hexadecimal
			 * 0        zero */
			if (c == 'b' || c == 'B') { /* 0bnnn */
				skipb(); /* skip 'b' */
				t.type = token_type_t::intval;
				t.v.intval.initu();
				decimal_constant(t.v.intval.v.u, 2);
			}
			else if (c == 'x' || c == 'X') { /* 0xnnnn */
				skipb(); /* skip 'x' */
				t.type = token_type_t::intval;
				t.v.intval.initu();
				hexadecimal_constant(t.v.intval.v.u);
			}
			else if (is_decimal_digit(c,8)) { /* 0nnnn */
				t.type = token_type_t::intval;
				t.v.intval.initu();
				decimal_constant(t.v.intval.v.u, 8);
			}
			else { /* 0 */
				t.type = token_type_t::intval;
				t.v.intval.initu();
				t.v.intval.v.u = 0;
			}
		}
		else if (is_decimal_digit(c)) {
			t.type = token_type_t::intval;
			t.v.intval.init();
			decimal_constant(t.v.intval.v.u, 10, c);
		}
		else if (c == ',') {
			t.type = token_type_t::comma;
		}
		else if (c == ';') {
			t.type = token_type_t::semicolon;
		}
		else {
			t.type = token_type_t::none;
		}
	}

	void token_to_string(std::string &s,token_t &t) {
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
			case token_type_t::comma:
				s = "<comma>";
				break;
			case token_type_t::semicolon:
				s = "<semicolon>";
				break;
			default:
				s = "?";
				break;
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

		/* fdctx destructor closes file descriptor */
	}

	return 0;
}

