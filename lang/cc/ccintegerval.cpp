
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

std::string integer_value_t::to_str(void) const {
	std::string s;
	char tmp[192];

	s += "v=";
	if (flags & FL_SIGNED) sprintf(tmp,"%lld/0x%llx",(signed long long)v.s,(unsigned long long)v.u);
	else sprintf(tmp,"%llu/0x%llx",(unsigned long long)v.u,(unsigned long long)v.u);
	s += tmp;

	s += " t=\"";
	switch (type) {
		case type_t::NONE:       s += "none"; break;
		case type_t::BOOL:       s += "bool"; break;
		case type_t::CHAR:       s += "char"; break;
		case type_t::SHORT:      s += "short"; break;
		case type_t::INT:        s += "int"; break;
		case type_t::LONG:       s += "long"; break;
		case type_t::LONGLONG:   s += "longlong"; break;
		default: break;
	}
	s += "\"";

	if (flags & FL_SIGNED) s += " signed";
	if (flags & FL_OVERFLOW) s += " overflow";

	return s;
}

void integer_value_t::init(void) { flags = FL_SIGNED; type=type_t::INT; v.s=0; }

bool integer_value_t::to_bool(void) const { return v.u != 0ull; }

