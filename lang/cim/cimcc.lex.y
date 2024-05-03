%{

#include "cimcc.lex.h"
#include "cimcc.tab.h"

extern int cimcc_yylex(YYSTYPE *,yyscan_t *scanner);
extern int cimcc_yyparse(yyscan_t *scanner);

void cimcc_yyerror(yyscan_t *scanner,const char *s);

%}

%define api.value.type {union cimcc_yystype_t}
%define api.pure full
%lex-param {yyscan_t *scanner}
%parse-param {yyscan_t *scanner}

%token T_INTEGER T_FLOAT T_EOF
%token T_MINUS T_PLUS T_STAR T_SLASH T_PERCENT
%token T_OPEN_PAREN T_CLOSE_PAREN
%token T_OPEN_CURLY T_CLOSE_CURLY
%token T_SEMICOLON T_UNKNOWN T_COMMA
%token T_AMPERSAND T_TILDE T_EXCLAMATION

%start one_unit

%%

one_unit
	: statement
	| one_unit statement
	;

statement
	: expression_statement
	;

expression_statement
	: T_EOF { YYACCEPT; } /* STOP PARSING NOW, NOT AN ERROR */
	| T_SEMICOLON
	| expression T_SEMICOLON
	;

expression
	: assignment_expression
	| expression T_COMMA assignment_expression
	;

assignment_expression
	: conditional_expression
	;

conditional_expression
	: logical_or_expression
	;

logical_or_expression
	: logical_and_expression
	;

logical_and_expression
	: inclusive_or_expression
	;

inclusive_or_expression
	: exclusive_or_expression
	;

exclusive_or_expression
	: and_expression
	;

and_expression
	: equality_expression
	;

equality_expression
	: relational_expression
	;

relational_expression
	: shift_expression
	;

shift_expression
	: additive_expression
	;

additive_expression
	: multiplicative_expression
	;

multiplicative_expression
	: cast_expression
	;

cast_expression
	: unary_expression
	;

unary_operator
	: T_AMPERSAND
	| T_STAR
	| T_PLUS
	| T_MINUS
	| T_TILDE
	| T_EXCLAMATION
	;

unary_expression
	: postfix_expression
	| unary_operator cast_expression
	;

postfix_expression
	: primary_expression
	;

primary_expression
	: T_INTEGER
	| T_FLOAT
	| T_OPEN_PAREN expression T_CLOSE_PAREN
	;

%%

