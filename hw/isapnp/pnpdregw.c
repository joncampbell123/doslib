
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

void isa_pnp_write_data_register(uint8_t x,uint8_t data) {
	isa_pnp_write_address(0x00);
	isa_pnp_write_address(x);
	isa_pnp_write_data(data);
}

