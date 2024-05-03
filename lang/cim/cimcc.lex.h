
/* An opaque pointer. */
typedef void* yyscan_t;

extern int cimcc_yydebug;

void (*cimcc_read_input)(char *buffer,int *size,int max_size);

int cimcc_yyparse(yyscan_t *scanner);
int cimcc_yylex_init(yyscan_t* scanner);
int cimcc_yylex_destroy(yyscan_t yyscanner);

enum {
	CIMCC_YYT_NONE=0,
	CIMCC_YYT_INTVAL=1,
	CIMCC_YYT_FLOATVAL=2
};

/* CIMCC_YYT_INTVAL */
#define CIMCC_YYINTVAL_FL_SIGNED			(1u << 0u)
#define CIMCC_YYINTVAL_FL_LONG				(1u << 1u) /* for float, double type */
#define CIMCC_YYINTVAL_FL_LONGLONG			(1u << 2u) /* for float, long double type */
#define CIMCC_YYINTVAL_FL_SHORT				(1u << 3u)
#define CIMCC_YYINTVAL_FL_CHAR				(1u << 4u)
#define CIMCC_YYINTVAL_FL_BOOL				(1u << 5u)

/* CIMCC_YYT_FLOATVAL */
#define CIMCC_YYFLOATVAL_FL_LONG			(1u << 0u) /* long double */
#define CIMCC_YYFLOATVAL_FL_DOUBLE			(1u << 1u) /* double */
#define CIMCC_YYFLOATVAL_FL_FLOAT			(1u << 2u) /* float */
#define CIMCC_YYFLOATVAL_FL_HALF			(1u << 3u) /* half float */

typedef struct cimcc_intval_t {
	unsigned int			type;
	unsigned int			flags;
	union {
		unsigned long long	u; /* unsigned */
		signed long long	s; /* signed */
	} val;
} cimcc_intval_t;

typedef struct cimcc_floatval_t {
	unsigned int			type;
	unsigned int			flags;
	long double			val;
} cimcc_floatval_t;

typedef union cimcc_yystype_t {
	unsigned int			type;
	struct cimcc_intval_t		intval;
	struct cimcc_floatval_t		floatval;
} cimcc_yystype_t;

void cimcc_parse_int_const(char *buffer,cimcc_yystype_t *yyt,int base);
void cimcc_parse_float_const(char *buffer,cimcc_yystype_t *yyt);

