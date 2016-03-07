
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <fcntl.h>
#include <dos.h>

#include <dos/dos.h>
#include <isapnp/isapnp.h>

#include <hw/8254/8254.h>		/* 8254 timer */

static const char *isapnp_sdf_priority_strs[3] = {
	"Good",
	"Acceptable",
	"Sub-optimal"
};

const char *isapnp_sdf_priority_str(uint8_t x) {
	if (x >= 3) return NULL;
	return isapnp_sdf_priority_strs[x];
}

