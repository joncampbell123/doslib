
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

void isa_pnp_product_id_to_str(char *str,unsigned long id) {
	sprintf(str,"%c%c%c%X%X%X%X",
		(unsigned char)(0x40+((id>>2)&0x1F)),
		(unsigned char)(0x40+((id&3)<<3)+((id>>13)&7)),
		(unsigned char)(0x40+((id>>8)&0x1F)),
		(unsigned char)((id>>20)&0xF),
		(unsigned char)((id>>16)&0xF),
		(unsigned char)((id>>28)&0xF),
		(unsigned char)((id>>24)&0xF));
}

