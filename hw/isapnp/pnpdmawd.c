
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

void isa_pnp_write_dma(unsigned int i,int dma) {
	if (i >= 2) return;
	if (dma < 0) dma = 4;
	isa_pnp_write_data_register(0x74 + i,dma);
	t8254_wait(t8254_us2ticks(250));
}

#endif //!defined(TARGET_PC98)

