
#include <stdint.h>
#include <string.h>

enum c11yy_unop {
	C11YY_UNOP_NONE=0,			// 0
	C11YY_UNOP_NEG,
	C11YY_UNOP_POS,
	C11YY_UNOP_BNOT,
	C11YY_UNOP_LNOT,

	C11YY_UNOP__MAX
};

int c11yy_do_compile();

typedef uint32_t c11yy_string_token_id;
#define c11yy_string_token_none			( ~((uint32_t)(0u)) )

struct c11yy_struct_base {
	unsigned int				t; /* from c11.y.h */
};

#define C11YY_INTF_OVERFLOW			(1u << 0u) /* value overflowed during parsing */
#define C11YY_INTF_PARSED			(1u << 1u) /* value was parsed, v is valid */
#define C11YY_INTF_SIGNED			(1u << 2u) /* value is signed, use v.s else use v.u */
#define C11YY_INTF_CHARCONST			(1u << 3u) /* value came from char constant */
#define C11YY_INTF_TRUNCATEOK			(1u << 4u) /* value can be truncated without issue */

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

struct c11yy_struct_strliteral {
	unsigned int				t; /* from c11.y.h == STRING_LITERAL */
	c11yy_string_token_id			id;
};

union c11yy_struct {
	struct c11yy_struct_base		base;
	struct c11yy_struct_integer		intval;
	struct c11yy_struct_float		floatval;
	struct c11yy_struct_strliteral		strlitval;
};

/////////////////////////////////////////////////////////////////////////////////////

enum c11yystringtype {
	C11YY_STRT_LOCAL=0,			// no prefix given, in whatever non-unicode locale
	C11YY_STRT_UTF8,			// UTF-8 encoding
	C11YY_STRT_UTF16,			// UTF-16 encoding
	C11YY_STRT_UTF32			// UTF-32 encoding
};

struct c11yy_string_obj {
	enum c11yystringtype			stype;
	uint16_t				len; /* in bytes -- we're not going to support strings >= 64KB long! */
	uint32_t				hash;
	union {
		void*				raw;
		uint8_t*			s8; /* local/UTF-8 */
		uint16_t*			s16; /* local/UTF-16 */
		uint32_t*			s32; /* local/UTF-32 */
	} str;
};

/////////////////////////////////////////////////////////////////////////////////////////

enum c11yyidenttype {
	C11YY_IDT_NONE=0,
	C11YY_IDT_IDENTIFIER,
	C11YY_IDT_TYPEDEF_NAME,
	C11YY_IDT_ENUMERATION_CONSTANT
};

typedef uint32_t c11yy_identifier_id;
#define c11yy_identifier_none			( ~((uint32_t)(0u)) )

typedef uint32_t c11yy_scope_id;
#define c11yy_scope_none			( ~((uint32_t)(0u)) )

struct c11yy_identifier_obj {
	enum c11yyidenttype			itype;
	uint8_t					len; /* in bytes -- we're not going to support identifiers >= 256 bytes long! */
	uint8_t*				s8;
};

/////////////////////////////////////////////////////////////////////////////////////////

struct c11yy_identifier_obj *c11yy_init_ident(const char *yytext,const char lexmatch);
void c11yy_init_strlit(struct c11yy_struct_strliteral *val,const char *yytext,int yyleng);
void c11yy_init_iconst(struct c11yy_struct_integer *val,const char *yytext,const char lexmatch);
int c11yy_unary(union c11yy_struct *d,const union c11yy_struct *s,const unsigned int unop);

