
#include <stdlib.h>

extern "C" {
#include "c11.h"
}

#include "c11.hpp"

void c11yy_string_obj_free(struct c11yy_string_obj &o) {
	if (o.str.raw) free(o.str.raw);
	o.str.raw = NULL;
	o.len = 0;
}

