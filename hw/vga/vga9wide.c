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

#ifdef TARGET_WINDOWS
# include <hw/dos/winfcon.h>
# include <windows/apihelp.h>
# include <windows/dispdib/dispdib.h>
# include <windows/win16eb/win16eb.h>
#endif

/* NTS: this also completes the change by setting the clock select bits in Misc. out register
 *      on the assumption that you are changing 8/9 bits in 80x25 alphanumeric mode. The
 *      clock change is necessary to retain the proper hsync/vsync rates on the VGA monitor. */
void vga_set_9wide(unsigned char en) {
	unsigned char c;

	if (en == vga_state.vga_9wide)
		return;

	c = vga_read_sequencer(1);
	c &= 0xFE;
	c |= en ^ 1;
	vga_write_sequencer(1,c);
	vga_state.vga_9wide = en;

	c = inp(0x3CC);
	c &= 0xF3;
	c |= (en ? 1 : 0) << 2;	/* 0=25MHz 1=28MHz */
	outp(0x3C2,c);
}

