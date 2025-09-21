
#include "c11.h"
#include "c11.hpp"

void c11yy_iconst_read(const unsigned int base,struct c11yy_struct_integer &val,const char* &s) {
	const uint64_t maxv = UINT64_MAX / (uint64_t)(base);
	int v;

	val.v.u = 0;
	while (*s) {
		if ((v=c11yy_iconst_readc(base,s)) < 0)
			break;

		if (val.v.u > maxv)
			val.flags |= C11YY_INTF_OVERFLOW;

		val.v.u = (val.v.u * ((uint64_t)base)) + ((uint64_t)(unsigned int)v);
	}
}

