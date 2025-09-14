
#include <stdint.h>

#include "c11.y.h"

int c11yy_do_compile();

typedef uint32_t c11yy_string_token_id;
static constexpr uint32_t c11yy_string_token_none = (uint32_t)(~0ul);

struct c11yy_struct_base {
	enum c11yytokentype			t; /* from c11.y.h */
};

struct c11yy_struct_integer {
	enum c11yytokentype			t; /* from c11.y.h */
	union {
		uint64_t			u;
		int64_t				s;
	} v;
	uint8_t					sz; /* 0 if no size, else in bits */
};

static constexpr uint8_t C11YY_FLOATF_NEGATIVE = (1u << 0u);
static constexpr uint8_t C11YY_FLOATF_SPECIAL = (1u << 1u); /* i.e. NaN, inf, etc */

struct c11yy_struct_float {
	enum c11yytokentype			t; /* from c11.y.h */
	uint64_t				mant;
	int					exponent;
	uint8_t					flags;
	uint8_t					sz; /* 0 if no size, else in bits */
};

union c11yy_struct {
	struct c11yy_struct_base		b;
	struct c11yy_struct_integer		intval;
	struct c11yy_struct_float		floatval;
};

