
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

CONST_PREFIX -?
NONCONST_PREFIX -?

%{

#include <stdio.h>

#include "cimcc.lex.h"
#include "cimcc.tab.h"

%}

%option noinput

%{

void cimcc_parse_int_const(char *buffer,unsigned long long *val,unsigned int *flags,int base);
void cimcc_parse_float_const(char *buffer,long double *val,unsigned int *flags);

void (*cimcc_read_input)(char *buffer,int *size,int max_size);
#define YY_INPUT(buffer, size, max_size) cimcc_read_input(buffer, &(size), max_size)

%}

%%

[ \t]                                                   // ignore whitespace
{CONST_PREFIX}{DECIMAL_FLOAT_CONSTANT}                  { cimcc_parse_float_const(cimcc_yytext,&cimcc_yylval.floatval,&cimcc_yylval.flags); return T_FLOAT; }   // float
{CONST_PREFIX}{DECIMAL_FLOAT_CONSTANT_BY_SUFFIX}        { cimcc_parse_float_const(cimcc_yytext,&cimcc_yylval.floatval,&cimcc_yylval.flags); return T_FLOAT; }   // float
{CONST_PREFIX}{HEXADECIMAL_CONSTANT}                    { cimcc_parse_int_const(cimcc_yytext,&cimcc_yylval.intval,&cimcc_yylval.flags,16); return T_INTEGER; }  // hexadecimal int
{CONST_PREFIX}{OCTAL_CONSTANT}                          { cimcc_parse_int_const(cimcc_yytext,&cimcc_yylval.intval,&cimcc_yylval.flags, 8); return T_INTEGER; }  // octal int
{CONST_PREFIX}{BINARY_CONSTANT}                         { cimcc_parse_int_const(cimcc_yytext,&cimcc_yylval.intval,&cimcc_yylval.flags, 2); return T_INTEGER; }  // binary int
{CONST_PREFIX}{DECIMAL_CONSTANT}                        { cimcc_parse_int_const(cimcc_yytext,&cimcc_yylval.intval,&cimcc_yylval.flags,10); return T_INTEGER; }  // decimal int

\/\/[^\n]*                            // ignore C++ style comment

"-"                                   { return T_MINUS; } // negate or subtract
"+"                                   { return T_PLUS; } // add
"*"                                   { return T_STAR; } // pointer deref or multiply
"/"                                   { return T_SLASH; } // divide
"%"                                   { return T_PERCENT; } // modulo
{NONCONST_PREFIX}"("                  { cimcc_yylval.flags = 0; if (*cimcc_yytext == '-') cimcc_yylval.flags |= CIMCC_YYFL_NEGATE; return T_OPEN_PAREN; }
")"                                   { return T_CLOSE_PAREN; }
"{"                                   { return T_OPEN_CURLY; }
"}"                                   { return T_CLOSE_CURLY; }
";"                                   { return T_SEMICOLON; }

%%

