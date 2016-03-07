
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

void isa_pnp_wake_csn(unsigned char id) {
	isa_pnp_write_address(3); /* Wake[CSN] */
	isa_pnp_write_data(id); /* isolation state */
}

