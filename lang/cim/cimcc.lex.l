
%{

#include <stdio.h>

#define yyparse cimcc_yyparse
#define yylval cimcc_yylval
#define yywrap cimcc_yyrap
#define yylex cimcc_yylex

#include "cimcc.tab.h"

%}

%%

[ \t]                                 // ignore whitespace
[0-9]+\.[0-9]+                        { yylval.floatval = strtold(yytext,NULL); yylval.flags = 0; return T_FLOAT; }      // float constant
0[xX][0-9a-fA-F]+                     { yylval.intval = strtoull(yytext,NULL,16); yylval.flags = 0; return T_INTEGER; }  // hexadecimal integer constant
0[0-6]+                               { yylval.intval = strtoull(yytext,NULL, 8); yylval.flags = 0; return T_INTEGER; }  // octal integer constant
0[bB][01]+                            { yylval.intval = strtoull(yytext,NULL, 8); yylval.flags = 0; return T_INTEGER; }  // binary integer constant
[0-9]+                                { yylval.intval = strtoull(yytext,NULL,10); yylval.flags = 0; return T_INTEGER; }  // decimal integer constant

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

