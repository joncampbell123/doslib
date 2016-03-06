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

uint32_t vga_clock_rates[4] = {
	25175000UL,
	(25175000UL*9UL)/8UL,	/* ~28MHz */
/* if you know your SVGA chipset's correct values you may overwrite entries 2 and 3 */
	0,
	0
};

