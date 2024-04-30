
/* An opaque pointer. */
typedef void* yyscan_t;

extern int cimcc_yydebug;

void (*cimcc_read_input)(char *buffer,int *size,int max_size);

int cimcc_yyparse(yyscan_t *scanner);
int cimcc_yylex_init(yyscan_t* scanner);
int cimcc_yylex_destroy(yyscan_t yyscanner);

#define CIMCC_YYFL_SIGNED               (1u << 0u)
#define CIMCC_YYFL_LONG                 (1u << 1u) /* for float, double type */
#define CIMCC_YYFL_LONGLONG             (1u << 2u) /* for float, long double type */
#define CIMCC_YYFL_SHORT                (1u << 3u)
#define CIMCC_YYFL_CHAR                 (1u << 4u)
#define CIMCC_YYFL_NEGATE               (1u << 5u)

void cimcc_parse_int_const(char *buffer,unsigned long long *val,unsigned int *flags,int base);
void cimcc_parse_float_const(char *buffer,long double *val,unsigned int *flags);

