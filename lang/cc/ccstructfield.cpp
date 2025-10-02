
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

structfield_t::structfield_t() { }
structfield_t::structfield_t(structfield_t &&x) { common_move(x); }
structfield_t &structfield_t::operator=(structfield_t &&x) { common_move(x); return *this; }

structfield_t::~structfield_t() {
	identifier.release(name);
}

void structfield_t::common_move(structfield_t &x) {
	spec = std::move(x.spec);
	ddip = std::move(x.ddip);
	identifier.assignmove(/*to*/name,/*from*/x.name);
	offset = x.offset;
	bf_start = x.bf_start;
	bf_length = x.bf_length;
}

