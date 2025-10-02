
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

bool pointer_t::operator==(const pointer_t &o) const {
	return tq == o.tq;
}

bool pointer_t::operator!=(const pointer_t &o) const {
	return !(*this == o);
}

