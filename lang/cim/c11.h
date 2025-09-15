
#include <stdint.h>

enum c11yy_unop {
	C11YY_UNOP_NONE=0,
	C11YY_UNOP_NEG,

	C11YY_UNOP__MAX
};

int c11yy_do_compile();

typedef uint32_t c11yy_string_token_id;
#define c11yy_string_token_none			((uint32_t)(~0ul))

struct c11yy_struct_base {
	unsigned int				t; /* from c11.y.h */
};

#define C11YY_INTF_OVERFLOW			(1u << 0u) /* value overflowed during parsing */
#define C11YY_INTF_PARSED			(1u << 1u) /* value was parsed, v is valid */
#define C11YY_INTF_SIGNED			(1u << 2u) /* value is signed, use v.s else use v.u */
#define C11YY_INTF_CHARCONST			(1u << 3u) /* value came from char constant */

struct c11yy_struct_integer {
	unsigned int				t; /* from c11.y.h == I_CONSTANT, ENUMERATION_CONSTANT */
	union {
		uint64_t			u;
		int64_t				s;
	} v;
	uint8_t					sz; /* 0 if no size, else in bits */
	uint8_t					flags;
};

#define C11YY_FLOATF_NEGATIVE			(1u << 0u)
#define C11YY_FLOATF_SPECIAL			(1u << 1u) /* i.e. NaN, inf, etc */

struct c11yy_struct_float {
	unsigned int				t; /* from c11.y.h == F_CONSTANT */
	uint64_t				mant;
	int					exponent;
	uint8_t					flags;
	uint8_t					sz; /* 0 if no size, else in bits */
};

enum c11yystringtype {
	C11YY_STRT_LOCAL=0,			// no prefix given, in whatever non-unicode locale
	C11YY_STRT_UTF8,			// UTF-8 encoding
	C11YY_STRT_UTF16,			// UTF-16 encoding
	C11YY_STRT_UTF32			// UTF-32 encoding
};

struct c11yy_struct_strliteral {
	unsigned int				t; /* from c11.y.h == STRING_LITERAL */
	c11yy_string_token_id			id;
	enum c11yystringtype			stype;
};

union c11yy_struct {
	struct c11yy_struct_base		base;
	struct c11yy_struct_integer		intval;
	struct c11yy_struct_float		floatval;
	struct c11yy_struct_strliteral		strlitval;
};

void c11yy_init_iconst(struct c11yy_struct_integer *val,const char *yytext,const char lexmatch);
int c11yy_unary(union c11yy_struct *d,const union c11yy_struct *s,const unsigned int unop);

