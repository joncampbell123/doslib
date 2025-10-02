
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

ddip_t::ddip_t() { }
ddip_t::ddip_t(const ddip_t &x) { common_copy(x); }
ddip_t &ddip_t::operator=(const ddip_t &x) { common_copy(x); return *this; }
ddip_t::ddip_t(ddip_t &&x) { common_move(x); }
ddip_t &ddip_t::operator=(ddip_t &&x) { common_move(x); return *this; }

ddip_t::~ddip_t() {
	ast_node_t::arrayrelease(arraydef);
}

void ddip_t::common_copy(const ddip_t &o) {
	ptr = o.ptr;
	ast_node_t::arraycopy(arraydef,o.arraydef);
	parameters = o.parameters;
	dd_flags = o.dd_flags;
}

void ddip_t::common_move(ddip_t &o) {
	ptr = std::move(o.ptr);
	arraydef = std::move(o.arraydef);
	parameters = std::move(o.parameters);
	dd_flags = o.dd_flags; o.dd_flags = 0;
}

bool ddip_t::empty(void) const {
	return ptr.empty() && arraydef.empty() && parameters.empty() && dd_flags == 0;
}

