
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

bool pptok_define_asm_allowed_token(const token_t &t) {
	switch (t.type) {
		/* NTS: lgtok() parsing pretty much prevents these tokens entirely within a #define.
		 *      even so, make sure! */
		case token_type_t::r_ppif:
		case token_type_t::r_ppifdef:
		case token_type_t::r_ppelse:
		case token_type_t::r_ppelif:
		case token_type_t::r_ppelifdef:
		case token_type_t::r_ppendif:
		case token_type_t::r_ppifndef:
		case token_type_t::r_ppelifndef:
			return false;
		default:
			break;
	}

	return true;
}

bool pptok_define_allowed_token(const token_t &t) {
	switch (t.type) {
		/* NTS: lgtok() parsing pretty much prevents these tokens entirely within a #define.
		 *      even so, make sure! */
		case token_type_t::r_ppif:
		case token_type_t::r_ppifdef:
		case token_type_t::r_ppdefine:
		case token_type_t::r_ppundef:
		case token_type_t::r_ppelse:
		case token_type_t::r_ppelif:
		case token_type_t::r_ppelifdef:
		case token_type_t::r_ppendif:
		case token_type_t::r_ppifndef:
		case token_type_t::r_ppelifndef:
		case token_type_t::r_ppinclude:
		case token_type_t::r_pperror:
		case token_type_t::r_ppwarning:
		case token_type_t::r_ppline:
		case token_type_t::r_pppragma:
		case token_type_t::r_ppembed:
			return false;
		default:
			break;
	}

	return true;
}

