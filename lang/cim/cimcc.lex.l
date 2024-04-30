
DECIMAL [0-9]
HEXADECIMAL [0-9a-fA-F]
OCTAL [0-6]
BINARY [0-1]
FLOATSUFFIX (f|F|d|D)
POINTFLOATSUFFIX (f|F|u|U|l|L)
INTSUFFIX (u|U|l|L|ll|LL)

DECIMAL_FLOAT_CONSTANT {DECIMAL}+\.{DECIMAL}+{POINTFLOATSUFFIX}?
DECIMAL_FLOAT_CONSTANT_BY_SUFFIX {DECIMAL}+{FLOATSUFFIX}+
HEXADECIMAL_CONSTANT 0[xX]{HEXADECIMAL}+{INTSUFFIX}?
BINARY_CONSTANT 0[bB]{BINARY}+{INTSUFFIX}?
DECIMAL_CONSTANT {DECIMAL}+{INTSUFFIX}?
OCTAL_CONSTANT 0{OCTAL}+{INTSUFFIX}? 

%{

#include <stdio.h>

#define yyparse cimcc_yyparse
#define yylval cimcc_yylval
#define yywrap cimcc_yyrap
#define yylex cimcc_yylex

#include "cimcc.tab.h"

%}

%option noinput

%{

void (*cimcc_read_input)(char *buffer,int *size,int max_size);
#define YY_INPUT(buffer, size, max_size) cimcc_read_input(buffer, &(size), max_size)

%}

%%

[ \t]                                 // ignore whitespace
{DECIMAL_FLOAT_CONSTANT}                  { yylval.floatval = strtold(yytext,NULL); yylval.flags = 0; return T_FLOAT; }      // float constant
{DECIMAL_FLOAT_CONSTANT_BY_SUFFIX}        { yylval.floatval = strtold(yytext,NULL); yylval.flags = 0; return T_FLOAT; }      // float constant
{HEXADECIMAL_CONSTANT}                    { yylval.intval = strtoull(yytext,NULL,16); yylval.flags = 0; return T_INTEGER; }  // hexadecimal integer constant
{OCTAL_CONSTANT}                          { yylval.intval = strtoull(yytext,NULL, 8); yylval.flags = 0; return T_INTEGER; }  // octal integer constant
{BINARY_CONSTANT}                         { yylval.intval = strtoull(yytext,NULL, 2); yylval.flags = 0; return T_INTEGER; }  // binary integer constant
{DECIMAL_CONSTANT}                        { yylval.intval = strtoull(yytext,NULL,10); yylval.flags = 0; return T_INTEGER; }  // decimal integer constant

\/\/[^\n]*                            // ignore C++ style comment

"-"                                   { return T_MINUS; } // negate or subtract
"+"                                   { return T_PLUS; } // add
"*"                                   { return T_STAR; } // pointer deref or multiply
"/"                                   { return T_SLASH; } // divide
"%"                                   { return T_PERCENT; } // modulo
"("                                   { return T_OPEN_PAREN; }
")"                                   { return T_CLOSE_PAREN; }
"{"                                   { return T_OPEN_CURLY; }
"}"                                   { return T_CLOSE_CURLY; }
";"                                   { return T_SEMICOLON; }

%%

