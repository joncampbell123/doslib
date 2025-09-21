
extern "C" {
#include "c11.h"
}

#include "c11.hpp"

std::vector<struct c11yy_string_obj> c11yy_string_table;

void c11yy_string_table_clear(void) {
	for (auto &st : c11yy_string_table) c11yy_string_obj_free(st);
	c11yy_string_table.clear();
}

