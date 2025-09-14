
#include <stdint.h>

#include "c11.y.h"

int c11yy_do_compile();

typedef uint32_t c11yy_string_token_id;
#define c11yy_string_token_none			((uint32_t)(~0ul))

struct c11yy_struct_base {
	enum c11yytokentype			t; /* from c11.y.h */
};

struct c11yy_struct_integer {
	enum c11yytokentype			t; /* from c11.y.h == I_CONSTANT */
	union {
		uint64_t			u;
		int64_t				s;
	} v;
	uint8_t					sz; /* 0 if no size, else in bits */
};

#define C11YY_FLOATF_NEGATIVE			(1u << 0u)
#define C11YY_FLOATF_SPECIAL			(1u << 1u) /* i.e. NaN, inf, etc */

struct c11yy_struct_float {
	enum c11yytokentype			t; /* from c11.y.h == F_CONSTANT */
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
	enum c11yytokentype			t; /* from c11.y.h == STRING_LITERAL */
	c11yy_string_token_id			id;
	enum c11yystringtype			stype;
};

union c11yy_struct {
	struct c11yy_struct_base		base;
	struct c11yy_struct_integer		intval;
	struct c11yy_struct_float		floatval;
	struct c11yy_struct_strliteral		strlitval;
};

