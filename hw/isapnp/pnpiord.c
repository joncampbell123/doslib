
#if !defined(TARGET_PC98)

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

int isa_pnp_read_io_resource(unsigned int i) {
	uint16_t ret;

	if (i >= 8) return -1;
	ret  = (uint16_t)isa_pnp_read_data_register(0x60 + (i*2)) << 8;
	ret |= (uint16_t)isa_pnp_read_data_register(0x60 + (i*2) + 1);
	return ret;
}

#endif //!defined(TARGET_PC98)

