
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

symbol_t::symbol_t() { }
symbol_t::symbol_t(symbol_t &&x) { common_move(x); }
symbol_t &symbol_t::operator=(symbol_t &&x) { common_move(x); return *this; }

symbol_t::~symbol_t() {
	ast_node.release(expr);
	identifier.release(name);
}

void symbol_t::common_move(symbol_t &x) {
	spec = std::move(x.spec);
	ddip = std::move(x.ddip);
	fields = std::move(x.fields);
	identifier.assignmove(/*to*/name,/*from*/x.name);
	ast_node.assignmove(/*to*/expr,/*from*/x.expr);
	scope = x.scope; x.scope = scope_none;
	parent_of_scope = x.parent_of_scope; x.parent_of_scope = scope_none;
	part_of_segment = x.part_of_segment; x.part_of_segment = segment_none;
	sym_type = x.sym_type; x.sym_type = NONE;
	flags = x.flags; x.flags = 0;
}

bool symbol_t::identifier_exists(const identifier_id_t &id) {
	if (id != identifier_none) {
		for (const auto &f : fields) {
			if (f.name != identifier_none) {
				if (identifier(f.name) == identifier(id))
					return true;
			}
		}
	}

	return false;
}

