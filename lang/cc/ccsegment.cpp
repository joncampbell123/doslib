
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

segment_t::segment_t() { }
segment_t::segment_t(segment_t &&x) { common_move(x); }
segment_t &segment_t::operator=(segment_t &&x) { common_move(x); return *this; }

segment_t::~segment_t() {
	identifier.release(name);
}

void segment_t::common_move(segment_t &x) {
	align = x.align; x.align = addrmask_none;
	identifier.assignmove(/*to*/name,/*from*/x.name);
	type = x.type; x.type = type_t::NONE;
	use = x.use; x.use = use_t::NONE;
	limit = x.limit; x.limit = addrmask_none;
	flags = x.flags; x.flags = 0;
}

