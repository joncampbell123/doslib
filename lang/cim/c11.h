
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

enum c11yyidenttype {
	C11YY_IDT_NONE=0,
	C11YY_IDT_IDENTIFIER,
	C11YY_IDT_TYPEDEF_NAME,
	C11YY_IDT_ENUMERATION_CONSTANT,

	C11YY_IDT__MAX
};

typedef uint32_t c11yy_identifier_id;
#define c11yy_identifier_none			( ~((uint32_t)(0u)) )

typedef uint32_t c11yy_scope_id;
#define c11yy_scope_none			( ~((uint32_t)(0u)) )

typedef uint32_t c11yy_astnode_id;
#define c11yy_astnode_none			( ~((uint32_t)(0u)) )

struct c11yy_struct_base {
	unsigned int				t; /* from c11.y.h */
};

#define C11YY_INTF_OVERFLOW			(1u << 0u) /* value overflowed during parsing */
#define C11YY_INTF_PARSED			(1u << 1u) /* value was parsed, v is valid */
#define C11YY_INTF_SIGNED			(1u << 2u) /* value is signed, use v.s else use v.u */
#define C11YY_INTF_CHARCONST			(1u << 3u) /* value came from char constant */
#define C11YY_INTF_TRUNCATEOK			(1u << 4u) /* value can be truncated without issue */

struct c11yy_struct_integer {
	unsigned int				t; /* from c11.y.h == I_CONSTANT */
	union {
		uint64_t			u;
		int64_t				s;
	} v;
	uint8_t					sz; /* 0 if no size, else in bits */
	uint8_t					flags;
};

extern const struct c11yy_struct_integer c11yy_struct_integer_I_CONSTANT_INIT;

#define C11YY_FLOATF_NEGATIVE			(1u << 0u)
#define C11YY_FLOATF_SPECIAL			(1u << 1u) /* i.e. NaN, inf, etc */

struct c11yy_struct_float {
	/* regardless of host format, this struct tracks floating point like the Intel 80-bit
	 * "extended precision" format would including how normalization normally leaves the
	 * 63rd bit of the mantissa set except for zero. Exponent is an int with a large enough
	 * range that "subnormal" numbers do not occur and it goes way beyound the 80-bit format
	 * exponent limits. The flags field is used to indicate whether it has special meaning
	 * such as NaN. */
	unsigned int				t; /* from c11.y.h == F_CONSTANT */
	uint64_t				mant;
	int					exponent;
	uint8_t					flags;
	uint8_t					sz; /* 0 if no size, else in bits */
};

extern const struct c11yy_struct_float c11yy_struct_float_F_CONSTANT_INIT;

struct c11yy_struct_strliteral {
	unsigned int				t; /* from c11.y.h == STRING_LITERAL */
	c11yy_string_token_id			id;
};

struct c11yy_struct_identifier {
	unsigned int				t; /* from c11.y.h == IDENTIFIER */
	c11yy_identifier_id			id;
	c11yy_scope_id				scopeid;
};

enum c11yy_astnode_op {
	ASTOP_NONE=0,				// 0
	ASTOP_NEGATE,

	ASTOP__MAX
};

struct c11yy_struct_astnode {
	unsigned int				t; /* from c11.y.h == ASTNODE */
	enum c11yy_astnode_op			op;
	c11yy_astnode_id			next;
	c11yy_astnode_id			child;
};

extern const struct c11yy_struct_astnode c11yy_struct_astnode_INIT;

union c11yy_struct {
	struct c11yy_struct_base		base;
	struct c11yy_struct_integer		intval;
	struct c11yy_struct_astnode		astnode;
	struct c11yy_struct_float		floatval;
	struct c11yy_struct_strliteral		strlitval;
	struct c11yy_struct_identifier		identifier;
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

struct c11yy_identifier_obj {
	enum c11yyidenttype			itype;
	uint8_t					len; /* in bytes -- we're not going to support identifiers >= 256 bytes long! */
	uint32_t				hash;
	uint8_t*				s8;
};

/////////////////////////////////////////////////////////////////////////////////////////

void c11yy_init_strlit(struct c11yy_struct_strliteral *val,const char *yytext,int yyleng);
void c11yy_init_fconst(struct c11yy_struct_float *val,const char *yytext,const char lexmatch);
void c11yy_init_iconst(struct c11yy_struct_integer *val,const char *yytext,const char lexmatch);
c11yy_identifier_id c11yy_ident_to_id(struct c11yy_identifier_obj *io,const c11yy_scope_id scope);
struct c11yy_identifier_obj *c11yy_ident_lookup(const uint32_t hash,const char *buf,int len,c11yy_scope_id *scope);
struct c11yy_identifier_obj *c11yy_init_ident(const char *yytext,int yyleng,c11yy_scope_id *scope);
int c11yy_unary(union c11yy_struct *d,const union c11yy_struct *s,const unsigned int unop);
extern int c11yy_check_type(const struct c11yy_identifier_obj *io);
extern uint32_t c11yy_string_hash(const uint8_t *s,size_t l);
extern uint8_t c11yy_iconstu_auto_size(uint64_t v);
extern uint8_t c11yy_iconsts_auto_size(int64_t v);
extern void c11yyerror(const char *);  /* prints grammar violation message */
int c11yy_char2digit(const char c);

int c11yy_add(union c11yy_struct *d,const union c11yy_struct *a,const union c11yy_struct *b);
int c11yy_sub(union c11yy_struct *d,const union c11yy_struct *a,const union c11yy_struct *b);
int c11yy_mul(union c11yy_struct *d,const union c11yy_struct *a,const union c11yy_struct *b);
int c11yy_div(union c11yy_struct *d,const union c11yy_struct *a,const union c11yy_struct *b);
int c11yy_mod(union c11yy_struct *d,const union c11yy_struct *a,const union c11yy_struct *b);

