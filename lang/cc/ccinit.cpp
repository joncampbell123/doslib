
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

bool cc_init(void) {
	assert(scopes.empty());
	assert(scope_global == scope_id_t(0));
	scopes.resize(scope_global+1); // make sure global scope exists

	assert(scope_stack.empty());
	scope_stack.push_back(scope_global);

	/* NTS: GCC x86_64 doesn't enforce a maximum packing by default, only if you use #pragma pack
	 *      Microsoft C++ uses a default maximum according to /Zp or the default of 8 */
	current_packing = default_packing;

	{
		const segment_id_t s = code_segment = new_segment(); segment_t &so = segref(s);
		const identifier_id_t i = so.name = identifier.alloc(); identifier_t &io = identifier(i);
		so.type = segment_t::type_t::CODE;
		so.flags = segment_t::FL_READABLE | segment_t::FL_EXECUTABLE;
		default_segment_setup(so);
		io.copy_from("CODE");
	}

	{
		const segment_id_t s = const_segment = new_segment(); segment_t &so = segref(s);
		const identifier_id_t i = so.name = identifier.alloc(); identifier_t &io = identifier(i);
		so.type = segment_t::type_t::CONST;
		so.flags = segment_t::FL_READABLE;
		default_segment_setup(so);
		io.copy_from("CONST");
	}

	{
		const segment_id_t s = conststr_segment = new_segment(); segment_t &so = segref(s);
		const identifier_id_t i = so.name = identifier.alloc(); identifier_t &io = identifier(i);
		so.type = segment_t::type_t::CONST;
		so.flags = segment_t::FL_READABLE;
		default_segment_setup(so);
		io.copy_from("CONST:str");
	}

	{
		const segment_id_t s = data_segment = new_segment(); segment_t &so = segref(s);
		const identifier_id_t i = so.name = identifier.alloc(); identifier_t &io = identifier(i);
		so.type = segment_t::type_t::DATA;
		so.flags = segment_t::FL_READABLE | segment_t::FL_WRITEABLE;
		default_segment_setup(so);
		io.copy_from("DATA");
	}

	{
		const segment_id_t s = stack_segment = new_segment(); segment_t &so = segref(s);
		const identifier_id_t i = so.name = identifier.alloc(); identifier_t &io = identifier(i);
		so.type = segment_t::type_t::STACK;
		so.flags = segment_t::FL_READABLE | segment_t::FL_WRITEABLE | segment_t::FL_NOTINEXE;
		default_segment_setup(so);
		io.copy_from("STACK");
	}

	{
		const segment_id_t s = bss_segment = new_segment(); segment_t &so = segref(s);
		const identifier_id_t i = so.name = identifier.alloc(); identifier_t &io = identifier(i);
		so.type = segment_t::type_t::BSS;
		so.flags = segment_t::FL_READABLE | segment_t::FL_WRITEABLE | segment_t::FL_NOTINEXE;
		default_segment_setup(so);
		io.copy_from("BSS");
	}

	{
		const segment_id_t s = fardata_segment = new_segment(); segment_t &so = segref(s);
		const identifier_id_t i = so.name = identifier.alloc(); identifier_t &io = identifier(i);
		so.type = segment_t::type_t::DATA;
		so.flags = segment_t::FL_READABLE | segment_t::FL_WRITEABLE;
		default_segment_setup(so);
		io.copy_from("FARDATA");
	}

	return true;
}

