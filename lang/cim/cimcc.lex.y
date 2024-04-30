%{

extern int cimcc_yylex(void);
extern int cimcc_yyparse(void);

void cimcc_yyerror(const char *s);

#define yyparse cimcc_yyparse
#define yyerror cimcc_yyerror
#define yylval cimcc_yylval
#define yylex cimcc_yylex
%}

%union {
	unsigned long long	intval;
	long double		floatval;
	unsigned int		flags;
}

%token<intval> T_INTEGER
%token<floatval> T_FLOAT
%token T_MINUS T_PLUS T_STAR T_SLASH T_PERCENT
%token T_OPEN_PAREN T_CLOSE_PAREN
%token T_OPEN_CURLY T_CLOSE_CURLY
%token T_SEMICOLON

%start one_unit

%%

one_unit
	: statement
	| one_unit statement
	;

statement
	: T_SEMICOLON
	| expression T_SEMICOLON
	;

expression
	: T_INTEGER
	| T_FLOAT
	| T_OPEN_PAREN expression T_CLOSE_PAREN
	| unary_operator expression
	| expression binary_operator expression
	;

unary_operator
	: T_MINUS
	;

binary_operator
	: T_PLUS
	| T_MINUS
	| T_STAR
	| T_SLASH
	| T_PERCENT
	;

%%

