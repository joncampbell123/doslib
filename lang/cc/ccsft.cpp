
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

bool source_file_t::empty(void) const {
	return refcount == 0 && path.empty();
}

void source_file_t::clear(void) {
	path.clear();
	refcount = 0;
}

