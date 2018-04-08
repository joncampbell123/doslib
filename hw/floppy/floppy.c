
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

struct floppy_controller        floppy_controllers[MAX_FLOPPY_CONTROLLER];
int8_t                          floppy_controllers_init = -1;

/* standard I/O ports for floppy controllers */
struct floppy_controller		floppy_standard_isa[2] = {
	/*base, IRQ, DMA*/
	{0x3F0, 6,   2},
	{0x370, 6,   2}
};

struct floppy_controller *floppy_get_standard_isa_port(int x) {
	if (x < 0 || x >= (sizeof(floppy_standard_isa)/sizeof(floppy_standard_isa[0]))) return NULL;
    if (x != 0 && !floppy_controllers_enable_2nd) return NULL;
	return &floppy_standard_isa[x];
}

struct floppy_controller *floppy_get_controller(int x) {
	if (x < 0 || x >= MAX_FLOPPY_CONTROLLER) return NULL;
	if (floppy_controllers[x].base_io == 0) return NULL;
	return &floppy_controllers[x];
}

