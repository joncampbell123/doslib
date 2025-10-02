
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

declarator_t::declarator_t() { }
declarator_t::declarator_t(const declarator_t &x) { common_copy(x); }
declarator_t &declarator_t::operator=(const declarator_t &x) { common_copy(x); return *this; }
declarator_t::declarator_t(declarator_t &&x) { common_move(x); }
declarator_t &declarator_t::operator=(declarator_t &&x) { common_move(x); return *this; }

void declarator_t::common_copy(const declarator_t &o) {
	ddip = o.ddip;
	identifier.assign(/*to*/name,/*from*/o.name);
	symbol = o.symbol;
	flags = o.flags;
	ast_node.assign(/*to*/expr,/*from*/o.expr);
}

void declarator_t::common_move(declarator_t &o) {
	ddip = std::move(o.ddip);
	identifier.assignmove(/*to*/name,/*from*/o.name);
	symbol = o.symbol; o.symbol = symbol_none;
	flags = o.flags; o.flags = 0;
	ast_node.assignmove(/*to*/expr,/*from*/o.expr);
}

declarator_t::~declarator_t() {
	identifier.release(name);
	ast_node.release(expr);
}

