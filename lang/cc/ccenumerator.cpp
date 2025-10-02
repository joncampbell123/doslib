
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

enumerator_t::enumerator_t() { }
enumerator_t::enumerator_t(const enumerator_t &x) { common_copy(x); }
enumerator_t &enumerator_t::operator=(const enumerator_t &x) { common_copy(x); return *this; }
enumerator_t::enumerator_t(enumerator_t &&x) { common_move(x); }
enumerator_t &enumerator_t::operator=(enumerator_t &&x) { common_move(x); return *this; }

void enumerator_t::common_copy(const enumerator_t &o) {
	identifier.assign(/*to*/name,/*from*/o.name);
	ast_node.assign(/*to*/expr,/*from*/o.expr);
	pos = o.pos;
}

void enumerator_t::common_move(enumerator_t &o) {
	identifier.assignmove(/*to*/name,/*from*/o.name);
	ast_node.assignmove(/*to*/expr,/*from*/o.expr);
	pos = std::move(o.pos);
}

enumerator_t::~enumerator_t() {
	identifier.release(name);
	ast_node.release(expr);
}

