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

#if defined(TARGET_PC98)
/*nothing*/
#else

void vga_bios_set_80x50_text() { /* switch to VGA 80x50 8-line high text */
#if defined(TARGET_WINDOWS)
#else
	union REGS regs = {0};
	regs.w.ax = 0x1112;
	regs.w.bx = 0;
# if TARGET_MSDOS == 32
	int386(0x10,&regs,&regs);
# else
	int86(0x10,&regs,&regs);
# endif
	vga_state.vga_height = 50;
#endif
}

#endif

