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

void vga_select_charset_a_b(unsigned short a,unsigned short b) {
	unsigned char c;

	c  =  a >> 14;
	c |= (b >> 14) << 2;
	c |= ((a >> 13) & 1) << 4;
	c |= ((b >> 13) & 1) << 5;

	vga_write_sequencer(3,c);
}

