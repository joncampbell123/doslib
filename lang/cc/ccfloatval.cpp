
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

	if (!(flags & FL_SPECIAL)) {
		sprintf(tmp,"v=0x%llx e=%ld ",(unsigned long long)mantissa,(long)exponent);
		s += tmp;

		if (flags & FL_NEGATIVE) s += '-';

		sprintf(tmp,"%.20f",ldexp(double(mantissa),(int)exponent-63));
		s += tmp;
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

	if (flags & FL_SPECIAL)  s += " special";
	if (flags & FL_OVERFLOW) s += " overflow";
	if (flags & FL_ZERO)     s += " zero";

	return s;
}

void floating_value_t::init(void) { flags=FL_ZERO; mantissa=0; exponent=0; type=type_t::DOUBLE; }

void floating_value_t::setsn(const uint64_t m,const int32_t e) {
	if (m) flags &= ~FL_ZERO;
	else flags |= FL_ZERO;
	mantissa = m;
	exponent = e+63;
	normalize();
}

unsigned int floating_value_t::normalize(void) {
	const unsigned int count = normalize_no_exponent_adjust();
	return (exponent -= (int)count);
}

unsigned int floating_value_t::normalize_no_exponent_adjust(void) {
	unsigned int count = 0;

	if (mantissa != uint64_t(0)) {
		while (!(mantissa & mant_msb8)) {
			mantissa <<= uint64_t(8ull);
			count += 8u;
		}
		while (!(mantissa & mant_msb)) {
			mantissa <<= uint64_t(1ull);
			count++;
		}
	}
	else {
		exponent = 0;
	}

	return count;
}

bool floating_value_t::to_bool(void) const {
	if (flags & FL_SPECIAL)
		return false;
	else
		return mantissa != 0ull;
}

