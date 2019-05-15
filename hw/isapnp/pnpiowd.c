
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

void isa_pnp_write_io_resource(unsigned int i,uint16_t port) {
	if (i >= 8) return;
	isa_pnp_write_data_register(0x60 + (i*2),port >> 8);
	isa_pnp_write_data_register(0x60 + (i*2) + 1,port);
	t8254_wait(t8254_us2ticks(250));
}

#endif //!defined(TARGET_PC98)

