%{

#include "cimcc.lex.h"
#include "cimcc.tab.h"

extern int cimcc_yylex(YYSTYPE *,yyscan_t *scanner);
extern int cimcc_yyparse(yyscan_t *scanner);

void cimcc_yyerror(yyscan_t *scanner,const char *s);

%}

%union {
	unsigned long long	intval;
	long double		floatval;
	unsigned int		flags;
}

%define api.pure full
%lex-param {yyscan_t *scanner}
%parse-param {yyscan_t *scanner}

%token<intval> T_INTEGER
%token<floatval> T_FLOAT
%token T_MINUS T_PLUS T_STAR T_SLASH T_PERCENT
%token T_OPEN_PAREN T_CLOSE_PAREN
%token T_OPEN_CURLY T_CLOSE_CURLY
%token T_SEMICOLON T_UNKNOWN

%start one_unit

%%

one_unit
	: statement
	| one_unit statement
	;

statement
	: T_SEMICOLON
	| expression_list T_SEMICOLON
	;

expression_list
	: expression
	| expression_list expression
	;

expression
	: T_INTEGER
	| T_FLOAT
	;

%%

