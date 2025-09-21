
#include <stdio.h>
#include <assert.h>

extern "C" {
#include "c11.h"
#include "c11.l.h"
#include "c11.y.h"
}

#include "c11.hpp"

////////////////////////////////////////////////////////////////////

static const uint16_t c11yy_itype_to_token[C11YY_IDT__MAX] = {
	IDENTIFIER,				// C11YY_IDT_NONE = 0
	IDENTIFIER,				// C11YY_IDT_IDENTIFIER
	TYPEDEF_NAME,				// C11YY_IDT_TYPEDEF_NAME
	ENUMERATION_CONSTANT			// C11YY_IDT_ENUMERATION_CONSTANT
};

extern "C" struct c11yy_identifier_obj *c11yy_init_ident(const char *yytext,int yyleng) {
	return NULL;
}

extern "C" int c11yy_check_type(const struct c11yy_identifier_obj *io) {
	if (io && io->itype < C11YY_IDT__MAX) return c11yy_itype_to_token[io->itype];
	return IDENTIFIER;
}

////////////////////////////////////////////////////////////////////

static int c11yy_init(void) {
	return 0;
}

static void c11yy_cleanup(void) {
}

/////////////////////////////////////////////////////////////

int main() {
	if (c11yy_init()) {
		c11yy_cleanup();
		return 1;
	}

	if (c11yy_do_compile()) {
		c11yylex_destroy();
		c11yy_cleanup();
		return 1;
	}

	c11yylex_destroy();
	c11yy_cleanup();
	return 0;
}

