
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <math.h>

#include "cc.hpp"

bool is_newline(const unsigned char b) {
	return b == '\r' || b == '\n';
}

bool is_whitespace(const unsigned char b) {
	return b == ' ' || b == '\t';
}

int cc_parsedigit(unsigned char c,const unsigned char base) {
	if (c >= '0' && c <= '9')
		return c - '0';
	else if (c >= 'a' && c < ('a'+base-10))
		return c + 10 - 'a';
	else if (c >= 'A' && c < ('A'+base-10))
		return c + 10 - 'A';

	return -1;
}

bool is_identifier_first_char(unsigned char c) {
	return (c == '_') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool is_identifier_char(unsigned char c) {
	return (c == '_') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

bool is_asm_non_ident_text_char(unsigned char c) {
	return !(c == '{' || c == '}' || c == '\n' || c == '\'' || c == '\"' || is_whitespace(c) || isdigit(c) || is_identifier_first_char(c));
}

bool is_asm_ident_text_char(unsigned char c) {
	return c == '.' || isdigit(c) || is_identifier_char(c);
}

bool is_hchar(unsigned char c) {
	return c != '>' && !is_newline(c);
}

