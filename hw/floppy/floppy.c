
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <ctype.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/vga/vga.h>
#include <hw/dos/dos.h>
#include <hw/8237/8237.h>		/* DMA controller */
#include <hw/8254/8254.h>		/* 8254 timer */
#include <hw/8259/8259.h>		/* 8259 PIC interrupts */
#include <hw/vga/vgagui.h>
#include <hw/vga/vgatty.h>
#include <hw/dos/doswin.h>
#include <hw/floppy/floppy.h>

int8_t                          floppy_controllers_enable_2nd = 1;

/* standard I/O ports for floppy controllers */
struct floppy_controller		floppy_standard_isa[2] = {
	/*base, IRQ, DMA*/
	{0x3F0, 6,   2},
	{0x370, 6,   2}
};

