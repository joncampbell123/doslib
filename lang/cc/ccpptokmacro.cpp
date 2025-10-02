
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

pptok_macro_t::pptok_macro_t() { }
pptok_macro_t::pptok_macro_t(pptok_macro_t &&x) { common_move(x); }
pptok_macro_t &pptok_macro_t::operator=(pptok_macro_t &&x) { common_move(x); return *this; }
pptok_macro_t::~pptok_macro_t() {
	for (auto &pid : parameters) identifier.release(pid);
	parameters.clear();
}

void pptok_macro_t::common_move(pptok_macro_t &o) {
	flags = o.flags; o.flags = 0;
	tokens = std::move(o.tokens);
	parameters = std::move(o.parameters);
}

