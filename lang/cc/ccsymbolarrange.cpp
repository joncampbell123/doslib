
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

bool arrange_symbols(void) {
	for (auto &sg : segments) {
		if (sg.align == addrmask_none)
			sg.align = addrmask_make(1);
	}

	for (auto &sym : symbols) {
		if (sym.part_of_segment == segment_none)
			sym.part_of_segment = decide_sym_segment(sym);

		if (sym.part_of_segment != segment_none) {
			segment_t &se = segref(sym.part_of_segment);

			const data_size_t sz = calc_sizeof(sym.spec,sym.ddip);
			const addrmask_t am = calc_alignofmask(sym.spec,sym.ddip);

			if ((sym.flags & symbol_t::FL_DEFINED) &&
					(sym.flags & (symbol_t::FL_PARAMETER|symbol_t::FL_STACK)) == 0 &&
					!(sym.part_of_segment == stack_segment) && sz != data_size_none &&
					am != addrmask_none) {

				if (sym.sym_type == symbol_t::VARIABLE ||
						sym.sym_type == symbol_t::CONST ||
						sym.sym_type == symbol_t::STR) {

					/* stack must align to largest alignment of symbol */
					se.align &= am;
				}
			}
		}
	}

	return true;
}

