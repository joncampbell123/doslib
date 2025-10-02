
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

std::string floating_value_t::to_str(void) const {
	std::string s;
	char tmp[256];

	if (!(flags & FL_NAN)) {
		sprintf(tmp,"v=0x%llx e=%ld ",(unsigned long long)mantissa,(long)exponent);
		s += tmp;

		sprintf(tmp,"%.20f",ldexp(double(mantissa),(int)exponent-63));
		s += tmp;
	}
	else {
		s += "NaN";
	}

	s += " t=\"";
	switch (type) {
		case type_t::NONE:       s += "none"; break;
		case type_t::FLOAT:      s += "float"; break;
		case type_t::DOUBLE:     s += "double"; break;
		case type_t::LONGDOUBLE: s += "long double"; break;
		default: break;
	}
	s += "\"";

	if (flags & FL_NEGATIVE) s += " negative";
	if (flags & FL_OVERFLOW) s += " overflow";

	return s;
}

void floating_value_t::init(void) { flags = 0; mantissa=0; exponent=0; type=type_t::DOUBLE; }

void floating_value_t::setsn(const uint64_t m,const int32_t e) {
	mantissa = m;
	exponent = e+63;
	normalize();
}

void floating_value_t::normalize(void) {
	if (mantissa != uint64_t(0)) {
		while (!(mantissa & mant_msb)) {
			mantissa <<= uint64_t(1ull);
			exponent--;
		}
	}
	else {
		exponent = 0;
	}
}

