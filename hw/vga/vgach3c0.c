#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <malloc.h>
#include <fcntl.h>
#include <ctype.h>
#include <i86.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/vga/vga.h>
#include <hw/dos/doswin.h>

int check_vga_3C0() {
	unsigned char mor;

	/* Misc. output register test. This register is supposed to be readable at 0x3CC, writeable at 0x3C2,
	 * and contain 7 documented bits that we can freely change */
	_cli();
	mor = inp(0x3CC);
	outp(0x3C2,0);	/* turn it all off */
	if ((inp(0x3CC)&0xEF) != 0) { /* NTS: do not assume bit 4 is changeable */
		/* well if I couldn't change it properly it isn't the Misc. output register now is it? */
		outp(0x3C2,mor); /* restore it */
		return 0;
	}
	outp(0x3C2,0xEF);	/* turn it all on (NTS: bit 4 is undocumented) */
	if ((inp(0x3CC)&0xEF) != 0xEF) { /* NTS: do not assume bit 4 is changeable */
		/* well if I couldn't change it properly it isn't the Misc. output register now is it? */
		outp(0x3C2,mor); /* restore it */
		return 0;
	}
	outp(0x3C2,mor);
	_sti();

	return 1;
}

