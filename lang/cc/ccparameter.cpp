
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

parameter_t::parameter_t() { }
parameter_t::parameter_t(const parameter_t &x) { common_copy(x); }
parameter_t &parameter_t::operator=(const parameter_t &x) { common_copy(x); return *this; }
parameter_t::parameter_t(parameter_t &&x) { common_move(x); }
parameter_t &parameter_t::operator=(parameter_t &&x) { common_move(x); return *this; }
parameter_t::~parameter_t() { }

void parameter_t::common_copy(const parameter_t &o) {
	spec = o.spec;
	decl = o.decl;
}

void parameter_t::common_move(parameter_t &o) {
	spec = std::move(o.spec);
	decl = std::move(o.decl);
}

