
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

int isa_pnp_read_dma(unsigned int i) {
	uint8_t c;

	if (i >= 2) return -1;
	c = isa_pnp_read_data_register(0x74 + i);
	if (c == 0xFF) return -1;
	if ((c & 7) == 4) return -1;	/* not assigned */
	return (c & 7);
}

#endif //!defined(TARGET_PC98)

