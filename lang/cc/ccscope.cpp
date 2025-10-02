
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

scope_t::scope_t() { }
scope_t::scope_t(scope_t &&x) { common_move(x); }
scope_t &scope_t::operator=(scope_t &&x) { common_move(x); return *this; }

scope_t::~scope_t() {
	ast_node.release(root);
}

scope_t::decl_t &scope_t::new_localdecl(void) {
	const size_t r = localdecl.size();
	localdecl.resize(r+1u);
	return localdecl[r];
}

void scope_t::common_move(scope_t &x) {
	ast_node.assignmove(/*to*/root,/*from*/x.root);
	localdecl = std::move(x.localdecl);
	symbols = std::move(x.symbols);
}

