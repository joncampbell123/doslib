
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

declarator_t &declaration_t::new_declarator(void) {
	const size_t i = declor.size();
	declor.resize(i+1);
	return declor[i];
}

declaration_t::declaration_t() { }
declaration_t::declaration_t(declaration_t &&x) { common_move(x); }
declaration_t &declaration_t::operator=(declaration_t &&x) { common_move(x); return *this; }

void declaration_t::common_move(declaration_t &o) {
	spec = std::move(o.spec);
	declor = std::move(o.declor);
}

declaration_t::~declaration_t() {
}

