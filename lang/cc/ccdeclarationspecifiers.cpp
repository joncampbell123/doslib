
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

bool declaration_specifiers_t::empty(void) const {
	return 	storage_class == 0 && type_specifier == 0 && type_qualifier == 0 &&
		type_identifier_symbol == symbol_none && enum_list.empty() && count == 0 &&
		align == addrmask_none && dcs_flags == 0 && size == data_size_none;
}

declaration_specifiers_t::declaration_specifiers_t() { }
declaration_specifiers_t::declaration_specifiers_t(const declaration_specifiers_t &x) { common_copy(x); }
declaration_specifiers_t &declaration_specifiers_t::operator=(const declaration_specifiers_t &x) { common_copy(x); return *this; }
declaration_specifiers_t::declaration_specifiers_t(declaration_specifiers_t &&x) { common_move(x); }
declaration_specifiers_t &declaration_specifiers_t::operator=(declaration_specifiers_t &&x) { common_move(x); return *this; }

declaration_specifiers_t::~declaration_specifiers_t() {
}

void declaration_specifiers_t::common_move(declaration_specifiers_t &o) {
	storage_class = o.storage_class; o.storage_class = 0;
	type_specifier = o.type_specifier; o.type_specifier = 0;
	type_qualifier = o.type_qualifier; o.type_qualifier = 0;
	type_identifier_symbol = o.type_identifier_symbol;
	size = o.size; o.size = data_size_none;
	align = o.align; o.align = addrmask_none;
	dcs_flags = o.dcs_flags; o.dcs_flags = 0;
	enum_list = std::move(o.enum_list);
	count = o.count; o.count = 0;
}

void declaration_specifiers_t::common_copy(const declaration_specifiers_t &o) {
	storage_class = o.storage_class;
	type_specifier = o.type_specifier;
	type_qualifier = o.type_qualifier;
	type_identifier_symbol = o.type_identifier_symbol;
	size = o.size;
	align = o.align;
	dcs_flags = o.dcs_flags;
	enum_list = o.enum_list;
	count = o.count;
}

