
/* An opaque pointer. */
typedef void* yyscan_t;

extern int cimcc_yydebug;

void (*cimcc_read_input)(char *buffer,int *size,int max_size);

int cimcc_yyparse(yyscan_t *scanner);
int cimcc_yylex_init(yyscan_t* scanner);
int cimcc_yylex_destroy(yyscan_t yyscanner);

typedef unsigned char cimcc_yy_type;

enum {
	CIMCC_YYT_NONE=0,
	CIMCC_YYT_INTVAL=1,
	CIMCC_YYT_FLOATVAL=2
};

/* CIMCC_YYT_INTVAL */
#define CIMCC_YYINTVAL_FL_SIGNED			(1u << 0u)

/* CIMCC_YYT_INTVAL */
enum {
	CIMCC_YYINTVAL_DT_UNSPEC=0,
	CIMCC_YYINTVAL_DT_CHAR=1,
	CIMCC_YYINTVAL_DT_SHORT=2,
	CIMCC_YYINTVAL_DT_INT=3,
	CIMCC_YYINTVAL_DT_LONG=4,
	CIMCC_YYINTVAL_DT_LONGLONG=5,
	CIMCC_YYINTVAL_DT_BOOL=6
};

/* CIMCC_YYT_FLOATVAL */
/* none defined */

/* CIMCC_YYT_FLOATVAL */
enum {
	CIMCC_YYFLOATVAL_DT_UNSPEC=0,
	CIMCC_YYFLOATVAL_DT_HALF=1,
	CIMCC_YYFLOATVAL_DT_FLOAT=2,
	CIMCC_YYFLOATVAL_DT_DOUBLE=3,
	CIMCC_YYFLOATVAL_DT_LONGDOUBLE=4
};

typedef struct cimcc_intval_t {
	cimcc_yy_type			type;
	unsigned char			flags; /* make short or int if more flags needed */
	unsigned char			datatype; /* CIMCC_YYINTVAL_DT_* */
	union {
		unsigned long long	u; /* unsigned */
		signed long long	s; /* signed */
	} val;
} cimcc_intval_t;

typedef struct cimcc_floatval_t {
	cimcc_yy_type			type;
	/* add flags field when it is needed */
	unsigned char			datatype; /* CIMCC_YYFLOATVAL_DT_* */
	long double			val;
} cimcc_floatval_t;

typedef union cimcc_yystype_t {
	cimcc_yy_type			type;
	struct cimcc_intval_t		intval;
	struct cimcc_floatval_t		floatval;
} cimcc_yystype_t;

void cimcc_parse_int_const(char *buffer,cimcc_yystype_t *yyt,int base);
void cimcc_parse_float_const(char *buffer,cimcc_yystype_t *yyt);

