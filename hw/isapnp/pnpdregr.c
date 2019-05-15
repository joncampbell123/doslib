
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

uint8_t isa_pnp_read_data_register(uint8_t x) {
	isa_pnp_write_address(x);
	return isa_pnp_read_data();
}

#endif //!defined(TARGET_PC98)

